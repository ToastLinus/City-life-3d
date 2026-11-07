#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "vshader_shbin.h"
#include "bayreuth_tiles.h"

#define XFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define MAX_VERTICES 60000
#define MAX_MAP_VERTICES 30000

typedef struct { float x,y,z,r,g,b,a; } Vtx;
typedef struct { float x,y,z; } Pos;
typedef struct { float r,g,b,a; } Clr;

static Vtx vertices[MAX_VERTICES];
static Vtx bayreuthVertices[MAX_MAP_VERTICES];
static int bayreuthVertexCount=0;
static int bayreuthTileX=999999, bayreuthTileZ=999999;
static int bayreuthReady=0;
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
#define NPC_COUNT 6
/* Pedestrians walk along the two clear sidewalk strips. */
static float npcX[NPC_COUNT]={-3.35f,-3.35f,-3.35f,3.35f,3.35f,3.35f};
static float npcZ[NPC_COUNT]={-8.0f,-2.5f,5.0f,8.0f,2.5f,-5.0f};
static float npcDirection[NPC_COUNT]={1.0f,1.0f,1.0f,-1.0f,-1.0f,-1.0f};
static float npcSpeed[NPC_COUNT]={.014f,0.0f,.018f,0.0f,.012f,0.0f};
static float npcWalkPhase[NPC_COUNT]={0,1,2,3,4,5};
static float playerYaw=0.0f, playerJump=0.0f, playerJumpVelocity=0.0f;
static float carX=2.5f, carZ=2.0f;
static float scooterX=-2.5f, scooterZ=1.5f;
static float trafficCarX=-1.55f, trafficCarZ=-10.0f, trafficCarYaw=3.1415926f;
static float trafficCar2X=1.55f, trafficCar2Z=11.0f, trafficCar2Yaw=0.0f;
static int vehicleStyle=0;
static int riding=0;
static int firstPerson=0;
static int scooterTrick=0;
static float cameraYaw=0.0f, cameraPitch=0.0f;
static int touchWasDown=0;
static touchPosition previousTouch;
static s16 *soundSamples=NULL;
static ndspWaveBuf soundWave;
static int audioReady=0;

static void playTone(float frequency,float duration,float volume) {
    if(!audioReady || !soundSamples) return;
    if(soundWave.status==NDSP_WBUF_QUEUED) return;
    int count=(int)(22050.0f*duration);
    if(count<1) count=1;
    if(count>4096) count=4096;
    for(int i=0;i<count;i++) {
        float envelope=1.0f-(float)i/(float)count;
        float sample=sinf(6.2831853f*frequency*(float)i/22050.0f);
        soundSamples[i]=(s16)(sample*envelope*volume*24000.0f);
    }
    DSP_FlushDataCache(soundSamples,count*sizeof(s16));
    memset(&soundWave,0,sizeof(soundWave));
    soundWave.data_vaddr=soundSamples;
    soundWave.nsamples=count;
    soundWave.looping=false;
    ndspChnWaveBufAdd(0,&soundWave);
}

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
    /* Stylized gabled roof instead of a flat floating slab. */
    float ex=w*.60f, ez=d*.57f, ridge=h+.72f;
    Vtx lf=V(x-ex,h,z-ez,.48f,.22f,.16f), rf=V(x+ex,h,z-ez,.48f,.22f,.16f);
    Vtx lb=V(x-ex,h,z+ez,.40f,.17f,.13f), rb=V(x+ex,h,z+ez,.40f,.17f,.13f);
    Vtx rfTop=V(x,ridge,z-ez,.58f,.28f,.19f), rbTop=V(x,ridge,z+ez,.52f,.23f,.16f);
    quad(lf,rfTop,rbTop,lb);
    quad(rfTop,rf,rb,rbTop);
    tri(lf,rf,rfTop); tri(lb,rbTop,rb);
    /* Front windows have contrasting frames and visible glass. */
    float front=z-d*.505f;
    box(x-w*.25f,h*.52f,front,w*.24f,h*.24f,.055f,.16f,.17f,.19f);
    box(x-w*.25f,h*.52f,front-.035f,w*.19f,h*.19f,.025f,.28f,.67f,.82f);
    box(x+w*.25f,h*.52f,front,w*.24f,h*.24f,.055f,.16f,.17f,.19f);
    box(x+w*.25f,h*.52f,front-.035f,w*.19f,h*.19f,.025f,.28f,.67f,.82f);
    box(x,.0f,front-.035f,w*.17f,h*.55f,.07f,.32f,.19f,.12f);
    box(x+w*.25f,h*.52f,front-.052f,.035f,h*.20f,.018f,.80f,.84f,.82f);
    box(x-w*.25f,h*.52f,front-.052f,.035f,h*.20f,.018f,.80f,.84f,.82f);
}
static void tree(float x,float z) {
    box(x,0.0f,z,.24f,1.25f,.24f,.36f,.20f,.12f);
    /* Two faceted foliage tiers give the trees a chunky cartoon silhouette. */
    float y0=1.0f, y1=2.25f, radius=.92f;
    Vtx tip=V(x,y1,z,.12f,.42f,.18f);
    Vtx p[4]={V(x-radius,y0,z,.12f,.50f,.20f),V(x,y0,z-radius,.16f,.58f,.22f),
              V(x+radius,y0,z,.10f,.43f,.16f),V(x,y0,z+radius,.08f,.38f,.14f)};
    for(int i=0;i<4;i++) tri(p[i],p[(i+1)%4],tip);
    Vtx tip2=V(x,1.65f,z,.18f,.58f,.22f);
    Vtx q[4]={V(x-.67f,0.82f,z,.18f,.62f,.24f),V(x,0.82f,z-.67f,.14f,.54f,.20f),
              V(x+.67f,0.82f,z,.10f,.48f,.18f),V(x,0.82f,z+.67f,.08f,.42f,.15f)};
    for(int i=0;i<4;i++) tri(q[i],q[(i+1)%4],tip2);
}
static void person(float x,float z,float r,float g,float b,float phase,float lift,float yaw) {
    /* Head, torso, arms and animated legs; face points toward local -Z. */
    float swing=sinf(phase)*.13f;
    int start=vertexCount;
    box(x,.38f+lift,z,.40f,.62f,.30f,r,g,b);
    box(x,1.06f+lift,z,.31f,.31f,.31f,.92f,.70f,.53f);
    box(x-.27f,.38f+lift,z,.14f,.56f,.16f,r*.82f,g*.82f,b*.82f);
    box(x+.27f,.38f+lift,z,.14f,.56f,.16f,r*.82f,g*.82f,b*.82f);
    box(x-.115f+swing,.0f+lift,z,.17f,.43f,.19f,.16f,.18f,.21f);
    box(x+.115f-swing,.0f+lift,z,.17f,.43f,.19f,.16f,.18f,.21f);
    box(x-.075f,.0f+lift,z-.163f,.055f,.055f,.025f,.08f,.08f,.08f);
    box(x+.075f,.0f+lift,z-.163f,.055f,.055f,.025f,.08f,.08f,.08f);
    float cs=cosf(yaw), sn=sinf(yaw);
    for(int i=start;i<vertexCount;i++) {
        float dx=vertices[i].x-x, dz=vertices[i].z-z;
        vertices[i].x=x+dx*cs+dz*sn;
        vertices[i].z=z-dx*sn+dz*cs;
    }
}
static int hitsTree(float x,float z,float radius) {
    for(int i=0;i<4;i++) {
        float treeX=-8.0f+(float)i*5.3f;
        float dx=x-treeX, dz=z-5.5f;
        float limit=radius+.32f;
        if(dx*dx+dz*dz<limit*limit) return 1;
    }
    return 0;
}
static float carYaw=0.0f, carVelocity=0.0f;
static float scooterYaw=0.0f;
static float objectYaw=0.0f, objectCenterX=0.0f, objectCenterZ=0.0f;
static void transformPoint(float *x,float *z) {
    float dx=*x-objectCenterX, dz=*z-objectCenterZ;
    float c=cosf(objectYaw), s=sinf(objectYaw);
    *x=objectCenterX+dx*c-dz*s;
    *z=objectCenterZ+dx*s+dz*c;
}
static void wheel(float x,float y,float z,float radius,float width) {
    const int sides=10;
    for(int i=0;i<sides;i++) {
        float a=(float)i*6.2831853f/(float)sides;
        float b=(float)(i+1)*6.2831853f/(float)sides;
        float y1=y+cosf(a)*radius, z1=z+sinf(a)*radius;
        float y2=y+cosf(b)*radius, z2=z+sinf(b)*radius;
        float x0=x-width*.5f, x1=x+width*.5f;
        float ax=x0, az=z1, bx=x0, bz=z2, cx=x1, cz=z2, dx=x1, dz=z1;
        transformPoint(&ax,&az); transformPoint(&bx,&bz);
        transformPoint(&cx,&cz); transformPoint(&dx,&dz);
        quad(V(ax,y1,az,.055f,.06f,.07f),V(bx,y2,bz,.055f,.06f,.07f),V(cx,y2,cz,.055f,.06f,.07f),V(dx,y1,dz,.055f,.06f,.07f));
    }
    /* Metallic hubs share the wheel transform, so they turn with the vehicle. */
    int hubStart=vertexCount;
    box(x,y,z,width*1.08f,radius*.72f,radius*.72f,.48f,.50f,.54f);
    for(int i=hubStart;i<vertexCount;i++)
        transformPoint(&vertices[i].x,&vertices[i].z);
}
static void scooterBox(float x,float y,float z,float w,float h,float d,
                       float r,float g,float b) {
    int start=vertexCount;
    box(x,y,z,w,h,d,r,g,b);
    for(int i=start;i<vertexCount;i++)
        transformPoint(&vertices[i].x,&vertices[i].z);
}
static void scooterAt(float x,float z,float yaw,int trick) {
    objectCenterX=x; objectCenterZ=z; objectYaw=yaw;
    /* Two proper wheels, a narrow footboard, steering stem and handlebars. */
    float frontLift=trick?.34f:0.0f;
    wheel(x,.23f+frontLift,z-.62f,.23f,.13f);
    wheel(x,.23f,z+.62f,.23f,.13f);
    scooterBox(x,.38f+frontLift*.35f,z-.20f,.24f,.10f,.52f,.16f,.18f,.20f);
    scooterBox(x,.38f,z+.28f,.24f,.10f,.38f,.16f,.18f,.20f);
    scooterBox(x,.43f+frontLift,z-.48f,.12f,.10f,.18f,.25f,.28f,.30f);
    scooterBox(x,.70f+frontLift,z-.55f,.075f,.68f,.075f,.72f,.75f,.78f);
    scooterBox(x,.99f+frontLift,z-.55f,.62f,.075f,.09f,.12f,.14f,.16f);
    scooterBox(x,.94f+frontLift,z-.55f,.12f,.12f,.12f,.10f,.11f,.12f);
    objectYaw=0.0f;
}
static void scooter(float x,float z) {
    scooterAt(x,z,scooterYaw,scooterTrick);
}
static void scooterRider(float x,float z,float yaw) {
    objectCenterX=x; objectCenterZ=z; objectYaw=yaw;
    int start=vertexCount;
    box(x,.78f,z,.34f,.58f,.25f,.18f,.30f,.82f);
    box(x,1.43f,z,.29f,.29f,.29f,.92f,.70f,.53f);
    box(x-.22f,.78f,z,.12f,.48f,.13f,.12f,.15f,.20f);
    box(x+.22f,.78f,z,.12f,.48f,.13f,.12f,.15f,.20f);
    for(int i=start;i<vertexCount;i++)
        transformPoint(&vertices[i].x,&vertices[i].z);
    objectYaw=0.0f;
}
static void carBox(float x,float y,float z,float w,float h,float d,float r,float g,float b) {
    objectCenterX=carX; objectCenterZ=carZ;
    objectYaw=carYaw;
    int start=vertexCount;
    box(x,y,z,w,h,d,r,g,b);
    for(int i=start;i<vertexCount;i++) transformPoint(&vertices[i].x,&vertices[i].z);
    objectYaw=0.0f;
}
static void vehicle(float x,float z,float r,float g,float b) {
    if(r>.5f) {
        if(vehicleStyle==1) {
            /* Utility van: taller roof and a longer, boxier rear cabin. */
            carBox(x,.10f,z,1.90f,.58f,3.05f,r,g,b);
            carBox(x,.67f,z+.12f,1.78f,.78f,1.95f,r*.82f,g*.82f,b*.82f);
            carBox(x,.79f,z-.83f,1.52f,.34f,.055f,.28f,.61f,.76f);
            carBox(x,.27f,z-1.55f,.38f,.18f,.10f,1.0f,.96f,.72f);
            carBox(x,.28f,z+1.55f,.38f,.18f,.10f,1.0f,.04f,.025f);
            objectCenterX=x; objectCenterZ=z; objectYaw=carYaw;
            wheel(x-.97f,.25f,z-1.02f,.34f,.20f);
            wheel(x+.97f,.25f,z-1.02f,.34f,.20f);
            wheel(x-.97f,.25f,z+1.02f,.34f,.20f);
            wheel(x+.97f,.25f,z+1.02f,.34f,.20f);
            objectYaw=0.0f;
        } else if(vehicleStyle==2) {
            /* Compact hatchback: shorter body and lower cabin. */
            carBox(x,.10f,z,1.62f,.43f,2.20f,r,g,b);
            carBox(x,.52f,z-.10f,1.38f,.55f,1.24f,r*.78f,g*.78f,b*.78f);
            carBox(x,.60f,z-.65f,1.20f,.29f,.05f,.28f,.62f,.78f);
            carBox(x,.25f,z-1.12f,.34f,.15f,.08f,1.0f,.96f,.72f);
            carBox(x,.25f,z+1.12f,.34f,.15f,.08f,1.0f,.04f,.025f);
            objectCenterX=x; objectCenterZ=z; objectYaw=carYaw;
            wheel(x-.83f,.23f,z-.70f,.29f,.18f);
            wheel(x+.83f,.23f,z-.70f,.29f,.18f);
            wheel(x-.83f,.23f,z+.70f,.29f,.18f);
            wheel(x+.83f,.23f,z+.70f,.29f,.18f);
            objectYaw=0.0f;
        } else {
        carBox(x,.10f,z,1.78f,.48f,2.65f,r,g,b);
        carBox(x,.57f,z-.12f,1.48f,.68f,1.48f,r*.78f,g*.78f,b*.78f);
        carBox(x,.63f,z-.12f,1.50f,.38f,1.49f,.18f,.43f,.58f);
        /* The car's FRONT is always local -Z: bright headlights and a grille. */
        carBox(x,.24f,z-1.31f,1.55f,.16f,.20f,.12f,.13f,.15f);
        carBox(x,.27f,z-1.37f,.38f,.18f,.10f,1.00f,.96f,.70f);
        carBox(x,.27f,z-1.37f,.12f,.20f,.12f,1.00f,1.00f,.92f);
        carBox(x,.27f,z-1.34f,.16f,.10f,.08f,1.00f,.66f,.12f);
        carBox(x,.23f,z+1.31f,1.55f,.16f,.20f,.12f,.10f,.11f);
        carBox(x,.28f,z+1.37f,.38f,.19f,.10f,1.00f,.035f,.025f);
        carBox(x,.28f,z+1.37f,.12f,.20f,.12f,.72f,.015f,.02f);
        /* Front bumper and lower grille make the driving direction unmistakable. */
        carBox(x,.12f,z-1.40f,1.72f,.12f,.12f,.34f,.36f,.39f);
        carBox(x,.34f,z-1.405f,.62f,.12f,.035f,.025f,.035f,.045f);
        /* A broad, blue-tinted windscreen sits behind the front edge. */
        carBox(x,.83f,z-.83f,1.30f,.34f,.055f,.30f,.67f,.82f);
        objectCenterX=x; objectCenterZ=z; objectYaw=carYaw;
        wheel(x-.91f,.25f,z-.82f,.34f,.20f);
        wheel(x+.91f,.25f,z-.82f,.34f,.20f);
        wheel(x-.91f,.25f,z+.82f,.34f,.20f);
        wheel(x+.91f,.25f,z+.82f,.34f,.20f);
        objectYaw=0.0f;
        }
    } else {
        scooter(x,z);
    }
}
static void buildCity(void) {
    vertexCount=0;
    box(0.0f,-.18f,0.0f,30.0f,.25f,30.0f,.26f,.58f,.29f);
    box(0.0f,-.025f,0.0f,5.0f,.035f,30.0f,.20f,.21f,.24f);
    for(int z=-12;z<=12;z+=4) box(0.0f,.0f,(float)z, .12f,.012f,1.5f,.94f,.82f,.48f);
    /* A zebra crossing and short stop line make the street read as a real road. */
    for(int i=0;i<7;i++)
        box(-1.8f+(float)i*.60f,.018f,-2.2f,.34f,.025f,.95f,.92f,.93f,.88f);
    box(0.0f,.018f, -3.05f,4.5f,.025f,.10f,.95f,.88f,.62f);
    house(-5.0f,-5.0f,3.0f,2.8f,3.0f,.82f,.48f,.28f);
    house(-5.0f,1.0f,3.4f,3.6f,3.0f,.50f,.67f,.83f);
    house(5.0f,-6.0f,3.2f,3.0f,3.0f,.78f,.56f,.34f);
    house(5.0f,1.0f,3.0f,3.8f,3.0f,.68f,.48f,.72f);
    /* Raised sidewalks separate pedestrians from the road. */
    box(-4.05f,-.01f,0.0f,2.5f,.12f,30.0f,.62f,.61f,.56f);
    box(4.05f,-.01f,0.0f,2.5f,.12f,30.0f,.62f,.61f,.56f);
    for(int i=0;i<4;i++) {
        float x=-8.0f+(float)i*5.3f;
        tree(x,5.5f);
    }
    box(0.0f,.0f,-9.0f,2.0f,1.5f,1.2f,.20f,.54f,.86f);
    box(0.0f,1.55f,-9.0f,1.25f,.12f,.85f,.92f,.82f,.38f);
    /* Small street details make the test district feel like a lived-in town. */
    for(int z=-10;z<=10;z+=5) {
        box(-4.65f,.0f,(float)z,.10f,2.65f,.10f,.24f,.25f,.27f);
        box(-4.65f,2.55f,(float)z,.42f,.18f,.42f,.96f,.82f,.42f);
        box(4.65f,.0f,(float)z,.10f,2.65f,.10f,.24f,.25f,.27f);
        box(4.65f,2.55f,(float)z,.42f,.18f,.42f,.96f,.82f,.42f);
    }
    /* Benches on the pavement: dark legs and warm wooden seats. */
    for(int side=-1;side<=1;side+=2) {
        float bx=(float)side*4.05f;
        box(bx,.28f,-4.0f,1.05f,.12f,.34f,.48f,.27f,.13f);
        box(bx-.38f,.05f,-4.0f,.08f,.30f,.08f,.20f,.21f,.22f);
        box(bx+.38f,.05f,-4.0f,.08f,.30f,.08f,.20f,.21f,.22f);
        box(bx,.55f,-4.16f,1.05f,.42f,.09f,.48f,.27f,.13f);
    }
    /* Side-street shopfront details and flower planters. */
    box(-5.0f,1.05f,2.58f,1.15f,.34f,.08f,.88f,.76f,.28f);
    box(-5.0f,.05f,2.64f,.62f,.42f,.08f,.28f,.18f,.12f);
    for(int i=0;i<5;i++) {
        float px=-5.0f+(float)i*.42f;
        box(px,.12f,3.15f,.22f,.22f,.22f,.22f,.40f,.18f);
        box(px,.34f,3.15f,.28f,.24f,.28f,.92f,.42f,.56f);
    }
}

static int addBayreuthFeature(int32_t tx,int32_t tz,uint8_t kind,const Bt3dPoint *points,uint16_t count,void *user) {
    (void)user;
    if(tx<bayreuthTileX-1 || tx>bayreuthTileX+1 || tz<bayreuthTileZ-1 || tz>bayreuthTileZ+1) return 0;
    if(kind==BT3D_KIND_ROAD) {
        for(uint16_t i=1;i<count;i++) {
            float x0=(float)tx*256.0f+(float)points[i-1].x_quarter_m*.25f;
            float z0=(float)tz*256.0f+(float)points[i-1].z_quarter_m*.25f;
            float x1=(float)tx*256.0f+(float)points[i].x_quarter_m*.25f;
            float z1=(float)tz*256.0f+(float)points[i].z_quarter_m*.25f;
            float dx=x1-x0,dz=z1-z0,len=sqrtf(dx*dx+dz*dz);
            if(len<.05f) continue;
            float ox=-dz/len*2.6f,oz=dx/len*2.6f;
            if(vertexCount+6>MAX_MAP_VERTICES) return -1;
            quad(V(x0+ox,.012f,z0+oz,.19f,.20f,.22f),V(x1+ox,.012f,z1+oz,.19f,.20f,.22f),V(x1-ox,.012f,z1-oz,.19f,.20f,.22f),V(x0-ox,.012f,z0-oz,.19f,.20f,.22f));
        }
    } else if(kind==BT3D_KIND_BUILDING && count>=3) {
        float bx=(float)tx*256.0f,bz=(float)tz*256.0f;
        for(uint16_t i=1;i+1<count;i++) {
            float ax=bx+(float)points[0].x_quarter_m*.25f,az=bz+(float)points[0].z_quarter_m*.25f;
            float px=bx+(float)points[i].x_quarter_m*.25f,pz=bz+(float)points[i].z_quarter_m*.25f;
            float qx=bx+(float)points[i+1].x_quarter_m*.25f,qz=bz+(float)points[i+1].z_quarter_m*.25f;
            tri(V(ax,4.0f,az,.66f,.48f,.34f),V(px,4.0f,pz,.72f,.55f,.40f),V(qx,4.0f,qz,.62f,.44f,.31f));
        }
        for(uint16_t i=0;i<count;i++) {
            uint16_t j=(uint16_t)((i+1)%count);
            float ax=bx+(float)points[i].x_quarter_m*.25f,az=bz+(float)points[i].z_quarter_m*.25f;
            float px=bx+(float)points[j].x_quarter_m*.25f,pz=bz+(float)points[j].z_quarter_m*.25f;
            quad(V(ax,.02f,az,.48f,.39f,.31f),V(px,.02f,pz,.48f,.39f,.31f),V(px,4.0f,pz,.66f,.52f,.39f),V(ax,4.0f,az,.66f,.52f,.39f));
            if(vertexCount>MAX_MAP_VERTICES-18) return 0;
        }
    }
    return 0;
}
static void rebuildBayreuthMap(void) {
    FILE *file; Bt3dMap map; Bt3dPoint points[4096];
    int saved=vertexCount;
    bayreuthTileX=(int)floorf(playerX/256.0f); bayreuthTileZ=(int)floorf(playerZ/256.0f);
    vertexCount=0;
    file=fopen("romfs:/bayreuth-tiles.bin","rb");
    if(file && bt3d_open(file,&map)==0) {
        while(map.tiles_read<map.tile_count) {
            int result=bt3d_read_next_tile(file,&map,points,4096,addBayreuthFeature,NULL);
            if(result<=0) break;
        }
        fclose(file);
    }
    bayreuthVertexCount=vertexCount;
    if(bayreuthVertexCount>MAX_MAP_VERTICES) bayreuthVertexCount=MAX_MAP_VERTICES;
    memcpy(bayreuthVertices,vertices,(size_t)bayreuthVertexCount*sizeof(Vtx));
    vertexCount=saved; bayreuthReady=(bayreuthVertexCount>0);
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
    romfsInit();
    if(ndspInit()==0) {
        soundSamples=(s16*)linearAlloc(4096*sizeof(s16));
        if(soundSamples) audioReady=1;
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspChnReset(0);
        ndspSetMasterVol(1.0f);
        ndspChnSetPaused(0,false);
        ndspChnSetInterp(0,NDSP_INTERP_LINEAR);
        ndspChnSetRate(0,22050.0f);
        ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);
        playTone(880.0f,.24f,.90f);
    }
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
    printf("\x1b[7;1HD-PAD: Move / steer");
    printf("\x1b[8;1HB: Jump on foot");
    printf("\x1b[9;1HA: Enter / leave vehicle");
    printf("\x1b[9;1HCar: UP gas / DOWN brake");
    printf("\x1b[10;1HSTART: Exit");
    printf("\x1b[11;1HAUDIO: %s",audioReady?"INITIALIZED":"UNAVAILABLE");

    while(aptMainLoop()) {
        hidScanInput();
        u32 keys=hidKeysHeld(), down=hidKeysDown();
        if(down&KEY_START) break;
        if(down&KEY_X) firstPerson=!firstPerson;
        if(down&KEY_B && riding==0 && playerJump<=0.001f) {
            playerJumpVelocity=.24f;
            playTone(880.0f,.10f,.55f);
        }
        if(down&KEY_Y && riding==1) playTone(520.0f,.08f,.45f);
        if(riding!=0) { playerJump=0.0f; playerJumpVelocity=0.0f; }
        scooterTrick=(riding==1 && (keys&KEY_Y))?1:0;
        if(down&KEY_A) {
            if(riding) {
                riding=0;
                playTone(420.0f,.12f,.55f);
            } else {
                float ds=(playerX-scooterX)*(playerX-scooterX)+(playerZ-scooterZ)*(playerZ-scooterZ);
                float dc=(playerX-carX)*(playerX-carX)+(playerZ-carZ)*(playerZ-carZ);
                if(ds<dc && ds<3.0f) {
                    riding=1;
                    playTone(660.0f,.12f,.55f);
                } else if(dc<3.0f) {
                    riding=2;
                    playTone(740.0f,.14f,.55f);
                }
            }
        }
        // Touchscreen drag rotates the camera; vertical drag adjusts camera height.
        if(keys&KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            if(touchWasDown) {
                cameraYaw += (float)(touch.px-previousTouch.px)*0.012f;
                cameraPitch += (float)(touch.py-previousTouch.py)*0.018f;
                if(cameraPitch < -1.5f) cameraPitch=-1.5f;
                if(cameraPitch > 5.0f) cameraPitch=5.0f;
            }
            previousTouch=touch;
            touchWasDown=1;
        } else {
            touchWasDown=0;
        }
        float speed=riding==2?.13f:(riding==1?.105f:.075f);
        float previousX=playerX, previousZ=playerZ;
        if(riding==2) {
            if(keys&KEY_UP) carVelocity+=.006f;
            if(keys&KEY_DOWN) carVelocity-=.008f;
            if(carVelocity>.22f) carVelocity=.22f;
            if(carVelocity<-.10f) carVelocity=-.10f;
            if(!(keys&(KEY_UP|KEY_DOWN))) carVelocity*=.96f;
            if(fabsf(carVelocity)<.001f) carVelocity=0.0f;
            /* Invert the car steering sign: the rendered vehicle turns toward the pressed screen direction. */
            if(keys&KEY_LEFT) carYaw+=carVelocity*.10f;
            if(keys&KEY_RIGHT) carYaw-=carVelocity*.10f;
            /* The visible front of the car is local -Z, so UP moves along -Z. */
            playerX+=sinf(carYaw)*carVelocity;
            playerZ-=cosf(carYaw)*carVelocity;
        } else {
            /* Walk relative to the camera: UP always means toward the view direction. */
            float forwardX=-sinf(cameraYaw), forwardZ=-cosf(cameraYaw);
            float rightX=cosf(cameraYaw), rightZ=-sinf(cameraYaw);
            float moveX=0.0f, moveZ=0.0f;
            if(keys&KEY_UP) { moveX+=forwardX; moveZ+=forwardZ; }
            if(keys&KEY_DOWN) { moveX-=forwardX; moveZ-=forwardZ; }
            /* Screen-left is the negative camera-right vector; screen-right is positive. */
            if(keys&KEY_LEFT) { moveX+=rightX; moveZ+=rightZ; }
            if(keys&KEY_RIGHT) { moveX-=rightX; moveZ-=rightZ; }
            /* Keep diagonal movement at the same speed as straight movement. */
            float moveLength=sqrtf(moveX*moveX+moveZ*moveZ);
            if(moveLength>0.001f) {
                playerYaw=atan2f(moveX/moveLength,-moveZ/moveLength);
                if(riding==1) scooterYaw=playerYaw;
                playerX+=(moveX/moveLength)*speed;
                playerZ+=(moveZ/moveLength)*speed;
            } else if(riding==1) {
                /* When stationary, point the scooter where the camera is looking. */
                scooterYaw=-cameraYaw;
            }
        }
        /* Tree trunks block walking, scooter and car movement. */
        float collisionRadius=riding==2?0.95f:(riding==1?0.48f:0.34f);
        if(hitsTree(playerX,playerZ,collisionRadius)) {
            playerX=previousX;
            playerZ=previousZ;
            if(riding==2) carVelocity=0.0f;
        }
        if(riding==1) { scooterX=playerX; scooterZ=playerZ; }
        if(riding==2) { carX=playerX; carZ=playerZ; }

        // Two independent cars provide the ambient road traffic.
        trafficCarZ+=0.055f;
        if(trafficCarZ>13.0f) trafficCarZ=-13.0f;
        trafficCar2Z-=0.045f;
        if(trafficCar2Z < -13.0f) trafficCar2Z=13.0f;

        /* Sidewalk pedestrians move at a visible pace and animate their steps. */
        for(int i=0;i<NPC_COUNT;i++) {
            npcZ[i]+=npcDirection[i]*npcSpeed[i];
            npcWalkPhase[i]+=(npcSpeed[i]>.001f)?.055f:.008f;
            if(npcZ[i] < -11.0f) { npcZ[i]=-11.0f; npcDirection[i]=1.0f; }
            if(npcZ[i] > 11.0f) { npcZ[i]=11.0f; npcDirection[i]=-1.0f; }
        }
        if(riding==0 && playerJumpVelocity!=0.0f) {
            playerJump+=playerJumpVelocity;
            playerJumpVelocity-=.018f;
            if(playerJump<=0.0f) { playerJump=0.0f; playerJumpVelocity=0.0f; }
        }

        if(!bayreuthReady || (int)floorf(playerX/256.0f)!=bayreuthTileX || (int)floorf(playerZ/256.0f)!=bayreuthTileZ) rebuildBayreuthMap();
        vertexCount=0;
        box(0.0f,-.18f,0.0f,600.0f,.25f,600.0f,.26f,.58f,.29f);
        if(bayreuthReady && vertexCount+bayreuthVertexCount<MAX_VERTICES) {
            memcpy(&vertices[vertexCount],bayreuthVertices,(size_t)bayreuthVertexCount*sizeof(Vtx));
            vertexCount+=bayreuthVertexCount;
        }
        person(npcX[0],npcZ[0],.88f,.46f,.20f,npcWalkPhase[0],0,1.5708f);
        person(npcX[1],npcZ[1],.66f,.30f,.76f,npcWalkPhase[1],0,1.5708f);
        person(npcX[2],npcZ[2],.18f,.58f,.82f,npcWalkPhase[2],0,1.5708f);
        person(npcX[3],npcZ[3],.84f,.66f,.22f,npcWalkPhase[3],0,-1.5708f);
        person(npcX[4],npcZ[4],.32f,.72f,.42f,npcWalkPhase[4],0,-1.5708f);
        person(npcX[5],npcZ[5],.72f,.32f,.28f,npcWalkPhase[5],0,-1.5708f);
        /* Only the player's own scooter is spawned here. There is no NPC traffic scooter. */
        vehicle(scooterX,scooterZ,.12f,.68f,.86f);
        if(riding==1) scooterRider(scooterX,scooterZ,scooterYaw);
        vehicle(carX,carZ,.86f,.25f,.18f);
        // Draw two independent traffic vehicles in opposite lanes.
        float savedCarX=carX, savedCarZ=carZ, savedCarYaw=carYaw;
        int savedStyle=vehicleStyle;
        carX=trafficCarX; carZ=trafficCarZ; carYaw=trafficCarYaw;
        vehicleStyle=1;
        vehicle(trafficCarX,trafficCarZ,.72f,.62f,.18f);
        carX=trafficCar2X; carZ=trafficCar2Z; carYaw=trafficCar2Yaw;
        vehicleStyle=2;
        vehicle(trafficCar2X,trafficCar2Z,.20f,.48f,.78f);
        carX=savedCarX; carZ=savedCarZ; carYaw=savedCarYaw;
        vehicleStyle=savedStyle;
        if(!riding) person(playerX,playerZ,.12f,.40f,.92f,0.0f,playerJump,playerYaw);
        uploadVertices();

        C3D_FVec eye, look;
        float viewYaw=(riding==2 && firstPerson)?carYaw:cameraYaw;
        if(firstPerson) {
            eye=FVec3_New(playerX,1.48f,playerZ);
            look=FVec3_New(playerX-sinf(viewYaw)*10.0f,
                           1.48f+(cameraPitch-1.5f)*0.45f,
                           playerZ-cosf(viewYaw)*10.0f);
        } else {
            eye=FVec3_New(playerX+sinf(cameraYaw)*8.5f,5.0f+cameraPitch,playerZ+cosf(cameraYaw)*8.5f);
            look=FVec3_New(playerX-sinf(cameraYaw)*1.0f,0.7f,playerZ-cosf(cameraYaw)*1.0f);
        }
        C3D_FVec up=FVec3_New(0.0f,1.0f,0.0f);
        Mtx_LookAt(&view,eye,look,up,true);
        Mtx_Multiply(&viewProjection,&projection,&view);

        printf("\x1b[12;1H%-12s %s",riding==1?"E-SCOOTER":(riding==2?"CITY CAR":"ON FOOT"),
               firstPerson?"FIRST PERSON":"THIRD PERSON");
        printf("\x1b[13;1HX: CAMERA  Y: SCOOTER TRICK");
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
    if(audioReady) { ndspExit(); linearFree(soundSamples); }
    romfsExit();
    gfxExit();
    return 0;
}
