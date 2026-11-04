/* Testes do núcleo no PC: simula a roda 60-2 e confere sincronismo, rotação e ângulo das centelhas. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "decoder.h"
#include "ignition.h"
#include "config.h"
#include "table.h"
#include "sensors.h"

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

/* ---- motor simulado: aceleração angular constante ---- */
typedef struct { double rpm0, rpm1, t_ramp_us; } profile_t;

/* ângulo em décimos de grau desde t=0 */
static double sim_angle(const profile_t *p, double t)
{
    double w0 = p->rpm0 * 6e-5, w1 = p->rpm1 * 6e-5;   /* décimos de grau por µs */
    if (t <= p->t_ramp_us) {
        double a = (p->t_ramp_us > 0) ? (w1 - w0) / p->t_ramp_us : 0;
        return w0 * t + 0.5 * a * t * t;
    }
    double th = w0 * p->t_ramp_us + 0.5 * (w1 - w0) * p->t_ramp_us;
    return th + w1 * (t - p->t_ramp_us);
}
static double sim_time_of(const profile_t *p, double theta)
{
    double lo = 0, hi = 1e9;
    for (int i = 0; i < 100; i++) { double m = (lo + hi) / 2; if (sim_angle(p, m) < theta) lo = m; else hi = m; }
    return (lo + hi) / 2;
}

typedef struct { ign_event_t ev; int used; } pend_t;

typedef struct {
    int sparks, dwells, sync_rev;
    double max_err_deg, max_dwell_err;
    int cut_seen;
} result_t;

/* Roda a simulação por n_rev voltas; opcional: injeta ruído ou remove um dente na volta indicada. */
static result_t run(const config_t *c, profile_t p, int n_rev, uint16_t map_kpa,
                    int drop_rev, int noise_rev, double start_deg10)
{
    decoder_t d; ignition_t ig;
    decoder_init(&d); ignition_init(&ig);
    result_t r = {0, 0, -1, 0, 0, 0};
    pend_t pend[64]; int np = 0;
    double on_t[2] = {-1, -1};

    /* lista de bordas: dente k em tooth0 + 60k (k = 0..57) a cada volta, a partir de start */
    for (int rev = 0; rev < n_rev; rev++) {
        for (int k = 0; k < 58; k++) {
            if (rev == drop_rev && k == 30) continue;
            double theta = rev * 3600.0 + c->tooth0_deg10 + k * 60.0 - start_deg10;
            if (theta < 0) continue;
            double t = sim_time_of(&p, theta);

            /* executa eventos pendentes antes desta borda, em ordem de tempo */
            for (;;) {
                int best = -1;
                for (int i = 0; i < np; i++)
                    if (!pend[i].used && (double)pend[i].ev.at_us <= t &&
                        (best < 0 || pend[i].ev.at_us < pend[best].ev.at_us)) best = i;
                if (best < 0) break;
                pend[best].used = 1;
                ign_event_t *e = &pend[best].ev;
                if (ignition_output(&ig, c, e->ch, e->action, e->at_us)) {
                    double ang = fmod(sim_angle(&p, e->at_us) + start_deg10, 3600.0);
                    if (e->action == IGN_DWELL_ON) { on_t[e->ch] = e->at_us; r.dwells++; }
                    else {
                        double want = fmod(3600.0 + (e->ch ? 1800.0 : 0.0) - ig.adv_deg10, 3600.0);
                        double err = fabs(ang - want); if (err > 1800) err = 3600 - err;
                        if (r.sync_rev >= 0 && err / 10.0 > r.max_err_deg) r.max_err_deg = err / 10.0;
                        if (on_t[e->ch] >= 0) {
                            double dw = e->at_us - on_t[e->ch];
                            double de = fabs(dw - ig.dwell_us) / (ig.dwell_us ? ig.dwell_us : 1);
                            if (de > r.max_dwell_err) r.max_dwell_err = de;
                        }
                        r.sparks++;
                    }
                }
            }
            int j = 0; for (int i = 0; i < np; i++) if (!pend[i].used) pend[j++] = pend[i]; np = j;

            if (rev == noise_rev && k == 10) {   /* borda espúria logo após um dente */
                double tn = sim_time_of(&p, theta - 40.0);
                decoder_on_edge(&d, (uint32_t)tn);
            }

            dec_event_t ev = decoder_on_edge(&d, (uint32_t)t);
            if ((ev & DEC_EV_SYNC_GAINED) && r.sync_rev < 0) r.sync_rev = rev;
            if (ev & DEC_EV_TOOTH) {
                ignition_update(&ig, c, decoder_rpm(&d), map_kpa, 25, 13500);
                if (ig.rev_cut) r.cut_seen = 1;
                ign_event_t out[4];
                uint8_t n = ignition_on_tooth(&ig, c, &d, (uint32_t)t, out);
                for (uint8_t i = 0; i < n && np < 64; i++) { pend[np].ev = out[i]; pend[np].used = 0; np++; }
            }
        }
    }
    return r;
}

static void test_steady(const config_t *c)
{
    const double rpms[] = {800, 1000, 3000, 6000, 7900};
    for (unsigned i = 0; i < sizeof rpms / sizeof rpms[0]; i++) {
        profile_t p = {rpms[i], rpms[i], 0};
        result_t r = run(c, p, 20, 50, -1, -1, 0);
        printf("rpm %5.0f: sinc na volta %d, centelhas %d, erro máx %.2f°, erro dwell %.1f%%\n",
               rpms[i], r.sync_rev, r.sparks, r.max_err_deg, r.max_dwell_err * 100);
        CHECK(r.sync_rev >= 0 && r.sync_rev <= 2, "sincronismo demorou (%d) a %.0f rpm", r.sync_rev, rpms[i]);
        CHECK(r.sparks >= 2 * 15, "poucas centelhas (%d) a %.0f rpm", r.sparks, rpms[i]);
        CHECK(r.max_err_deg < 0.3, "erro de ângulo %.2f° a %.0f rpm", r.max_err_deg, rpms[i]);
        CHECK(r.max_dwell_err < 0.03, "erro de dwell %.1f%% a %.0f rpm", r.max_dwell_err * 100, rpms[i]);
    }
}

static void test_rpm_reading(const config_t *c)
{
    decoder_t d; decoder_init(&d);
    profile_t p = {3000, 3000, 0};
    for (int rev = 0; rev < 4; rev++)
        for (int k = 0; k < 58; k++)
            decoder_on_edge(&d, (uint32_t)sim_time_of(&p, rev * 3600.0 + c->tooth0_deg10 + k * 60.0));
    uint16_t rpm = decoder_rpm(&d);
    printf("leitura de rpm a 3000: %u\n", rpm);
    CHECK(d.synced && abs((int)rpm - 3000) <= 30, "rpm lida %u", rpm);
}

static void test_cranking(const config_t *c)
{
    profile_t p = {150, 900, 1.0e6};
    result_t r = run(c, p, 12, 100, -1, -1, 0);
    printf("partida 150->900 rpm: sinc na volta %d, centelhas %d, erro máx %.2f°\n", r.sync_rev, r.sparks, r.max_err_deg);
    CHECK(r.sync_rev >= 0 && r.sync_rev <= 2, "sem sincronismo na partida");
    CHECK(r.max_err_deg < 3.0, "erro de ângulo na partida %.2f°", r.max_err_deg);
}

static void test_faults(const config_t *c)
{
    profile_t p = {2000, 2000, 0};
    result_t r = run(c, p, 12, 50, 5, -1, 0);
    printf("dente perdido na volta 5: centelhas %d, erro máx %.2f°\n", r.sparks, r.max_err_deg);
    CHECK(r.max_err_deg < 0.3, "centelha fora do lugar depois de dente perdido (%.2f°)", r.max_err_deg);

    r = run(c, p, 12, 50, -1, 5, 0);
    printf("ruído na volta 5: centelhas %d, erro máx %.2f°\n", r.sparks, r.max_err_deg);
    CHECK(r.max_err_deg < 0.3, "centelha fora do lugar depois de ruído (%.2f°)", r.max_err_deg);

    r = run(c, p, 12, 50, -1, -1, 1234.0);
    printf("partida em ângulo qualquer: sinc na volta %d, erro máx %.2f°\n", r.sync_rev, r.max_err_deg);
    CHECK(r.sync_rev >= 0 && r.sync_rev <= 2, "sem sincronismo começando no meio da roda");
}

static void test_rev_limit(const config_t *c)
{
    profile_t p = {8300, 8300, 0};
    result_t r = run(c, p, 10, 50, -1, -1, 0);
    printf("corte a 8300 rpm: dwells %d, centelhas %d\n", r.dwells, r.sparks);
    CHECK(r.cut_seen && r.dwells == 0, "limitador não cortou (%d dwells)", r.dwells);
}

static void test_dwell_voltage(const config_t *c)
{
    ignition_t ig; ignition_init(&ig);
    ignition_update(&ig, c, 2000, 50, 25, 13500);
    uint16_t d135 = ig.dwell_us;
    ignition_update(&ig, c, 2000, 50, 25, 9000);
    uint16_t d9 = ig.dwell_us;
    ignition_update(&ig, c, 2000, 50, 25, 0);
    uint16_t dnone = ig.dwell_us;
    ignition_update(&ig, c, 6500, 50, 25, 8000);
    uint16_t dhi = ig.dwell_us;
    printf("dwell: %u us a 13,5 V, %u us a 9 V, %u us sem leitura, %u us a 6500 rpm/8 V\n", d135, d9, dnone, dhi);
    CHECK(d135 == 2800 && d9 == 4400, "dwell por tensão (%u, %u)", d135, d9);
    CHECK(dnone == 3200, "dwell sem leitura de tensão (%u)", dnone);
    CHECK(dhi <= 3700, "teto por rotação não aplicado (%u)", dhi);
    CHECK(d9 <= c->max_dwell_us, "dwell acima do máximo");
}

static void test_watchdog(const config_t *c)
{
    ignition_t ig; ignition_init(&ig);
    ignition_output(&ig, c, 0, IGN_DWELL_ON, 1000);
    CHECK(ignition_watchdog(&ig, c, 1000 + c->max_dwell_us) == 0, "watchdog cortou cedo");
    CHECK(ignition_watchdog(&ig, c, 1000 + c->max_dwell_us + 500) == 1, "watchdog não cortou");
}

static void test_tables_config_sensors(void)
{
    static const uint16_t x[3] = {0, 100, 200};
    static const int16_t v[3] = {0, 100, 300};
    CHECK(table1d(x, v, 3, 50) == 50, "table1d meio");
    CHECK(table1d(x, v, 3, 150) == 200, "table1d segundo trecho");
    CHECK(table1d(x, v, 3, 999) == 300, "table1d satura");

    config_t c; config_defaults(&c);
    CHECK(config_valid(&c), "config padrão inválida");
    c.adv[0]++;
    CHECK(!config_valid(&c), "CRC não detectou mudança");
    config_seal(&c);
    CHECK(config_valid(&c), "config após seal");

    config_defaults(&c);
    /* 2,5 V no sensor -> 1,5 V no pino -> adc ≈ 1861 ; esperado ≈ 16 + (2300/4500)*685 ≈ 366 kPa */
    uint16_t kpa = sensor_map_kpa(&c, 1861);
    printf("MAP com 2,5 V no sensor: %u kPa\n", kpa);
    CHECK(kpa >= 360 && kpa <= 372, "MAP %u kPa", kpa);
    /* NTC com R = R25 (2800 Ω): V = 3,3*2800/(2800+2490) -> adc ≈ 2167 -> 25 °C */
    int16_t t = sensor_iat_c(&c, 2167);
    printf("IAT com R = R25: %d °C\n", t);
    CHECK(t >= 24 && t <= 26, "IAT %d", t);
}

int main(void)
{
    config_t c; config_defaults(&c);
    test_tables_config_sensors();
    test_rpm_reading(&c);
    test_steady(&c);
    test_cranking(&c);
    test_faults(&c);
    test_rev_limit(&c);
    test_dwell_voltage(&c);
    test_watchdog(&c);
    printf("\n%d verificações, %d falhas\n", checks, fails);
    return fails ? 1 : 0;
}
