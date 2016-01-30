#ifndef __sl_h
#define __sl_h
#include <esp8266.h>

struct control_block {
    uint8_t *buf;
    size_t n;
    size_t max;
    void *outp;
    int (*send)(void*, uint8_t*, size_t);
};

#define DEFAULT_BUF_SIZE 255

struct control_block *new_lb(void* o, int(*send)(void*,uint8_t*,size_t));

void lb_send(void*, const char *b, unsigned short len);
#endif
