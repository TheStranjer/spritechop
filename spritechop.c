#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#define GIF_H_IMPLEMENTATION
#include "include/gif.h"

typedef struct {
    int x;
    int y;
} Point;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s -i <input image> -o <output image> -s <width>x<height> [-f <delay cs>] <x1,y1> [x2,y2 ...]\n", prog);
    fprintf(stderr, "Options may appear in any order before the coordinates. Size uses the form 80x114. -f sets frame delay in centiseconds (default 8 = 80ms).\n");
    fprintf(stderr, "Example: %s -i ninja.png -s 80x114 -o ninja.gif 35,24 159,24 278,24 397,24\n", prog);
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

static bool parse_size(const char *arg, int *out_w, int *out_h) {
    const char *x = strchr(arg, 'x');
    if (!x) {
        x = strchr(arg, 'X');
    }
    if (!x) {
        return false;
    }

    errno = 0;
    char *endptr = NULL;
    long w = strtol(arg, &endptr, 10);
    if (errno != 0 || endptr != x || w <= 0) {
        return false;
    }

    long h = strtol(x + 1, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || h <= 0) {
        return false;
    }

    *out_w = (int)w;
    *out_h = (int)h;
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
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *input_path = NULL;
    const char *output_path = NULL;
    int frame_w = 0;
    int frame_h = 0;
    uint32_t delay_cs = 8; // default to 80 ms per frame

    int argi = 1;
    for (; argi < argc; ++argi) {
        const char *arg = argv[argi];
        if (strcmp(arg, "-i") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "Missing value for -i\n");
                usage(argv[0]);
                return 1;
            }
            input_path = argv[++argi];
            continue;
        }
        if (strcmp(arg, "-o") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "Missing value for -o\n");
                usage(argv[0]);
                return 1;
            }
            output_path = argv[++argi];
            continue;
        }
        if (strcmp(arg, "-s") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "Missing value for -s\n");
                usage(argv[0]);
                return 1;
            }
            if (!parse_size(argv[argi + 1], &frame_w, &frame_h)) {
                fprintf(stderr, "Invalid size (expected <width>x<height>): %s\n", argv[argi + 1]);
                usage(argv[0]);
                return 1;
            }
            ++argi;
            continue;
        }
        if (strcmp(arg, "-f") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "Missing value for -f\n");
                usage(argv[0]);
                return 1;
            }
            errno = 0;
            char *endptr = NULL;
            long parsed_delay = strtol(argv[argi + 1], &endptr, 10);
            if (errno != 0 || *endptr != '\0' || parsed_delay <= 0 || (unsigned long)parsed_delay > UINT32_MAX) {
                fprintf(stderr, "Invalid frame delay (expected positive centiseconds): %s\n", argv[argi + 1]);
                usage(argv[0]);
                return 1;
            }
            delay_cs = (uint32_t)parsed_delay;
            ++argi;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            usage(argv[0]);
            return 1;
        }
        break;
    }

    if (!input_path) {
        fprintf(stderr, "Input image is required (-i)\n");
        usage(argv[0]);
        return 1;
    }
    if (!output_path) {
        fprintf(stderr, "Output image is required (-o)\n");
        usage(argv[0]);
        return 1;
    }
    if (frame_w == 0 || frame_h == 0) {
        fprintf(stderr, "Frame size is required (-s <width>x<height>)\n");
        usage(argv[0]);
        return 1;
    }

    const int frame_count = argc - argi;
    if (frame_count <= 0) {
        fprintf(stderr, "At least one coordinate is required\n");
        usage(argv[0]);
        return 1;
    }

    Point *points = (Point *)malloc(sizeof(Point) * (size_t)frame_count);
    if (!points) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    for (int i = 0; i < frame_count; ++i) {
        if (!parse_coord(argv[argi + i], &points[i])) {
            fprintf(stderr, "Invalid coordinate: %s (expected x,y)\n", argv[argi + i]);
            free(points);
            return 1;
        }
    }

    int img_w = 0, img_h = 0, channels = 0;
    uint8_t *img = stbi_load(input_path, &img_w, &img_h, &channels, 4);
    if (!img) {
        fprintf(stderr, "Failed to load image '%s': %s\n", input_path, stbi_failure_reason());
        free(points);
        return 1;
    }

    GifWriter writer = {0};

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
