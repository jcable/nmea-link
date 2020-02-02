// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include "esp8266.h"

#include "linebuf.h"
#include "uart.h"
#include "udp.h"
#include "tcp.h"
#include "crc16.h"
#include "mux.h"
#include "serled.h"
#include "config.h"
#include "console.h"
#include "syslog.h"
#include "initpins.h"

#define SKIP_AT_RESET

// Connection pool
MuxData mux;
static struct lb_control_block* uart_lb_cb;

MuxOpData *getConnData(uint8_t id) {
    return &mux.connection[id];
}

int ICACHE_FLASH_ATTR
mux_write(MuxData *mux, mux_buf* mb) {
    // push the buffer into each open connection
    for (uint8_t i=0; i<mux->n; i++) {
        if(i!=mb->source_id) { // TODO - allow serial connections to forward
            MuxOpData *op = &mux->connection[i];
            if (op->conn) {
                op->send(op, (char*)mb->data, mb->len);
            }
        }
    }
    return 0;
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
serbridgeUartCb(char *data, short len)
{
    os_printf("serial: data received\n");
    lb_send(uart_lb_cb, data, len);
    serledFlash(50); // short blink on serial LED
}

static void ICACHE_FLASH_ATTR
uart_send(void *conn, const char *data, uint16 len)
{
    os_printf("send to serial\n");
    uart0_tx_buffer(data, len);
    serledFlash(50); // short blink on serial LED
}

int new_mux_dest() {
    int i;
    for (i=0; i<MAX_CONN; i++) if (mux.connection[i].conn==NULL) break;
    if (i==MAX_CONN) {
        return -1;
    }
    return i;
}

void ICACHE_FLASH_ATTR
setConnectionRecord(
    int crid,
    espconn *send_conn, void (*send)(void*, const char*, uint16)
)
{
    MuxOpData *cd = getConnData(crid);
    os_memset(cd, 0, sizeof(MuxOpData));
    cd->conn = send_conn;
    if(send_conn != NULL)
        send_conn->reverse = cd;
    cd->readytosend = true;
    cd->send = send;
}

int ICACHE_FLASH_ATTR
allocateConnectionRecord(
    espconn *send_conn, void (*send)(void*, const char*, uint16)
)
{
    int i = new_mux_dest();
    if (i==-1) {
        return -1;
    }
    setConnectionRecord(i, send_conn, send);
    return i;
}

// Start transparent serial bridge TCP server on specified port (typ. 23)
void ICACHE_FLASH_ATTR
muxInit(int port)
{
    serbridgeInitPins();
    os_memset(&mux, 0, sizeof(mux));
    mux.n=MAX_CONN;
    tcpServerInit(port);
    if(flashConfig.udp_enable) {
        espconn* udpConn = udpBroadcastInit(flashConfig.udp_port);
// TODO decide if to receive UDP
        if(allocateConnectionRecord(udpConn, udp_broadcast_send) == -1) {
            syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "nmea-link", "udp broadcast enabled\n");
        }
    }
    if(allocateConnectionRecord(NULL, console_send) == -1) {
        os_printf("nmea-link console enabled\n");
    }
    uint8_t uart_sid = new_mux_dest();
    uart_lb_cb = new_lb(&mux, uart_sid);
    setConnectionRecord(uart_sid, NULL, uart_send);
    os_printf("nmea-link uart enabled\n");
}
