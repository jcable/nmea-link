#ifndef __sl_h
#define __sl_h
#include <esp8266.h>
#include <mux.h>

struct lb_control_block {
    uint8_t *buf;
    size_t n;
    size_t max;
    uint8_t source_id;
    MuxData *mux;
};

#define DEFAULT_BUF_SIZE 255

struct lb_control_block *new_lb(MuxData* mux, uint8_t sid);

void lb_send(void*, const char *b, unsigned short len);
#endif
