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

static void mandelbrot_v2(unsigned char *img, int width, int height, int max_iter,
                          float xmin, float xmax, float ymin, float ymax) {
    const int vec_size = 8;
    float step_x = (xmax - xmin) / width;
    float step_y = (ymax - ymin) / height;
    float vec_step_x = step_x * vec_size;

    float cy = ymin;

    for (int py = 0; py < height; py++) {
        int row = py * width;
        float cx0 = xmin;
        float c_x[8] = {0};

        for (int i = 0; i < vec_size; i++)
            c_x[i] = cx0 + step_x * i;

        int px = 0;
        for (; px <= width - vec_size; px += vec_size) {
            float z_x[8] = {0};
            float z_y[8] = {0};
            float z_xy[8] = {0};
            float z_x2[8] = {0};
            float z_y2[8] = {0};
            int iters[8] = {0};
            int cmp[8] = {0};

            for (int it = 0; it < max_iter; it++) {
                int all_zero = 1;

                for (int i = 0; i < vec_size; i++) {
                    float radius2 = z_x2[i] + z_y2[i];
                    cmp[i] = (radius2 <= L_MAX);
                    if (cmp[i])
                        all_zero = 0;
                }

                if (all_zero)
                    break;

                for (int i = 0; i < vec_size; i++) {
                    z_x[i] = z_x2[i] - z_y2[i] + c_x[i];
                    z_y[i] = 2.0f * z_xy[i] + cy;
                    iters[i] += cmp[i];
                    z_x2[i] = z_x[i] * z_x[i];
                    z_y2[i] = z_y[i] * z_y[i];
                    z_xy[i] = z_x[i] * z_y[i];
                }
            }

            for (int i = 0; i < vec_size; i++)
                img[row + px + i] = (unsigned char)(iters[i] >= max_iter ? 255 : iters[i]);

            for (int i = 0; i < vec_size; i++)
                c_x[i] += vec_step_x;
        }

        // Завершаю оставшиеся пиксели, если ширина не делится на 8.
        for (; px < width; px++) {
            float cx = xmin + px * step_x;
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

            img[row + px] = (unsigned char)(iter >= max_iter ? 255 : iter);
        }

        cy += step_y;
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
    switch (impl) {
    case MANDEL_IMPL_V1:
        mandelbrot_v1(img, width, height, max_iter, xmin, xmax, ymin, ymax);
        return;
    case MANDEL_IMPL_V2:
        mandelbrot_v2(img, width, height, max_iter, xmin, xmax, ymin, ymax);
        return;
    case MANDEL_IMPL_V3_NEON:
    case MANDEL_IMPL_V3_X86:
    default:
        mandelbrot_v1(img, width, height, max_iter, xmin, xmax, ymin, ymax);
        return;
    }
}
