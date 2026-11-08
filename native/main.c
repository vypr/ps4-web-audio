/* PS4 Web Audio — standalone OpenOrbis home-screen client for FW14 Web Audio V2 (No Background).
 * Copyright (c) 2026. MIT license (see LICENSE).
 * Server is loaded as a bundled standalone payload via GoldHEN BinLoader once;
 * no permanent AutoRun or Bluetooth changes are made.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include "ui.h"

#define WA_PAYLOAD_PATH "/app0/assets/BTA-Web-Audio-FW14-V2-NO-BG.bin"
#define WA_STOP_PATH "/data/ps4_web_audio/stop"
#define WA_LOG_PATH "/data/ps4_web_audio/app.log"
#define WA_PAYLOAD_MAX (1024u*1024u)
static int pad=-1;
static uint64_t pad_retry;
static char notice[144];
static struct wa_status web_audio_status;
static char lan_ip[40]="<PS4 IP>";

static void log_msg(const char *msg){
 int f=sceKernelOpen(WA_LOG_PATH,0x209,0644);
 if(f<0)return;
 sceKernelWrite(f,msg,strlen(msg));sceKernelWrite(f,"\n",1);sceKernelClose(f);
}
static void set_note(const char*s){snprintf(notice,sizeof(notice),"%s",s);log_msg(s);}
static int local_socket(int port){
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
 struct timeval tv={.tv_sec=1,.tv_usec=0};
 setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 int one=1;setsockopt(fd,SOL_SOCKET,0x800,&one,sizeof(one));
 struct sockaddr_in addr;memset(&addr,0,sizeof(addr));
 addr.sin_len=sizeof(addr);addr.sin_family=AF_INET;addr.sin_port=htons((uint16_t)port);
 addr.sin_addr.s_addr=htonl(0x7f000001u);
 if(connect(fd,(struct sockaddr*)&addr,sizeof(addr))<0){close(fd);return -1;}
 return fd;
}
static int send_all(int fd,const void*data,size_t len){
 const uint8_t*p=(const uint8_t*)data;
 while(len){ssize_t n=send(fd,p,len,0x20000);if(n<=0)return -1;p+=n;len-=(size_t)n;}
 return 0;
}
static int http_get_status(void){
 int fd=local_socket(8098);if(fd<0){web_audio_status.online=0;return -1;}
 static const char request[]="GET /status HTTP/1.1\r\nHost: 127.0.0.1:8098\r\nConnection: close\r\n\r\n";
 if(send_all(fd,request,sizeof(request)-1)<0){close(fd);web_audio_status.online=0;return -1;}
 char reply[2048];size_t n=0;
 while(n+1<sizeof(reply)){
  ssize_t k=recv(fd,reply+n,sizeof(reply)-n-1,0);
  if(k<=0)break;
  n+=(size_t)k;
 }
 close(fd);reply[n]=0;
 char*body=strstr(reply,"\r\n\r\n");
 if(!body||strncmp(reply,"HTTP/1.1 200",12)){web_audio_status.online=0;return -1;}
 body+=4;web_audio_status.online=1;
 /* Response is a tiny fixed-schema JSON object emitted by Web Audio V2 (No Background). */
 #define INT_FIELD(field,key) do {char*p=strstr(body,"\""key"\":"); if(p) web_audio_status.field=atoi(p+sizeof(key)+2);}while(0)
 INT_FIELD(clients,"clients");INT_FIELD(maximum,"max_listeners");
 INT_FIELD(capture_open,"capture_open");INT_FIELD(capture_fresh,"capture_fresh");
 INT_FIELD(underruns,"underruns");INT_FIELD(trims,"trims");INT_FIELD(slow_clients,"slow_clients");
 #undef INT_FIELD
 char*v=strstr(body,"\"version\":\"");
 if(v){v+=11;size_t j=0;while(v[j]&&v[j]!='\"'&&j+1<sizeof(web_audio_status.version)){web_audio_status.version[j]=v[j];j++;}web_audio_status.version[j]=0;}
 return 0;
}
static int try_lan_ip(void){
 int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;
 struct sockaddr_in dst;memset(&dst,0,sizeof(dst));dst.sin_len=sizeof(dst);
 dst.sin_family=AF_INET;dst.sin_port=htons(53);dst.sin_addr.s_addr=htonl(0xc0a80101u);
 int rc=connect(fd,(struct sockaddr*)&dst,sizeof(dst));
 if(rc==0){struct sockaddr_in src;socklen_t len=sizeof(src);memset(&src,0,sizeof(src));
  if(!getsockname(fd,(struct sockaddr*)&src,&len)){
   uint32_t ip=ntohl(src.sin_addr.s_addr);
   if(ip!=0&&ip!=0x7f000001u){snprintf(lan_ip,sizeof(lan_ip),"%u.%u.%u.%u",(ip>>24)&255,(ip>>16)&255,(ip>>8)&255,ip&255);close(fd);return 0;}
  }
 }
 close(fd);return -1;
}
static int start_payload(void){
 if(web_audio_status.online){set_note("Server is already running. Open the URL on your device.");return 0;}
 static uint8_t payload[WA_PAYLOAD_MAX];
 int f=sceKernelOpen(WA_PAYLOAD_PATH,0,0);
 if(f<0){set_note("Bundled No-BG payload not found. Package installation incomplete.");return -1;}
 size_t used=0;
 while(used<sizeof(payload)){
  long n=sceKernelRead(f,payload+used,sizeof(payload)-used);
  if(n<=0)break;
  used+=(size_t)n;
 }
 sceKernelClose(f);
 if(used<1024||used>=sizeof(payload)){set_note("Bundled payload has unexpected size.");return -1;}
 /* Never probe BinLoader 9090: opening its socket consumes the loader. */
 int fd=local_socket(9090);
 if(fd<0){set_note("Enable GoldHEN Payload Server (port 9090), then retry X.");return -1;}
 int rc=send_all(fd,payload,used);
 close(fd);
 if(rc){set_note("Unable to send the payload to GoldHEN.");return -1;}
 set_note("Payload sent. The web audio server is starting...");return 0;
}
static void stop_server(void){
 if(!web_audio_status.online){set_note("Server is already offline.");return;}
 sceKernelMkdir("/data/ps4_web_audio",0777);
 int fd=sceKernelOpen(WA_STOP_PATH,0x601,0666);
 if(fd<0){set_note("Unable to write the stop request.");return;}
 sceKernelWrite(fd,"stop\n",5);sceKernelClose(fd);
 set_note("Stop requested. The streaming worker will exit shortly.");
}
static unsigned get_pad(uint64_t now){
 OrbisPadData data;memset(&data,0,sizeof(data));
 if(pad>=0&&scePadReadState(pad,&data)>=0&&data.connected)return data.buttons;
 if(now<pad_retry)return 0;
 pad_retry=now+2000000;
 if(pad>=0){scePadClose(pad);pad=-1;}
 OrbisUserServiceLoginUserIdList users;
 for(int i=0;i<4;i++)users.userId[i]=-1;
 if(sceUserServiceGetLoginUserIdList(&users)<0)sceUserServiceGetInitialUser(&users.userId[0]);
 for(int i=0;i<4;i++)if(users.userId[i]>=0){
  int h=scePadGetHandle(users.userId[i],ORBIS_PAD_PORT_TYPE_STANDARD,0);
  if(h<0)h=scePadOpen(users.userId[i],ORBIS_PAD_PORT_TYPE_STANDARD,0,0);
  if(h<0)continue;
  memset(&data,0,sizeof(data));
  if(scePadReadState(h,&data)>=0&&data.connected){pad=h;return data.buttons;}
  scePadClose(h);
 }
 return 0;
}
int main(void){
 sceKernelMkdir("/data/ps4_web_audio",0777);
 log_msg("PS4 Web Audio native shell started");
 int video=sceVideoOutOpen(0xff,0,0,0);
 if(video<0){log_msg("sceVideoOutOpen failed");return 1;}
 off_t memory=0;size_t size=8*1024*1024;void*base=0;
 if(sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),size,0x200000,3,&memory)<0||
    sceKernelMapDirectMemory(&base,size,0x33,0,memory,0x200000)<0){log_msg("Unable to allocate video memory");return 2;}
 void*buffers[2]={base,(uint8_t*)base+WA_WIDTH*WA_HEIGHT*4};
 OrbisVideoOutBufferAttribute attr;
 sceVideoOutSetBufferAttribute(&attr,0x80000000,1,0,WA_WIDTH,WA_HEIGHT,WA_WIDTH);
 if(sceVideoOutRegisterBuffers(video,0,buffers,2,&attr)<0){log_msg("Unable to register video buffers");return 3;}
 sceVideoOutSetFlipRate(video,1);
 OrbisUserServiceInitializeParams p={ORBIS_KERNEL_PRIO_FIFO_LOWEST};
 sceUserServiceInitialize(&p);scePadInit();sceSystemServiceHideSplashScreen();
 try_lan_ip();http_get_status();
 if(web_audio_status.online)set_note("Existing Web Audio service detected. Ready.");
 else {set_note("Starting bundled Web Audio V2 No-BG service...");start_payload();}
 uint64_t poll=0;unsigned previous=0;int armed=0,which=0;long flip=0;
 for(;;){
  uint64_t now=sceKernelGetProcessTime();
  if(now>=poll){http_get_status();poll=now+1000000;}
  unsigned buttons=get_pad(now);if(!armed){previous=buttons;armed=1;}
  unsigned pressed=buttons&~previous;previous=buttons;
  if(pressed&ORBIS_PAD_BUTTON_CIRCLE)break;
  if(pressed&ORBIS_PAD_BUTTON_CROSS){if(!web_audio_status.online)start_payload();else set_note("Already online. Connect via browser URL shown.");}
  if(pressed&ORBIS_PAD_BUTTON_SQUARE)stop_server();
  if(pressed&ORBIS_PAD_BUTTON_TRIANGLE){http_get_status();try_lan_ip();set_note(web_audio_status.online?"Refreshed server status.":"Offline. Press X to start Web Audio.");}
  wa_render(&web_audio_status,lan_ip,notice);
  memcpy(buffers[which],wa_frame,sizeof(wa_frame));
  if(sceVideoOutSubmitFlip(video,which,1,flip)<0){sceKernelUsleep(33000);continue;}
  for(int spins=0;spins<1000;spins++){
   OrbisVideoOutFlipStatus st;
   if(!sceVideoOutGetFlipStatus(video,&st)&&st.flipArg==flip)break;
   sceKernelUsleep(1000);
  }
  which^=1;flip++;
 }
 log_msg("Native shell closed; Web Audio stream left running");
 if(pad>=0)scePadClose(pad);
 sceVideoOutClose(video);
 return 0;
}
