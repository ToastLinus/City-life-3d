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
static int vertexCount=0;
static Vtx bayreuthVertices[MAX_MAP_VERTICES];
static int bayreuthVertexCount=0;
static int bayreuthTileX=999999, bayreuthTileZ=999999;
static int bayreuthReady=0;
static int bayreuthGpuDirty=1;
#define TERRAIN_SAMPLES 9
typedef struct { int32_t x,z; float h[81]; } TerrainTile;
static TerrainTile terrainTiles[9];
static int terrainTileCount=0;
static float terrainBaseHeight=0.0f;
static float terrainHeightRaw(float x,float z) {
 int tx=(int)floorf(x/256.0f), tz=(int)floorf(z/256.0f);
 float fx=(x-(float)tx*256.0f)/32.0f, fz=(z-(float)tz*256.0f)/32.0f;
 if(fx<0)fx=0; if(fx>8)fx=8; if(fz<0)fz=0; if(fz>8)fz=8;
 for(int t=0;t<terrainTileCount;t++) if(terrainTiles[t].x==tx && terrainTiles[t].z==tz) {
  int x0=(int)fx,z0=(int)fz,x1=x0<8?x0+1:x0,z1=z0<8?z0+1:z0;
  float ax=fx-x0,az=fz-z0;
  float h00=terrainTiles[t].h[z0*9+x0],h10=terrainTiles[t].h[z0*9+x1];
  float h01=terrainTiles[t].h[z1*9+x0],h11=terrainTiles[t].h[z1*9+x1];
  float a=h00+(h10-h00)*ax,b=h01+(h11-h01)*ax;
  return a+(b-a)*az;
 }
 return terrainBaseHeight;
}
static Vtx V(float x,float y,float z,float r,float g,float b);
static void quad(Vtx a,Vtx b,Vtx c,Vtx d);
static float terrainHeight(float x,float z){return terrainHeightRaw(x,z)-terrainBaseHeight;}
static void loadTerrainTiles(void) {
 FILE *f=fopen("romfs:/bayreuth-dem.bin","rb"); unsigned char h[16]; terrainTileCount=0;
 if(!f)return;
 if(fread(h,1,16,f)!=16 || memcmp(h,"BTH1",4)!=0 || h[4]!=1 || h[5]!=0 || h[6]!=9 || h[7]!=0){fclose(f);return;}
 uint32_t count=(uint32_t)h[12]|((uint32_t)h[13]<<8)|((uint32_t)h[14]<<16)|((uint32_t)h[15]<<24);
 if(count>100000u){fclose(f);return;}
 for(uint32_t i=0;i<count;i++){
  unsigned char th[8]; float values[81];
  if(fread(th,1,8,f)!=8 || fread(values,sizeof(float),81,f)!=81)break;
  int32_t tx=(int32_t)((uint32_t)th[0]|((uint32_t)th[1]<<8)|((uint32_t)th[2]<<16)|((uint32_t)th[3]<<24));
  int32_t tz=(int32_t)((uint32_t)th[4]|((uint32_t)th[5]<<8)|((uint32_t)th[6]<<16)|((uint32_t)th[7]<<24));
  if(tx<bayreuthTileX-1||tx>bayreuthTileX+1||tz<bayreuthTileZ-1||tz>bayreuthTileZ+1)continue;
  if(terrainTileCount<9){TerrainTile *tile=&terrainTiles[terrainTileCount++];tile->x=tx;tile->z=tz;memcpy(tile->h,values,sizeof(values));}
 }
 fclose(f); terrainBaseHeight=terrainHeightRaw(0.0f,0.0f);
}
static void addTerrainMesh(void) {
 for(int t=0;t<terrainTileCount;t++)for(int z=0;z<8;z++)for(int x=0;x<8;x++){
  TerrainTile *tile=&terrainTiles[t];
  float x0=tile->x*256.0f+x*32.0f,x1=x0+32.0f,z0=tile->z*256.0f+z*32.0f,z1=z0+32.0f;
  float h00=tile->h[z*9+x]-terrainBaseHeight,h10=tile->h[z*9+x+1]-terrainBaseHeight;
  float h01=tile->h[(z+1)*9+x]-terrainBaseHeight,h11=tile->h[(z+1)*9+x+1]-terrainBaseHeight;
  float shade=.34f+(h00+h10+h01+h11)*.0012f;if(shade<.22f)shade=.22f;if(shade>.48f)shade=.48f;
  if(vertexCount+6<=MAX_MAP_VERTICES)quad(V(x0,h00,z0,.22f,shade,.25f),V(x1,h10,z0,.24f,shade+.02f,.27f),V(x1,h11,z1,.22f,shade,.24f),V(x0,h01,z1,.20f,shade-.01f,.23f));
 }
}
static void liftNewVertices(int start,float amount){for(int i=start;i<vertexCount;i++)vertices[i].y+=amount;}
static Pos positions[MAX_VERTICES];
static Clr colors[MAX_VERTICES];
static void *posVbo,*colorVbo;
static DVLB_s *dvlb;
static shaderProgram_s prog;
static int projectionLoc;
static C3D_Mtx projection, view, viewProjection;
static C3D_RenderTarget *target;

static float playerX=0.0f, playerZ=0.0f;
static int interiorMode=0; /* 0=outside, 1=Lidl, 2=Rotmain-Center mall */
static float savedOutsideX=0.0f, savedOutsideZ=0.0f;
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
static int landmarkNear(float x,float z,float cx,float cz,float radius) {
    float dx=x-cx,dz=z-cz;
    return dx*dx+dz*dz<radius*radius;
}
/* Custom low-poly landmarks are added only when the player is nearby.
   Coordinates use the same local Bayreuth projection as the OSM tiles. */
static void addLandmarkModels(void) {
    /* Rotmain-Center: broad multi-storey shopping-centre mass and glass bands. */
    const float rcx=-690.4f, rcz=400.6f;
    if(landmarkNear(playerX,playerZ,rcx,rcz,125.0f)) {
        float y=terrainHeight(rcx,rcz);
        box(rcx,y,rcz,78.0f,15.0f,40.0f,.58f,.60f,.61f);
        box(rcx,y+15.0f,rcz,80.0f,1.0f,42.0f,.34f,.36f,.38f);
        for(int level=0;level<3;level++) {
            float wy=y+3.0f+(float)level*3.7f;
            box(rcx,wy,rcz-20.35f,70.0f,2.1f,.18f,.20f,.43f,.56f);
            box(rcx,wy,rcz+20.35f,70.0f,2.1f,.18f,.20f,.43f,.56f);
            box(rcx-39.1f,wy,rcz, .18f,2.1f,34.0f,.20f,.43f,.56f);
            box(rcx+39.1f,wy,rcz, .18f,2.1f,34.0f,.20f,.43f,.56f);
        }
        box(rcx,y+1.5f,rcz-20.8f,18.0f,3.0f,.7f,.72f,.12f,.14f);
        box(rcx,y+.25f,rcz-22.0f,12.0f,.25f,3.0f,.28f,.30f,.32f);
        /* Forecourt and visible parking rows. */
        box(rcx,y+.03f,rcz+34.0f,88.0f,.08f,22.0f,.28f,.29f,.30f);
        for(int i=0;i<10;i++)
            box(rcx-38.0f+(float)i*8.4f,y+.10f,rcz+34.0f,.16f,.035f,8.0f,.88f,.88f,.82f);
    }
    /* Lidl, Meranierring 49: low supermarket, blue/yellow fascia and car park. */
    const float lx=-2357.7f, lz=1014.1f;
    if(landmarkNear(playerX,playerZ,lx,lz,125.0f)) {
        float y=terrainHeight(lx,lz);
        box(lx,y,lz,48.0f,7.0f,29.0f,.83f,.84f,.80f);
        box(lx,y+7.0f,lz,49.0f,.65f,30.0f,.28f,.30f,.32f);
        box(lx,y+4.8f,lz-14.8f,45.0f,1.15f,.25f,.08f,.22f,.68f);
        box(lx,y+5.0f,lz-15.0f,10.0f,.72f,.32f,.98f,.83f,.08f);
        box(lx,y+2.1f,lz-14.85f,12.0f,2.6f,.20f,.20f,.62f,.72f);
        box(lx-3.0f,y+.1f,lz-14.98f,2.2f,2.0f,.18f,.10f,.18f,.22f);
        box(lx+3.0f,y+.1f,lz-14.98f,2.2f,2.0f,.18f,.10f,.18f,.22f);
        /* Parking apron, pedestrian strip and individual bay markings. */
        box(lx,y+.025f,lz+31.0f,68.0f,.06f,29.0f,.27f,.28f,.29f);
        box(lx,y+.075f,lz+14.5f,56.0f,.06f,4.0f,.68f,.67f,.61f);
        for(int i=0;i<12;i++)
            box(lx-31.0f+(float)i*5.6f,y+.11f,lz+31.0f,.13f,.025f,10.0f,.91f,.91f,.86f);
        box(lx,y+.09f,lz-17.0f,14.0f,.08f,3.0f,.72f,.72f,.68f);
    }
    /* Hofgarten: formal paths, water channel, tree alleys and the Sonnentempel. */
    const float hx=78.3f, hz=-260.7f;
    if(landmarkNear(playerX,playerZ,hx,hz,125.0f)) {
        float y=terrainHeight(hx,hz);
        box(hx,y+.025f,hz,8.0f,.06f,150.0f,.48f,.39f,.25f);
        box(hx,y+.03f,hz,120.0f,.06f,5.0f,.48f,.39f,.25f);
        box(hx+28.0f,y+.04f,hz,5.0f,.07f,105.0f,.12f,.42f,.66f);
        box(hx+28.0f,y+.08f,hz,2.4f,.08f,105.0f,.22f,.60f,.78f);
        for(int i=0;i<8;i++) {
            float tx=hx-43.0f+(float)i*12.0f;
            tree(tx,hz-45.0f);
            tree(tx,hz+45.0f);
        }
        for(int i=0;i<5;i++) {
            float tz=hz-30.0f+(float)i*15.0f;
            tree(hx-52.0f,tz);
            tree(hx+52.0f,tz);
        }
        /* Eight-column circular pavilion, a simplified Sonnentempel silhouette. */
        const float sx=35.9f, sz=-263.3f;
        float sy=terrainHeight(sx,sz);
        for(int i=0;i<8;i++) {
            float a=(float)i*6.2831853f/8.0f;
            box(sx+cosf(a)*2.6f,sy,sz+sinf(a)*2.6f,.38f,3.1f,.38f,.88f,.84f,.70f);
        }
        box(sx,sy+3.15f,sz,6.8f,.48f,6.8f,.86f,.82f,.68f);
        box(sx,sy+3.65f,sz,5.2f,.35f,5.2f,.72f,.66f,.50f);
        box(sx,sy+.12f,sz,5.5f,.18f,5.5f,.52f,.46f,.34f);
    }
    /* Unterpreuschwitz keeps its real OSM road and house footprints; no invented blocks. */
}
static void addInteriorScene(void) {
    if(interiorMode==1) {
        box(0,-.16f,0,25.0f,.22f,21.0f,.67f,.67f,.63f);
        box(-12.3f,0,0,.35f,4.0f,21.0f,.88f,.87f,.80f);
        box(12.3f,0,0,.35f,4.0f,21.0f,.88f,.87f,.80f);
        box(0,0,-10.3f,25.0f,4.0f,.35f,.88f,.87f,.80f);
        box(-9.5f,0,10.3f,5.5f,4.0f,.35f,.88f,.87f,.80f);
        box(9.5f,0,10.3f,5.5f,4.0f,.35f,.88f,.87f,.80f);
        box(0,2.9f,-10.05f,5.0f,1.0f,.12f,.06f,.20f,.68f);
        box(0,3.0f,-9.96f,2.5f,.62f,.08f,.98f,.84f,.08f);
        for(int i=0;i<5;i++) {
            float x=-9.0f+(float)i*4.5f;
            box(x,.05f,-7.8f,3.3f,1.8f,.75f,.72f,.75f,.77f);
            box(x,1.0f,-7.38f,3.0f,.10f,.08f,.30f,.68f,.78f);
        }
        for(int row=0;row<3;row++) {
            float z=-3.0f+(float)row*4.0f;
            float aisleX[3]={-4.8f,0.0f,4.8f};
            float shelfColor[3][3]={{.72f,.48f,.25f},{.26f,.48f,.68f},{.72f,.48f,.25f}};
            for(int aisle=0;aisle<3;aisle++) {
                float x=aisleX[aisle];
                box(x,.0f,z,1.0f,1.65f,3.0f,shelfColor[aisle][0],shelfColor[aisle][1],shelfColor[aisle][2]);
                for(int level=0;level<3;level++) {
                    float sy=.34f+(float)level*.48f;
                    box(x,sy,z,1.12f,.09f,3.1f,.90f,.88f,.82f);
                    for(int item=0;item<4;item++) {
                        float pz=z-1.15f+(float)item*.72f;
                        float pr=(item+aisle+level)%3==0?.82f:((item+level)%2==0?.22f:.90f);
                        float pg=(item+aisle)%2==0?.34f:.68f;
                        float pb=(level+item)%2==0?.18f:.72f;
                        box(x-.18f,sy+.12f,pz,.24f,.25f,.28f,pr,pg,pb);
                        box(x+.18f,sy+.12f,pz,.24f,.25f,.28f,pb,pr,pg);
                    }
                }
            }
        }
        /* Fresh-produce island and refrigerated cabinets along the rear wall. */
        for(int i=0;i<4;i++) {
            float x=-7.5f+(float)i*5.0f;
            box(x,.0f,8.7f,2.6f,1.0f,1.0f,.38f,.30f,.20f);
            for(int j=0;j<3;j++) box(x-0.7f+(float)j*.7f,.82f,8.7f,.42f,.28f,.42f,
                                      j==0?.86f:.30f,j==1?.72f:.82f,j==2?.18f:.28f);
        }
        box(10.8f,1.35f,-1.0f,1.4f,2.6f,7.0f,.72f,.82f,.84f);
        box(10.05f,1.35f,-1.0f,.08f,2.35f,6.6f,.22f,.62f,.76f);
        for(int i=0;i<5;i++) box(10.0f,1.0f,-3.4f+(float)i*1.2f,.10f,.16f,.65f,.88f,.90f,.92f);
        for(int i=0;i<3;i++) {
            float x=-5.0f+(float)i*5.0f;
            box(x,.0f,7.0f,3.4f,.95f,1.0f,.20f,.24f,.27f);
            box(x,1.0f,6.75f,1.0f,.10f,.55f,.12f,.14f,.16f);
        }
    } else if(interiorMode==2) {
        box(0,-.16f,0,37.0f,.22f,23.0f,.70f,.69f,.65f);
        box(-18.3f,0,0,.35f,4.5f,23.0f,.82f,.83f,.82f);
        box(18.3f,0,0,.35f,4.5f,23.0f,.82f,.83f,.82f);
        box(0,0,-11.3f,37.0f,4.5f,.35f,.82f,.83f,.82f);
        box(-16.0f,0,11.3f,4.0f,4.5f,.35f,.82f,.83f,.82f);
        box(16.0f,0,11.3f,4.0f,4.5f,.35f,.82f,.83f,.82f);
        /* Four separate shop fronts, each with a distinct display and fascia. */
        for(int shop=0;shop<4;shop++) {
            float cx=-13.2f+(float)shop*8.8f;
            float sr=shop==0?.12f:(shop==1?.12f:(shop==2?.72f:.18f));
            float sg=shop==0?.35f:(shop==1?.52f:(shop==2?.22f:.58f));
            float sb=shop==0?.68f:(shop==1?.76f:(shop==2?.18f:.38f));
            box(cx,0,-7.8f,8.0f,3.5f,.25f,.55f,.57f,.60f);
            box(cx,2.65f,-7.55f,7.4f,.62f,.18f,sr,sg,sb);
            box(cx-4.0f,0,-5.5f,.22f,3.5f,4.7f,.55f,.57f,.60f);
            box(cx+4.0f,0,-5.5f,.22f,3.5f,4.7f,.55f,.57f,.60f);
            /* Glass shop window and a clearly visible entrance opening. */
            box(cx-2.45f,1.25f,-7.62f,2.5f,2.25f,.08f,.24f,.57f,.70f);
            box(cx+2.45f,1.25f,-7.62f,2.5f,2.25f,.08f,.24f,.57f,.70f);
            box(cx,1.05f,-7.62f,.10f,2.1f,.10f,.82f,.84f,.82f);
            if(shop==0) {
                /* Fashion shop: display rails and colourful clothing. */
                box(cx,1.65f,-6.9f,5.5f,.10f,.12f,.72f,.74f,.76f);
                for(int i=0;i<6;i++) box(cx-2.3f+(float)i*.92f,.75f,-6.9f,.48f,1.45f,.28f,
                                         i%3==0?.82f:(i%3==1?.18f:.88f),.34f,.48f);
            } else if(shop==1) {
                /* Electronics shop: counters, screens and product plinths. */
                for(int i=0;i<3;i++) {
                    float px=cx-2.5f+(float)i*2.5f;
                    box(px,.0f,-6.9f,1.55f,.95f,1.15f,.28f,.32f,.36f);
                    box(px,.95f,-6.30f,1.18f,.82f,.08f,.12f,.20f,.26f);
                    box(px,1.0f,-6.24f,.98f,.62f,.035f,.20f,.58f,.76f);
                }
            } else if(shop==2) {
                /* Toy and gifts shop with colourful shelf stock. */
                for(int row=0;row<2;row++) for(int i=0;i<5;i++) {
                    float px=cx-2.8f+(float)i*1.4f;
                    box(px,.35f+(float)row*.95f,-6.9f,1.05f,.72f,.72f,.68f,.48f,.24f);
                    box(px,.40f+(float)row*.95f,-6.48f,.55f,.42f,.08f,.90f,.72f,.18f);
                }
            } else {
                /* Café: service counter, tables and seats. */
                box(cx,0,-6.8f,5.8f,1.0f,1.0f,.48f,.27f,.16f);
                for(int i=0;i<3;i++) {
                    float px=cx-2.0f+(float)i*2.0f;
                    box(px,.85f,-6.8f,.72f,.72f,.72f,.72f,.28f,.18f);
                    box(px,1.55f,-6.8f,.95f,.10f,.95f,.60f,.34f,.20f);
                }
            }
        }
        /* Bright atrium lighting, planters and seating along the public mall walk. */
        for(int i=0;i<6;i++) {
            float x=-14.0f+(float)i*5.6f;
            box(x,3.9f,0.0f,2.4f,.10f,.65f,.96f,.91f,.70f);
            box(x,.0f,1.2f,.72f,.48f,.72f,.28f,.20f,.12f);
            tree(x,1.2f);
        }
        for(int i=0;i<3;i++) {
            float x=-10.0f+(float)i*10.0f;
            box(x,.38f,8.0f,2.8f,.16f,.7f,.48f,.28f,.14f);
            box(x,.78f,8.0f,2.8f,.12f,.7f,.32f,.19f,.10f);
        }
        for(int i=0;i<4;i++) {
            float x=-12.0f+(float)i*8.0f;
            box(x,.35f,3.2f,2.4f,.18f,.65f,.43f,.25f,.13f);
            box(x-1.0f,.0f,3.2f,.12f,.35f,.12f,.20f,.21f,.22f);
            box(x+1.0f,.0f,3.2f,.12f,.35f,.12f,.20f,.21f,.22f);
            box(x,.25f,8.0f,1.4f,.5f,1.2f,.34f,.22f,.12f);
            box(x,.8f,8.0f,.9f,.7f,.8f,.12f,.46f,.20f);
        }
    }
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
            float mx=(x0+x1)*.5f-playerX,mz=(z0+z1)*.5f-playerZ;
            if(mx*mx+mz*mz>125.0f*125.0f) continue;
            float ox=-dz/len*2.6f,oz=dx/len*2.6f;
            if(vertexCount+6>MAX_MAP_VERTICES) return -1;
            float y0=terrainHeight(x0,z0)+.055f,y1=terrainHeight(x1,z1)+.055f;
            quad(V(x0+ox,y0,z0+oz,.19f,.20f,.22f),V(x1+ox,y1,z1+oz,.19f,.20f,.22f),V(x1-ox,y1,z1-oz,.19f,.20f,.22f),V(x0-ox,y0,z0-oz,.19f,.20f,.22f));
        }
    } else if(kind==BT3D_KIND_BUILDING && count>=3) {
        float bx=(float)tx*256.0f,bz=(float)tz*256.0f;
        float cx=0.0f,cz=0.0f;
        for(uint16_t i=0;i<count;i++){cx+=(float)points[i].x_quarter_m*.25f;cz+=(float)points[i].z_quarter_m*.25f;}
        cx=cx/(float)count+bx-playerX; cz=cz/(float)count+bz-playerZ;
        if(cx*cx+cz*cz>125.0f*125.0f) return 0;
        /* Replace the two most recognizable shop footprints with hand-built models. */
        float worldCx=cx+playerX,worldCz=cz+playerZ;
        if(landmarkNear(worldCx,worldCz,-690.4f,400.6f,48.0f) ||
           landmarkNear(worldCx,worldCz,-2357.7f,1014.1f,30.0f)) return 0;
        for(uint16_t i=1;i+1<count;i++) {
            float ax=bx+(float)points[0].x_quarter_m*.25f,az=bz+(float)points[0].z_quarter_m*.25f;
            float px=bx+(float)points[i].x_quarter_m*.25f,pz=bz+(float)points[i].z_quarter_m*.25f;
            float qx=bx+(float)points[i+1].x_quarter_m*.25f,qz=bz+(float)points[i+1].z_quarter_m*.25f;
            if(vertexCount+3>MAX_MAP_VERTICES) return -1;
            tri(V(ax,terrainHeight(ax,az)+4.0f,az,.66f,.48f,.34f),V(px,terrainHeight(px,pz)+4.0f,pz,.72f,.55f,.40f),V(qx,terrainHeight(qx,qz)+4.0f,qz,.62f,.44f,.31f));
        }
        for(uint16_t i=0;i<count;i++) {
            uint16_t j=(uint16_t)((i+1)%count);
            float ax=bx+(float)points[i].x_quarter_m*.25f,az=bz+(float)points[i].z_quarter_m*.25f;
            float px=bx+(float)points[j].x_quarter_m*.25f,pz=bz+(float)points[j].z_quarter_m*.25f;
            if(vertexCount+6>MAX_MAP_VERTICES) return -1;
            float ay=terrainHeight(ax,az),py=terrainHeight(px,pz);
            quad(V(ax,ay,az,.48f,.39f,.31f),V(px,py,pz,.48f,.39f,.31f),V(px,py+4.0f,pz,.66f,.52f,.39f),V(ax,ay+4.0f,az,.66f,.52f,.39f));
        }
    }
    return 0;
}
static void rebuildBayreuthMap(void) {
    FILE *file; Bt3dMap map; Bt3dPoint points[4096];
    int saved=vertexCount;
    bayreuthTileX=(int)floorf(playerX/256.0f); bayreuthTileZ=(int)floorf(playerZ/256.0f);
    loadTerrainTiles();
    vertexCount=0;
    addTerrainMesh();
    addLandmarkModels();
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
    vertexCount=saved; bayreuthReady=(bayreuthVertexCount>0); bayreuthGpuDirty=1;
}

static void uploadVertices(void) {
    int mapCount=(bayreuthReady && !interiorMode)?bayreuthVertexCount:0;
    if(mapCount>MAX_VERTICES) mapCount=MAX_VERTICES;
    if(bayreuthGpuDirty) {
        for(int i=0;i<mapCount;i++) {
            positions[i]=(Pos){bayreuthVertices[i].x,bayreuthVertices[i].y,bayreuthVertices[i].z};
            colors[i]=(Clr){bayreuthVertices[i].r,bayreuthVertices[i].g,bayreuthVertices[i].b,bayreuthVertices[i].a};
        }
        if(mapCount>0) {
            memcpy(posVbo,positions,(size_t)mapCount*sizeof(Pos));
            memcpy(colorVbo,colors,(size_t)mapCount*sizeof(Clr));
            GSPGPU_FlushDataCache(posVbo,(size_t)mapCount*sizeof(Pos));
            GSPGPU_FlushDataCache(colorVbo,(size_t)mapCount*sizeof(Clr));
        }
        bayreuthGpuDirty=0;
    }
    if(vertexCount<mapCount) vertexCount=mapCount;
    for(int i=mapCount;i<vertexCount;i++) {
        positions[i]=(Pos){vertices[i].x,vertices[i].y,vertices[i].z};
        colors[i]=(Clr){vertices[i].r,vertices[i].g,vertices[i].b,vertices[i].a};
    }
    if(vertexCount>mapCount) {
        size_t offset=(size_t)mapCount;
        size_t count=(size_t)(vertexCount-mapCount);
        memcpy((Pos*)posVbo+offset,positions+offset,count*sizeof(Pos));
        memcpy((Clr*)colorVbo+offset,colors+offset,count*sizeof(Clr));
        GSPGPU_FlushDataCache((Pos*)posVbo+offset,count*sizeof(Pos));
        GSPGPU_FlushDataCache((Clr*)colorVbo+offset,count*sizeof(Clr));
    }
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
            if(interiorMode) {
                if(playerZ>8.0f && fabsf(playerX)<2.8f) {
                    interiorMode=0;
                    playerX=savedOutsideX;
                    playerZ=savedOutsideZ;
                    cameraYaw=0.0f;
                    playTone(420.0f,.12f,.55f);
                }
            } else if(!riding) {
                float drc=(playerX+690.4f)*(playerX+690.4f)+(playerZ-378.6f)*(playerZ-378.6f);
                float dl=(playerX+2357.7f)*(playerX+2357.7f)+(playerZ-997.1f)*(playerZ-997.1f);
                if(drc<64.0f || dl<64.0f) {
                    savedOutsideX=playerX; savedOutsideZ=playerZ;
                    interiorMode=(drc<dl)?2:1;
                    playerX=0.0f; playerZ=7.0f; cameraYaw=0.0f;
                    playTone(660.0f,.14f,.55f);
                } else {
                    float ds=(playerX-scooterX)*(playerX-scooterX)+(playerZ-scooterZ)*(playerZ-scooterZ);
                    float dc=(playerX-carX)*(playerX-carX)+(playerZ-carZ)*(playerZ-carZ);
                    if(ds<dc && ds<3.0f) { riding=1; playTone(660.0f,.12f,.55f); }
                    else if(dc<3.0f) { riding=2; playTone(740.0f,.14f,.55f); }
                }
            } else {
                riding=0;
                playTone(420.0f,.12f,.55f);
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
        if(interiorMode) {
            float sideLimit=interiorMode==1?11.5f:17.5f;
            if(playerX < -sideLimit) playerX=-sideLimit;
            if(playerX > sideLimit) playerX=sideLimit;
            if(playerZ < -9.4f) playerZ=-9.4f;
            /* Keep the south doorway open; elsewhere the shop wall blocks passage. */
            if(playerZ > 10.0f && fabsf(playerX)>2.8f) playerZ=10.0f;
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

        if(!interiorMode && (!bayreuthReady || (int)floorf(playerX/256.0f)!=bayreuthTileX || (int)floorf(playerZ/256.0f)!=bayreuthTileZ)) rebuildBayreuthMap();
        vertexCount=(!interiorMode && bayreuthReady)?bayreuthVertexCount:0;
        if(interiorMode) addInteriorScene();
        else if(!bayreuthReady) box(0.0f,-100.0f,0.0f,600.0f,.10f,600.0f,.26f,.58f,.29f);
        if(!interiorMode) {
        { int start=vertexCount; person(npcX[0],npcZ[0],.88f,.46f,.20f,npcWalkPhase[0],0,1.5708f); liftNewVertices(start,terrainHeight(npcX[0],npcZ[0])); }
        { int start=vertexCount; person(npcX[1],npcZ[1],.66f,.30f,.76f,npcWalkPhase[1],0,1.5708f); liftNewVertices(start,terrainHeight(npcX[1],npcZ[1])); }
        { int start=vertexCount; person(npcX[2],npcZ[2],.18f,.58f,.82f,npcWalkPhase[2],0,1.5708f); liftNewVertices(start,terrainHeight(npcX[2],npcZ[2])); }
        { int start=vertexCount; person(npcX[3],npcZ[3],.84f,.66f,.22f,npcWalkPhase[3],0,-1.5708f); liftNewVertices(start,terrainHeight(npcX[3],npcZ[3])); }
        { int start=vertexCount; person(npcX[4],npcZ[4],.32f,.72f,.42f,npcWalkPhase[4],0,-1.5708f); liftNewVertices(start,terrainHeight(npcX[4],npcZ[4])); }
        { int start=vertexCount; person(npcX[5],npcZ[5],.72f,.32f,.28f,npcWalkPhase[5],0,-1.5708f); liftNewVertices(start,terrainHeight(npcX[5],npcZ[5])); }
        /* Only the player's own scooter is spawned here. There is no NPC traffic scooter. */
        { int start=vertexCount; vehicle(scooterX,scooterZ,.12f,.68f,.86f); liftNewVertices(start,terrainHeight(scooterX,scooterZ)); }
        { int start=vertexCount; if(riding==1) scooterRider(scooterX,scooterZ,scooterYaw); liftNewVertices(start,terrainHeight(scooterX,scooterZ)); }
        { int start=vertexCount; vehicle(carX,carZ,.86f,.25f,.18f); liftNewVertices(start,terrainHeight(carX,carZ)); }
        // Draw two independent traffic vehicles in opposite lanes.
        float savedCarX=carX, savedCarZ=carZ, savedCarYaw=carYaw;
        int savedStyle=vehicleStyle;
        carX=trafficCarX; carZ=trafficCarZ; carYaw=trafficCarYaw;
        vehicleStyle=1;
        { int start=vertexCount; vehicle(trafficCarX,trafficCarZ,.72f,.62f,.18f); liftNewVertices(start,terrainHeight(trafficCarX,trafficCarZ)); }
        carX=trafficCar2X; carZ=trafficCar2Z; carYaw=trafficCar2Yaw;
        vehicleStyle=2;
        { int start=vertexCount; vehicle(trafficCar2X,trafficCar2Z,.20f,.48f,.78f); liftNewVertices(start,terrainHeight(trafficCar2X,trafficCar2Z)); }
        carX=savedCarX; carZ=savedCarZ; carYaw=savedCarYaw;
        vehicleStyle=savedStyle;
        }
        { int start=vertexCount; if(!riding) person(playerX,playerZ,.12f,.40f,.92f,0.0f,playerJump,playerYaw); liftNewVertices(start,terrainHeight(playerX,playerZ)); }
        uploadVertices();

        C3D_FVec eye, look;
        float viewYaw=(riding==2 && firstPerson)?carYaw:cameraYaw;
        if(firstPerson) {
            float groundY=terrainHeight(playerX,playerZ);
            eye=FVec3_New(playerX,groundY+1.48f,playerZ);
            look=FVec3_New(playerX-sinf(viewYaw)*10.0f,groundY+1.48f+(cameraPitch-1.5f)*0.45f,playerZ-cosf(viewYaw)*10.0f);
        } else {
            float groundY=terrainHeight(playerX,playerZ);
            eye=FVec3_New(playerX+sinf(cameraYaw)*8.5f,groundY+5.0f+cameraPitch,playerZ+cosf(cameraYaw)*8.5f);
            look=FVec3_New(playerX-sinf(cameraYaw)*1.0f,groundY+0.7f,playerZ-cosf(cameraYaw)*1.0f);
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
