/* Configuração e mapas, gravados na 24C256 (endereço 0x50). */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CFG_MAGIC    0x4D415253u   /* "MARS" */
#define CFG_VERSION  1u

#define ADV_NX 16   /* eixo de rotação */
#define ADV_NY 12   /* eixo de MAP */
#define DWELL_N 8
#define IATC_N  8

typedef enum {
    COIL_SMART_5V  = 0,   /* bobina inteligente ou placa IGBT: SEL_5V = 1 */
    COIL_SMART_12V = 1,   /* bobina inteligente com entrada de 12 V: SEL_5V = 0 */
} coil_type_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t length;              /* sizeof(config_t) */

    /* roda fônica e motor */
    uint16_t tooth0_deg10;        /* ângulo do dente 0 (1º após a falha) em décimos de grau após o PMS do cil. 1 */
    uint8_t  coil_type;           /* coil_type_t */
    uint8_t  reserved0;

    /* limites */
    uint16_t rev_limit_rpm;       /* corte total acima disso */
    uint16_t rev_hyst_rpm;        /* volta a acender abaixo de rev_limit - hyst */
    uint16_t crank_rpm;           /* abaixo disso, modo partida */
    int16_t  crank_adv_deg10;     /* avanço fixo na partida */
    uint16_t max_dwell_us;        /* teto de software, abaixo do limitador de hardware (~6 ms) */
    uint16_t min_spark_off_us;    /* tempo mínimo desligado entre faíscas do mesmo canal */

    /* avanço: [ny][nx] em décimos de grau APMS */
    uint16_t adv_rpm[ADV_NX];
    uint16_t adv_map_kpa[ADV_NY];
    int16_t  adv[ADV_NY * ADV_NX];

    /* correção de avanço por IAT (décimos de grau, somada) */
    uint16_t iatc_c[IATC_N];      /* °C + 40 (para caber em unsigned) */
    int16_t  iatc_deg10[IATC_N];

    /* dwell em µs por rotação (a placa não mede a tensão da bateria) */
    uint16_t dwell_rpm[DWELL_N];
    int16_t  dwell_us[DWELL_N];

    /* sensor MAP: reta entre dois pontos de tensão no pino do sensor (mV) */
    uint16_t map_mv0, map_kpa0;
    uint16_t map_mv1, map_kpa1;

    /* NTC do IAT */
    uint16_t iat_r25_ohm;
    uint16_t iat_beta;

    uint16_t crc;                 /* CRC-16/CCITT de todos os bytes anteriores */
} config_t;

void     config_defaults(config_t *c);
uint16_t config_crc(const config_t *c);
/* 1 se magic, versão, tamanho e CRC conferem */
int      config_valid(const config_t *c);
void     config_seal(config_t *c);   /* recalcula o CRC */

#endif
