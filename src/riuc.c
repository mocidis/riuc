#include "riuc4_uart.h"
#include "ansi-utils.h"

#include <pjsua-lib/pjsua.h>
#include <unistd.h>

#include "ics.h"
#include "arbiter-client.h"
#include "oiu-client.h"
#include "riu-server.h"

typedef struct {
    arbiter_client_t aclient;
    oiu_client_t oclient;
    riu_server_t rserver;
    char ports_status[100];
} app_data_t;

app_data_t app_data;

//Callback function for Arbiter
static int arbiter_send(arbiter_client_t *uclient, arbiter_request_t *req) {
	return arbiter_client_send(uclient, req);
}

//Auto send msg to arbiter
static void *auto_send_to_arbiter(void * arg);

//Receive msg from OIUC
static void on_request(riu_server_t *rserver, riu_request_t *req);

void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus);

int main(int argc, char *argv[]) {
    char temp[10];
    int i;
    int fEnd = 0;
    int port_idx;

    pthread_t thread;

    serial_t serial;
    riuc4_t riuc4;

    strcpy(app_data.ports_status, "{1-0-0, 2-0-0, 3-0-0, 4-0-0}");

    if (argc < 2) {
        fprintf(stdout, "Usage: myapp-riuc4 <device> <request connect string> <listen connection string>\n");
        exit(-1);
    }
    
    fprintf(stdout, "RIUC - 4COM - RIUC-ccp-v0.2\n");

    for (i = 0; i < N_RIUC; i++) {
        riuc4.riuc4_status[i].user_data = &app_data.oclient;
    }

    riuc4_init(&serial, &riuc4, &on_riuc4_status);

    riuc4_start(&serial, argv[1]);

    //SEND
    char sqa_mip[50] = "udp:239.0.0.1:12345";
 
    arbiter_client_open(&app_data.aclient, argv[2]);
    oiu_client_open(&app_data.oclient, sqa_mip);

    //Arbiter path
    // LISTEN
    app_data.rserver.user_data = &riuc4;

    app_data.rserver.on_request_f = &on_request;

    riu_server_init(&app_data.rserver, argv[3]);
    riu_server_start(&app_data.rserver);

    //End Arbiter path


    pthread_create(&thread, NULL, auto_send_to_arbiter, &app_data.aclient);
    
    while(!fEnd) {
        printf("main = %s\n", app_data.ports_status);
        fprintf(stdout, "COMMAND:<port[1-4]><command[DdEertsl+-q]>:\n");
        if (fgets(temp, sizeof(temp), stdin) == NULL)
            printf("NULL CMD\n");
        port_idx = temp[0] - '1';
        if( port_idx < 0 || port_idx > 3 ) {
            fprintf(stdout, "Command error\n");
            continue;
        }
        switch (temp[1]) {
            case 'D':
                    riuc4_disable_rx(&riuc4, port_idx);
                    break;
            case 'd':
                    riuc4_disable_tx(&riuc4, port_idx);
                    break;
            case 'E':
                    riuc4_enable_rx(&riuc4, port_idx);
                    break;
            case 'e':
                    riuc4_enable_tx(&riuc4, port_idx);
                    break;
            case 'r':
                    riuc4_probe_rx(&riuc4, port_idx);
                    break;
            case 't':
                    riuc4_probe_tx(&riuc4, port_idx);
                    break;
            case 's':
                    riuc4_probe_sq(&riuc4, port_idx);
                    break;
            case 'l':
                    riuc4_probe_ptt(&riuc4, port_idx);
                    break;
            case '+':
                    riuc4_on_ptt(&riuc4, port_idx);
                    break;
            case '-':
                    riuc4_off_ptt(&riuc4, port_idx);
                    break;
            case 'q':
                    fEnd = 1;
                    break;
            default:
                    fprintf(stdout, "Unknown command");
                    break;
        }
    }  
    pthread_join(thread, NULL);

    riuc4_end(&serial);

    return 0;
}

static void on_request(riu_server_t *rserver, riu_request_t *req) {
    riuc4_t *riuc4 = (riuc4_t *)rserver->user_data;
    int port_idx;
    char temp[10];
    strncpy(temp,req->riuc_ptt.cmd, sizeof(temp));
    printf("Do something here...\n");
}

void *auto_send_to_arbiter(void * arg) {
    arbiter_request_t req;

    arbiter_client_t *aclient = (arbiter_client_t *) arg;
    
    req.msg_id = ABT_UP;
    strncpy(req.abt_up.type, "RIU", sizeof(req.abt_up.type));
    strncpy(req.abt_up.username, "R1", sizeof(req.abt_up.username));
    strncpy(req.abt_up.location, "HN", sizeof(req.abt_up.location));
    strncpy(req.abt_up.location, "HN", sizeof(req.abt_up.location));
    strncpy(req.abt_up.des, "M16 Rifle", sizeof(req.abt_up.des));
    strncpy(req.abt_up.ip_addr, "127.0.0.1:1111", sizeof(req.abt_up.ip_addr));
    req.abt_up.frequence = 96.5;
    req.abt_up.is_online = 1;

    while (1) {
        strncpy(req.abt_up.ports_status,app_data.ports_status, sizeof(req.abt_up.ports_status));
        arbiter_send(aclient, &req);
        sleep(5);
    }
}

void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus) {
    fprintf(stdout, "RIUC4 port %d, update signal %s. Status: tx=%d,rx=%d,ptt=%d,sq=%d\n", port, RIUC4_SIGNAL_NAME[signal], ustatus->tx, ustatus->rx, ustatus->ptt, ustatus->sq);

    switch(port) {
        case 0:
            app_data.ports_status[3] = '1';
            if (ustatus->tx == 1)
                app_data.ports_status[5] = '1';
            if (ustatus->tx == 0)
                app_data.ports_status[5] = '0';
            break;
        case 1:
            app_data.ports_status[10] = '1';
            if (ustatus->tx == 1)
                app_data.ports_status[12] = '1';
            if (ustatus->tx == 0)
                app_data.ports_status[12] = '0';
            break;
        case 2:
            app_data.ports_status[17] = '1';
            if (ustatus->tx == 1)
                app_data.ports_status[19] = '1';
            if (ustatus->tx == 0)
                app_data.ports_status[19] = '0';
            break;
        case 3:
            app_data.ports_status[24] = '1';
            if (ustatus->tx == 1)
                app_data.ports_status[26] = '1';
            if (ustatus->tx == 0)
                app_data.ports_status[26] = '0';
            break;
        default:
            printf("Unknown port.\n");
            break;
    }

    if (0 == strcmp(RIUC4_SIGNAL_NAME[signal], "SQ")) {
        oiu_client_t *oclient = (oiu_client_t *)ustatus->user_data;

        oiu_request_t req;
        req.msg_id = OIUC_SQ;
        strncpy(req.oiuc_sq._id, "RIUC1", sizeof(req.oiuc_sq._id));
        req.oiuc_sq.port = port;
        switch (port) {
            case 0:
                strncpy(req.oiuc_sq.multicast_addr,"udp:129.0.0.1:4320", sizeof(req.oiuc_sq.multicast_addr));
                break;
            case 1:
                strncpy(req.oiuc_sq.multicast_addr,"udp:129.0.0.1:4321", sizeof(req.oiuc_sq.multicast_addr));
                break; 
            case 2:
                strncpy(req.oiuc_sq.multicast_addr,"udp:129.0.0.1:4322", sizeof(req.oiuc_sq.multicast_addr));
                break;
            case 3:
                strncpy(req.oiuc_sq.multicast_addr,"udp:129.0.0.1:4323", sizeof(req.oiuc_sq.multicast_addr));
                break;
            default:
                printf("Unknow port number.\n");
                break;
        }
        oiu_client_send(oclient, &req);
        }    
    }

