#include "udp.h"

static struct espconn udpConn;

//callback after the data are sent
static void ICACHE_FLASH_ATTR
udpSentCb(void *arg)
{
    os_printf("udp: data sent\n");
}

//callback when data received
static void ICACHE_FLASH_ATTR
udpRecvCb(void *arg, char *pdata, unsigned short len)
{
    os_printf("udp: data received\n");
}

void ICACHE_FLASH_ATTR
udp_broadcast_send(void *conn, const char *data, uint16 len)
{
    //os_printf("udp: send\n");
    espconn_send(&udpConn, (uint8_t*)data, len);
}

espconn* ICACHE_FLASH_ATTR
udpBroadcastInit(int port)
{
#ifdef SERBR_DBG
    //os_printf("Mux: enable udp\n");
#endif
    udpConn.proto.udp = (esp_udp*)os_zalloc(sizeof(esp_udp));
    udpConn.type = ESPCONN_UDP;
    udpConn.proto.udp->remote_port = port;
    struct ip_info info;
    if(wifi_get_ip_info(STATION_IF, &info)) {
        uint32_t nw = info.ip.addr & info.netmask.addr;
        uint32_t bc = nw | ~info.netmask.addr;
        os_memcpy(udpConn.proto.udp->remote_ip, &bc, 4);
    }
    udpConn.proto.udp->local_port = espconn_port();
    espconn_regist_recvcb(&udpConn, udpRecvCb);
    espconn_regist_sentcb(&udpConn, udpSentCb);
    //wifi_set_broadcast_if(STATIONAP_MODE); only sending, not listening
    espconn_create(&udpConn);
    return &udpConn;
}
