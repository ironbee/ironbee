/*
 * LibHTP (http://www.libhtp.org)
 * Copyright 2009,2010 Ivan Ristic <ivanr@webkreator.com>
 *
 * LibHTP is an open source product, released under terms of the General Public Licence
 * version 2 (GPLv2). Please refer to the file LICENSE, which contains the complete text
 * of the license.
 *
 * In addition, there is a special exception that allows LibHTP to be freely
 * used with any OSI-approved open source licence. Please refer to the file
 * LIBHTP_LICENSING_EXCEPTION for the full text of the exception.
 *
 */
 
/*
 * This program is a simple example of how to use LibHTP to parse a HTTP
 * connection stream. It uses libnids for TCP reassembly and LibHTP for
 * HTTP parsing.
 *
 * This program is only meant as an demonstration; it is not suitable
 * to be used in production.
 *
 * Compile with:
 *
 *     $ gcc htptest.c -lhtp -lz -lnids -o htptest
 *
 */
            
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include "nids.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <htp/htp.h>
#include <htp/dslib.h>

#define CLIENT 1
#define SERVER 2

typedef struct chunk_t chunk_t;
typedef struct stream_data stream_data;

struct chunk_t {
    char *data;
    size_t len;
    int direction;
    size_t consumed;
};

struct stream_data {
    int id;
    htp_connp_t *connp;
    int direction;
    //int inbound_status;
    //int outbound_status;
    int parser_status;
    int fd;    
    int chunk_counter;
    int log_level;
    int req_count;
    list_t *chunks;
    list_t *inbound_chunks;
    list_t *outbound_chunks;
};

htp_cfg_t *cfg;

int counter = 1000;

/**
 *
 */
void free_stream_data(stream_data *sd) {
    if (sd == NULL) return;

    if (sd->chunks != NULL) {
        chunk_t *chunk = NULL;
        
        list_iterator_reset(sd->chunks);
        while((chunk = list_iterator_next(sd->chunks)) != NULL) {
            free(chunk->data);
            free(chunk);
        }
        
        list_destroy(sd->chunks);
    }
    
    if (sd->fd != -1) {
        close(sd->fd);
    }
    
    free(sd);
}

/**
 *
 *
 * @param sd
 */
void process_stored_stream_data(stream_data *sd) {
    int loop = 0;

    do {
        loop = 0;
        
        fprintf(stdout, "process_stored_stream_data: in_status %d out_status %d\n",
            sd->connp->in_status, sd->connp->out_status);
        
        // Send as much inbound data as possible    
        while((sd->connp->in_status == STREAM_STATE_DATA)&&(list_size(sd->inbound_chunks) > 0)) {
            fprintf(stdout, "%d inbound chunks in queue\n", list_size(sd->inbound_chunks));
                
            chunk_t *chunk = (chunk_t *)list_get(sd->inbound_chunks, 0);
            //fprint_raw_data(stdout, "INBOUND DATA", chunk->data + chunk->consumed, chunk->len - chunk->consumed);
                
            int rc = htp_connp_req_data(sd->connp, 0, chunk->data + chunk->consumed, chunk->len - chunk->consumed);
                
            fprintf(stdout, "INBOUND STATUS: %d; CONSUMED DATA: %d; OUTBOUND STATUS: %d\n",
                sd->connp->in_status, htp_connp_req_data_consumed(sd->connp), sd->connp->out_status);                
                    
            if (rc == STREAM_STATE_DATA) {
                list_shift(sd->inbound_chunks);
            } else {
                chunk->consumed = htp_connp_req_data_consumed(sd->connp);
            }
        }
   
        // Send as much outbound data as possible          
        while((sd->connp->out_status == STREAM_STATE_DATA)&&(list_size(sd->outbound_chunks) > 0)) {
            fprintf(stdout, "%d outbound chunks in queue\n", list_size(sd->outbound_chunks));
                
            chunk_t *chunk = (chunk_t *)list_get(sd->outbound_chunks, 0);
            //fprint_raw_data(stdout, "OUTBOUND DATA", chunk->data + chunk->consumed, chunk->len - chunk->consumed);
                
            int rc = htp_connp_res_data(sd->connp, 0, chunk->data + chunk->consumed, chunk->len - chunk->consumed);
                
            fprintf(stdout, "INBOUND STATUS: %d; CONSUMED DATA: %d; OUTBOUND STATUS: %d\n",
                sd->connp->in_status, htp_connp_res_data_consumed(sd->connp), sd->connp->out_status);                
                    
            if (rc == STREAM_STATE_DATA) {
                list_shift(sd->outbound_chunks);
            } else {
                chunk->consumed = htp_connp_res_data_consumed(sd->connp);
            }
            
            loop = 1;
        }
    } while(loop);
}

/**
 *
 *
 * @param sd
 * @param direction
 * @param hlf
 */
void process_stream_data(stream_data *sd, int direction, struct half_stream *hlf) {
    chunk_t *chunk = NULL;
    int rc;
    
    if (sd->direction == direction) {
        // Inbound data
        switch(sd->connp->in_status) {
            case STREAM_STATE_NEW :
            case STREAM_STATE_DATA :
                // Send data to parser
                //fprint_raw_data(stdout, "INBOUND DATA", hlf->data, hlf->count_new);
                
                rc = htp_connp_req_data(sd->connp, 0, hlf->data, hlf->count_new);
                
                fprintf(stdout, "INBOUND STATUS: %d; CONSUMED DATA: %d; OUTBOUND STATUS: %d\n",
                    sd->connp->in_status, htp_connp_req_data_consumed(sd->connp), sd->connp->out_status);
                
                if (rc == STREAM_STATE_DATA_OTHER) {
                    // Encountered inbound parsing block
                    
                    // Store partial chunk for later
                    chunk = calloc(1, sizeof(chunk_t));
                    chunk->len = hlf->count_new - htp_connp_req_data_consumed(sd->connp);
                    chunk->data = malloc(chunk->len);
                    memcpy(chunk->data, hlf->data + htp_connp_req_data_consumed(sd->connp), chunk->len);
                    list_add(sd->inbound_chunks, chunk);
                    
                    fprintf(stdout, "Added chunk with %d bytes to inbound\n", chunk->len);
                } else
                if (rc != STREAM_STATE_DATA) {
                    // Inbound parsing error
                    sd->log_level = 0;
                    fprintf(stderr, "[#%d] Inbound parsing error: %d\n", sd->id, rc);
                }
                break;
                
            case STREAM_STATE_ERROR :
                // Do nothing
                break;
                
            case STREAM_STATE_DATA_OTHER :
                // Store data for later
                chunk = calloc(1, sizeof(chunk_t));
                chunk->len = hlf->count_new;
                chunk->data = malloc(chunk->len);
                memcpy(chunk->data, hlf->data, chunk->len);
                list_add(sd->inbound_chunks, chunk);
                fprintf(stdout, "Added chunk with %d bytes to inbound\n", chunk->len);
                break;
        }
    } else {
        // Outbound data
        switch(sd->connp->out_status) {
            case STREAM_STATE_NEW :
            case STREAM_STATE_DATA :
                // Send data to parser
                //fprint_raw_data(stdout, "OUTBOUND DATA", hlf->data, hlf->count_new);
                
                rc = htp_connp_res_data(sd->connp, 0, hlf->data, hlf->count_new);
                
                fprintf(stdout, "INBOUND STATUS: %d; CONSUMED DATA: %d; OUTBOUND STATUS: %d\n",
                    sd->connp->in_status, htp_connp_res_data_consumed(sd->connp), sd->connp->out_status);
                
                if (rc == STREAM_STATE_DATA_OTHER) {
                    // Encountered outbound parsing block
                    
                    // Store partial chunk for later
                    chunk = calloc(1, sizeof(chunk_t));
                    chunk->len = hlf->count_new - htp_connp_res_data_consumed(sd->connp);
                    chunk->data = malloc(chunk->len);
                    memcpy(chunk->data, hlf->data + htp_connp_res_data_consumed(sd->connp), chunk->len);
                    list_add(sd->outbound_chunks, chunk);
                    fprintf(stdout, "Added chunk with %d bytes to outbound\n", chunk->len);
                } else
                if (rc != STREAM_STATE_DATA) {
                    // Outbound parsing error
                    sd->log_level = 0;
                    fprintf(stderr, "[#%d] Outbound parsing error: %d\n", sd->id, rc);
                }
                break;
                
            case STREAM_STATE_ERROR :
                // Do nothing
                break;
                
            case STREAM_STATE_DATA_OTHER :
                // Store data for later
                chunk = calloc(1, sizeof(chunk_t));
                chunk->len = hlf->count_new;
                chunk->data = malloc(chunk->len);
                memcpy(chunk->data, hlf->data, chunk->len);
                list_add(sd->outbound_chunks, chunk);
                fprintf(stdout, "Added chunk with %d bytes to outbound\n", chunk->len);
                break;
        }
    }
    
    // Process as much stored data as possible    
    process_stored_stream_data(sd);
}

/**
 *
 *
 * @param tcp
 * @param user_data
 */
void tcp_callback (struct tcp_stream *tcp, void **user_data) {	
    stream_data *sd = *user_data;
    char buf[1025];

    strcpy(buf, "SRC ");
    strncat(buf, inet_ntoa(*((struct in_addr *)&tcp->addr.saddr)), 1024 - strlen(buf));
    snprintf(buf + strlen(buf), 1024 - strlen(buf), ":%i ", tcp->addr.source);
    strncat(buf, "DST ", 1024 - strlen(buf));
    strncat(buf, inet_ntoa(*((struct in_addr *)&tcp->addr.daddr)), 1024 - strlen(buf));
    snprintf(buf + strlen(buf), 1024 - strlen(buf), ":%i", tcp->addr.dest);
    
    // fprintf(stdout, "%s\n", buf);
  
    if (tcp->nids_state == NIDS_JUST_EST) {
        tcp->client.collect++;
        tcp->server.collect++;
        tcp->server.collect_urg++;
        tcp->client.collect_urg++;

        // Allocate custom per-stream data      
        sd = calloc(1, sizeof(stream_data));
        //sd->inbound_status = STREAM_STATE_DATA;
        //sd->outbound_status = STREAM_STATE_DATA;
        sd->id = counter++;
        sd->direction = -1;
        sd->fd = -1;
        sd->log_level = -1;
        sd->chunks = list_array_create(16);
        sd->inbound_chunks = list_array_create(16);
        sd->outbound_chunks = list_array_create(16);
        sd->req_count = 1;
        
        // Init LibHTP parser
        sd->connp = htp_connp_create(cfg);
        if (sd->connp == NULL) {
            // TODO Error message
            
            free(sd);
            return;            
        }
        
        // Associate TCP stream information with the HTTP connection parser
        htp_connp_set_user_data(sd->connp, sd);
        
        *user_data = sd;
        
        //fprintf(stdout, "[#%d] Connection established [%s]\n", sd->id, buf);
      
        return;
    }
    
    if (tcp->nids_state == NIDS_CLOSE) {
        if (sd == NULL) return;
        
        //fprintf(stdout, "[#%d] Connection closed\n", sd->id);
        
        // Destroy parser
        htp_connp_destroy_all(sd->connp);
            
        // Free custom per-stream data
        free_stream_data(sd);
      
        return;
    }
    
    if (tcp->nids_state == NIDS_RESET) {
        if (sd == NULL) return;
        
        //fprintf(stdout, "[#%d] Connection closed (RST)\n", sd->id);
        
        // Destroy parser
        htp_connp_destroy_all(sd->connp);
            
        // Free custom per-stream data
        free_stream_data(sd);
      
        return;
    }

    if (tcp->nids_state == NIDS_DATA) {
        struct half_stream *hlf;
        int direction;
        
        if (tcp->client.count_new) {
            hlf = &tcp->client;
            direction = SERVER;
            //fprintf(stdout, "[#%d] Outbound data (%d bytes)\n", sd->id, hlf->count_new);
            //fprint_raw_data(stdout, "OUTBOUND", hlf->data, hlf->count_new);
        } else {
            hlf = &tcp->server;
            direction = CLIENT;
            //fprintf(stdout, "[#%d] Inbound data (%d bytes)\n", sd->id, hlf->count_new);
            //fprint_raw_data(stdout, "INBOUND", hlf->data, hlf->count_new);
        }
    
        if (sd == NULL) return;
        
        if (sd->direction == -1) {
            sd->direction = direction;
        }

        // Write data to disk or store for later        
        if (sd->fd == -1) {
            // Store data, as we may need it later
            chunk_t *chunk = calloc(1, sizeof(chunk_t));
            // TODO
            chunk->direction = direction;
            chunk->data = malloc(hlf->count_new);
            // TODO
            chunk->len = hlf->count_new;
            memcpy(chunk->data, hlf->data, chunk->len);
            
            list_add(sd->chunks, chunk);
        } else {        
            // No need to store, write directly to file
            
            if (sd->chunk_counter != 0) {
                write(sd->fd, "\r\n", 2);
            }
            
            if (sd->direction == direction) {
                write(sd->fd, ">>>\r\n", 5);
            } else {
                write(sd->fd, "<<<\r\n", 5);
            }
            
            write(sd->fd, hlf->data, hlf->count_new);                
            
            sd->chunk_counter++;
        }

        // Process data        
        process_stream_data(sd, direction, hlf);
        
        return;        
    }
}

/**
 *
 */
int callback_response(htp_connp_t *connp) {
    stream_data *sd = (stream_data *)htp_connp_get_user_data(connp);
    
    char *x = bstr_tocstr(connp->out_tx->request_line);
    fprintf(stdout, "[#%d/%d] %s\n", sd->id, sd->req_count, x);
    free(x);
    
    sd->req_count++;
}

/**
 *
 */
int callback_log(htp_log_t *log) {
    stream_data *sd = (stream_data *)htp_connp_get_user_data(log->connp);
    
    if ((sd->log_level == -1)||(sd->log_level > log->level)) {
        sd->log_level = log->level;
    }

    if (log->code != 0) {
        fprintf(stderr, "[#%d/%d][%d][code %d][file %s][line %d] %s\n", sd->id, sd->req_count,
            log->level, log->code, log->file, log->line, log->msg);
    } else {
        fprintf(stderr, "[#%d/%d][%d][file %s][line %d] %s\n", sd->id, sd->req_count,
            log->level, log->file, log->line, log->msg);
    }
    
    if (sd->fd == -1) {
        char filename[256];
        chunk_t *chunk;
        
        sprintf(filename, "conn-%d.t", sd->id);
        
        sd->fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
        
        list_iterator_reset(sd->chunks);
        while((chunk = list_iterator_next(sd->chunks)) != NULL) {
            if (sd->chunk_counter != 0) {
                write(sd->fd, "\r\n", 2);
            }
            
            if (sd->direction == chunk->direction) {
                write(sd->fd, ">>>\r\n", 5);
            } else {
                write(sd->fd, "<<<\r\n", 5);
            }
            
            write(sd->fd, chunk->data, chunk->len);
            
            sd->chunk_counter++;
        }
    }
}

/**
 *
 */
void print_usage() {
    fprintf(stdout, "Usage: htpMon [-r file] [expression]\n");
}

/**
 *
 *
 * @param argc
 * @param argv
 */
int main(int argc, char *argv[]) {
    // Check parameters
    if ((argc < 2)||(argc > 4)) {
        print_usage();
        return 1;
    }

    // Configure libnids
    if (argc > 2) {
        if (strcmp(argv[1], "-r") != 0) {
            print_usage();
            return 1;
        }
        
        nids_params.filename = argv[2];        
        
        if (argc == 4) {
            nids_params.pcap_filter = argv[3];
        }
    } else {
        nids_params.pcap_filter = argv[1];
    }
    
    if (argc == 4) {
    }
    
    // Initialize libnids
    if (!nids_init()) {
        fprintf(stderr, "libnids initialization failed: %s\n", nids_errbuf);
        return 1;
    }

    // Create LibHTP configuration    
    cfg = htp_config_create();
    htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2_2);
    
    htp_config_register_response(cfg, callback_response);
    htp_config_register_log(cfg, callback_log);

    // Run libnids
    nids_register_tcp(tcp_callback);
    nids_run();

    // Destroy LibHTP configuration    
    htp_config_destroy(cfg);
    
    return 0;    
}

