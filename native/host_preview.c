#include "ui.h"
#include <stdlib.h>
int main(int argc,char**argv){
 struct wa_status s={.online=1,.clients=2,.maximum=8,.capture_open=1,.capture_fresh=1};
 snprintf(s.version,sizeof(s.version),"WEB-AUDIO-V2");
 wa_render(&s,"192.168.1.104","Ready. DualShock controller may remain wireless.");
 const char*path=argc>1?argv[1]:"preview.ppm";
 FILE*f=fopen(path,"wb");if(!f)return 1;
 fprintf(f,"P6\n%d %d\n255\n",WA_WIDTH,WA_HEIGHT);
 for(size_t i=0;i<(size_t)WA_WIDTH*WA_HEIGHT;i++){
  uint32_t v=wa_frame[i];unsigned char rgb[3]={v>>16,v>>8,v};
  fwrite(rgb,1,3,f);
 }fclose(f);return 0;
}
