#include "table.h"

/* Acha o segmento [i, i+1] e a fração em 1/256 (fora da faixa satura nas pontas). */
static void locate(const uint16_t *axis, uint8_t n, uint16_t v, uint8_t *i, uint16_t *frac)
{
    if (n < 2 || v <= axis[0]) { *i = 0; *frac = 0; return; }
    if (v >= axis[n - 1])      { *i = (uint8_t)(n - 2); *frac = 256; return; }
    uint8_t k = 0;
    while (k < n - 2 && v >= axis[k + 1]) k++;
    uint16_t span = (uint16_t)(axis[k + 1] - axis[k]);
    *i = k;
    *frac = span ? (uint16_t)(((uint32_t)(v - axis[k]) << 8) / span) : 0;
}

static int16_t lerp(int16_t a, int16_t b, uint16_t frac)
{
    int32_t r = (int32_t)a + (((int32_t)(b - a) * frac) >> 8);
    return (int16_t)r;
}

int16_t table1d(const uint16_t *x_axis, const int16_t *values, uint8_t n, uint16_t x)
{
    if (n == 1) return values[0];
    uint8_t i; uint16_t f;
    locate(x_axis, n, x, &i, &f);
    return lerp(values[i], values[i + 1], f);
}

int16_t table2d(const uint16_t *x_axis, uint8_t nx,
                const uint16_t *y_axis, uint8_t ny,
                const int16_t *values, uint16_t x, uint16_t y)
{
    uint8_t ix, iy; uint16_t fx, fy;
    locate(x_axis, nx, x, &ix, &fx);
    locate(y_axis, ny, y, &iy, &fy);
    int16_t a = lerp(values[iy * nx + ix],       values[iy * nx + ix + 1],       fx);
    int16_t b = lerp(values[(iy + 1) * nx + ix], values[(iy + 1) * nx + ix + 1], fx);
    return lerp(a, b, fy);
}
