/* Host-level transport framing and multi-client admission tests. Does not
 * execute PS4 syscalls or simulate real headphones. */
#include <assert.h>
#include <stdio.h>
#include "../app/worker.c"
void notify(const char *message){(void)message;}
static unsigned pending(const struct client*c){return c->out_write-c->out_read;}
static int check_chunk(const struct client*c,unsigned n){
    static const char hex[]="0123456789ABCDEF";
    return c->out[0]==(u8)hex[(n>>8)&15] &&
           c->out[1]==(u8)hex[(n>>4)&15] && c->out[2]==(u8)hex[n&15] &&
           c->out[3]=='\r'&&c->out[4]=='\n' &&
           c->out[n+5]=='\r'&&c->out[n+6]=='\n';
}
int main(void){
    struct app *a=&g_app;
    memset(a,0,sizeof(*a));
    for(int i=0;i<WA_CLIENTS;i++)a->clients[i].fd=-1;
    short samples[WA_SAMPLES];float floats[WA_SAMPLES];
    for(unsigned i=0;i<WA_SAMPLES;i++){
        samples[i]=(short)(i-192);floats[i]=samples[i]/32768.f;
    }
    for(unsigned q=0;q<3;q++){
        struct client *c=&a->clients[q];
        c->fd=100+q;c->mode=2;c->quality=q;
        enqueue_pcm(a,c,samples,floats);
        assert(pending(c)==wa_chunk_bytes(q)+7);
        assert(check_chunk(c,wa_chunk_bytes(q)));
        assert(c->fd==100+(long)q);
    }
    /* New browser must not alter the global capture prebuffer. */
    a->prebuffer=0;
    struct client *c=&a->clients[3];
    c->fd=103;
    const char *get="GET /tone.pcm?quality=studio HTTP/1.1\r\nHost: x\r\n\r\n";
    memcpy(c->request,get,length(get)+1);
    parse_request(a,c,500);
    assert(c->mode==3&&c->quality==WA_Q_STUDIO);
    assert(a->prebuffer==0);
    /* All eight active stream slots are occupied: give ninth a 503 status,
     * while preserving the existing streams. */
    for(int j=4;j<8;j++){a->clients[j].mode=2;a->clients[j].fd=100+j;}
    struct client *ninth=&a->clients[8];ninth->fd=108;
    const char *get2="GET /stream.pcm?quality=fast HTTP/1.1\r\n\r\n";
    memcpy(ninth->request,get2,length(get2)+1);
    parse_request(a,ninth,600);
    assert(ninth->mode==1 && ninth->unavailable==1);
    assert(starts(ninth->header,"HTTP/1.1 503"));
    assert(a->clients[0].mode==2 && a->clients[7].mode==2);
    puts("V2 multi-profile chunk framing, join isolation and 8-client cap: PASS");
    return 0;
}
