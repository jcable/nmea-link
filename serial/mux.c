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
#include "cmd.h"
#include "syslog.h"

#define SKIP_AT_RESET



// Connection pool
serbridgeConnData connData[MAX_CONN];

void* ICACHE_FLASH_ATTR
new_mux_src() {
    return new_lb(connData, mux_write);
}

int ICACHE_FLASH_ATTR
mux_write(void* vcd, uint8_t* buf, size_t len) {
    serbridgeConnData *cd = (serbridgeConnData*)vcd;
    // push the buffer into each open connection
    for (short i=0; i<MAX_CONN; i++) {
        serbridgeConnData *cdi = cd+i;
        if (cdi->conn) {
            cdi->send(cdi, (char*)buf, len);
        }
    }
    return 0;
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
serbridgeUartCb(char *data, short len)
{
    mux_write(connData, (uint8_t*)data, len);
    serledFlash(50); // short blink on serial LED
}

static void ICACHE_FLASH_ATTR
uart_send(void *conn, const char *data, uint16 len)
{
    uart0_tx_buffer(data, len);
    serledFlash(50); // short blink on serial LED
}

int ICACHE_FLASH_ATTR
allocateConnectionRecord(
    espconn *send_conn, void (*send)(void*, const char*, uint16),
    void *recv_conn, void(*recv)(void*, const char*, uint16)

)
{
    int i;
    for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
    if (i==MAX_CONN) {
        syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_WARNING, "nmea-link", "Aiee, conn pool overflow!\n");
        return -1;
    }
    struct serbridgeConnData *cd = connData+i;
    os_memset(cd, 0, sizeof(struct serbridgeConnData));
    cd->conn = send_conn;
    if(send_conn != NULL)
        send_conn->reverse = cd;
    cd->readytosend = true;
    cd->send = send;
    cd->recv_conn = recv_conn;
    cd->recv = recv;
    return 0;
}

void ICACHE_FLASH_ATTR
serbridgeInitPins()
{
    if (flashConfig.swap_uart) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 4);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 4);
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTCK_U);
        if (flashConfig.rx_pullup) PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDO_U);
        else                       PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDO_U);
        system_uart_swap();
    } else {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, 0);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0);
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        if (flashConfig.rx_pullup) PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
        else                       PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
        system_uart_de_swap();
    }
}

// Start transparent serial bridge TCP server on specified port (typ. 23)
void ICACHE_FLASH_ATTR
serbridgeInit(int port)
{
    serbridgeInitPins();
    os_memset(connData, 0, sizeof(connData));
    tcpServerInit(port);
    if(flashConfig.udp_enable) {
        espconn* udpConn = udpBroadcastInit(flashConfig.udp_port);
// TODO decide if to receive UDP
        if(allocateConnectionRecord(udpConn, udp_broadcast_send, NULL, NULL) == -1) {
            syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "nmea-link", "udp broadcast enabled\n");
        }
    }
    if(allocateConnectionRecord(NULL, console_send, NULL, NULL) == -1) {
        syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "nmea-link", "console enabled\n");
    }
    if(allocateConnectionRecord(NULL, uart_send, new_mux_src(), lb_send) == -1) {
        syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "nmea-link", "uart out enabled\n");
    }
}
