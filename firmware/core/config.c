#include "config.h"
#include <string.h>
#include <stddef.h>

static uint16_t crc16_ccitt(const uint8_t *p, uint32_t n)
{
    uint16_t crc = 0xFFFF;
    while (n--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t config_crc(const config_t *c)
{
    return crc16_ccitt((const uint8_t *)c, (uint32_t)offsetof(config_t, crc));
}

int config_valid(const config_t *c)
{
    return c->magic == CFG_MAGIC && c->version == CFG_VERSION &&
           c->length == sizeof(config_t) && c->crc == config_crc(c);
}

void config_seal(config_t *c)
{
    c->crc = config_crc(c);
}

void config_defaults(config_t *c)
{
    static const uint16_t rpm[ADV_NX] = {500, 800, 1000, 1250, 1500, 2000, 2500, 3000,
                                         3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000};
    static const uint16_t kpa[ADV_NY] = {20, 30, 40, 50, 60, 70, 80, 90, 100, 120, 150, 200};

    memset(c, 0, sizeof(*c));
    c->magic = CFG_MAGIC;
    c->version = CFG_VERSION;
    c->length = sizeof(config_t);

    c->tooth0_deg10 = 3600 - 1200;   /* exemplo: PMS do cil. 1 no 20º dente após a falha; AJUSTAR no motor */
    c->coil_type = COIL_SMART_5V;

    c->rev_limit_rpm = 6800;
    c->rev_hyst_rpm = 200;
    c->crank_rpm = 400;
    c->crank_adv_deg10 = 100;        /* 10° na partida */
    c->max_dwell_us = 4800;          /* abaixo do limitador de hardware (6 ms ±10%) */
    c->min_spark_off_us = 1000;

    memcpy(c->adv_rpm, rpm, sizeof(rpm));
    memcpy(c->adv_map_kpa, kpa, sizeof(kpa));
    /* mapa conservador: 10° em marcha lenta subindo até ~32° em vácuo alto e rotação alta,
       retirando avanço com carga. Só para bancada; NÃO é um mapa de motor. */
    for (int iy = 0; iy < ADV_NY; iy++) {
        for (int ix = 0; ix < ADV_NX; ix++) {
            int base = 100 + ix * 15;              /* 10,0° .. 32,5° */
            int load = (kpa[iy] > 60) ? (kpa[iy] - 60) / 2 : 0;
            int v = base - load;
            if (v < 50) v = 50;
            c->adv[iy * ADV_NX + ix] = (int16_t)v;
        }
    }

    static const uint16_t iat_c[IATC_N] = {0, 20, 40, 60, 80, 100, 120, 140}; /* -40..100 °C */
    static const int16_t  iat_d[IATC_N] = {20, 15, 10, 0, 0, -10, -20, -30};
    memcpy(c->iatc_c, iat_c, sizeof(iat_c));
    memcpy(c->iatc_deg10, iat_d, sizeof(iat_d));

    /* dwell por tensão (bobina típica; AJUSTAR conforme a bobina usada) */
    static const uint16_t dv_mv[DWELLV_N] = {6000, 8000, 9000, 10000, 11000, 12000, 13500, 15000};
    static const int16_t  dv_us[DWELLV_N] = {4800, 4800, 4400, 3900, 3500, 3200, 2800, 2500};
    memcpy(c->dwell_mv, dv_mv, sizeof(dv_mv));
    memcpy(c->dwell_us_v, dv_us, sizeof(dv_us));
    /* teto por rotação (em rotação alta não há tempo nem necessidade de dwell longo) */
    static const uint16_t dw_rpm[DWELL_N] = {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000};
    static const int16_t  dw_us[DWELL_N]  = {4800, 4800, 4800, 4500, 4200, 4000, 3800, 3600};
    memcpy(c->dwell_rpm, dw_rpm, sizeof(dw_rpm));
    memcpy(c->dwell_us, dw_us, sizeof(dw_us));
    c->vbat_default_mv = 12000;   /* conservador: dwell um pouco maior que a 13,5 V */

    /* MAP da tabela FuelTech: 0,2 V = -0,85 bar (≈ 16 kPa abs), 4,7 V = 6 bar (≈ 701 kPa abs) */
    c->map_mv0 = 200;  c->map_kpa0 = 16;
    c->map_mv1 = 4700; c->map_kpa1 = 701;

    c->iat_r25_ohm = 2800;   /* modelo ajustado do MTE; substituir pelo valor medido */
    c->iat_beta = 4067;

    config_seal(c);
}
