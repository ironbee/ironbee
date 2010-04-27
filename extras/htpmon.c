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
 *     $ gcc htpmon.c -lhtp -lz -lnids -o htpmon
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

typedef struct stream_data stream_data;

struct stream_data {
    htp_connp_t *connp;
    int id;
};

htp_cfg_t *cfg;

int counter = 1000;

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
        sd->id = counter++;
        
        // Init LibHTP parser
        sd->connp = htp_connp_create(cfg);
        if (sd->connp == NULL) {
            // TODO Error message
            
            free(sd);
            return;
        }
        
        *user_data = sd;
        
        fprintf(stdout, "[#%d] Connection established [%s]\n", sd->id, buf);
      
        return;
    }
    
    if (tcp->nids_state == NIDS_CLOSE) {
        if (sd == NULL) return;
        
        fprintf(stdout, "[#%d] Connection closed\n", sd->id);
        
        // Destroy parser
        htp_connp_destroy_all(sd->connp);
            
        // Free custom per-stream data
        free(sd);
      
        return;
    }
    
    if (tcp->nids_state == NIDS_RESET) {
        if (sd == NULL) return;
        
        fprintf(stdout, "[#%d] Connection closed (RST)\n", sd->id);
        
        // Destroy parser
        htp_connp_destroy_all(sd->connp);
            
        // Free custom per-stream data
        free(sd);
      
        return;
    }

    if (tcp->nids_state == NIDS_DATA) {
        struct half_stream *hlf;
        
        if (tcp->client.count_new) {
            hlf = &tcp->client;
            fprintf(stdout, "[#%d] Outbound data (%d bytes)\n", sd->id, hlf->count_new);
        } else {
            hlf = &tcp->server;
            fprintf(stdout, "[#%d] Inbond data (%d bytes)\n", sd->id, hlf->count_new);
        }
    
        if (sd == NULL) return;
        
        // TODO Process data

        return;        
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

    // Run libnids
    nids_register_tcp(tcp_callback);
    nids_run();

    // Destroy LibHTP configuration    
    htp_config_destroy(cfg);
    
    return 0;    
}
