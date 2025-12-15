#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GIF_H_IMPLEMENTATION
#include "gif.h"

typedef struct {
    int x;
    int y;
} Point;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <image> <width> <height> <x1,y1> [x2,y2 ...]\n", prog);
}

static bool parse_coord(const char *arg, Point *out) {
    const char *comma = strchr(arg, ',');
    if (!comma) {
        return false;
    }

    errno = 0;
    char *endptr = NULL;
    long x = strtol(arg, &endptr, 10);
    if (errno != 0 || endptr != comma) {
        return false;
    }

    long y = strtol(comma + 1, &endptr, 10);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }

    out->x = (int)x;
    out->y = (int)y;
    return true;
}

static bool copy_frame(uint8_t *dst, const uint8_t *src, int src_w, int src_h, int frame_w, int frame_h, Point origin) {
    if (origin.x < 0 || origin.y < 0) {
        return false;
    }
    if (origin.x + frame_w > src_w || origin.y + frame_h > src_h) {
        return false;
    }

    const int stride_src = src_w * 4;
    const int stride_dst = frame_w * 4;
    for (int row = 0; row < frame_h; ++row) {
        const uint8_t *src_row = src + ((origin.y + row) * stride_src) + origin.x * 4;
        uint8_t *dst_row = dst + row * stride_dst;
        memcpy(dst_row, src_row, (size_t)stride_dst);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }

    const char *image_path = argv[1];

    errno = 0;
    char *end = NULL;
    long frame_w_long = strtol(argv[2], &end, 10);
    if (errno != 0 || *end != '\0' || frame_w_long <= 0) {
        fprintf(stderr, "Invalid frame width: %s\n", argv[2]);
        return 1;
    }

    errno = 0;
    long frame_h_long = strtol(argv[3], &end, 10);
    if (errno != 0 || *end != '\0' || frame_h_long <= 0) {
        fprintf(stderr, "Invalid frame height: %s\n", argv[3]);
        return 1;
    }

    const int frame_w = (int)frame_w_long;
    const int frame_h = (int)frame_h_long;

    const int frame_count = argc - 4;
    Point *points = (Point *)malloc(sizeof(Point) * (size_t)frame_count);
    if (!points) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    for (int i = 0; i < frame_count; ++i) {
        if (!parse_coord(argv[4 + i], &points[i])) {
            fprintf(stderr, "Invalid coordinate: %s (expected x,y)\n", argv[4 + i]);
            free(points);
            return 1;
        }
    }

    int img_w = 0, img_h = 0, channels = 0;
    uint8_t *img = stbi_load(image_path, &img_w, &img_h, &channels, 4);
    if (!img) {
        fprintf(stderr, "Failed to load image '%s': %s\n", image_path, stbi_failure_reason());
        free(points);
        return 1;
    }

    const char *output_path = "spritechop.gif";
    GifWriter writer = {0};
    const uint32_t delay_cs = 8; // 80 ms per frame

    if (!GifBegin(&writer, output_path, (uint32_t)frame_w, (uint32_t)frame_h, delay_cs, 8, false)) {
        fprintf(stderr, "Failed to open output GIF for writing\n");
        stbi_image_free(img);
        free(points);
        return 1;
    }

    uint8_t *frame_buffer = (uint8_t *)malloc((size_t)frame_w * (size_t)frame_h * 4);
    if (!frame_buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        GifEnd(&writer);
        stbi_image_free(img);
        free(points);
        return 1;
    }

    bool ok = true;
    for (int i = 0; i < frame_count; ++i) {
        if (!copy_frame(frame_buffer, img, img_w, img_h, frame_w, frame_h, points[i])) {
            fprintf(stderr, "Frame %d with origin (%d,%d) is out of bounds for image %dx%d\n",
                    i + 1, points[i].x, points[i].y, img_w, img_h);
            ok = false;
            break;
        }

        if (!GifWriteFrame(&writer, frame_buffer, (uint32_t)frame_w, (uint32_t)frame_h, delay_cs, 8, false)) {
            fprintf(stderr, "Failed to write frame %d\n", i + 1);
            ok = false;
            break;
        }
    }

    GifEnd(&writer);
    stbi_image_free(img);
    free(frame_buffer);
    free(points);

    if (!ok) {
        remove(output_path);
        return 1;
    }

    printf("Wrote %d frame(s) to %s (%dx%d)\n", frame_count, output_path, frame_w, frame_h);
    return 0;
}
