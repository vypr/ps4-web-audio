#include "runtime.h"
extern const char __image_start[], __image_end[];
void*memset(void*p,int v,u64 n){u8*d=p;while(n--)*d++=(u8)v;return p;}
void*memcpy(void*d,const void*s,u64 n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;return d;}
void notify(const char*message){
 int(*send)(int,void*,u64,int)=0;
 if(resolve("sceKernelSendNotificationRequest",&send)||!send)return;
 u8 req[0xc30];memset(req,0,sizeof(req));*(int*)(req+0x10)=-1;
 u64 n=length(message);if(n>1023)n=1023;memcpy(req+0x2d,message,n);send(0,req,sizeof(req),0);
}
/* Independent web audio payload, intentionally NOT using port 8097 or /dev/bt. */
#define WA_PORT 8098
static long wa_listen(void){
 long fd=sc3(97,2,1,0);if(fd<0)return -1;
 int yes=1;sc5(105,fd,0xffff,4,(long)&yes,4);
 struct sockaddr4 sa={.len=16,.family=2,.port=(WA_PORT>>8)|((WA_PORT&255)<<8)};
 if(sc3(104,fd,(long)&sa,16)<0 || sc3(106,fd,8,0)<0 || nonblock(fd)<0){closefd(fd);return -1;}
 return fd;
}
__attribute__((section(".text.launcher"))) int _start(void){
 long listen=wa_listen();
 if(listen<0){notify("PS4 Web Audio: port 8098 is occupied. Stop the previous Web Audio payload first.");return 3;}
 void*(*map)(void*,u64,int,int,int,u64)=0;
 int(*unmap)(void*,u64)=0;
 int(*create)(void**,void*,void*(*)(void*),void*,const char*)=0;
 int(*detach)(void*)=0;
 if(resolve("mmap",&map)||resolve("munmap",&unmap)||resolve("scePthreadCreate",&create)||resolve("scePthreadDetach",&detach)||!map||!unmap||!create||!detach){closefd(listen);return 4;}
 const u8 *source,*end,*entry;
 __asm__ volatile("lea __image_start(%%rip),%0; lea __image_end(%%rip),%1; lea wa_worker(%%rip),%2":"=r"(source),"=r"(end),"=r"(entry));
 u64 bytes=end-source;
 if(bytes<1024||bytes>1024*1024){closefd(listen);return 6;}
 u64 alloc=(bytes+0x3fff)&~0x3fffUL;
 u8*copy=map(0,alloc,7,0x1002,-1,0);
 if(!copy||copy==(void*)-1){closefd(listen);return 5;}
 memcpy(copy,source,bytes);
 void *thread=0;
 void*(*fn)(void*)=(void*)(copy+(entry-source));
 int rc=create(&thread,0,fn,(void*)listen,"PS4 Web Audio");
 if(rc){unmap(copy,alloc);closefd(listen);return rc;}
 detach(thread);
 return 0;
}
