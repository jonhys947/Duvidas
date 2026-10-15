#!/usr/bin/env python3
"""Tabela de calibração NTC + divisor (R17) + ADS1115, com orçamento de erro.

A curva R(T) usa o modelo Beta:  R = R25 * exp(BETA * (1/T - 1/298.15)).
Os valores padrão de R25/BETA foram ajustados por mínimos quadrados aos 15
pontos da tabela original e NÃO são do datasheet do MTE 4053. Substitua por
--r25/--beta, ou ajuste a partir de medições reais com --points.

Exemplos:
  python3 ntc_table.py
  python3 ntc_table.py --points 0:9000,25:2500,100:190,150:52
  python3 ntc_table.py --r25 2500 --beta 3900 --csv tabela.csv
"""
import argparse
import math

VCC = 3.3           # V, trilho do divisor
R_PULL = 220.0      # R17, ohm
R_SERIES = 4700.0   # R18, ohm
LSB = 4.096 / 32768  # V por contagem, ADS1115 com PGA = 1 (+/-4,096 V)
LEAK_A = 1e-6       # fuga reversa assumida dos BAT42, A (ver datasheet)
TABLE_TEMPS = [0, 20, 25, 40, 60, 80, 100, 110, 120, 130, 140, 150, 160, 170, 180]

DEFAULT_R25 = 2806.0  # ohm, ajuste aos pontos originais
DEFAULT_BETA = 4067.0  # K


def fit_beta(points):
    """Ajuste Beta por mínimos quadrados em ln(R) x 1/T. Retorna (r25, beta)."""
    xs = [1 / (t + 273.15) for t, _ in points]
    ys = [math.log(r) for _, r in points]
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    beta = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    r25 = math.exp(my - beta * mx + beta / 298.15)
    return r25, beta


def row(t_c, r25, beta):
    tk = t_c + 273.15
    r = r25 * math.exp(beta * (1 / tk - 1 / 298.15))
    ratio = r / (r + R_PULL)
    v = VCC * ratio
    i = VCC / (r + R_PULL)
    dv_dt = VCC * R_PULL * r / (r + R_PULL) ** 2 * beta / tk ** 2  # V/K
    rail_err = 0.01 * v / dv_dt                                    # 1% no trilho
    r17_err = 0.01 * VCC * r * R_PULL / (r + R_PULL) ** 2 / dv_dt  # 1% em R17
    leak_err = LEAK_A * (R_SERIES + R_PULL * r / (r + R_PULL)) / dv_dt
    return {
        "t": t_c, "r": r, "ratio": ratio, "v": v, "raw": v / LSB,
        "i_ma": i * 1e3, "p_mw": i * i * r * 1e3,
        "lsb_per_c": dv_dt / LSB, "c_per_lsb": LSB / dv_dt,
        "rail_err": rail_err, "r17_err": r17_err, "leak_err": leak_err,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--r25", type=float, default=DEFAULT_R25)
    ap.add_argument("--beta", type=float, default=DEFAULT_BETA)
    ap.add_argument("--points", help="pares T:R (°C:ohm) separados por vírgula; ajusta r25/beta")
    ap.add_argument("--csv", help="grava CSV de 5 em 5 °C (0 a 180 °C) neste caminho")
    args = ap.parse_args()

    r25, beta = args.r25, args.beta
    if args.points:
        pts = [(float(a), float(b)) for a, b in (p.split(":") for p in args.points.split(","))]
        r25, beta = fit_beta(pts)
    print(f"Modelo: R25 = {r25:.0f} ohm, BETA = {beta:.0f} K | R17 = {R_PULL:.0f} ohm, "
          f"VCC = {VCC} V, LSB = {LSB * 1e6:.0f} uV\n")

    rows = [row(t, r25, beta) for t in TABLE_TEMPS]
    print("| T (°C) | R (Ω) | R/(R+R17) | V @3,300 V | RAW | I (mA) | P sensor (mW) | LSB/°C |")
    print("|---:|---:|---:|---:|---:|---:|---:|---:|")
    for x in rows:
        print(f"| {x['t']} | {x['r']:.0f} | {x['ratio']:.4f} | {x['v']:.3f} | {x['raw']:.0f} | "
              f"{x['i_ma']:.1f} | {x['p_mw']:.1f} | {x['lsb_per_c']:.0f} |")
    print("\n| T (°C) | trilho ±1% | R17 ±1% | fuga 1 µA | °C por LSB |")
    print("|---:|---:|---:|---:|---:|")
    for x in rows:
        print(f"| {x['t']} | {x['rail_err']:.2f} | {x['r17_err']:.2f} | {x['leak_err']:.2f} | {x['c_per_lsb']:.4f} |")

    if args.csv:
        with open(args.csv, "w") as f:
            f.write("t_c,r_ohm,ratio,raw_at_3v3\n")
            for t in range(0, 181, 5):
                x = row(t, r25, beta)
                f.write(f"{t},{x['r']:.1f},{x['ratio']:.5f},{x['raw']:.0f}\n")


if __name__ == "__main__":
    main()
