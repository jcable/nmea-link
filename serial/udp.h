#ifndef __UDP
#define __UDP

#include <esp8266.h>

espconn* ICACHE_FLASH_ATTR udpBroadcastInit(int port);
void ICACHE_FLASH_ATTR
udp_broadcast_send(void *conn, const char *data, uint16 len);

#endif
