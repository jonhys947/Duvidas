/* Conversão das leituras do ADC (12 bits, referência 3,3 V) em grandezas físicas. */
#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include "config.h"

#define ADC_FULL      4095u
#define ADC_VREF_MV   3300u
/* MAP: divisor R3 10k / R4 15k no esquemático -> pino = sensor * 0,6 */
#define MAP_DIV_NUM   3u
#define MAP_DIV_DEN   5u
/* IAT: pull-up R25 = 2,49 kΩ para 3,3 V */
#define IAT_PULLUP_OHM 2490u

uint16_t sensor_map_kpa(const config_t *c, uint16_t adc);
/* temperatura em °C; retorna -40 ou 150 se o sensor estiver em curto/aberto */
int16_t  sensor_iat_c(const config_t *c, uint16_t adc);

#endif
