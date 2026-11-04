/* Decodificador de roda fônica 60-2 (58 dentes, 2 faltando, 6° por dente). */
#ifndef DECODER_H
#define DECODER_H

#include <stdint.h>

#define DEC_TEETH_TOTAL   60u
#define DEC_TEETH_REAL    58u
#define DEC_TOOTH_DEG10   60u   /* 6,0° em décimos de grau */

typedef enum {
    DEC_EV_NONE        = 0,
    DEC_EV_TOOTH       = 1u << 0,  /* dente válido processado */
    DEC_EV_SYNC_GAINED = 1u << 1,
    DEC_EV_SYNC_LOST   = 1u << 2,
    DEC_EV_NOISE       = 1u << 3,  /* borda rejeitada como ruído */
} dec_event_t;

typedef struct {
    uint8_t  synced;        /* 1 = posição angular conhecida */
    uint8_t  tooth;         /* índice do dente atual, 0 = primeiro após a falha */
    uint8_t  have_gap;      /* já viu uma falha desde o último reset */
    uint8_t  edges;         /* bordas válidas recebidas (satura em 255) */
    uint32_t last_t_us;     /* instante da última borda válida */
    uint32_t period_us;     /* intervalo de um dente normal (6°) */
    uint32_t rev_count;     /* voltas completas desde a sincronização */
    uint32_t sync_losses;   /* perdas de sincronismo acumuladas */
} decoder_t;

void        decoder_init(decoder_t *d);
/* Chamar a cada borda do sinal do sensor, com o instante em µs (32 bits, pode dar a volta). */
dec_event_t decoder_on_edge(decoder_t *d, uint32_t t_us);
/* Chamar periodicamente: perde o sincronismo se nenhuma borda chegar dentro do limite. */
dec_event_t decoder_check_timeout(decoder_t *d, uint32_t now_us);
/* Rotação em rpm (0 se sem sincronismo ou parado). */
uint16_t    decoder_rpm(const decoder_t *d);
/* Espaçamento até a próxima borda real, em décimos de grau (60 normal, 180 antes da falha). */
uint16_t    decoder_next_edge_deg10(const decoder_t *d);

#endif
