#ifndef BAYREUTH_TILES_H
#define BAYREUTH_TILES_H

#include <stdint.h>
#include <stdio.h>

#define BT3D_VERSION 1u
#define BT3D_KIND_ROAD 1u
#define BT3D_KIND_BUILDING 2u

typedef struct {
    double origin_lat;
    double origin_lon;
    float tile_size_m;
    uint32_t tile_count;
    uint32_t tiles_read;
} Bt3dMap;

typedef struct {
    int16_t x_quarter_m;
    int16_t z_quarter_m;
} Bt3dPoint;

/* Returns 0 on success, -1 for an invalid/truncated file. */
int bt3d_open(FILE *file, Bt3dMap *map);

/*
 * Reads one tile and calls visit once for each feature.
 * point_buffer is caller-owned scratch memory; oversized features are rejected.
 * Returns 1 when a tile was read, 0 at end of map, or -1 on malformed data.
 */
int bt3d_read_next_tile(FILE *file, Bt3dMap *map,
                        Bt3dPoint *point_buffer, uint16_t point_capacity,
                        int (*visit)(int32_t tile_x, int32_t tile_z,
                                     uint8_t kind, const Bt3dPoint *points,
                                     uint16_t point_count, void *user),
                        void *user);

#endif
