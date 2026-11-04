/* Interpolação linear em tabelas 1D e 2D com eixos crescentes. */
#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>

int16_t table1d(const uint16_t *x_axis, const int16_t *values, uint8_t n, uint16_t x);
/* values em ordem de linhas: values[iy * nx + ix] (y = linha, x = coluna) */
int16_t table2d(const uint16_t *x_axis, uint8_t nx,
                const uint16_t *y_axis, uint8_t ny,
                const int16_t *values, uint16_t x, uint16_t y);

#endif
