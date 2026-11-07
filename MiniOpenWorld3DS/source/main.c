#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "vshader_shbin.h"
#define XFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
typedef struct { float x,y,z,r,g,b,a; } Vtx;
typedef struct { float x,y,z; } Pos;
typedef struct { float r,g,b,a; } Clr;
static Vtx v[600]; static Pos positions[600]; static Clr colors[600]; static int n=0; static void *posVbo,*colorVbo;
static DVLB_s* dvlb; static shaderProgram_s prog; static int loc; static C3D_Mtx proj; static C3D_RenderTarget* target;
#define AUDIO_RATE 22050
#define AUDIO_SAMPLES (AUDIO_RATE/30)
static u32* audioData=NULL; static ndspWaveBuf audioBuf[2]; static bool audioReady=false; static size_t audioOffset=0; static int audioBlock=0;
static Vtx P(float x,float y,float z,float r,float g,float b){return (Vtx){x,240.0f-y,z,r,g,b,1};}
static void T(Vtx a,Vtx b,Vtx c){if(n+3<=600){v[n++]=a;v[n++]=b;v[n++]=c;}}
static void uploadVertices(void){for(int i=0;i<n;i++){positions[i]=(Pos){v[i].x,v[i].y,v[i].z};colors[i]=(Clr){v[i].r,v[i].g,v[i].b,v[i].a};}memcpy(posVbo,positions,n*sizeof(Pos));memcpy(colorVbo,colors,n*sizeof(Clr));GSPGPU_FlushDataCache(posVbo,n*sizeof(Pos));GSPGPU_FlushDataCache(colorVbo,n*sizeof(Clr));}
static void Q(float x,float y,float w,float h,float z,float r,float g,float b){Vtx a=P(x,y,z,r,g,b),bb=P(x+w,y,z,r,g,b),c=P(x+w,y+h,z,r,g,b),d=P(x,y+h,z,r,g,b);T(a,bb,c);T(a,c,d);}
static void house(float x,float y,float w,float h,float r,float g,float b){
 Vtx a=P(x+w*.5f,y-h,.45f,r*1.12f,g*1.12f,b*1.12f),bb=P(x+w*.82f,y-h*.72f,.45f,r,g,b),c=P(x+w*.5f,y-h*.44f,.45f,r*.86f,g*.86f,b*.86f),d=P(x+w*.18f,y-h*.72f,.45f,r*1.05f,g*1.05f,b*1.05f);
 Vtx e=P(x+w*.5f,y,.55f,r*.65f,g*.65f,b*.65f),f=P(x+w*.82f,y+h*.28f,.55f,r*.58f,g*.58f,b*.58f),g1=P(x+w*.5f,y+h*.56f,.55f,r*.52f,g*.52f,b*.52f),hh=P(x+w*.18f,y+h*.28f,.55f,r*.62f,g*.62f,b*.62f);
 T(a,bb,c);T(a,c,d);T(d,c,g1);T(d,g1,hh);T(bb,f,g1);T(bb,g1,c);T(e,f,g1);T(e,g1,hh);
}
static float playerX=200.0f, playerY=184.0f;
static float scooterX=244.0f, scooterY=184.0f;
static float carX=145.0f, carY=184.0f;
static float npcX=78.0f;
static float npc2X=350.0f;
/* riding: 0 = on foot, 1 = scooter, 2 = car */
static int riding=0, npcDirection=1, npc2Direction=-1;
static void person(float x,float y,float shirtR,float shirtG,float shirtB){
 Q(x-4.0f,y-3.0f,8.0f,12.0f,.18f,shirtR,shirtG,shirtB);
 T(P(x-4.0f,y-3.0f,.17f,shirtR,shirtG,shirtB),P(x+4.0f,y-3.0f,.17f,shirtR,shirtG,shirtB),P(x,y-11.0f,.17f,shirtR,shirtG,shirtB));
 Q(x-3.0f,y-18.0f,6.0f,6.0f,.16f,.96f,.72f,.48f);
}
static void scooter(float x,float y){
 /* Small original low-poly scooter; wheels are simple dark blocks. */
 Q(x-12.0f,y+1.0f,6.0f,5.0f,.22f,.12f,.13f,.16f);
 Q(x+7.0f,y+1.0f,6.0f,5.0f,.22f,.12f,.13f,.16f);
 Q(x-8.0f,y-4.0f,17.0f,4.0f,.20f,.15f,.72f,.88f);
 Q(x+5.0f,y-12.0f,3.0f,9.0f,.19f,.15f,.72f,.88f);
 Q(x+4.0f,y-13.0f,7.0f,2.0f,.18f,.18f,.18f,.2f);
 Q(x-2.0f,y-8.0f,6.0f,3.0f,.17f,.96f,.78f,.32f);
}
static void car(float x,float y){
 /* Original compact city car, built from simple low-poly blocks. */
 Q(x-15.0f,y+2.0f,8.0f,6.0f,.24f,.10f,.11f,.14f);
 Q(x+8.0f,y+2.0f,8.0f,6.0f,.24f,.10f,.11f,.14f);
 Q(x-17.0f,y-5.0f,34.0f,9.0f,.21f,.88f,.28f,.20f);
 Q(x-9.0f,y-12.0f,19.0f,8.0f,.20f,.95f,.43f,.25f);
 Q(x-6.0f,y-11.0f,7.0f,6.0f,.19f,.42f,.78f,.90f);
 Q(x+2.0f,y-11.0f,6.0f,6.0f,.19f,.42f,.78f,.90f);
 Q(x-16.0f,y-3.0f,4.0f,3.0f,.18f,1.0f,.88f,.42f);
 Q(x+12.0f,y-3.0f,4.0f,3.0f,.18f,1.0f,.88f,.42f);
}
static void player(void){ person(playerX,playerY,.12f,.42f,.92f); }

static void fillAudio(void* dst,size_t offset,size_t count){
 u32* out=(u32*)dst; static const int notes[]={330,392,440,392,294,349,392,349};
 for(size_t i=0;i<count;i++){float t=(float)(offset+i)/(float)AUDIO_RATE;int note=notes[((offset+i)/(AUDIO_SAMPLES*2))%8];s16 sample=(s16)(3000.0f*sinf(t*6.2831853f*(float)note));out[i]=((u32)(u16)sample<<16)|(u16)sample;}
 DSP_FlushDataCache(dst,count*sizeof(u32));
}
static void startAudio(void){
 if(R_FAILED(ndspInit()))return;
 audioReady=true;ndspSetOutputMode(NDSP_OUTPUT_STEREO);ndspSetMasterVol(0.20f);
 ndspChnSetInterp(0,NDSP_INTERP_LINEAR);ndspChnSetRate(0,AUDIO_RATE);ndspChnSetFormat(0,NDSP_FORMAT_STEREO_PCM16);
 float mix[12]={0};mix[0]=1.0f;mix[1]=1.0f;ndspChnSetMix(0,mix);
 audioData=(u32*)linearAlloc(AUDIO_SAMPLES*4*2);memset(audioBuf,0,sizeof(audioBuf));
 for(int i=0;i<2;i++){audioBuf[i].data_vaddr=&audioData[i*AUDIO_SAMPLES];audioBuf[i].nsamples=AUDIO_SAMPLES;fillAudio(audioBuf[i].data_vaddr,audioOffset,AUDIO_SAMPLES);audioOffset+=AUDIO_SAMPLES;ndspChnWaveBufAdd(0,&audioBuf[i]);}
}
static void updateAudio(void){
 if(!audioReady)return;
 if(audioBuf[audioBlock].status==NDSP_WBUF_DONE){fillAudio(audioBuf[audioBlock].data_vaddr,audioOffset,AUDIO_SAMPLES);audioOffset+=AUDIO_SAMPLES;ndspChnWaveBufAdd(0,&audioBuf[audioBlock]);audioBlock^=1;}
}

static void city(void){
 n=0;
 /* GPU diagnostic: three large triangles with independent primary colors. */
 T(P(35,205,.5f,1.0f,0.0f,0.0f),P(125,45,.5f,1.0f,0.0f,0.0f),P(215,205,.5f,1.0f,0.0f,0.0f));
 T(P(145,205,.5f,0.0f,1.0f,0.0f),P(235,45,.5f,0.0f,1.0f,0.0f),P(325,205,.5f,0.0f,1.0f,0.0f));
 T(P(255,205,.5f,1.0f,1.0f,0.0f),P(345,45,.5f,1.0f,1.0f,0.0f),P(395,205,.5f,1.0f,1.0f,0.0f));
}
int main(void){
 gfxInitDefault(); consoleInit(GFX_BOTTOM,NULL); startAudio(); if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)){if(audioReady)ndspExit();gfxExit();return 1;}
 target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8); C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,XFER);
 dvlb=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size); shaderProgramInit(&prog); shaderProgramSetVsh(&prog,&dvlb->DVLE[0]); C3D_BindProgram(&prog); loc=shaderInstanceGetUniformLocation(prog.vertexShader,"projection");
 C3D_AttrInfo* ai=C3D_GetAttrInfo();AttrInfo_Init(ai);AttrInfo_AddLoader(ai,0,GPU_FLOAT,3);AttrInfo_AddLoader(ai,1,GPU_FLOAT,4);
 C3D_TexEnv* env=C3D_GetTexEnv(0);C3D_TexEnvInit(env);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);posVbo=linearAlloc(sizeof(positions));colorVbo=linearAlloc(sizeof(colors));city();uploadVertices();C3D_BufInfo* bi=C3D_GetBufInfo();BufInfo_Init(bi);BufInfo_Add(bi,posVbo,sizeof(Pos),1,0x0);BufInfo_Add(bi,colorVbo,sizeof(Clr),1,0x1);Mtx_OrthoTilt(&proj,0,400,0,240,0,1,true);
 printf("\x1b[1;1H+--------------------------------+");printf("\x1b[2;1H|          CITY LIFE 3D          |");printf("\x1b[3;1H+--------------------------------+");printf("\x1b[5;1HGPU COLOR DIAGNOSTIC");printf("\x1b[7;1HRED / GREEN / YELLOW");printf("\x1b[9;1HSTART  Exit game");printf("\x1b[13;1H-------------------------------");
 while(aptMainLoop()){
  hidScanInput();updateAudio();u32 keys=hidKeysHeld();u32 down=hidKeysDown();if(down&KEY_START)break;
  if(down&KEY_A){if(riding==1){riding=0;scooterX=playerX;scooterY=playerY;}else if(riding==2){riding=0;carX=playerX;carY=playerY;}else{float ds=(playerX-scooterX)*(playerX-scooterX)+(playerY-scooterY)*(playerY-scooterY);float dc=(playerX-carX)*(playerX-carX)+(playerY-carY)*(playerY-carY);if(ds<dc&&ds<625.0f)riding=1;else if(dc<625.0f)riding=2;}}
  float speed=riding==2?3.2f:(riding==1?2.5f:1.6f);
  if(keys&KEY_LEFT)playerX-=speed;if(keys&KEY_RIGHT)playerX+=speed;
  if(keys&KEY_UP)playerY-=speed;if(keys&KEY_DOWN)playerY+=speed;
  if(playerX<8.0f)playerX=8.0f;if(playerX>392.0f)playerX=392.0f;
  if(playerY<18.0f)playerY=18.0f;if(playerY>232.0f)playerY=232.0f;
  if(!riding){npcX+=0.22f*npcDirection;if(npcX>142.0f)npcDirection=-1;if(npcX<76.0f)npcDirection=1;
   npc2X+=0.16f*npc2Direction;if(npc2X>366.0f)npc2Direction=-1;if(npc2X<310.0f)npc2Direction=1;}
  if(riding==1){scooterX=playerX;scooterY=playerY;}else if(riding==2){carX=playerX;carY=playerY;}
  city();person(npcX,178.0f,.88f,.48f,.22f);person(npc2X,205.0f,.68f,.30f,.76f);
  scooter(scooterX,scooterY);car(carX,carY);
  if(!riding)player();
  uploadVertices();
  printf("\x1b[14;1HGPU COLOR TEST");
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x202020FF,0);C3D_FrameDrawOn(target);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,loc,&proj);C3D_DrawArrays(GPU_TRIANGLES,0,n);C3D_FrameEnd(0);
 }
 linearFree(posVbo);linearFree(colorVbo);shaderProgramFree(&prog);DVLB_Free(dvlb);C3D_Fini();if(audioReady){ndspExit();linearFree(audioData);}gfxExit();return 0;
}