#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 400
#define SCREEN_H 240
#define WORLD_W 1200
#define WORLD_H 900

typedef struct { u8 r, g, b; } Color;
typedef struct { int x, y, w, h; Color color; } Building;

static u8 *framebuffer;
static u16 fb_width, fb_height;
static int player_x = 250;
static int player_y = 250;
static int in_car = 0;
static int mission_done = 0;
static const int mission_x = 820;
static const int mission_y = 700;

static const Color GRASS = {75, 145, 75};
static const Color ROAD = {76, 79, 85};
static const Color ROAD_LINE = {235, 218, 130};
static const Color ROOF = {105, 65, 57};
static const Color TREE = {35, 100, 52};
static const Color PLAYER = {245, 65, 70};
static const Color CAR = {55, 145, 235};

static const Building buildings[] = {
    {120, 100, 105, 90, {204,153,112}},
    {285, 105, 115, 100, {186,194,205}},
    {680, 105, 120, 95, {215,180,105}},
    {900, 105, 135, 100, {185,155,190}},
    {120, 590, 130, 100, {190,180,150}},
    {330, 600, 115, 95, {190,205,180}},
    {680, 590, 105, 90, {205,160,130}},
    {930, 590, 140, 105, {170,185,205}}
};

static void put_pixel(int x, int y, Color c) {
    if (!framebuffer || x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    /* The 3DS top framebuffer is stored column-major: 240 pixels per column. */
    unsigned int i = ((unsigned int)x * (unsigned int)fb_width + (unsigned int)y) * 3;
    framebuffer[i] = c.b;
    framebuffer[i + 1] = c.g;
    framebuffer[i + 2] = c.r;
}

static void fill_rect(int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    for (int px = x0; px < x1; px++)
        for (int py = y0; py < y1; py++)
            put_pixel(px, py, c);
}

static void world_rect(int wx, int wy, int w, int h, int cam_x, int cam_y, Color c) {
    fill_rect(wx - cam_x, wy - cam_y, w, h, c);
}

static void draw_tree(int wx, int wy, int cam_x, int cam_y) {
    int x = wx - cam_x, y = wy - cam_y;
    fill_rect(x + 5, y + 12, 5, 12, (Color){105,75,45});
    fill_rect(x, y + 3, 15, 13, TREE);
    fill_rect(x + 3, y, 9, 17, (Color){45,120,58});
}

static void draw_world(void) {
    framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fb_width, &fb_height);
    fill_rect(0, 0, SCREEN_W, SCREEN_H, GRASS);

    int cam_x = player_x - SCREEN_W / 2;
    int cam_y = player_y - SCREEN_H / 2;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > WORLD_W - SCREEN_W) cam_x = WORLD_W - SCREEN_W;
    if (cam_y > WORLD_H - SCREEN_H) cam_y = WORLD_H - SCREEN_H;

    world_rect(0, 390, WORLD_W, 100, cam_x, cam_y, (Color){145,145,135});
    world_rect(0, 400, WORLD_W, 80, cam_x, cam_y, ROAD);
    world_rect(470, 0, 100, WORLD_H, cam_x, cam_y, (Color){145,145,135});
    world_rect(480, 0, 80, WORLD_H, cam_x, cam_y, ROAD);

    for (int x = 20; x < WORLD_W; x += 80)
        world_rect(x, 438, 38, 4, cam_x, cam_y, ROAD_LINE);
    for (int y = 20; y < WORLD_H; y += 80)
        world_rect(518, y, 4, 38, cam_x, cam_y, ROAD_LINE);

    for (unsigned int i = 0; i < sizeof(buildings)/sizeof(buildings[0]); i++) {
        const Building *b = &buildings[i];
        world_rect(b->x, b->y, b->w, b->h, cam_x, cam_y, b->color);
        world_rect(b->x - 4, b->y - 8, b->w + 8, 12, cam_x, cam_y, ROOF);
        world_rect(b->x + 16, b->y + 25, 18, 24, cam_x, cam_y, (Color){90,150,190});
        world_rect(b->x + b->w - 35, b->y + 25, 18, 24, cam_x, cam_y, (Color){90,150,190});
        world_rect(b->x + b->w/2 - 9, b->y + b->h - 28, 18, 28, cam_x, cam_y, (Color){95,65,45});
    }

    world_rect(720, 650, 250, 190, cam_x, cam_y, (Color){65,135,70});
    for (int i = 0; i < 8; i++)
        draw_tree(750 + (i % 4) * 55, 675 + (i / 4) * 75, cam_x, cam_y);
    world_rect(mission_x - 10, mission_y - 10, 20, 20, cam_x, cam_y, (Color){250,220,70});

    int sx = player_x - cam_x;
    int sy = player_y - cam_y;
    if (in_car) {
        fill_rect(sx - 11, sy - 7, 22, 14, CAR);
        fill_rect(sx - 5, sy - 5, 10, 10, (Color){185,225,245});
        fill_rect(sx - 8, sy + 5, 5, 3, (Color){35,35,40});
        fill_rect(sx + 4, sy + 5, 5, 3, (Color){35,35,40});
    } else {
        fill_rect(sx - 4, sy - 4, 9, 9, PLAYER);
        fill_rect(sx - 2, sy - 7, 5, 4, (Color){245,205,165});
    }

    fill_rect(0, 0, SCREEN_W, 15, (Color){20,35,48});
    gfxFlushBuffers();
    gfxSwapBuffers();
}

static void draw_hud(void) {
    consoleClear();
    printf("\x1b[1;1H  CITY LIFE 3D  |  DEMO 0.1");
    printf("\x1b[3;1H  Gebiet: Kleine Stadt");
    printf("\x1b[5;1H  Modus: %s", in_car ? "Auto" : "Zu Fuss");
    printf("\x1b[7;1H  Aufgabe: Erreiche den Park");
    if (mission_done)
        printf("\x1b[9;1H  GESCHAFFT! Der Park ist erreicht.");
    else
        printf("\x1b[9;1H  Ziel: gelber Marker im Park");
    printf("\x1b[12;1H  Steuerung:");
    printf("\x1b[14;1H  Steuerkreuz  - Bewegen");
    printf("\x1b[15;1H  A            - Ein-/Aussteigen");
    printf("\x1b[16;1H  START        - Beenden");
    printf("\x1b[19;1H  CITY LIFE 3D");
    printf("\x1b[20;1H  Ein kleines Open-World-Projekt");
    printf("\x1b[21;1H  fuer deinen Nintendo 3DS.");
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();

        if (down & KEY_START) break;
        if (down & KEY_A) in_car = !in_car;

        int speed = in_car ? 4 : 2;
        if (held & KEY_UP) player_y -= speed;
        if (held & KEY_DOWN) player_y += speed;
        if (held & KEY_LEFT) player_x -= speed;
        if (held & KEY_RIGHT) player_x += speed;

        if (player_x < 12) player_x = 12;
        if (player_y < 12) player_y = 12;
        if (player_x > WORLD_W - 12) player_x = WORLD_W - 12;
        if (player_y > WORLD_H - 12) player_y = WORLD_H - 12;

        int dx = player_x - mission_x;
        int dy = player_y - mission_y;
        if (!mission_done && dx*dx + dy*dy < 34*34) mission_done = 1;

        draw_world();
        draw_hud();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
