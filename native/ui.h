#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "font.h"

/* All drawing is a self-contained, console-independent 1280x720 framebuffer.
   Layout is influenced by the original Bluetooth Audio home-screen app. */
#define WA_WIDTH 1280
#define WA_HEIGHT 720
static uint32_t wa_frame[WA_WIDTH*WA_HEIGHT];
#define WA_BG 0x0b1323u
#define WA_PANEL 0x15253au
#define WA_PANEL2 0x1b3048u
#define WA_EDGE 0x2a465eu
#define WA_WHITE 0xf3f8ffu
#define WA_MUTED 0xabc4d4u
#define WA_GREEN 0x64e1b1u
#define WA_YELLOW 0xffca78u
#define WA_RED 0xff788du
static inline void wa_pixel(int x,int y,uint32_t rgb){if((unsigned)x<WA_WIDTH&&(unsigned)y<WA_HEIGHT)wa_frame[y*WA_WIDTH+x]=0xff000000u|rgb;}
static void wa_rect(int x,int y,int w,int h,uint32_t rgb){
 int x0=x<0?0:x,x1=x+w>WA_WIDTH?WA_WIDTH:x+w,y0=y<0?0:y,y1=y+h>WA_HEIGHT?WA_HEIGHT:y+h;
 if(x1<=x0||y1<=y0)return;
 for(int j=y0;j<y1;j++){uint32_t*line=&wa_frame[j*WA_WIDTH];for(int i=x0;i<x1;i++)line[i]=0xff000000u|rgb;}
}
static void wa_border(int x,int y,int w,int h,uint32_t c){wa_rect(x,y,w,2,c);wa_rect(x,y+h-2,w,2,c);wa_rect(x,y,2,h,c);wa_rect(x+w-2,y,2,h,c);}
static void wa_text(int x,int y,const char*str,int scale,uint32_t rgb){
 if(scale<1)scale=1;if(scale>3)scale=3;
 for(const unsigned char*p=(const unsigned char*)str;*p;p++){
  if(*p=='\n'){y+=22*scale;x=20;continue;}
  if(*p<32||*p>126){x+=8*scale;continue;}
  const uint16_t *glyph=wa_glyph[*p-32];
  for(int r=0;r<18;r++){
   unsigned bits=glyph[r];if(!bits)continue;
   for(int c=0;c<12;c++)if((bits>>(11-c))&1){
    for(int yy=0;yy<scale;yy++)for(int xx=0;xx<scale;xx++)wa_pixel(x+c*scale+xx,y+r*scale+yy,rgb);
   }
  }
  x+=12*scale;
 }
}
/* PS4 controller shapes are drawn as geometry, NOT font glyphs.
   This works on a standard video framebuffer regardless of font support. */
static void wa_brush(int x,int y,int r,uint32_t color){
 for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r)wa_pixel(x+i,y+j,color);
}
static void wa_line(int x0,int y0,int x1,int y1,int weight,uint32_t color){
 int dx=x1>x0?x1-x0:x0-x1;
 int sx=x0<x1?1:-1;
 int dy=-(y1>y0?y1-y0:y0-y1);
 int sy=y0<y1?1:-1;
 int err=dx+dy;
 for(;;){
  wa_brush(x0,y0,weight/2,color);
  if(x0==x1&&y0==y1)break;
  int e2=err*2;
  if(e2>=dy){err+=dy;x0+=sx;}
  if(e2<=dx){err+=dx;y0+=sy;}
 }
}
static void wa_disk(int cx,int cy,int radius,uint32_t color){
 for(int y=-radius;y<=radius;y++)for(int x=-radius;x<=radius;x++)
  if(x*x+y*y<=radius*radius)wa_pixel(cx+x,cy+y,color);
}
static void wa_ring(int cx,int cy,int radius,int weight,uint32_t color){
 int outer=radius*radius,inner=(radius-weight)*(radius-weight);
 for(int y=-radius;y<=radius;y++)for(int x=-radius;x<=radius;x++){
  int d=x*x+y*y;if(d<=outer&&d>=inner)wa_pixel(cx+x,cy+y,color);
 }
}
enum wa_button_shape{WA_BTN_CROSS,WA_BTN_SQUARE,WA_BTN_TRIANGLE,WA_BTN_CIRCLE};
static void wa_controller_icon(int cx,int cy,enum wa_button_shape shape){
 static const uint32_t colors[]={0x67b5ffu,0xf09bdeu,0x7bf0a8u,0xff858fu};
 uint32_t color=colors[(unsigned)shape<4?(unsigned)shape:0];
 wa_disk(cx,cy,27,0x23405cu);
 switch(shape){
  case WA_BTN_CROSS:
   wa_line(cx-12,cy-12,cx+12,cy+12,5,color);
   wa_line(cx-12,cy+12,cx+12,cy-12,5,color);
   break;
  case WA_BTN_SQUARE:
   wa_rect(cx-15,cy-15,30,4,color);
   wa_rect(cx-15,cy+11,30,4,color);
   wa_rect(cx-15,cy-15,4,30,color);
   wa_rect(cx+11,cy-15,4,30,color);
   break;
  case WA_BTN_TRIANGLE:
   wa_line(cx,cy-17,cx-17,cy+13,5,color);
   wa_line(cx-17,cy+13,cx+17,cy+13,5,color);
   wa_line(cx+17,cy+13,cx,cy-17,5,color);
   break;
  case WA_BTN_CIRCLE:
   wa_ring(cx,cy,16,5,color);
   break;
 }
}
static void wa_control_row(int y,enum wa_button_shape icon,const char*label,const char*details){
 wa_rect(827,y,377,61,WA_PANEL2);
 wa_controller_icon(864,y+31,icon);
 wa_text(911,y+9,label,2,WA_WHITE);
 wa_text(914,y+39,details,1,WA_MUTED);
}
struct wa_status{int online,clients,maximum,capture_open,capture_fresh,underruns,trims,slow_clients;char version[40];};
static void wa_render(const struct wa_status *s,const char*ip,const char*note){
 /* Dark, quiet background keeps the essential information readable on a TV. */
 for(int y=0;y<WA_HEIGHT;y++){
  unsigned a=y*17/WA_HEIGHT;
  uint32_t c=((11+a)<<16)|((19+a)<<8)|(35+a*2);
  wa_rect(0,y,WA_WIDTH,1,c);
 }
 /* Title and server state */
 wa_rect(48,42,7,73,WA_GREEN);
 wa_text(80,47,"PS4 WEB AUDIO",3,WA_WHITE);
 wa_text(82,109,"STREAM PS4 SOUND TO YOUR DEVICES",1,WA_MUTED);
 wa_rect(991,58,237,42,s->online?0x1a553fu:0x563541u);
 wa_disk(1012,79,6,s->online?WA_GREEN:WA_RED);
 wa_text(1030,68,s->online?"SERVER ONLINE":"SERVER OFFLINE",1,s->online?WA_GREEN:WA_RED);

 /* One clear link and only two prominent diagnostic values. */
 wa_rect(52,158,727,448,WA_PANEL);wa_border(52,158,727,448,WA_EDGE);
 wa_text(77,184,"LISTEN ON YOUR DEVICES",2,WA_WHITE);
 wa_text(80,236,"Open this address on your phone or PC",1,WA_MUTED);
 wa_rect(77,269,679,82,WA_PANEL2);
 char url[104];snprintf(url,sizeof(url),"http://%s:8098",ip&&ip[0]?ip:"<PS4 IP>");
 wa_text(97,293,url,2,WA_GREEN);
 wa_rect(77,375,325,100,WA_PANEL2);
 wa_rect(415,375,341,100,WA_PANEL2);
 wa_text(98,393,"CONNECTED DEVICES",1,WA_MUTED);
 char counts[48];snprintf(counts,sizeof(counts),"%d / %d",s->clients,s->maximum>0?s->maximum:8);
 wa_text(99,425,counts,2,WA_WHITE);
 wa_text(435,393,"PS4 SOUND",1,WA_MUTED);
 wa_text(435,425,!s->online?"OFFLINE":(s->capture_fresh?"ACTIVE":(s->capture_open?"WAITING":"IDLE")),2,
         s->capture_fresh?WA_GREEN:WA_YELLOW);
 wa_text(79,513,"On your device, tap LISTEN TO PS4.",1,WA_WHITE);
 wa_text(79,550,"Audio quality: 1 Fast   2 Balanced   3 High",1,WA_MUTED);

 /* PS4 controller keys are VECTOR ICONS, never font-dependent symbols. */
 wa_rect(801,158,427,448,WA_PANEL);wa_border(801,158,427,448,WA_EDGE);
 wa_text(828,184,"CONTROLS",2,WA_WHITE);
 wa_control_row(225,WA_BTN_CROSS,   "START",  "Start or retry the server");
 wa_control_row(297,WA_BTN_SQUARE,  "STOP",   "Stop the audio stream");
 wa_control_row(369,WA_BTN_TRIANGLE,"REFRESH","Update server status");
 wa_control_row(441,WA_BTN_CIRCLE,  "CLOSE",  "Audio keeps playing");
 wa_text(832,530,"Exit the app anytime.",1,WA_MUTED);
 wa_text(832,570,"github.com/vypr",1,WA_GREEN);

 /* Compact status at bottom: don't display outdated "starting" after online. */
 wa_rect(52,624,1176,57,WA_PANEL2);
 const char*message=note&&note[0]?note:"Ready. Open the address above on your device.";
 if(s->online && strstr(message,"Payload sent"))message="Server ready. Open the address above on your device.";
 char bounded[92];snprintf(bounded,sizeof(bounded),"%.91s",message);
 wa_text(72,642,bounded,1,WA_MUTED);
 wa_text(55,699,s->version[0]?s->version:"WEB-AUDIO-V2",1,WA_MUTED);
 wa_text(1025,699,"Made by vypr",1,WA_GREEN);
}
