#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "vshader_shbin.h"

#define XFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define MAX_VERTICES 600

typedef struct { float x,y,z,r,g,b,a; } Vtx;
typedef struct { float x,y,z; } Pos;
typedef struct { float r,g,b,a; } Clr;

static Vtx vertices[MAX_VERTICES];
static Pos positions[MAX_VERTICES];
static Clr colors[MAX_VERTICES];
static int vertexCount=0;
static void *posVbo,*colorVbo;
static DVLB_s *dvlb;
static shaderProgram_s prog;
static int projectionLoc;
static C3D_Mtx projection, view, viewProjection;
static C3D_RenderTarget *target;

static float playerX=0.0f, playerZ=0.0f;
static float npcX=-3.0f, npcZ=-1.0f;
static float npc2X=3.0f, npc2Z=-3.0f;
static float carX=2.5f, carZ=2.0f;
static float scooterX=-2.5f, scooterZ=1.5f;
static int riding=0;

static Vtx V(float x,float y,float z,float r,float g,float b) {
    return (Vtx){x,y,z,r,g,b,1.0f};
}
static void tri(Vtx a,Vtx b,Vtx c) {
    if(vertexCount+3<=MAX_VERTICES) {
        vertices[vertexCount++]=a;
        vertices[vertexCount++]=b;
        vertices[vertexCount++]=c;
    }
}
static void quad(Vtx a,Vtx b,Vtx c,Vtx d) {
    tri(a,b,c); tri(a,c,d);
}
static void box(float x,float y,float z,float w,float h,float d,float r,float g,float b) {
    float x0=x-w*.5f, x1=x+w*.5f;
    float y0=y, y1=y+h;
    float z0=z-d*.5f, z1=z+d*.5f;
    Vtx a=V(x0,y0,z0,r,g,b), bb=V(x1,y0,z0,r,g,b);
    Vtx c=V(x1,y1,z0,r,g,b), dd=V(x0,y1,z0,r,g,b);
    Vtx e=V(x0,y0,z1,r*.82f,g*.82f,b*.82f), f=V(x1,y0,z1,r*.82f,g*.82f,b*.82f);
    Vtx gg=V(x1,y1,z1,r*.82f,g*.82f,b*.82f), hh=V(x0,y1,z1,r*.82f,g*.82f,b*.82f);
    quad(a,bb,c,dd); quad(f,e,hh,gg);
    quad(e,a,dd,hh); quad(bb,f,gg,c);
    quad(dd,c,gg,hh); quad(e,f,bb,a);
}
static void house(float x,float z,float w,float h,float d,float r,float g,float b) {
    box(x,0.0f,z,w,h,d,r,g,b);
    box(x,h,z,w*1.08f,.22f,d*1.08f,.34f,.20f,.16f);
    box(x-w*.22f,.0f,z-d*.505f,w*.22f,h*.38f,.035f,.24f,.52f,.68f);
    box(x+w*.22f,.0f,z-d*.505f,w*.22f,h*.38f,.035f,.24f,.52f,.68f);
    box(x,.0f,z-d*.53f,w*.16f,h*.55f,.04f,.32f,.19f,.12f);
}
static void person(float x,float z,float r,float g,float b) {
    box(x,.0f,z,.48f,1.0f,.38f,r,g,b);
    box(x,1.05f,z,.34f,.34f,.34f,.92f,.69f,.48f);
}
static void vehicle(float x,float z,float r,float g,float b) {
    box(x,.12f,z,1.45f,.48f,.82f,r,g,b);
    box(x,.60f,z-.04f,.78f,.42f,.68f,r*.8f,g*.8f,b*.8f);
    box(x-.48f,.0f,z-.34f,.28f,.28f,.12f,.08f,.08f,.09f);
    box(x+.48f,.0f,z-.34f,.28f,.28f,.12f,.08f,.08f,.09f);
    box(x-.48f,.0f,z+.34f,.28f,.28f,.12f,.08f,.08f,.09f);
    box(x+.48f,.0f,z+.34f,.28f,.28f,.12f,.08f,.08f,.09f);
}
static void buildCity(void) {
    vertexCount=0;
    box(0.0f,-.18f,0.0f,30.0f,.25f,30.0f,.26f,.58f,.29f);
    box(0.0f,-.025f,0.0f,5.0f,.035f,30.0f,.20f,.21f,.24f);
    for(int z=-12;z<=12;z+=4) box(0.0f,.0f,(float)z, .12f,.012f,1.5f,.94f,.82f,.48f);
    house(-5.0f,-5.0f,3.0f,2.8f,3.0f,.82f,.48f,.28f);
    house(-5.0f,1.0f,3.4f,3.6f,3.0f,.50f,.67f,.83f);
    house(5.0f,-6.0f,3.2f,3.0f,3.0f,.78f,.56f,.34f);
    house(5.0f,1.0f,3.0f,3.8f,3.0f,.68f,.48f,.72f);
    for(int i=0;i<4;i++) {
        float x=-8.0f+(float)i*5.3f;
        box(x,0.0f,5.5f,.22f,1.4f,.22f,.38f,.23f,.12f);
        box(x,.95f,5.5f,1.5f,.9f,1.5f,.12f,.48f,.20f);
    }
    box(0.0f,.0f,-9.0f,2.0f,1.5f,1.2f,.20f,.54f,.86f);
    box(0.0f,1.55f,-9.0f,1.25f,.12f,.85f,.92f,.82f,.38f);
}
static void uploadVertices(void) {
    for(int i=0;i<vertexCount;i++) {
        positions[i]=(Pos){vertices[i].x,vertices[i].y,vertices[i].z};
        colors[i]=(Clr){vertices[i].r,vertices[i].g,vertices[i].b,vertices[i].a};
    }
    memcpy(posVbo,positions,vertexCount*sizeof(Pos));
    memcpy(colorVbo,colors,vertexCount*sizeof(Clr));
    GSPGPU_FlushDataCache(posVbo,vertexCount*sizeof(Pos));
    GSPGPU_FlushDataCache(colorVbo,vertexCount*sizeof(Clr));
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM,NULL);
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) { gfxExit(); return 1; }
    target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,XFER);
    dvlb=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);
    shaderProgramInit(&prog);
    shaderProgramSetVsh(&prog,&dvlb->DVLE[0]);
    C3D_BindProgram(&prog);
    C3D_CullFace(GPU_CULL_NONE);
    projectionLoc=shaderInstanceGetUniformLocation(prog.vertexShader,"projection");
    C3D_AttrInfo *ai=C3D_GetAttrInfo();
    AttrInfo_Init(ai);
    AttrInfo_AddLoader(ai,0,GPU_FLOAT,3);
    AttrInfo_AddLoader(ai,1,GPU_FLOAT,4);
    C3D_TexEnv *env=C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);
    C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
    posVbo=linearAlloc(sizeof(positions));
    colorVbo=linearAlloc(sizeof(colors));
    C3D_BufInfo *bi=C3D_GetBufInfo();
    BufInfo_Init(bi);
    BufInfo_Add(bi,posVbo,sizeof(Pos),1,0x0);
    BufInfo_Add(bi,colorVbo,sizeof(Clr),1,0x1);
    Mtx_PerspTilt(&projection,C3D_AngleFromDegrees(55.0f),400.0f/240.0f,.1f,100.0f,true);

    printf("\x1b[1;1H+--------------------------------+");
    printf("\x1b[2;1H|          CITY LIFE 3D          |");
    printf("\x1b[3;1H+--------------------------------+");
    printf("\x1b[5;1H3D TEST WORLD");
    printf("\x1b[7;1HD-PAD: Move around");
    printf("\x1b[8;1HA: Enter / leave vehicle");
    printf("\x1b[10;1HSTART: Exit");

    while(aptMainLoop()) {
        hidScanInput();
        u32 keys=hidKeysHeld(), down=hidKeysDown();
        if(down&KEY_START) break;
        if(down&KEY_A) {
            if(riding) riding=0;
            else {
                float ds=(playerX-scooterX)*(playerX-scooterX)+(playerZ-scooterZ)*(playerZ-scooterZ);
                float dc=(playerX-carX)*(playerX-carX)+(playerZ-carZ)*(playerZ-carZ);
                if(ds<dc && ds<3.0f) riding=1;
                else if(dc<3.0f) riding=2;
            }
        }
        float speed=riding==2?.13f:(riding==1?.105f:.075f);
        if(keys&KEY_LEFT) playerX-=speed;
        if(keys&KEY_RIGHT) playerX+=speed;
        if(keys&KEY_UP) playerZ-=speed;
        if(keys&KEY_DOWN) playerZ+=speed;
        if(playerX < -12.0f) playerX=-12.0f;
        if(playerX > 12.0f) playerX=12.0f;
        if(playerZ < -12.0f) playerZ=-12.0f;
        if(playerZ > 12.0f) playerZ=12.0f;
        if(riding==1) { scooterX=playerX; scooterZ=playerZ; }
        if(riding==2) { carX=playerX; carZ=playerZ; }

        buildCity();
        person(npcX,npcZ,.88f,.46f,.20f);
        person(npc2X,npc2Z,.66f,.30f,.76f);
        vehicle(scooterX,scooterZ,.12f,.68f,.86f);
        vehicle(carX,carZ,.86f,.25f,.18f);
        if(!riding) person(playerX,playerZ,.12f,.40f,.92f);
        uploadVertices();

        C3D_FVec eye=FVec3_New(playerX,5.0f,playerZ+8.5f);
        C3D_FVec look=FVec3_New(playerX,0.7f,playerZ-1.0f);
        C3D_FVec up=FVec3_New(0.0f,1.0f,0.0f);
        Mtx_LookAt(&view,&eye,&look,&up,true);
        Mtx_Multiply(&viewProjection,&projection,&view);

        printf("\x1b[12;1HVEHICLE: %-12s",riding==1?"SCOOTER":(riding==2?"CITY CAR":"ON FOOT"));
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x78BDEBFF,0);
        C3D_FrameDrawOn(target);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projectionLoc,&viewProjection);
        C3D_DrawArrays(GPU_TRIANGLES,0,vertexCount);
        C3D_FrameEnd(0);
    }
    linearFree(posVbo);
    linearFree(colorVbo);
    shaderProgramFree(&prog);
    DVLB_Free(dvlb);
    C3D_Fini();
    gfxExit();
    return 0;
}
