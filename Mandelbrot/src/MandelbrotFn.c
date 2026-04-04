#include "mandelbrot.h"

#include <string.h>

#define L_MAX 100.0f

static void mandelbrot_v1(unsigned char *img, int width, int height, int max_iter,
                          float xmin, float xmax, float ymin, float ymax) {
    float dx = (xmax - xmin) / width;
    float dy = (ymax - ymin) / height;

    for (int y = 0; y < height; y++) {
        float cy = ymin + y * dy;
        int row = y * width;

        for (int x = 0; x < width; x++) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ((zx * zx + zy * zy <= L_MAX) && (iter < max_iter)) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;

                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + x] = (unsigned char)(iter >= max_iter ? 255 : iter);
        }
    }
}

const char *mandelbrot_impl_name(MandelbrotImpl impl) {
    switch (impl) {
    case MANDEL_IMPL_V1:
        return "v1";
    case MANDEL_IMPL_V2:
        return "v2";
    case MANDEL_IMPL_V3_NEON:
        return "v3_neon";
    case MANDEL_IMPL_V3_X86:
        return "v3_x86";
    default:
        return "unknown";
    }
}

int mandelbrot_impl_from_string(const char *name, MandelbrotImpl *out_impl) {
    if (!name || !out_impl)
        return 0;

    if (strcmp(name, "v1") == 0 || strcmp(name, "naive") == 0) {
        *out_impl = MANDEL_IMPL_V1;
        return 1;
    }
    if (strcmp(name, "v2") == 0 || strcmp(name, "arrayed") == 0) {
        *out_impl = MANDEL_IMPL_V2;
        return 1;
    }
    if (strcmp(name, "v3_neon") == 0 || strcmp(name, "neon") == 0) {
        *out_impl = MANDEL_IMPL_V3_NEON;
        return 1;
    }
    if (strcmp(name, "v3_x86") == 0 || strcmp(name, "x86") == 0) {
        *out_impl = MANDEL_IMPL_V3_X86;
        return 1;
    }

    return 0;
}

void mandelbrot_compute(unsigned char *img, int width, int height, int max_iter,
                        float xmin, float xmax, float ymin, float ymax,
                        MandelbrotImpl impl) {
    (void)impl;
    mandelbrot_v1(img, width, height, max_iter, xmin, xmax, ymin, ymax);
}
