#include "mandelbrot.h"

#include <stdalign.h>
#include <string.h>

#define L_MAX 100.0f

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

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

static void mandelbrot_v3_neon(unsigned char *img, int width, int height, int max_iter,
                               float xmin, float xmax, float ymin, float ymax) {
    mandelbrot_v1(img, width, height, max_iter, xmin, xmax, ymin, ymax);
}

static void mandelbrot_v3_x86(unsigned char *img, int width, int height, int max_iter,
                              float xmin, float xmax, float ymin, float ymax) {
#if defined(__x86_64__) || defined(_M_X64)
    float dx = (xmax - xmin) / width;
    float dy = (ymax - ymin) / height;

    __m256 radius = _mm256_set1_ps(L_MAX);
    __m256 two = _mm256_set1_ps(2.0f);
    __m256 one_ps = _mm256_castsi256_ps(_mm256_set1_epi32(1));
    __m256 idx = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);

    for (int y = 0; y < height; y++) {
        float cy_scalar = ymin + y * dy;
        __m256 cy = _mm256_set1_ps(cy_scalar);
        int row = y * width;
        int x = 0;

        for (; x <= width - 8; x += 8) {
            float base_x = xmin + x * dx;
            __m256 cx = _mm256_add_ps(_mm256_set1_ps(base_x), _mm256_mul_ps(idx, _mm256_set1_ps(dx)));
            __m256 zx = _mm256_setzero_ps();
            __m256 zy = _mm256_setzero_ps();
            __m256 iters = _mm256_setzero_ps();

            for (int it = 0; it < max_iter; it++) {
                __m256 zx2 = _mm256_mul_ps(zx, zx);
                __m256 zy2 = _mm256_mul_ps(zy, zy);
                __m256 radius2 = _mm256_add_ps(zx2, zy2);
                __m256 mask = _mm256_cmp_ps(radius2, radius, _CMP_LE_OQ);

                if (_mm256_movemask_ps(mask) == 0)
                    break;

                __m256 zxy = _mm256_mul_ps(zx, zy);
                __m256 nx = _mm256_add_ps(_mm256_sub_ps(zx2, zy2), cx);
                __m256 ny = _mm256_add_ps(_mm256_mul_ps(two, zxy), cy);

                zx = _mm256_blendv_ps(zx, nx, mask);
                zy = _mm256_blendv_ps(zy, ny, mask);
                iters = _mm256_add_ps(iters, _mm256_and_ps(mask, one_ps));
            }

            alignas(32) float out_iters[8];
            _mm256_store_ps(out_iters, iters);
            for (int k = 0; k < 8; k++) {
                int iter = (int)out_iters[k];
                img[row + x + k] = (unsigned char)(iter >= max_iter ? 255 : iter);
            }
        }

        for (; x < width; x++) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ((zx * zx + zy * zy <= L_MAX) && (iter < max_iter)) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy_scalar;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + x] = (unsigned char)(iter >= max_iter ? 255 : iter);
        }
    }
#else
    mandelbrot_v1(img, width, height, max_iter, xmin, xmax, ymin, ymax);
#endif
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
        mandelbrot_v3_neon(img, width, height, max_iter, xmin, xmax, ymin, ymax);
        return;
    case MANDEL_IMPL_V3_X86:
        mandelbrot_v3_x86(img, width, height, max_iter, xmin, xmax, ymin, ymax);
        return;
    default:
        mandelbrot_v1(img, width, height, max_iter, xmin, xmax, ymin, ymax);
        return;
    }
}
