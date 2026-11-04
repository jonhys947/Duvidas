#include "ignition.h"
#include "table.h"

#define DEG10_REV 3600u
static const uint16_t tdc_deg10[IGN_CHANNELS] = {0u, 1800u};

void ignition_init(ignition_t *ig)
{
    ig->adv_deg10 = 0;
    ig->dwell_us = 0;
    ig->rev_cut = 0;
    for (int i = 0; i < IGN_CHANNELS; i++) {
        ig->charging[i] = 0;
        ig->dwell_start_us[i] = 0;
        ig->last_spark_us[i] = 0;
    }
}

void ignition_update(ignition_t *ig, const config_t *c, uint16_t rpm, uint16_t map_kpa, int16_t iat_c)
{
    int16_t adv;
    if (rpm < c->crank_rpm) {
        adv = c->crank_adv_deg10;
    } else {
        adv = table2d(c->adv_rpm, ADV_NX, c->adv_map_kpa, ADV_NY, c->adv, rpm, map_kpa);
        int16_t t = iat_c + 40;
        if (t < 0) t = 0;
        adv = (int16_t)(adv + table1d(c->iatc_c, c->iatc_deg10, IATC_N, (uint16_t)t));
    }
    if (adv < -100) adv = -100;     /* limites de segurança: -10° .. +50° */
    if (adv > 500)  adv = 500;
    ig->adv_deg10 = adv;

    int32_t dw = table1d(c->dwell_rpm, c->dwell_us, DWELL_N, rpm);
    if (dw > c->max_dwell_us) dw = c->max_dwell_us;
    /* cada canal dispara uma vez por volta: o dwell não pode comer o tempo mínimo desligado */
    if (rpm > 0) {
        int32_t rev_us = 60000000L / rpm;
        int32_t room = rev_us - c->min_spark_off_us;
        if (dw > room) dw = room;
    }
    if (dw < 0) dw = 0;
    ig->dwell_us = (uint16_t)dw;

    if (rpm >= c->rev_limit_rpm) ig->rev_cut = 1;
    else if (rpm + c->rev_hyst_rpm < c->rev_limit_rpm) ig->rev_cut = 0;
}

uint16_t ignition_tooth_angle(const config_t *c, const decoder_t *d)
{
    return (uint16_t)((c->tooth0_deg10 + (uint32_t)d->tooth * DEC_TOOTH_DEG10) % DEG10_REV);
}

static uint16_t wrap(int32_t a)
{
    a %= (int32_t)DEG10_REV;
    if (a < 0) a += DEG10_REV;
    return (uint16_t)a;
}

uint8_t ignition_on_tooth(const ignition_t *ig, const config_t *c, const decoder_t *d,
                          uint32_t t_us, ign_event_t out[4])
{
    if (!d->synced || d->period_us == 0) return 0;

    uint16_t now = ignition_tooth_angle(c, d);
    uint16_t span = decoder_next_edge_deg10(d);
    uint32_t per = d->period_us;                   /* µs por 6° (60 décimos) */
    int32_t dwell_deg10 = (int32_t)((uint32_t)ig->dwell_us * DEC_TOOTH_DEG10 / per);
    uint8_t n = 0;

    for (uint8_t ch = 0; ch < IGN_CHANNELS; ch++) {
        uint16_t spark = wrap((int32_t)tdc_deg10[ch] - ig->adv_deg10);
        uint16_t on    = wrap((int32_t)spark - dwell_deg10);

        if (!ig->rev_cut && ig->dwell_us > 0) {
            uint16_t delta = wrap((int32_t)on - now);
            if (delta < span) {
                out[n].ch = ch; out[n].action = IGN_DWELL_ON;
                out[n].at_us = t_us + (uint32_t)delta * per / DEC_TOOTH_DEG10;
                n++;
            }
        }
        uint16_t delta = wrap((int32_t)spark - now);
        if (delta < span) {
            out[n].ch = ch; out[n].action = IGN_SPARK;
            out[n].at_us = t_us + (uint32_t)delta * per / DEC_TOOTH_DEG10;
            n++;
        }
    }
    return n;
}

uint8_t ignition_output(ignition_t *ig, const config_t *c, uint8_t ch, uint8_t action, uint32_t now_us)
{
    if (ch >= IGN_CHANNELS) return 0;
    if (action == IGN_DWELL_ON) {
        if (ig->charging[ch]) return 0;
        if ((uint32_t)(now_us - ig->last_spark_us[ch]) < c->min_spark_off_us &&
            ig->last_spark_us[ch] != 0)
            return 0;
        ig->charging[ch] = 1;
        ig->dwell_start_us[ch] = now_us;
        return 1;
    }
    if (!ig->charging[ch]) return 0;     /* centelha sem carga: ignora */
    ig->charging[ch] = 0;
    ig->last_spark_us[ch] = now_us;
    return 1;
}

uint8_t ignition_watchdog(ignition_t *ig, const config_t *c, uint32_t now_us)
{
    uint8_t cut = 0;
    for (uint8_t ch = 0; ch < IGN_CHANNELS; ch++) {
        if (ig->charging[ch] &&
            (uint32_t)(now_us - ig->dwell_start_us[ch]) > (uint32_t)c->max_dwell_us + 200u) {
            ig->charging[ch] = 0;
            ig->last_spark_us[ch] = now_us;
            cut |= (uint8_t)(1u << ch);
        }
    }
    return cut;
}
