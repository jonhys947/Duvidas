#include "sensors.h"
#include <math.h>

uint16_t sensor_map_kpa(const config_t *c, uint16_t adc)
{
    uint32_t pin_mv = (uint32_t)adc * ADC_VREF_MV / ADC_FULL;
    uint32_t sensor_mv = pin_mv * MAP_DIV_DEN / MAP_DIV_NUM;
    int32_t dmv = (int32_t)c->map_mv1 - (int32_t)c->map_mv0;
    if (dmv == 0) return c->map_kpa0;
    int32_t kpa = (int32_t)c->map_kpa0 +
                  ((int32_t)sensor_mv - (int32_t)c->map_mv0) *
                  ((int32_t)c->map_kpa1 - (int32_t)c->map_kpa0) / dmv;
    if (kpa < 0) kpa = 0;
    if (kpa > 1000) kpa = 1000;
    return (uint16_t)kpa;
}

int16_t sensor_iat_c(const config_t *c, uint16_t adc)
{
    if (adc < 20)  return 150;   /* NTC em curto */
    if (adc > ADC_FULL - 20) return -40;  /* NTC aberto */
    /* V = 3,3 * R / (R + Rpu)  ->  R = Rpu * adc / (FULL - adc) */
    float r = (float)IAT_PULLUP_OHM * (float)adc / (float)(ADC_FULL - adc);
    float inv_t = 1.0f / 298.15f + logf(r / (float)c->iat_r25_ohm) / (float)c->iat_beta;
    float t = 1.0f / inv_t - 273.15f;
    if (t < -40.0f) t = -40.0f;
    if (t > 150.0f) t = 150.0f;
    return (int16_t)lrintf(t);
}
