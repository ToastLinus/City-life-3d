#include "bayreuth_tiles.h"

#include <math.h>
#include <stddef.h>

static int read_exact(FILE *file, unsigned char *out, size_t count) {
    return file && fread(out, 1, count, file) == count;
}

static uint16_t u16le(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t u32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t i32le(const unsigned char *p) {
    uint32_t value = u32le(p);
    /* Convert through int64_t so values above INT32_MAX never overflow. */
    if (value <= 0x7fffffffu) return (int32_t)value;
    return (int32_t)((int64_t)value - 4294967296LL);
}

static float f32le(const unsigned char *p) {
    uint32_t bits = u32le(p);
    float value;
    unsigned char *dst = (unsigned char *)&value;
    dst[0] = (unsigned char)(bits & 0xffu);
    dst[1] = (unsigned char)((bits >> 8) & 0xffu);
    dst[2] = (unsigned char)((bits >> 16) & 0xffu);
    dst[3] = (unsigned char)((bits >> 24) & 0xffu);
    return value;
}

static double f64le(const unsigned char *p) {
    uint64_t bits = (uint64_t)u32le(p) | ((uint64_t)u32le(p + 4) << 32);
    double value;
    unsigned char *dst = (unsigned char *)&value;
    unsigned int i;
    for (i = 0; i < 8; ++i) dst[i] = (unsigned char)(bits >> (i * 8));
    return value;
}

int bt3d_open(FILE *file, Bt3dMap *map) {
    unsigned char header[32];
    if (!map || !read_exact(file, header, sizeof(header))) return -1;
    if (header[0] != 'B' || header[1] != 'T' ||
        header[2] != '3' || header[3] != 'D') return -1;
    if (u16le(header + 4) != BT3D_VERSION) return -1;
    map->origin_lat = f64le(header + 8);
    map->origin_lon = f64le(header + 16);
    map->tile_size_m = f32le(header + 24);
    map->tile_count = u32le(header + 28);
    map->tiles_read = 0;
    if (!isfinite(map->origin_lat) || !isfinite(map->origin_lon) ||
        !isfinite(map->tile_size_m) || map->origin_lat < -90.0 ||
        map->origin_lat > 90.0 || map->origin_lon < -180.0 ||
        map->origin_lon > 180.0 || map->tile_size_m <= 0.0f ||
        map->tile_size_m > 100000.0f || map->tile_count > 1000000u)
        return -1;
    return 0;
}

int bt3d_read_next_tile(FILE *file, Bt3dMap *map,
                        Bt3dPoint *point_buffer, uint16_t point_capacity,
                        int (*visit)(int32_t, int32_t, uint8_t,
                                     const Bt3dPoint *, uint16_t, void *),
                        void *user) {
    unsigned char tile_header[12];
    uint32_t feature_count, feature_index;
    int32_t tile_x, tile_z;

    if (!file || !map || !point_buffer || point_capacity == 0 || !visit)
        return -1;
    if (map->tiles_read >= map->tile_count) return 0;
    if (!read_exact(file, tile_header, sizeof(tile_header))) return -1;
    tile_x = i32le(tile_header);
    tile_z = i32le(tile_header + 4);
    feature_count = u32le(tile_header + 8);
    if (feature_count > 100000u) return -1;

    for (feature_index = 0; feature_index < feature_count; ++feature_index) {
        unsigned char feature_header[4];
        uint8_t kind;
        uint16_t point_count, i;
        if (!read_exact(file, feature_header, sizeof(feature_header))) return -1;
        kind = feature_header[0];
        point_count = u16le(feature_header + 2);
        if ((kind != BT3D_KIND_ROAD && kind != BT3D_KIND_BUILDING) ||
            point_count == 0 || point_count > point_capacity) return -1;
        for (i = 0; i < point_count; ++i) {
            unsigned char point[4];
            if (!read_exact(file, point, sizeof(point))) return -1;
            point_buffer[i].x_quarter_m = (int16_t)u16le(point);
            point_buffer[i].z_quarter_m = (int16_t)u16le(point + 2);
        }
        if (visit(tile_x, tile_z, kind, point_buffer, point_count, user) != 0)
            return -1;
    }
    map->tiles_read++;
    return 1;
}
