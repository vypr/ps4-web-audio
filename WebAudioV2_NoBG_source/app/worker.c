/* PS4 Web Audio V2 / FW14 capture-ring-to-LAN experiment.
 * Standalone payload. No /dev/bt, no Bluetooth pairing, no kernel patch,
 * no writes to Bluetooth Audio's files or settings, no HDMI mute changes.
 */
#include "runtime.h"
#include "web_audio.h"
#include "web_page.h"

#define WA_PORT 8098
#define WA_CLIENTS 10
#define WA_MAX_STREAMS 8
#define WA_QUEUE_CAP 32768u /* power of two */
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_ACCEPT 30
#define SYS_MKDIR 136
#define SYS_SETSOCKOPT 105
#define WA_STOP "/data/ps4_web_audio/stop"

/* The listener and all client sockets are nonblocking. No send or capture
 * operation may wait for a browser. Nonblocking backpressure is per-client. */
struct client {
    long fd;
    int mode; /* 0 request, 1 page/status, 2 game, 3 PS4 network tone */
    unsigned quality; /* Each browser selects its own speed/fidelity */
    unsigned unavailable; /* Listener-cap response */
    u64 opened;
    u32 received;
    char request[1024];
    char header[256];
    u32 head_len,head_sent;
    const u8 *body;
    u32 body_len,body_sent;
    char reply[768];
    u32 out_read,out_write;
    u8 out[WA_QUEUE_CAP];
};
struct app {
    int (*sleep)(unsigned);
    u64 (*clock)(void);
    int (*shm)(const char *, int, int);
    void*(*map)(void *,u64,int,int,int,u64);
    int (*unmap)(void *,u64);
    long listener,fd;
    u8 *ring;
    u32 readpos,last_write, tone_phase;
    u64 last_write_at, next_open, due, packets, und, trims, stalled, overflows, rejected, disconnects, accepts;
    int prebuffer, status;
    struct client clients[WA_CLIENTS];
};
static struct app g_app;
static void close_client(struct client *c){
    if(c->fd>=0){closefd(c->fd);g_app.disconnects++;}
    c->fd=-1;
    c->mode=0;
}
static void append_text(char *dst,u32 *pos,u32 cap,const char *src){
    while(*src && *pos+1<cap)dst[(*pos)++]=*src++;
    dst[*pos]=0;
}
static void append_num(char *dst,u32 *pos,u32 cap,u64 n){
    char buf[24];u32 m=0;
    do{buf[m++]=(char)('0'+n%10);n/=10;}while(n&&m<sizeof(buf));
    while(m&&*pos+1<cap)dst[(*pos)++]=buf[--m];
    dst[*pos]=0;
}
static void status_body(struct app *a,struct client *c,u64 now){
    u32 n=0;char *b=c->reply;u32 cap=sizeof(c->reply);b[0]=0;
    append_text(b,&n,cap,"{\"version\":\"WEB-AUDIO-V1\",\"port\":8098,\"sample_rate\":48000,\"channels\":2,");
    append_text(b,&n,cap,"\"capture_open\":");append_num(b,&n,cap,a->ring!=0);
    append_text(b,&n,cap,",\"capture_fresh\":");append_num(b,&n,cap,a->ring && a->last_write_at && now-a->last_write_at<250000);
    append_text(b,&n,cap,",\"clients\":");
    u32 peers=0;for(int i=0;i<WA_CLIENTS;i++)if(a->clients[i].fd>=0&&a->clients[i].mode>=2)peers++;
    append_num(b,&n,cap,peers);
    append_text(b,&n,cap,",\"max_listeners\":8,\"accepted\":");append_num(b,&n,cap,a->accepts);
    append_text(b,&n,cap,",\"disconnected\":");append_num(b,&n,cap,a->disconnects);
    append_text(b,&n,cap,",\"packets\":");append_num(b,&n,cap,a->packets);
    append_text(b,&n,cap,",\"underruns\":");append_num(b,&n,cap,a->und);
    append_text(b,&n,cap,",\"trims\":");append_num(b,&n,cap,a->trims);
    append_text(b,&n,cap,",\"slow_clients\":");append_num(b,&n,cap,a->overflows);
    append_text(b,&n,cap,",\"rejected_peers\":");append_num(b,&n,cap,a->rejected);
    append_text(b,&n,cap,"}");
    c->body=(const u8*)b;c->body_len=n;
}
static void setup_header(struct client *c,const char *type,int stream){
    u32 n=0;c->header[0]=0;
    append_text(c->header,&n,sizeof(c->header),c->unavailable?"HTTP/1.1 503 Service Unavailable\r\nContent-Type: ":"HTTP/1.1 200 OK\r\nContent-Type: ");
    append_text(c->header,&n,sizeof(c->header),type);
    append_text(c->header,&n,sizeof(c->header),"\r\nCache-Control: no-store\r\nConnection: close\r\n");
    if(stream) append_text(c->header,&n,sizeof(c->header),"Transfer-Encoding: chunked\r\n");
    else {append_text(c->header,&n,sizeof(c->header),"Content-Length: ");append_num(c->header,&n,sizeof(c->header),c->body_len);append_text(c->header,&n,sizeof(c->header),"\r\n");}
    append_text(c->header,&n,sizeof(c->header),"\r\n");
    c->head_len=n;c->head_sent=0;
}
static void parse_request(struct app *a,struct client *c,u64 now){
    if(starts(c->request,"GET /stream.pcm ") || starts(c->request,"GET /tone.pcm ") ||
       starts(c->request,"GET /stream.pcm?") || starts(c->request,"GET /tone.pcm?")){
        int streams=0;for(int j=0;j<WA_CLIENTS;j++)if(a->clients[j].fd>=0&&a->clients[j].mode>=2)streams++;
        if(streams>=WA_MAX_STREAMS){
            c->mode=1;c->unavailable=1;u32 n=0;append_text(c->reply,&n,sizeof(c->reply),"Too many live listeners (maximum 8).");
            c->body=(const u8*)c->reply;c->body_len=n;setup_header(c,"text/plain; charset=utf-8",0);return;
        }
        int new_mode=starts(c->request,"GET /tone.pcm") ? 3 : 2;
        if(new_mode==2){
            int previous_game=0;
            for(int j=0;j<WA_CLIENTS;j++)if(j!=(int)(c-a->clients) && a->clients[j].fd>=0 && a->clients[j].mode==2)previous_game++;
            if(!previous_game)a->prebuffer=1; /* Re-arm on first game listener only. */
        }
        c->mode=new_mode;
        c->quality=(unsigned)wa_quality_from_path(c->request);
        c->body=0;c->body_len=0;
        setup_header(c,"application/octet-stream",1);
        /* Do not reset the shared ring when a second browser joins. */
    }else if(starts(c->request,"GET /status ")){
        c->mode=1;status_body(a,c,now);setup_header(c,"application/json; charset=utf-8",0);
    }else if(starts(c->request,"GET / ")||starts(c->request,"GET /index.html ")){
        c->mode=1;c->body=web_page;c->body_len=sizeof(web_page);
        setup_header(c,"text/html; charset=utf-8",0);
    }else{
        c->mode=1;u32 n=0;append_text(c->reply,&n,sizeof(c->reply),"Open http://PS4-IP:8098/ to listen.");
        c->body=(const u8*)c->reply;c->body_len=(u32)length(c->reply);
        setup_header(c,"text/plain; charset=utf-8",0);
    }
}
static void out_byte(struct client *c,u8 v){c->out[c->out_write++ & (WA_QUEUE_CAP-1u)]=v;}
static void out_pcm16(struct client*c,short sample){
    u16 s=(u16)sample;out_byte(c,(u8)(s&255));out_byte(c,(u8)(s>>8));
}
static void out_f32(struct client*c,float sample){
    union {float f;u32 u;} x; x.f=wa_float_limited(sample);
    out_byte(c,(u8)x.u);out_byte(c,(u8)(x.u>>8));out_byte(c,(u8)(x.u>>16));out_byte(c,(u8)(x.u>>24));
}
static void enqueue_pcm(struct app*a,struct client*c,const short pcm[WA_SAMPLES],const float fp[WA_SAMPLES]){
    u32 bytes=wa_chunk_bytes(c->quality);
    /* Transfer-Encoding: chunked. No blocking on slow clients. */
    u32 frame_len=bytes+7u; /* 3 hex digits + CRLF + payload + CRLF */
    if(c->out_write-c->out_read>WA_QUEUE_CAP-frame_len-1u){a->overflows++;close_client(c);return;}
    static const char hex[]="0123456789ABCDEF";
    out_byte(c,(u8)hex[(bytes>>8)&15]);out_byte(c,(u8)hex[(bytes>>4)&15]);out_byte(c,(u8)hex[bytes&15]);
    out_byte(c,'\r');out_byte(c,'\n');
    if(c->quality==WA_Q_FAST){
        for(unsigned i=0;i<WA_FRAMES/2;i++)out_pcm16(c,wa_fast_mono_sample(pcm,i));
    }else if(c->quality==WA_Q_STUDIO){
        for(unsigned i=0;i<WA_SAMPLES;i++)out_f32(c,fp[i]);
    }else{
        for(unsigned i=0;i<WA_SAMPLES;i++)out_pcm16(c,pcm[i]);
    }
    out_byte(c,'\r');out_byte(c,'\n');
}
static void network_tick(struct app *a,u64 now){
    for(int i=0;i<WA_CLIENTS;i++){
        struct client *c=&a->clients[i];
        if(c->fd<0){
            struct sockaddr4 peer;u32 addrlen=sizeof(peer);
            long fd=sc3(SYS_ACCEPT,a->listener,(long)&peer,(long)&addrlen);
            if(fd<0)continue;
            /* Only local/private-network devices: the stream has no authentication. */
            if(addrlen!=sizeof(peer)||!local_ip((const u8*)&peer.addr)||nonblock(fd)<0){a->rejected++;closefd(fd);continue;}
            int yes=1;
            /* FreeBSD/Orbis SO_NOSIGPIPE. Avoid process termination on peer close. */
            sc5(SYS_SETSOCKOPT,fd,0xffff,WA_SO_NOSIGPIPE,(long)&yes,4);
            memset(c,0,sizeof(*c));c->fd=fd;c->opened=now;a->accepts++;
        }
        if(c->mode==0){
            if(now-c->opened>3000000){close_client(c);continue;}
            long n=sc3(SYS_READ,c->fd,(long)(c->request+c->received),sizeof(c->request)-c->received-1);
            if(n==0||(n<0&&n!=-35)){close_client(c);continue;}
            if(n>0){
                c->received+=(u32)n;c->request[c->received]=0;
                if(find(c->request,"\r\n\r\n"))parse_request(a,c,now);
                else if(c->received>=sizeof(c->request)-1)close_client(c);
            }
        }
        if(c->mode!=0){
            if(c->head_sent<c->head_len){
                long n=wa_send(c->fd,(const void*)(c->header+c->head_sent),c->head_len-c->head_sent);
                if(n>0)c->head_sent+=(u32)n;
                else if(n<0&&n!=-35){close_client(c);continue;}
            }
            if(c->head_sent!=c->head_len)continue;
            if(c->mode==1){
                if(c->body_sent<c->body_len){
                    long n=wa_send(c->fd,(const void*)(c->body+c->body_sent),c->body_len-c->body_sent);
                    if(n>0)c->body_sent+=(u32)n;
                    else if(n<0&&n!=-35){close_client(c);continue;}
                }
                if(c->body_sent==c->body_len)close_client(c);
            }else{
                u32 used=c->out_write-c->out_read;
                if(used){
                    u32 start=c->out_read & (WA_QUEUE_CAP-1u);
                    u32 take=WA_QUEUE_CAP-start;
                    if(take>used)take=used;
                    long n=wa_send(c->fd,(const void*)(c->out+start),take);
                    if(n>0)c->out_read+=(u32)n;
                    else if(n<0&&n!=-35)close_client(c);
                }
            }
        }
    }
}
static void try_capture_open(struct app *a,u64 now){
    if(a->ring||now<a->next_open)return;
    a->next_open=now+2000000;
    long fd=a->shm("/shm_acap.full",0,0);
    if(fd<0){a->status=1;return;}
    void *p=a->map(0,0x10000,1,1,fd,0);
    if(!p||p==(void*)-1){closefd(fd);a->status=2;return;}
    if(*(volatile u32*)p!=0x10000u){a->unmap(p,0x10000);closefd(fd);a->status=3;return;}
    a->fd=fd;a->ring=p;a->status=0;a->prebuffer=1;
    a->last_write=*(volatile u32*)(a->ring+12);
    if(!wa_valid_cursor(a->last_write)){
        a->unmap(a->ring,0x10000);a->ring=0;closefd(a->fd);a->fd=-1;a->status=4;return;
    }
    a->readpos=a->last_write;
    a->last_write_at=now;
}
static void sample_tick(struct app *a,u64 now){
    int game=0,tone=0;
    for(int i=0;i<WA_CLIENTS;i++){
        if(a->clients[i].fd<0)continue;
        if(a->clients[i].mode==2)game=1;
        if(a->clients[i].mode==3)tone=1;
    }
    if(!game&&!tone)return;
    if(game)try_capture_open(a,now);
    short pcm[WA_SAMPLES],sine[WA_SAMPLES];
    float fp[WA_SAMPLES],sine_f[WA_SAMPLES];
    if(game){
        int sample_ok=0;
        if(a->ring){
            u32 wp=(u32)(*(volatile u64*)(a->ring+8)>>32);
            if(wa_valid_cursor(wp)){
                if(wp!=a->last_write){a->last_write=wp;a->last_write_at=now;}
                if(now-a->last_write_at<250000){
                    u32 avail=wa_available(wp,a->readpos);
                    if(a->prebuffer){
                        if(avail<WA_BUFFER_BYTES){/* Not yet enough input; send silence */}
                        else{
                            a->readpos=wa_advance(wp,WA_RING_BYTES-WA_BUFFER_BYTES);
                            a->prebuffer=0;
                            avail=WA_BUFFER_BYTES;
                        }
                    }
                    if(!a->prebuffer && avail>=WA_RAW_BYTES){
                        /* Keep source backlog between ~32 and ~40 ms; no bursts. */
                        if(avail>WA_BUFFER_BYTES+WA_RAW_BYTES){
                            u32 skip=(avail-WA_BUFFER_BYTES)&~7u;
                            a->readpos=wa_advance(a->readpos,skip);a->trims++;
                        }
                        a->readpos=wa_read_capture(a->ring,a->readpos,pcm,fp);
                        sample_ok=1;
                    }
                }else{a->prebuffer=1;a->stalled++;}
            }else {a->prebuffer=1;a->status=4;}
        }
        if(!sample_ok){memset(pcm,0,sizeof(pcm));memset(fp,0,sizeof(fp));a->und++;}
    }
    if(tone){wa_make_tone(sine,&a->tone_phase);for(unsigned i=0;i<WA_SAMPLES;i++)sine_f[i]=(float)sine[i]/32768.f;}
    for(int i=0;i<WA_CLIENTS;i++){
        struct client *c=&a->clients[i];
        if(c->fd<0)continue;
        if(c->mode==2)enqueue_pcm(a,c,pcm,fp);
        else if(c->mode==3)enqueue_pcm(a,c,sine,sine_f);
    }
    a->packets++;
}
void*wa_worker(void *arg){
    struct app *a=&g_app;
    memset(a,0,sizeof(*a));
    a->listener=(long)arg;a->fd=-1;
    for(int i=0;i<WA_CLIENTS;i++)a->clients[i].fd=-1;
    if(resolve("sceKernelUsleep",&a->sleep)||resolve("sceKernelGetProcessTime",&a->clock)||
       resolve("shm_open",&a->shm)||resolve("mmap",&a->map)||resolve("munmap",&a->unmap)||
       !a->sleep||!a->clock||!a->shm||!a->map||!a->unmap){closefd(a->listener);return 0;}
    sc3(SYS_MKDIR,(long)"/data/ps4_web_audio",0777,0);
    sc3(15,(long)"/data/ps4_web_audio",0777,0);
    sc3(10,(long)WA_STOP,0,0);
    notify("PS4 Web Audio V2 running. On PC open http://YOUR-PS4-IP:8098/ then click Listen.");
    u64 now=a->clock();a->due=now+WA_PERIOD_US;a->next_open=now;
    u64 stop_check_at=now+1000000;
    for(;;){
        now=a->clock();
        if(now>=stop_check_at){if(exists(WA_STOP))break;stop_check_at=now+1000000;}
        network_tick(a,now);
        if(now>=a->due){
            sample_tick(a,now);
            if(now-a->due>12000)a->due=now+WA_PERIOD_US;
            else a->due+=WA_PERIOD_US;
        }
        /* No Bluetooth driver or HDMI operations anywhere in this loop. */
        a->sleep(900);
    }
    for(int i=0;i<WA_CLIENTS;i++)close_client(&a->clients[i]);
    if(a->ring)a->unmap(a->ring,0x10000);
    closefd(a->fd);closefd(a->listener);
    return 0;
}
