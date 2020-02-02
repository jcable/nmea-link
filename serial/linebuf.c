#include "linebuf.h"

struct lb_control_block *new_lb(MuxData* mux, uint8_t sid) {
    struct lb_control_block *cb = (struct lb_control_block *)os_malloc(sizeof(struct lb_control_block));
    if(cb == NULL) {
      os_printf("linebuf: out of memory");
      return NULL;
    }
    cb->buf = NULL;
    cb->max = DEFAULT_BUF_SIZE;
    cb->mux = mux;
    cb->source_id = sid;
    return cb;
}

void init_lb(struct lb_control_block *cb, size_t len) {
    if(len>cb->max) cb->max=len;
    cb->buf = (uint8_t*)os_malloc(cb->max);
    cb->n = 0;
}

void lb_send(void* vcb, const char *data, unsigned short n) {
    struct lb_control_block *cb = (struct lb_control_block*)vcb;
    os_printf("lb_send %d bytes, %d already in buff\n", n, cb->n);
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
                mux_buf *mb = (mux_buf*)os_malloc(sizeof(mux_buf));
                mb->data = (uint8_t*)os_malloc(len);
                mb->len = len;
                mb->source_id = cb->source_id;
                for(int i=0; i<cb->n; i++) mb->data[i]=cb->buf[i];
                for(int i=cb->n; i<len; i++) mb->data[i]=b[i-cb->n];
		os_printf("lb_send sending merged %d bytes\n", len);
                mux_write(cb->mux, mb);
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
    if(m>0) {
	os_printf("lb_send sending %d bytes\n", m);
        mux_buf *mb = (mux_buf*)os_malloc(sizeof(mux_buf));
        mb->data = p; // TODO where does this buffer get freed?
        mb->len = m;
        mb->source_id = cb->source_id;
        mux_write(cb->mux, mb);
    }
}
