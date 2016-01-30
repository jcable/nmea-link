#ifndef CONSOLE_H
#define CONSOLE_H

#include "httpd.h"

void ICACHE_FLASH_ATTR console_send(void *nu, const char *data, uint16 len);
void consoleInit(void);
int ajaxConsole(HttpdConnData *connData);
int ajaxConsoleReset(HttpdConnData *connData);
int ajaxConsoleBaud(HttpdConnData *connData);
int ajaxConsoleSend(HttpdConnData *connData);
int tplConsole(HttpdConnData *connData, char *token, void **arg);

#endif
