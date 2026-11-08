#ifndef PS4_WEB_AUDIO_H
#define PS4_WEB_AUDIO_H
#include "bta_types.h"
/* 48 kHz float32 stereo PS4 capture ring; every transport profile starts
 * from the same shared source packet and is encoded independently per listener. */
#define WA_RING_END 0x10000u
#define WA_RING_BEGIN 16u
#define WA_RING_BYTES (WA_RING_END-WA_RING_BEGIN)
#define WA_FRAMES 192u
#define WA_SAMPLES (WA_FRAMES*2u)
#define WA_RAW_BYTES (WA_SAMPLES*4u)
#define WA_PCM_BYTES (WA_SAMPLES*2u)
#define WA_PERIOD_US 4000u
#define WA_BUFFER_BYTES (WA_RAW_BYTES*8u)
#define WA_Q_FAST 0u
#define WA_Q_BALANCED 1u
#define WA_Q_STUDIO 2u
#define WA_BYTES_FAST (WA_FRAMES) /* 96 mono frames @ 24 kHz, PCM16 */
#define WA_BYTES_BALANCED WA_PCM_BYTES
#define WA_BYTES_STUDIO WA_RAW_BYTES

static inline int wa_valid_cursor(u32 v) { return v >= WA_RING_BEGIN && v < WA_RING_END && (v & 7u) == 0u; }
static inline u32 wa_advance(u32 v,u32 n) { return WA_RING_BEGIN+((v-WA_RING_BEGIN+n)%WA_RING_BYTES); }
static inline u32 wa_available(u32 w,u32 r) { return w>=r ? w-r : WA_RING_BYTES-r+w; }
static inline float wa_float_limited(float f){
    if(!(f==f))return 0.f;
    if(f>1.f)return 1.f;
    if(f < -1.f)return -1.f;
    return f;
}
static inline short wa_clamp_float(float f) { return (short)(int)(wa_float_limited(f)*32767.f); }
static inline u32 wa_read_capture(const u8 *ring,u32 pos,short *pcm,float *fl){
    for(unsigned i=0;i<WA_SAMPLES;i++){
        float f=wa_float_limited(*(const volatile float*)(ring+pos));
        fl[i]=f;pcm[i]=(short)(int)(f*32767.f);
        pos+=4;if(pos==WA_RING_END)pos=WA_RING_BEGIN;
    }
    return pos;
}
/* Legacy conversion API retained for source/test compatibility. */
static inline u32 wa_read_pcm(const u8 *ring,u32 pos,short *out) {
    for(unsigned i=0;i<WA_SAMPLES;i++){
        out[i]=wa_clamp_float(*(const volatile float*)(ring+pos));
        pos+=4;if(pos==WA_RING_END)pos=WA_RING_BEGIN;
    }
    return pos;
}
static inline int wa_prefix(const char*s,const char*p){while(*p)if(*s++!=*p++)return 0;return 1;}
static inline int wa_quality_from_path(const char *s){
    if(wa_prefix(s,"GET /stream.pcm?quality=fast ")||wa_prefix(s,"GET /tone.pcm?quality=fast "))return WA_Q_FAST;
    if(wa_prefix(s,"GET /stream.pcm?quality=studio ")||wa_prefix(s,"GET /tone.pcm?quality=studio "))return WA_Q_STUDIO;
    return WA_Q_BALANCED; /* Default for old V1 clients */
}
static inline unsigned wa_chunk_bytes(unsigned quality){
    return quality==WA_Q_FAST?WA_BYTES_FAST:quality==WA_Q_STUDIO?WA_BYTES_STUDIO:WA_BYTES_BALANCED;
}
/* Mono 24 kHz PCM16 with a symmetric four-sample lowpass/decimate step;
 * frame 0 and frame 191 duplicate their nearest neighbor at boundaries. */
static inline short wa_fast_mono_sample(const short pcm[WA_SAMPLES],unsigned j){
    unsigned a=j*2; unsigned before=a?a-1:0; unsigned after=a+2<WA_FRAMES?a+2:WA_FRAMES-1;
    int x0=((int)pcm[before*2]+pcm[before*2+1])/2;
    int x1=((int)pcm[a*2]+pcm[a*2+1])/2;
    int x2=((int)pcm[(a+1)*2]+pcm[(a+1)*2+1])/2;
    int x3=((int)pcm[after*2]+pcm[after*2+1])/2;
    return (short)((x0+3*x1+3*x2+x3)/8);
}
static const short wa_sine32[32]={0,1561,3061,4445,5657,6652,7391,7846,8000,7846,7391,6652,5657,4445,3061,1561,0,-1561,-3061,-4445,-5657,-6652,-7391,-7846,-8000,-7846,-7391,-6652,-5657,-4445,-3061,-1561};
static inline void wa_make_tone(short *out,u32 *phase){
    for(unsigned i=0;i<WA_FRAMES;i++){
        short s=wa_sine32[(*phase>>8)&31u];*phase+=75u;
        out[i*2]=s;out[i*2+1]=s;
    }
}
#endif
