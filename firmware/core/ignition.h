/* Cálculo de avanço/dwell e agendamento dos eventos das duas saídas (centelha perdida). */
#ifndef IGNITION_H
#define IGNITION_H

#include <stdint.h>
#include "config.h"
#include "decoder.h"

/* Saída A: cilindros 1/4 (PMS em 0°). Saída B: cilindros 3/2 (PMS em 180°), ordem 1-3-4-2. */
#define IGN_CHANNELS 2

typedef enum { IGN_SPARK = 0, IGN_DWELL_ON = 1 } ign_action_t;

typedef struct {
    uint8_t  ch;        /* 0 = A, 1 = B */
    uint8_t  action;    /* ign_action_t */
    uint32_t at_us;     /* instante absoluto (mesmo relógio do decoder) */
} ign_event_t;

typedef struct {
    /* calculados em ignition_update() */
    int16_t  adv_deg10;
    uint16_t dwell_us;
    uint8_t  rev_cut;
    /* estado por canal, atualizado em ignition_output() */
    uint8_t  charging[IGN_CHANNELS];
    uint32_t dwell_start_us[IGN_CHANNELS];
    uint32_t last_spark_us[IGN_CHANNELS];
} ignition_t;

void    ignition_init(ignition_t *ig);
/* Recalcula avanço e dwell (chamar no laço principal, p.ex. a cada 5 ms).
   vbat_mv = 0 quando a tensão não estiver disponível (usa config.vbat_default_mv). */
void    ignition_update(ignition_t *ig, const config_t *c, uint16_t rpm, uint16_t map_kpa,
                        int16_t iat_c, uint16_t vbat_mv);
/* Chamar a cada dente válido. Preenche até 4 eventos que caem antes da próxima borda; retorna quantos. */
uint8_t ignition_on_tooth(const ignition_t *ig, const config_t *c, const decoder_t *d,
                          uint32_t t_us, ign_event_t out[4]);
/* A HAL chama ao executar um evento; devolve 1 se o evento foi aplicado, 0 se foi descartado. */
uint8_t ignition_output(ignition_t *ig, const config_t *c, uint8_t ch, uint8_t action, uint32_t now_us);
/* Chamar periodicamente: força a centelha se algum canal passar do dwell máximo. Retorna máscara de canais cortados. */
uint8_t ignition_watchdog(ignition_t *ig, const config_t *c, uint32_t now_us);
/* Ângulo da borda do dente atual em décimos de grau após o PMS do cil. 1 */
uint16_t ignition_tooth_angle(const config_t *c, const decoder_t *d);

#endif
