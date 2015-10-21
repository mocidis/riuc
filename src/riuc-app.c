#include <unistd.h>
#include "riuc4_uart.h"
#include "ansi-utils.h"
#include "arbiter-client.h"
#include "oiu-client.h"
#include "riu-server.h"

typedef struct {
    // TODO
} inbound_stream_t;
typedef struct {
    // TODO
} outbound_stream_t;

typedef struct {
    // Runtime state
    int is_online;
    int is_tx;
    int is_sq;

    // Management info
    int port;
    double frequence;
    
    // Sound device info
    double volume;
    // TODO: add PJMEDIA sound device info here!!

    // Stream params
    char multicast_ip[16];
    int stream_port;
    inbound_stream_t in_stream;
    outbound_stream_t out_stream;
} radio_t;

typedef struct {
//    riuc_t riuc;
    riu_server_t rserver;
    arbiter_client_t aclient;
    riuc4_t riuc4;
    serial_t serial;

    // Management infomation
    char name[10];
    char desc[250];
    char location[50];

    // Hardware configuration information
    char serial_file[30];
    
    // Network information
    char riu_connection_string[40];
    char abt_connection_string[40];

    radio_t radios[4];
} app_data_t;

app_data_t app_data;

//Callback function for Arbiter
static int arbiter_send(arbiter_client_t *uclient, arbiter_request_t *req) {
	return arbiter_client_send(uclient, req);
}

//Auto send msg to arbiter
static void *auto_send_to_arbiter(void * arg);

//Receive msg from OIUC

void extract_ip(char *connection_string, char *ip_addr) {
    char * temp, *temp2;
    temp = strchr(connection_string, ':');
#if 0
    temp2 = strchr(temp, ':');
    strncpy(ip_addr, temp, temp2 - temp);
#endif
#if 1
    temp++;
    strncpy(ip_addr, temp, strlen(temp) + 1);
#endif
}

static void send_abt_ptt(app_data_t *app_data, int i) {
    arbiter_request_t req;

    req.msg_id = ABT_PTT;
    req.abt_ptt.ptt_on = 1;
    req.abt_ptt.ptt_port = i;
    strncpy(req.abt_ptt.ptt_name, app_data->name, sizeof(app_data->name));
    strncpy(req.abt_ptt.sdp_ip, app_data->radios[i].multicast_ip, sizeof(app_data->radios[i].multicast_ip));
    req.abt_ptt.sdp_port = app_data->radios[i].stream_port;

    arbiter_send(&app_data->aclient, &req);
}

static void send_abt_up(app_data_t *app_data, int i) {
    arbiter_request_t req;
    char multicast_ip[16];    
    int n, len;
    req.msg_id = ABT_UP;

    len = strlen(req.abt_up.username);

    strncpy(req.abt_up.type, "RIU", sizeof("RIU"));
    strncpy(req.abt_up.username, app_data->name, sizeof(app_data->name));
    n = sprintf(req.abt_up.username + len, "%d", i);
    req.abt_up.username[n + len] = '\0';
    strncpy(req.abt_up.location, app_data->location, sizeof(app_data->location));
    strncpy(req.abt_up.desc, app_data->desc, sizeof(app_data->desc));
    extract_ip(app_data->riu_connection_string, req.abt_up.ip_addr);

    //State
    req.abt_up.is_online = app_data->radios[i].is_online;
    req.abt_up.is_tx = app_data->radios[i].is_tx;
    req.abt_up.is_sq = app_data->radios[i].is_sq;

    //Info   
    req.abt_up.frequence = app_data->radios[i].frequence;
    req.abt_up.port = app_data->radios[i].port;
    
    //Sound info
    req.abt_up.volume = app_data->radios[i].volume;

    //Stream params
    strncpy(req.abt_up.multicast_ip, app_data->radios[i].multicast_ip, sizeof(app_data->radios[i].multicast_ip));
    req.abt_up.stream_port = app_data->radios[i].stream_port;

    arbiter_send(&app_data->aclient, &req);
}

// *************** CALLBACK functions *************
void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus) {
    if (signal == RIUC_SIGNAL_SQ) {
        app_data.radios[port].is_sq = ustatus->sq;
        send_abt_up(&app_data, port);
        send_abt_ptt(&app_data, port);
    }
}
void open_stream_from_mcast_session(char *mcast_ip, int mcast_port, int radio_port) {
    // TODO: capture stream from mcast session (mcast_ip:port) and transmit on radio interface radio_port
    // - open socket (in stream_t of radio_t)
    // - join multicast group (dung socket vua mo)
    // - create media port (in stream_t of radio_t)
    // - connect media port 
}

void close_stream_from_mcast_session(char *mcast_ip, int mcast_port, int radio_port) {
    // TODO: 
    // - destroy media port (in stream_t of radio_t)
    // - leave multicast group (dung socket vua mo)
    // - close socket (in stream_t of radio_t)
}

void update_tx_status( int is_tx, int i) {
    arbiter_client_t *aclient = &app_data.aclient;
    app_data.radios[i].is_tx = is_tx;
    app_data.radios[i].is_sq = is_tx?!is_tx:app_data.radios[i].is_sq;
    send_abt_up(&app_data, i);
}

static void on_request(riu_server_t *rserver, riu_request_t *req) {
    switch (req->msg_id) {
        case RIUC_PTT:
            if( req->riuc_ptt.ptt_on == 1) {
                open_stream_from_mcast_session(req->riuc_ptt.sdp_ip, req->riuc_ptt.sdp_port, req->riuc_ptt.ptt_port);
                update_tx_status(1, req->riuc_ptt.ptt_port - 1);
            }
            else {
                close_stream_from_mcast_session(req->riuc_ptt.sdp_ip, req->riuc_ptt.sdp_port, req->riuc_ptt.ptt_port);
                update_tx_status(0, req->riuc_ptt.ptt_port - 1);
            }
            break;
        default:
            EXIT_IF_TRUE(1, "Unexpected protocol message");
    }

}
static void radio_init(radio_t *radio, int port) {
    // TODO
    // State
    radio->is_online = 0;
    radio->is_tx = 0;
    radio->is_sq = 0;

    // Management info
    radio->port = port+1;
    radio->frequence = 100;

    // Sound device info
    radio->volume = 0.5;
    // TODO: add PJMEDIA sound device info here!!

    // Stream paramsa => DONE
}

static void load_info_from_db(const char *db_file, app_data_t *app_data) {
    // FIXME
    int i;

    memset(app_data->name, 0, sizeof(app_data->name));
    strncpy(app_data->name, "RIUC1", strlen("RIUC1"));
    memset(app_data->desc, 0, sizeof(app_data->desc));
    strncpy(app_data->desc, "Barrett radio", strlen("Barrett radio"));
    memset(app_data->location, 0, sizeof(app_data->location));
    strncpy(app_data->location, "Hanoi", strlen("Hanoi"));

    for (i = 0; i < 4; i++) {
        strncpy(app_data->radios[i].multicast_ip, "239.0.0.1", sizeof("239.0.0.1"));
        app_data->radios[i].port = 11110 + i;
    }
}

static void init(app_data_t *app_data) {
    int i;
    char riu_cnt_string[50];

    // PJLIB and PJMEDIA ....
    CHECK(__FILE__, pj_init());
    // TODO: init other pjmedia related stuffs here!

    // INIT MANAGEMENT INFO ...
    load_info_from_db("database/riu.db", app_data);

    // Init radios ...
    for(i = 0; i < 4; i++) {
        radio_init(&app_data->radios[i], i);
    }

    // Serial RIUC4 interface for radios ...
    riuc4_init(&app_data->serial, &app_data->riuc4, &on_riuc4_status);
    riuc4_start(&app_data->serial, app_data->serial_file); // Consider when to start !!!
    
    // RIU server ...
    strncpy(riu_cnt_string, app_data->riu_connection_string, sizeof(app_data->riu_connection_string));
    app_data->rserver.on_request_f = &on_request;
    riu_server_init(&app_data->rserver, riu_cnt_string);
    riu_server_start(&app_data->rserver);

    // Arbiter client ...
    arbiter_client_open(&app_data->aclient, app_data->abt_connection_string);
}



int main(int argc, char *argv[]) {
    int i;

    // FIXME: hardcode
    memset(app_data.serial_file, 0, sizeof(app_data.serial_file));
    strncpy(app_data.serial_file, argv[1], strlen(argv[1]));
    memset(app_data.riu_connection_string, 0, sizeof(app_data.riu_connection_string));
    strncpy(app_data.riu_connection_string, argv[3], strlen(argv[3]));
    memset(app_data.abt_connection_string, 0, sizeof(app_data.abt_connection_string));
    strncpy(app_data.abt_connection_string, argv[2], strlen(argv[2]));

    init(&app_data);

    // Periodically send ABT_UP to arbiter 
    while (1) {
        for (i = 0; i < 4; i++)
            send_abt_up(&app_data, i);
        sleep(1);   
    }

    return 0;
}

