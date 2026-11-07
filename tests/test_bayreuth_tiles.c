#include "bayreuth_tiles.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int seen;
static int visit(int32_t tx, int32_t tz, uint8_t kind,
                 const Bt3dPoint *points, uint16_t count, void *user) {
    (void)user;
    assert(tx == -2 && tz == 3);
    assert(kind == BT3D_KIND_ROAD);
    assert(count == 2);
    assert(points[0].x_quarter_m == 4 && points[0].z_quarter_m == -8);
    assert(points[1].x_quarter_m == 12 && points[1].z_quarter_m == 16);
    seen++;
    return 0;
}

int main(void) {
    static const unsigned char bytes[] = {
        'B','T','3','D', 1,0, 0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,128,67, 1,0,0,0,
        254,255,255,255, 3,0,0,0, 1,0,0,0,
        1,0, 2,0, 4,0, 248,255, 12,0, 16,0
    };
    FILE *file = tmpfile();
    Bt3dMap map;
    Bt3dPoint points[8];
    assert(file != NULL);
    assert(fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    rewind(file);
    assert(bt3d_open(file, &map) == 0);
    assert(map.tile_count == 1 && map.tiles_read == 0);
    assert(map.tile_size_m == 256.0f);
    assert(bt3d_read_next_tile(file, &map, points, 8, visit, NULL) == 1);
    assert(seen == 1 && map.tiles_read == 1);
    assert(bt3d_read_next_tile(file, &map, points, 8, visit, NULL) == 0);
    fclose(file);

    file = tmpfile();
    assert(file != NULL);
    assert(fwrite("NOPE", 1, 4, file) == 4);
    rewind(file);
    assert(bt3d_open(file, &map) == -1);
    fclose(file);
    puts("BT3D reader tests passed");
    return 0;
}
