#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ts/ts.h>

// This gets the PRI*64 types
# define __STDC_FORMAT_MACROS 1
# include <inttypes.h>

#include <ironbee/engine.h>
#include <ironbee/plugin.h>
#include <ironbee/config.h>
#include <ironbee/module.h> /* Only needed while config is in here. */
#include <ironbee/provider.h>
#include <ironbee/plugin.h>


static void addr2str(const struct sockaddr *addr, char *str, int *port);

#define ADDRSIZE 48	/* what's the longest IPV6 addr ? */

ib_engine_t DLL_LOCAL *ironbee = NULL;

/* Plugin Structure */
ib_plugin_t DLL_LOCAL ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "ts-ironbee"
};

typedef struct {
  unsigned int init;  /* use as bitfield with fields to be initialised ? */
  ib_conn_t *iconn;

  /* store the IPs here so we can clean them up and not leak memory */
  char *remote_ip;
  char *local_ip;

  TSHttpTxn txnp;

  /* data filtering stuff */
  TSVIO output_vio;
  TSIOBuffer output_buffer;
  TSIOBufferReader output_reader;
} ib_ctx;

/* mod_ironbee uses ib_state_notify_conn_data_[in|out]
 * for both headers and data
 */
typedef struct {
  const char *word;
  int (*hdr_get)(TSHttpTxn, TSMBuffer*, TSMLoc*);
  ib_status_t (*ib_notify)(ib_engine_t*, ib_conndata_t*);
} ironbee_direction;
static ironbee_direction ironbee_direction_req = {
  "request", TSHttpTxnClientReqGet, ib_state_notify_conn_data_in
};
static ironbee_direction ironbee_direction_resp = {
  "response", TSHttpTxnClientRespGet, ib_state_notify_conn_data_out
};



static void ib_ctx_destroy(ib_ctx * data, TSEvent event)
{
  if (data) {
    if (data->output_buffer) {
      TSIOBufferDestroy(data->output_buffer);
      data->output_buffer = NULL;
    }
      data->init = 0;
      if (data->remote_ip)
        TSfree(data->remote_ip);
      if (data->local_ip)
        TSfree(data->local_ip);
      if (data->iconn)
        ib_state_notify_conn_closed(ironbee, data->iconn);
      TSfree(data);
  }
}

static void process_data(TSCont contp)
{
  TSVConn output_conn;
  TSIOBuffer buf_test;
  TSVIO input_vio;
  ib_ctx *data;
  int64_t towrite;
  int64_t avail;

  TSDebug("ironbee", "Entering process_data()");
  /* Get the output (downstream) vconnection where we'll write data to. */

  output_conn = TSTransformOutputVConnGet(contp);

  /* Get the write VIO for the write operation that was performed on
   * ourself. This VIO contains the buffer that we are to read from
   * as well as the continuation we are to call when the buffer is
   * empty. This is the input VIO (the write VIO for the upstream
   * vconnection).
   */
  input_vio = TSVConnWriteVIOGet(contp);

  data = TSContDataGet(contp);
  if (!data->init) {
    data->init = 1;
    data->output_buffer = TSIOBufferCreate();
    data->output_reader = TSIOBufferReaderAlloc(data->output_buffer);
    TSDebug("ironbee", "\tWriting %d bytes on VConn", TSVIONBytesGet(input_vio));
    data->output_vio = TSVConnWrite(output_conn, contp, data->output_reader, INT64_MAX);
  }

  /* test for input data */
  buf_test = TSVIOBufferGet(input_vio);

  if (!buf_test) {
    TSDebug("ironbee", "No more data, finishing");
    TSVIONBytesSet(data->output_vio, TSVIONDoneGet(input_vio));
    TSVIOReenable(data->output_vio);
    /* FIXME - is this right here - can conn data be kept across reqs? */
    data->init = 0;
    data->output_buffer = NULL;
    data->output_reader = NULL;
    data->output_vio = NULL;
    return;
  }

  /* Determine how much data we have left to read. For this null
   * transform plugin this is also the amount of data we have left
   * to write to the output connection.
   */
  towrite = TSVIONTodoGet(input_vio);
  TSDebug("ironbee", "\ttoWrite is %" PRId64 "", towrite);

  if (towrite > 0) {
    /* The amount of data left to read needs to be truncated by
     * the amount of data actually in the read buffer.
     */
    avail = TSIOBufferReaderAvail(TSVIOReaderGet(input_vio));
    TSDebug("ironbee", "\tavail is %" PRId64 "", avail);
    if (towrite > avail) {
      towrite = avail;
    }

    if (towrite > 0) {
      int btowrite = towrite;
      /* Copy the data from the read buffer to the output buffer. */
      TSIOBufferCopy(TSVIOBufferGet(data->output_vio), TSVIOReaderGet(input_vio), towrite, 0);

      /* feed the data to ironbee, and consume them */
      while (btowrite > 0) {
        ib_conndata_t icdata;
        int64_t ilength;
        TSIOBufferReader input_reader = TSVIOReaderGet(input_vio);
        TSIOBufferBlock blkp = TSIOBufferReaderStart(input_reader);
        const char *ibuf = TSIOBufferBlockReadStart(blkp, input_reader, &ilength);

        /* feed it to ironbee */
        icdata.ib = ironbee;
        icdata.mp = data->iconn->mp;
        icdata.conn = data->iconn;
        icdata.dalloc = ilength;
        icdata.dlen = ilength;
        icdata.data = (uint8_t *)ibuf;
        ib_state_notify_conn_data_out(ironbee, &icdata);

        /* and mark it as all consumed */
        btowrite -= ilength;
        TSIOBufferReaderConsume(input_reader, ilength);
        TSVIONDoneSet(input_vio, TSVIONDoneGet(input_vio) + ilength);
      }
    }
  }

  /* Now we check the input VIO to see if there is data left to
   * read.
   */
  if (TSVIONTodoGet(input_vio) > 0) {
    if (towrite > 0) {
      /* If there is data left to read, then we reenable the output
       * connection by reenabling the output VIO. This will wake up
       * the output connection and allow it to consume data from the
       * output buffer.
       */
      TSVIOReenable(data->output_vio);

      /* Call back the input VIO continuation to let it know that we
       * are ready for more data.
       */
      TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_READY, input_vio);
    }
  } else {
    /* If there is no data left to read, then we modify the output
     * VIO to reflect how much data the output connection should
     * expect. This allows the output connection to know when it
     * is done reading. We then reenable the output connection so
     * that it can consume the data we just gave it.
     */
    TSVIONBytesSet(data->output_vio, TSVIONDoneGet(input_vio));
    TSVIOReenable(data->output_vio);

    /* Call back the input VIO continuation to let it know that we
     * have completed the write operation.
     */
    TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_COMPLETE, input_vio);
  }
}

/* THIS IS A CLONE OF OUT_DATA_EVENT AND IS UNLIKELY TO WORK - YET */
static int in_data_event(TSCont contp, TSEvent event, void *edata)
{
  TSDebug("ironbee", "Entering in_data_event()");
  if (TSVConnClosedGet(contp)) {
    TSDebug("ironbee", "\tVConn is closed");
    TSContDestroy(contp);	/* from null-transform, ???? */


    return 0;
  }
  switch (event) {
    case TS_EVENT_ERROR:
      {
        TSVIO input_vio;

        TSDebug("ironbee", "\tEvent is TS_EVENT_ERROR");
        /* Get the write VIO for the write operation that was
         * performed on ourself. This VIO contains the continuation of
         * our parent transformation. This is the input VIO.
         */
        input_vio = TSVConnWriteVIOGet(contp);

        /* Call back the write VIO continuation to let it know that we
         * have completed the write operation.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_ERROR, input_vio);
      }
      break;
  case TS_EVENT_VCONN_WRITE_COMPLETE:
    TSVConnShutdown(TSTransformOutputVConnGet(contp), 0, 1);
    break;
    case TS_EVENT_VCONN_WRITE_READY:
      TSDebug("ironbee", "\tEvent is TS_EVENT_VCONN_WRITE_READY");
      /* fallthrough */
    default:
      TSDebug("ironbee", "\t(event is %d)", event);
      /* If we get a WRITE_READY event or any other type of
       * event (sent, perhaps, because we were reenabled) then
       * we'll attempt to transform more data.
       */
      process_data(contp);
      break;
  }
  return 0;
}
static int out_data_event(TSCont contp, TSEvent event, void *edata)
{
  /* Check to see if the transformation has been closed by a call to
   * TSVConnClose.
   */
  TSDebug("ironbee", "Entering out_data_event()");

  if (TSVConnClosedGet(contp)) {
    TSDebug("ironbee", "\tVConn is closed");
//    ib_ctx_destroy(TSContDataGet(contp));
    TSContDestroy(contp);	/* from null-transform, ???? */


    return 0;
  }
    switch (event) {
    case TS_EVENT_ERROR:
      {
        TSVIO input_vio;

        TSDebug("ironbee", "\tEvent is TS_EVENT_ERROR");
        /* Get the write VIO for the write operation that was
         * performed on ourself. This VIO contains the continuation of
         * our parent transformation. This is the input VIO.
         */
        input_vio = TSVConnWriteVIOGet(contp);

        /* Call back the write VIO continuation to let it know that we
         * have completed the write operation.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_ERROR, input_vio);
      }
      break;
    case TS_EVENT_VCONN_WRITE_COMPLETE:
      TSDebug("ironbee", "\tEvent is TS_EVENT_VCONN_WRITE_COMPLETE");
      /* When our output connection says that it has finished
       * reading all the data we've written to it then we should
       * shutdown the write portion of its connection to
       * indicate that we don't want to hear about it anymore.
       */
      TSVConnShutdown(TSTransformOutputVConnGet(contp), 0, 1);
      break;
    case TS_EVENT_VCONN_WRITE_READY:
      TSDebug("ironbee", "\tEvent is TS_EVENT_VCONN_WRITE_READY");
      /* fallthrough */
    default:
      TSDebug("ironbee", "\t(event is %d)", event);
      /* If we get a WRITE_READY event or any other type of
       * event (sent, perhaps, because we were reenabled) then
       * we'll attempt to transform more data.
       */
      process_data(contp);
      break;
    }

  return 0;
}


static void process_hdr(ib_ctx *data, ironbee_direction *ibd)
{
  ib_conndata_t icdata;
  int i, nhdr, len, rv;
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc field_loc;
  const char *val;

  TSDebug("ironbee", "process %s headers\n", ibd->word);

  rv = (*ibd->hdr_get) (data->txnp, &bufp, &hdr_loc);
  if (rv) {
    TSError ("couldn't retrieve client %s header: %d\n", ibd->word, rv);
    return;
  }

  icdata.ib = ironbee;
  icdata.mp = data->iconn->mp;
  icdata.conn = data->iconn;

  nhdr = TSMimeHdrFieldsCount(bufp, hdr_loc);

  for (i = 0; i < nhdr; i++) {
    field_loc = TSMimeHdrFieldGet(bufp, hdr_loc, i);

    val = TSMimeHdrFieldNameGet (bufp, hdr_loc, field_loc, &len);
    icdata.data = (void*)val;
    icdata.dalloc = icdata.dlen = len;
    (*ibd->ib_notify)(ironbee, &icdata);

    icdata.data = (void*)": ";
    icdata.dalloc = icdata.dlen = strlen(": ");
    (*ibd->ib_notify)(ironbee, &icdata);

    val = TSMimeHdrFieldValueStringGet (bufp, hdr_loc, field_loc, 0, &len);
    icdata.data = (void*)val;
    icdata.dalloc = icdata.dlen = len;
    (*ibd->ib_notify)(ironbee, &icdata);

    TSHandleMLocRelease(bufp, hdr_loc, field_loc);
  }

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

static int ironbee_plugin(TSCont contp, TSEvent event, void *edata)
{
  TSVConn connp;
  TSHttpTxn txnp = (TSHttpTxn) edata;
  TSHttpSsn ssnp = (TSHttpSsn) edata;
  ib_ctx *data;
  ib_conn_t *iconn = NULL;
  ib_status_t rc;

  TSDebug("ironbee", "Entering ironbee_plugin with %d", event);
  switch (event) {

  /* CONNECTION */
  case TS_EVENT_HTTP_SSN_START:
    /* start of connection */
    /* But we can't initialise conn stuff here, because there's
     * no API to get the connection stuff required by ironbee
     * at this point.  So instead, intercept the first TXN
     */
    TSHttpSsnHookAdd (ssnp, TS_HTTP_TXN_START_HOOK, contp);
    TSContDataSet(contp, NULL);
    TSHttpSsnReenable (ssnp, TS_EVENT_HTTP_CONTINUE);
    break;
  case TS_EVENT_HTTP_TXN_START:
    /* start of Request */
    /* First req on a connection, we set up conn stuff */
    data = TSContDataGet(contp);
    if (data == NULL) {
      rc = ib_conn_create(ironbee, &iconn, contp);
      if (rc != IB_OK) {
        TSError("ironbee", "ib_conn_create: %d\n", rc);
        return rc; // FIXME - figure out what to do
      }
      data = TSmalloc(sizeof(ib_ctx));
      memset(data, 0, sizeof(ib_ctx));
      data->iconn = iconn;
      data->txnp = txnp;
      TSContDataSet(contp, data);
      ib_state_notify_conn_opened(ironbee, iconn);
    }

    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  /* HTTP RESPONSE */
  case TS_EVENT_HTTP_READ_RESPONSE_HDR:
    data = TSContDataGet(contp);

    /* hook to examine output headers */
    /* Not sure why we can't do it right now, but it seems headers
     * are not yet available.
     * Can we use another case switch in this function?
     */
    TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);

    /* hook an output filter to watch data */
    connp = TSTransformCreate(out_data_event, txnp);
    TSContDataSet(connp, data);
    TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, connp);

    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  /* hook for processing response headers */
  case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
    data = TSContDataGet(contp);
    process_hdr(data, &ironbee_direction_resp);
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  /* HTTP REQUEST */
  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    data = TSContDataGet(contp);

    /* hook to examine output headers */
    /* Not sure why we can't do it right now, but it seems headers
     * are not yet available.
     * Can we use another case switch in this function?
     */
    TSHttpTxnHookAdd(txnp, TS_HTTP_OS_DNS_HOOK, contp);

    /* hook an input filter to watch data */
    connp = TSTransformCreate(in_data_event, txnp);
    TSContDataSet(connp, data);
    TSHttpTxnHookAdd(txnp, TS_HTTP_REQUEST_TRANSFORM_HOOK, connp);

    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  /* hook for processing request headers */
  /* No idea why it's called OS_DNS, but it figures in sample plugins */
  case TS_EVENT_HTTP_OS_DNS:
    data = TSContDataGet(contp);
    process_hdr(data, &ironbee_direction_req);
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;


  /* CLEANUP EVENTS */
  case TS_EVENT_HTTP_TXN_CLOSE:
  case TS_EVENT_HTTP_SSN_CLOSE:
    ib_ctx_destroy(TSContDataGet(contp), event);
    TSContDataSet(contp, NULL);
    break;

  /* if we get here we've got a bug */
  default:
    TSError("BUG: unhandled event %d in ironbee_plugin\n", event);
    break;
  }

  return 0;
}

int
check_ts_version()
{

  const char *ts_version = TSTrafficServerVersionGet();
  int result = 0;

  if (ts_version) {
    int major_ts_version = 0;
    int minor_ts_version = 0;
    int patch_ts_version = 0;

    if (sscanf(ts_version, "%d.%d.%d", &major_ts_version, &minor_ts_version, &patch_ts_version) != 3) {
      return 0;
    }

    /* Need at least TS 2.0 */
    if (major_ts_version >= 2) {
      result = 1;
    }

  }

  return result;
}






static void ironbee_logger(void *dummy, int level,
                           const char *prefix, const char *file, int line,
                           const char *fmt, va_list ap)
{
    char buf[8192 + 1];
    int limit = 7000;
    int ec;

    /* Buffer the log line. */
    ec = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (ec >= limit) {
        /* Mark as truncated, with a " ...". */
        memcpy(buf + (limit - 5), " ...", 5);

        /// @todo Do something about it
    }

    /* Write it to the error log. */
    TSError("[ts-ironbee] %s: %s\n", prefix?prefix:"", buf);
}
static void addr2str(const struct sockaddr *addr, char *str, int *port)
{
  char serv[8]; /* port num */
  int rv = getnameinfo(addr, sizeof(*addr), str, ADDRSIZE, serv, 8,
                       NI_NUMERICHOST|NI_NUMERICSERV);
  if (rv != 0) {
    TSError("[ts-ironbee] getnameinfo: %d\n", rv);
  }
  *port = atoi(serv);
}
static ib_status_t ironbee_conn_init(ib_engine_t *ib,
                                   ib_conn_t *iconn, void *cbdata)
{
  /* when does this happen? */
  ib_status_t rc;
  const struct sockaddr *addr;
  char chip[ADDRSIZE];
  int port;

  TSCont contp = iconn->pctx;
  ib_ctx* data = TSContDataGet(contp);
//  ib_clog_debug(....);

  /* remote ip */
  addr = TSHttpTxnClientAddrGet(data->txnp);

  addr2str(addr, chip, &port);

  iconn->remote_ipstr = data->remote_ip = TSstrdup(chip);
  rc = ib_data_add_bytestr(iconn->dpi,
                             "remote_ip",
                             (uint8_t *)iconn->remote_ipstr,
                             strlen(chip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

  /* remote port */
  iconn->remote_port = port;
    rc = ib_data_add_num(iconn->dpi, "remote_port", port, NULL);
    if (rc != IB_OK) {
        return rc;
    }

  /* local end */
  addr = TSHttpTxnIncomingAddrGet(data->txnp);

  addr2str(addr, chip, &port);

    iconn->local_ipstr = data->local_ip = TSstrdup(chip);
    rc = ib_data_add_bytestr(iconn->dpi,
                             "local_ip",
                             (uint8_t *)iconn->local_ipstr,
                             strlen(chip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local_port */
    iconn->local_port = port;
    rc = ib_data_add_num(iconn->dpi, "local_port", port, NULL);
    if (rc != IB_OK) {
        return rc;
    }
  return IB_OK;
}

static IB_PROVIDER_IFACE_TYPE(logger) ironbee_logger_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    (ib_log_logger_fn_t)ironbee_logger
};





/* this can presumably be global since it's only setup on init */
//static ironbee_config_t ibconfig;
//#define TRACEFILE "/tmp/ironbee-trace"
#define TRACEFILE NULL

static void ibexit()
{
  ib_engine_destroy(ironbee);
}
static int ironbee_init(const char *configfile)
{
  /* grab from httpd module's post-config */
  ib_status_t rc;
  ib_provider_t *lpr;
  ib_cfgparser_t *cp;
  ib_context_t *ctx;

  rc = ib_initialize();
  if (rc != IB_OK) {
    return rc;
  }

  ib_util_log_level(9);

  ib_trace_init(TRACEFILE);

  rc = ib_engine_create(&ironbee, &ibplugin);
  if (rc != IB_OK) {
    return rc;
  }

  rc = ib_provider_register(ironbee, IB_PROVIDER_TYPE_LOGGER, "ironbee-ts",
                            &lpr, &ironbee_logger_iface, NULL);
  if (rc != IB_OK) {
    return rc;
  }

  ib_context_set_string(ib_context_engine(ironbee),
                        IB_PROVIDER_TYPE_LOGGER, "ironbee-ts");
  ib_context_set_num(ib_context_engine(ironbee),
                     IB_PROVIDER_TYPE_LOGGER ".log_level", 4);

  rc = ib_engine_init(ironbee);
  if (rc != IB_OK) {
    return rc;
  }
   
  rc = atexit(ibexit);
  if (rc != 0) {
    return IB_OK + rc;
  }

  ib_hook_register(ironbee, conn_opened_event,
                   (ib_void_fn_t)ironbee_conn_init, NULL);


  ib_state_notify_cfg_started(ironbee);
  ctx = ib_context_main(ironbee);

  ib_context_set_string(ctx, IB_PROVIDER_TYPE_LOGGER, "ironbee-ts");
  ib_context_set_num(ctx, "logger.log_level", 4);

  rc = ib_cfgparser_create(&cp, ironbee);
  if (rc != IB_OK) {
    return rc;
  }
  if (cp != NULL) {  // huh?
    ib_cfgparser_parse(cp, configfile);
    ib_cfgparser_destroy(cp);
  }
  ib_state_notify_cfg_finished(ironbee);


  return IB_OK;
}

void
TSPluginInit(int argc, const char *argv[])
{
  int rv;
  TSPluginRegistrationInfo info;
  TSCont cont;

  info.plugin_name = "ironbee";
  info.vendor_name = "Qualys, Inc";
  info.support_email = "ironbee-users@lists.sourceforge.com";

  if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {
    TSError("[ironbee] Plugin registration failed.\n");
    goto Lerror;
  }

  if (!check_ts_version()) {
    TSError("[ironbee] Plugin requires Traffic Server 3.0 or later\n");
    goto Lerror;
  }

  cont = TSContCreate(ironbee_plugin, NULL);

  /* connection initialisation & cleanup */
  TSHttpHookAdd(TS_HTTP_SSN_START_HOOK, cont);

  /* With both of these, SSN_CLOSE gets called first.
   * I must be misunderstanding SSN
   * So hook it all to TXN
   */
  //TSHttpHookAdd(TS_HTTP_SSN_CLOSE_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_TXN_CLOSE_HOOK, cont);

  /* Hook to process responses */
  TSHttpHookAdd(TS_HTTP_READ_RESPONSE_HDR_HOOK, cont);

  /* Hook to process requests */
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont);

  if (argc != 2) {
    TSError("[ironbee] requires one argument: configuration file name\n");
    goto Lerror;
  }
  rv = ironbee_init(argv[1]);
  if (rv != IB_OK) {
    TSError("[ironbee] initialisation failed with %d\n", rv);
  }
  return;

Lerror:
  TSError("[ironbee] Unable to initialize plugin (disabled).\n");
}
