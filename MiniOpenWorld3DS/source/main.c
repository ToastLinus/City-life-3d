#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include "vshader_shbin.h"
#define XFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
typedef struct { float x,y,z,r,g,b,a; } Vtx;
static Vtx v[600]; static int n=0; static void* vbo;
static DVLB_s* dvlb; static shaderProgram_s prog; static int loc; static C3D_Mtx proj; static C3D_RenderTarget* target;
static Vtx P(float x,float y,float z,float r,float g,float b){return (Vtx){x,y,z,r,g,b,1};}
static void T(Vtx a,Vtx b,Vtx c){if(n+3<=600){v[n++]=a;v[n++]=b;v[n++]=c;}}
static void Q(float x,float y,float w,float h,float z,float r,float g,float b){Vtx a=P(x,y,z,r,g,b),bb=P(x+w,y,z,r,g,b),c=P(x+w,y+h,z,r,g,b),d=P(x,y+h,z,r,g,b);T(a,bb,c);T(a,c,d);}
static void house(float x,float y,float w,float h,float r,float g,float b){
 Vtx a=P(x+w*.5f,y-h,.45f,r*1.12f,g*1.12f,b*1.12f),bb=P(x+w*.82f,y-h*.72f,.45f,r,g,b),c=P(x+w*.5f,y-h*.44f,.45f,r*.86f,g*.86f,b*.86f),d=P(x+w*.18f,y-h*.72f,.45f,r*1.05f,g*1.05f,b*1.05f);
 Vtx e=P(x+w*.5f,y,.55f,r*.65f,g*.65f,b*.65f),f=P(x+w*.82f,y+h*.28f,.55f,r*.58f,g*.58f,b*.58f),g1=P(x+w*.5f,y+h*.56f,.55f,r*.52f,g*.52f,b*.52f),hh=P(x+w*.18f,y+h*.28f,.55f,r*.62f,g*.62f,b*.62f);
 T(a,bb,c);T(a,c,d);T(d,c,g1);T(d,g1,hh);T(bb,f,g1);T(bb,g1,c);T(e,f,g1);T(e,g1,hh);
}
static float playerX=200.0f, playerY=184.0f;
static void player(void){
 /* Simple, colorful placeholder character drawn above the street. */
 Q(playerX-5.0f,playerY-3.0f,10.0f,13.0f,.18f,.12f,.42f,.92f);
 T(P(playerX-5.0f,playerY-3.0f,.17f,.12f,.42f,.92f),P(playerX+5.0f,playerY-3.0f,.17f,.12f,.42f,.92f),P(playerX,playerY-12.0f,.17f,1.0f,.78f,.48f));
 Q(playerX-3.0f,playerY-18.0f,6.0f,6.0f,.16f,.96f,.72f,.48f);
}
static void city(void){
 n=0; Q(0,0,400,240,.9f,.27f,.65f,.32f); Q(0,104,400,50,.8f,.23f,.25f,.29f); Q(172,0,52,240,.79f,.23f,.25f,.29f);
 for(int x=8;x<400;x+=48)Q(x,127,22,3,.7f,.95f,.84f,.48f);
 house(28,92,60,55,.82f,.48f,.27f); house(105,92,62,69,.53f,.68f,.84f); house(244,92,62,58,.8f,.57f,.35f); house(324,92,54,75,.71f,.49f,.74f);
 house(32,220,60,55,.78f,.68f,.5f); house(108,220,56,46,.5f,.73f,.56f); house(260,220,58,55,.82f,.55f,.42f); house(333,220,48,46,.55f,.66f,.82f);
 for(int i=0;i<4;i++){float x=18+i*92;Q(x+8,91,5,22,.48f,.4f,.25f,.12f);T(P(x,92,.4f,.12f,.5f,.2f),P(x+23,92,.4f,.12f,.5f,.2f),P(x+12,66,.4f,.18f,.68f,.25f));}
 Q(185,168,30,14,.35f,.12f,.5f,.9f); Q(192,160,17,9,.34f,.48f,.8f,1);
}
int main(void){
 gfxInitDefault(); consoleInit(GFX_BOTTOM,NULL); if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)){gfxExit();return 1;}
 target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8); C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,XFER);
 dvlb=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size); shaderProgramInit(&prog); shaderProgramSetVsh(&prog,&dvlb->DVLE[0]); C3D_BindProgram(&prog); loc=shaderInstanceGetUniformLocation(prog.vertexShader,"projection");
 C3D_AttrInfo* ai=C3D_GetAttrInfo();AttrInfo_Init(ai);AttrInfo_AddLoader(ai,0,GPU_FLOAT,3);AttrInfo_AddLoader(ai,1,GPU_FLOAT,4);
 C3D_TexEnv* env=C3D_GetTexEnv(0);C3D_TexEnvInit(env);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);vbo=linearAlloc(sizeof(v));city();memcpy(vbo,v,n*sizeof(Vtx));C3D_BufInfo* bi=C3D_GetBufInfo();BufInfo_Init(bi);BufInfo_Add(bi,vbo,sizeof(Vtx),2,0x10);Mtx_OrthoTilt(&proj,0,400,0,240,0,1,true);
 printf("\x1b[1;1HCITY LIFE 3D");printf("\x1b[3;1HFirst 3D city scene");printf("\x1b[5;1HColorful low-poly prototype");printf("\x1b[7;1HD-Pad: Move");printf("\x1b[8;1HSTART: Exit");
 while(aptMainLoop()){
  hidScanInput();u32 keys=hidKeysHeld();if(hidKeysDown()&KEY_START)break;
  if(keys&KEY_LEFT)playerX-=1.6f;if(keys&KEY_RIGHT)playerX+=1.6f;
  if(keys&KEY_UP)playerY-=1.6f;if(keys&KEY_DOWN)playerY+=1.6f;
  if(playerX<8.0f)playerX=8.0f;if(playerX>392.0f)playerX=392.0f;
  if(playerY<18.0f)playerY=18.0f;if(playerY>232.0f)playerY=232.0f;
  city();player();memcpy(vbo,v,n*sizeof(Vtx));
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x78BDEBFF,0);C3D_FrameDrawOn(target);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,loc,&proj);C3D_DrawArrays(GPU_TRIANGLES,0,n);C3D_FrameEnd(0);
 }
 linearFree(vbo);shaderProgramFree(&prog);DVLB_Free(dvlb);C3D_Fini();gfxExit();return 0;
}