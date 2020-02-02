// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include "tcp.h"
#include "mux.h"
#include "linebuf.h"

static espconn serverConn;
static esp_tcp serverTcp;

// Receive callback
static void ICACHE_FLASH_ATTR
tcpClientRecvCb(void *arg, char *data, unsigned short len)
{
    MuxOpData *conn = ((espconn*)arg)->reverse;
    //os_printf("Receive callback on conn %p\n", conn);
    if (conn == NULL) return;
    lb_send(conn->recv_data, data, len);
}

//===== UART -> TCP

// Send all data in conn->txbuffer
// returns result from espconn_send if data in buffer or ESPCONN_OK (0)
// Use only internally from espbuffsend and tcpClientSentCb
static sint8 ICACHE_FLASH_ATTR
sendtxbuffer(MuxOpData *conn)
{
    sint8 result = ESPCONN_OK;
    if (conn->txbufferlen != 0) {
        //os_printf("TX %p %d\n", conn, conn->txbufferlen);
        conn->readytosend = false;
        result = espconn_send(conn->conn, (uint8_t*)conn->txbuffer, conn->txbufferlen);
        conn->txbufferlen = 0;
        if (result != ESPCONN_OK) {
            os_printf("sendtxbuffer: espconn_sent error %d on conn %p\n", result, conn);
            conn->txbufferlen = 0;
            if (!conn->txoverflow_at) conn->txoverflow_at = system_get_time();
        } else {
            conn->sentbuffer = conn->txbuffer;
            conn->txbuffer = NULL;
            conn->txbufferlen = 0;
        }
    }
    return result;
}

// espbuffsend adds data to the send buffer. If the previous send was completed it calls
// sendtxbuffer and espconn_sent.
// Returns ESPCONN_OK (0) for success, -128 if buffer is full or error from  espconn_sent
// Use espbuffsend instead of espconn_sent as it solves the problem that espconn_sent must
// only be called *after* receiving an espconn_sent_callback for the previous packet.
static void ICACHE_FLASH_ATTR
espbuffsend(void *vconn, const char *data, uint16 len)
{
    MuxOpData *conn = (MuxOpData*)vconn;

    if (conn->txbufferlen >= MAX_TXBUFFER) goto overflow;

    // make sure we indeed have a buffer
    if (conn->txbuffer == NULL) conn->txbuffer = os_zalloc(MAX_TXBUFFER);
    if (conn->txbuffer == NULL) {
        os_printf("Mux:tcp: cannot alloc tx buffer\n");
        //return -128;
    }

    // add to send buffer
    uint16_t avail = conn->txbufferlen+len > MAX_TXBUFFER ? MAX_TXBUFFER-conn->txbufferlen : len;
    os_memcpy(conn->txbuffer + conn->txbufferlen, data, avail);
    conn->txbufferlen += avail;

    // try to send
    //sint8 result = ESPCONN_OK;
    if (conn->readytosend)
        sendtxbuffer(conn);

    if (avail < len) {
        // some data didn't fit into the buffer
        if (conn->txbufferlen == 0) {
            // we sent the prior buffer, so try again
            return espbuffsend(conn, data+avail, len-avail);
        }
        goto overflow;
    }
    //return result;

overflow:
    if (conn->txoverflow_at) {
        // we've already been overflowing
        if (system_get_time() - conn->txoverflow_at > 10*1000*1000) {
            // no progress in 10 seconds, kill the connection
            os_printf("mux:tcp: killing overlowing stuck conn %p\n", conn);
            espconn_disconnect(conn->conn);
        }
        // else be silent, we already printed an error
    } else {
        // print 1-time message and take timestamp
        os_printf("mux:tcp: txbuffer full, conn %p\n", conn);
        conn->txoverflow_at = system_get_time();
    }
    //return -128;
}

//callback after the data are sent
static void ICACHE_FLASH_ATTR
tcpClientSentCb(void *arg)
{
    MuxOpData *conn = ((espconn*)arg)->reverse;
    //os_printf("Sent CB %p\n", conn);
    if (conn == NULL) return;
    //os_printf("%d ST\n", system_get_time());
    if (conn->sentbuffer != NULL) os_free(conn->sentbuffer);
    conn->sentbuffer = NULL;
    conn->readytosend = true;
    conn->txoverflow_at = 0;
    sendtxbuffer(conn); // send possible new data in txbuffer
}


// Disconnection callback
static void ICACHE_FLASH_ATTR
tcpClientDisconCb(void *arg)
{
    MuxOpData *conn = ((espconn*)arg)->reverse;
    if (conn == NULL) return;
    // Free buffers
    if (conn->sentbuffer != NULL) os_free(conn->sentbuffer);
    conn->sentbuffer = NULL;
    if (conn->txbuffer != NULL) os_free(conn->txbuffer);
    conn->txbuffer = NULL;
    conn->txbufferlen = 0;
    conn->conn = NULL;
}

// Connection reset callback (note that there will be no DisconCb)
static void ICACHE_FLASH_ATTR
tcpClientResetCb(void *arg, sint8 err)
{
    os_printf("mux:tcp: connection reset err=%d\n", err);
    tcpClientDisconCb(arg);
}

// New connection callback, use one of the connection descriptors, if we have one left.
static void ICACHE_FLASH_ATTR
tcpClientConnectCb(void *arg)
{
    espconn *conn = arg;

    int r = new_mux_dest();
    if(r==-1) {
	os_printf("tcp: out of mem");
        espconn_disconnect(conn);
        return;
    }
    uint8_t sid = (uint8_t)r;
    MuxOpData *mod = getConnData(sid);
    mod->recv_data = new_lb(&mux,sid);
    setConnectionRecord(sid, conn, espbuffsend);
    espconn_regist_recvcb(conn, tcpClientRecvCb);
    espconn_regist_disconcb(conn, tcpClientDisconCb);
    espconn_regist_reconcb(conn, tcpClientResetCb);
    espconn_regist_sentcb(conn, tcpClientSentCb);

    espconn_set_opt(conn, ESPCONN_REUSEADDR|ESPCONN_NODELAY);
}

void ICACHE_FLASH_ATTR
tcpServerInit(int port)
{
    os_memset(&serverTcp, 0, sizeof(serverTcp));
    serverConn.type = ESPCONN_TCP;
    serverConn.state = ESPCONN_NONE;
    serverTcp.local_port = port;
    serverConn.proto.tcp = &serverTcp;
    espconn_regist_connectcb(&serverConn, tcpClientConnectCb);
    espconn_accept(&serverConn);
    espconn_tcp_set_max_con_allow(&serverConn, MAX_CONN);
    espconn_regist_time(&serverConn, SER_BRIDGE_TIMEOUT, 0);
}
