#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "web_audio.h"
int main(void){
    assert(wa_valid_cursor(16));assert(wa_valid_cursor(65528));assert(!wa_valid_cursor(0));
    assert(!wa_valid_cursor(17));assert(!wa_valid_cursor(65536));
    assert(wa_advance(65520,16)==16);
    assert(wa_advance(16,1536)==1552);
    assert(wa_available(32,65520)==32);
    assert(wa_clamp_float(2.0f)==32767);
    assert(wa_clamp_float(-2.0f)==-32767);
    assert(wa_clamp_float(NAN)==0);
    assert(wa_clamp_float(0.5f)>16300);
    unsigned char ring[65536]={0};
    short output[WA_SAMPLES];
    for(unsigned i=0;i<WA_SAMPLES;i++){
        unsigned pos=WA_RING_BEGIN+(i*4)%WA_RING_BYTES;
        float f=(i&1)?-0.25f:0.25f;
        memcpy(ring+pos,&f,4);
    }
    assert(wa_read_pcm(ring,WA_RING_BEGIN,output)==WA_RING_BEGIN+WA_RAW_BYTES);
    for(unsigned i=0;i<WA_SAMPLES;i++)assert(output[i]==((i&1)?-8191:8191));
    unsigned char wrapping[65536]={0};
    unsigned pos=65520;
    for(unsigned i=0;i<WA_SAMPLES;i++){
        float f=(i&1)?-0.5f:0.5f;
        memcpy(wrapping+pos,&f,4);pos=wa_advance(pos,4);
    }
    assert(wa_read_pcm(wrapping,65520,output)==pos);
    assert(output[0]==16383 && output[1]==-16383);
    u32 phase=0;wa_make_tone(output,&phase);
    assert(phase==192u*75u && output[0]==output[1]);
    /* V2: independent stream profiles and 4 ms frame sizes */
    assert(WA_BYTES_FAST==192 && WA_BYTES_BALANCED==768 && WA_BYTES_STUDIO==1536);
    assert(wa_chunk_bytes(WA_Q_FAST)==192);
    assert(wa_chunk_bytes(WA_Q_BALANCED)==768);
    assert(wa_chunk_bytes(WA_Q_STUDIO)==1536);
    assert(wa_quality_from_path("GET /stream.pcm?quality=fast HTTP/1.1")==WA_Q_FAST);
    assert(wa_quality_from_path("GET /tone.pcm?quality=studio HTTP/1.1")==WA_Q_STUDIO);
    assert(wa_quality_from_path("GET /stream.pcm HTTP/1.1")==WA_Q_BALANCED);
    assert(wa_quality_from_path("GET /stream.pcm?quality=balanced HTTP/1.1")==WA_Q_BALANCED);
    assert(wa_fast_mono_sample(output,0)<=32767);
    float floats[WA_SAMPLES];
    assert(wa_read_capture(ring,WA_RING_BEGIN,output,floats)==WA_RING_BEGIN+WA_RAW_BYTES);
    assert(floats[0]==0.25f && floats[1]==-0.25f);
    assert(wa_float_limited(NAN)==0.f);
    puts("Web Audio V2 PCM, profiles, capture-ring and tone tests: PASS");
    return 0;
}
