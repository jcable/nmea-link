#include "linebuf.h"

struct control_block *new_lb(void* o, int(*send)(void*,uint8_t*,size_t)) {
    struct control_block *cb = (struct control_block *)os_malloc(sizeof(struct control_block));
    cb->buf = NULL;
    cb->max = DEFAULT_BUF_SIZE;
    cb->outp = o;
    cb->send = send;
    return cb;
}

void init_lb(struct control_block *cb, size_t len) {
    if(len>cb->max) cb->max=len;
    cb->buf = (uint8_t*)os_malloc(cb->max);
    cb->n = 0;
}

void lb_send(void* vcb, const char *data, unsigned short n) {
    struct control_block *cb = (struct control_block*)vcb;
    uint8_t *b = (uint8_t*)data;
    uint8_t *p = b;
    uint8_t *q = p+n-1;
    size_t m = n;
    // if have bytes pending
    if(cb->buf!=NULL) {
        // get up to end of first line
        while(p<q && *p!='\n') p++;
        if(*p=='\n') {
            p++;
            // merge with cb->buf and send
            int newn = p-b;
            int len = cb->n + newn;
            if(len>0) {
                uint8_t *r = (uint8_t*)os_malloc(len);
                for(int i=0; i<cb->n; i++) r[i]=cb->buf[i];
                for(int i=cb->n; i<len; i++) r[i]=b[i-cb->n];
                cb->send(cb->outp, r, len);
                m -= newn;
            }
            // free cb->buf
            os_free(cb->buf);
            cb->buf = NULL;
        }
        else {
            p=b;
        }
    }
    // find last end of line
    while(q>=p && *q!='\n') q--;
    // save in cb->buf
    if(q>=p) {
        if(*q=='\n') {
            q++;
            int len = b+n-q;
            if(cb->buf==NULL) {
                init_lb(cb, len);
            }
            for(int i=0; i<len; i++) {
                cb->buf[cb->n++] = *q++;
            }
            m -= len;
        }
        else {
            // can't happen?
        }
    }
    else {
        // no end of line in buffer, add this buffer to cb->buf
        if(cb->buf==NULL) {
            init_lb(cb, m);
        }
        else {
            if(cb->n+n>cb->max) {
                cb->max+=m;
                uint8_t *o = (uint8_t*)os_malloc(cb->max);
                for(int i=0; i<cb->n; i++) {
                    o[i] = cb->buf[i];
                }
                os_free(cb->buf);
                cb->buf=o;
            }
        }
        for(int i=0; i<m; i++) {
            cb->buf[cb->n++] = *p++;
        }
        m = 0;
    }
    // send to end of last complete line
    if(m>0)
        cb->send(cb->outp, p, m);
}
