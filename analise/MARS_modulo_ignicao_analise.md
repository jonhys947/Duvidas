# MARS – Módulo de Ignição (CH32V203K8T6) – Análise de circuito e PCB

**Revisão analisada:** 3 (netlist/SVG `SCH_MARS_Modulo_Ignicao_2026-11-04`, Gerbers `PCB_MARS_Modulo_Ignicao_CH32V_v1.1` gerados às 12:03, Pick&Place `CH32V_v1.1`).
**MCU:** WCH CH32V203K8T6 (RISC-V, LQFP32). O título do esquemático ainda diz "STM32 IGNITION MODULE".

Legenda: ✅ confere · ⚠️ ressalva ou item a verificar · 🔒 decisão do projetista.

---

## 1. Resumo

O circuito está coerente e o layout corresponde à netlist: sem curtos e sem aberturas nos componentes verificáveis. A falha de projeto encontrada na primeira revisão (acoplamento entre as saídas A e B no modo 12 V) foi corrigida. O limitador de dwell foi redimensionado para 6 ms com componentes de precisão.

O que ainda depende de verificação na bancada ou de conferência física:

| # | Item | Situação |
|---|---|---|
| 1 | K real do 74HC123 (clone LX) e largura do limitador | ⚠️ medir na primeira placa e ajustar R27/R28 se preciso (seção 3.3) |
| 2 | Nível da saída no modo 5 V (≈ 4,8–5,4 V) | ⚠️ confirmar o máximo aceito pela entrada da bobina |
| 3 | Pinagem física do módulo U11 (AHT20+BMP280) e do conector U12 (Micro-Fit) | ⚠️ conferir contra a peça e o chicote |
| 4 | TVS do CAN: trocar o D1 por SMCJ8 | ⚠️ ainda não está nos arquivos; ver seção 5.2 |
| 5 | Hall sem histerese; `CKP_CAPTURE` com ≈ 50 mm de trilha em 4,7 kΩ; NPTH a ≈ 0,3 mm da borda | ⚠️ menores |

Decisões do projetista, que prevalecem sobre recomendações anteriores:

- 🔒 **D7 (SMCJ18A):** jump-start de 24 V e inversão de polaridade não existem na aplicação.
- 🔒 **D4/D5 (SMAJ18A):** mantidos.
- 🔒 **D13/D14 (MM3Z4V3, SOD-323):** mantidos.
- 🔒 **TVS do CAN:** será SMCJ8.
- 🔒 **BOM:** usado só para os nomes dos componentes; códigos de fabricante não foram avaliados.

---

## 2. Arquitetura

Módulo de ignição autônomo para motor de 4 cilindros em centelha perdida (duas saídas: cilindros 1-4 e 2-3).

```
Micro-Fit 12P ──┬─ 12V_IN ── SS56U ── +12V ── AP63205 (5 V) ── AMS1117 (3,3 V)
                ├─ VR+ ──► NCV1124 ──► PB3
                ├─ HALLIN ──► Q5/Q6 ──► PA15
                ├─ CKPIN ──► 4,7 kΩ + clamp ──► PA0
                ├─ MAPSIG ──► divisor 10k/15k ──► PA2 (ADC)
                ├─ IATIN ──► NTC com pull-up 2,49 kΩ ──► PA3 (ADC)
                └─ CANH/CANL ──► SN65HVD230 ──► PA11/PA12
CH32V203 ── PA6/PA7 (IGN_A/B) ─ AND PA1 (IGN_ENABLE) ─ AND janela 74HC123 ─ NPN+PNP ─ 1 kΩ ─ saída
```

O MCU lê a posição do virabrequim (VR ou Hall), MAP e IAT, calcula avanço e dwell e gera os dois pulsos. A saída é um **sinal de comando** (12 V ou ≈ 5 V, impedância de 1 kΩ, ≤ ≈ 11 mA), para bobinas inteligentes ou igniters externos. Não aciona bobina simples.

Pinagem do MCU conferida com o datasheet do CH32V203 (LQFP32): VDD 1/17, OSC 2/3, NRST 4, VDDA 5, PA0–PB1 6–15, VSS 16/32, PA8–PA15 18–25, PB3–PB7 26–30, BOOT0 31 ✅.

---

## 3. Cadeia de ignição e dwell

### 3.1 Caminho do sinal (canal A; o B é idêntico)

1. `PA6 (IGN_A)` AND `PA1 (IGN_ENABLE)` em U2 (74LVC2G08) → `IGN_A_ENABLED`. Pull-downs de 47 kΩ mantêm o canal desligado no reset ✅.
2. `IGN_A_ENABLED` passa por R29 4,7 kΩ + C28 1 nF (atraso ≈ 3 µs só no início do dwell) e dispara a entrada 1B do 74HC123 (U9), com 1A# em GND.
3. A saída 1Q (`DWELL_WINDOW_A`) fica em nível alto durante tW.
4. U10 (AND): `IGN_A_LIMITED = IGN_A_ENABLED AND DWELL_WINDOW_A`.
5. Q1 (BC817) aciona Q3 (BC807, PNP de alto lado). Saída `IGN_A_RAW`.
6. D2 (BAS21C) + R21 (1 kΩ) → `IGN_A_PROT` (conector). R22 10 kΩ mantém 0 V com o canal desligado; D5 (SMAJ18A) para surtos.

### 3.2 Modos de tensão e grampo da saída

`PA5 (SEL_5V)` liga o Q7:

- **Q7 desligado (modo 12 V):** saída ≈ 9–10 V com carga de 10 kΩ, até ≈ 11 V sem carga.
- **Q7 ligado (modo 5 V):** cada saída é grampeada por zener 4,3 V (D13/D14, MM3Z4V3) + diodo do D15 (BAS21C) + Vce(sat) do Q7 → **≈ 5,1 V típico, 4,8–5,4 V** com a faixa de Vz de 4,0–4,6 V.

Ligação atual: D13 e D14 com catodo em `IGN_x_PROT`; ânodos em pinos separados do D15 (pinos 1 e 2, ânodos); pino 3 (catodo comum) no coletor do Q7. Com o Q7 desligado, o diodo do canal desligado fica em reversa e **bloqueia o acoplamento** entre as saídas (simulação DC: canal desligado ≈ 0 V para qualquer impedância de entrada da bobina) ✅.

Pinagem do BAS21C (pinos 1 e 2 ânodos, 3 catodo comum) confirmada pelo datasheet SXSEMI com marcação JS3, a mesma do código BAS21C-JS3 ✅.

### 3.3 Limitador de dwell (tW nominal = 6 ms)

**Função.** A saída só fica ligada enquanto o MCU **e** a janela do 74HC123 estão em nível alto. Se o MCU desce o sinal antes de tW (caso normal), o limitador não atua. Se o MCU ainda estiver em nível alto quando tW termina, a saída cai sozinha. No reset ou com o MCU travado em nível alto, o canal fica desligado ✅.

**Componentes:**

| Ref. | Valor | Especificação |
|---|---|---|
| R27, R28 | 120 kΩ | 1206, ±0,1%, 25 ppm/°C, filme fino (ex.: Yageo RT1206BRD07120KL) |
| C15, C18 | 100 nF | 1206, C0G/NP0, ±5% ou melhor, 50 V (ex.: Murata GRM31C5C1H104JA01L) |

**Cálculo.** `tW = K · Rext · Cext` (datasheet NXP, válida para Cext > 10 nF; K = 0,45 em 5 V). Em 3,3 V, K ≈ 0,5 (interpolação, não está no datasheet) → 0,5 × 120 kΩ × 100 nF = **6,0 ms**.

**Incerteza.** Resistor ±0,35% (tolerância + deriva) e C0G ±5% (±0,3% de deriva). O maior termo é o K, não documentado para o clone LX (estimativa ±5–10%). **Total estimado ±7 a ±11% antes da calibração → ≈ 5,4 a 6,6 ms.**

**Calibração na primeira placa:** segurar `IGN_A` em nível alto por ~20 ms, medir a largura do pulso no pino 13 do U9 (1Q) e em `IGN_A_PROT`, repetir com spray frio e soprador. K = tW ÷ (Rext · Cext). Ajustar R27/R28:

| K medido | Rext para 6 ms | Valor E96 |
|---|---|---|
| 0,45 | 133,3 kΩ | 133 kΩ |
| 0,50 | 120,0 kΩ | 120 kΩ |
| 0,55 | 109,1 kΩ | 110 kΩ |

**Regras para o firmware:**

- Dwell máximo ≤ ≈ 4,8 ms (abaixo do tW mínimo esperado, com margem).
- Se o dwell pedido passar de tW, a saída cai em tW e a **centelha ocorre antes do instante calculado**. Erro ≈ (dwell − tW) × rpm × 0,006 °: 1 ms de excesso = 18° a 3000 rpm e 36° a 6000 rpm.
- O 74HC123 é retriggerável: protege contra **nível alto preso**, não contra oscilação rápida do MCU.
- Confirmar que a bobina suporta ≈ 6,7 ms de acionamento.

---

## 4. Entradas de sensores

| Entrada | Circuito | Avaliação |
|---|---|---|
| VR+ | R33 22 kΩ → IN1 do NCV1124; IN2 em GND; VR− no GND do conector; C33 47 pF | ✅ conforme datasheet. Saída 0–5 V push-pull → divisor 10k/20k + 330 Ω → 3,33 V em PB3 (FT) |
| Limiar VR | RAdj = R31 20 kΩ + trimpot 0–5 kΩ; INP ≈ 11 µA; VHYS 160 mV | ✅ ≈ 120–190 mV ajustável |
| DIAG (NCV1124) | R34 10 kΩ para GND | ✅ modo normal |
| HALLIN | 5 V → R44 10 kΩ → D8 → HALLIN; R42 33k/R43 47k; Q5/Q6 | ✅ coletor aberto e saída até ~12–24 V. ⚠️ limiar ≈ 1 V sem histerese |
| D9 (BAT42W) | catodo na base de Q5 | ✅ polaridade conferida pela serigrafia |
| CKPIN | R7 4,7 kΩ + clamp BAT54SW → PA0 | ✅ sem pull-down (flutua se desconectada) |
| MAPSIG | 10k/15k (fator 0,6) + 1 kΩ + 100 nF + clamp | ✅ 0,2–4,7 V → 0,12–2,82 V |
| IATIN | pull-up 2,49 kΩ, 10 kΩ + 100 nF, clamp | ✅ NTC de ar. ⚠️ não serve para 100–180 °C |
| I2C | 24C256 (0x50), AHT20 (0x38), BMP280 (0x76/77), pull-ups 4,7 kΩ | ✅ sem conflito |

Firmware: PA15 e PB3 exigem remap do TIM2 para captura, e PA0 (TIM2_CH1 padrão) pode disputar o canal com PA15 conforme o remap.

---

## 5. Alimentação, proteção e comunicação

### 5.1 Alimentação

| Bloco | Avaliação |
|---|---|
| D6 SS56U em série | ✅ |
| D7 SMCJ18A | 🔒 mantido (sem jump-start nem inversão na aplicação) |
| U5 AP63205 (5 V) | ✅ FB no 5 V, EN em VIN, 4,7 µH, 2× 22 µF, BST 100 nF (circuito típico; VIN 3,8–32 V) |
| U4 AMS1117-3.3 | ✅ ≈ 0,2 W; C5 eletrolítico |
| VDDA | ✅ 10 Ω + 4,7 µF + 100 nF + 1 µF |
| Reset / BOOT0 | ✅ 10 kΩ + 100 nF; BOOT0 com 10 kΩ + chave |
| Cristal 8 MHz com 20 pF | ✅ pequeno desvio de CL, sem importância |
| Programação CN2 | ✅ 3V3, SWCLK PA14, SWDIO PA13, RESET, GND (WCH-LinkE) |
| UART J3 | ✅ PA9/PA10 com 100 Ω |

### 5.2 CAN

SN65HVD230 a 3,3 V, R2 10 kΩ no RS (slope control, 15 V/µs), terminação 120 Ω selecionável por SB1 ✅.

Proteção: nos arquivos atuais o D1 ainda é **SMBJ24CA ligado entre CANH e CANL**. A troca planejada é por SMCJ8 🔒. Cuidados para essa troca:

- SMCJ8.0A: VRWM 8 V, VBR 8,89–9,83 V, VC 13,6 V (listagens de distribuidores), abaixo do máximo absoluto do HVD230 (16 V) ✅.
- **Ligação:** um TVS de cada linha para GND (dois componentes), não entre CANH e CANL.
- **Unidirecional (A):** grampeia o negativo em ≈ −0,7 V. A bidirecional (CA) iria a ≈ −13,6 V, além dos −4 V do transceiver.
- **Capacitância:** TVS de 1500 W têm capacitância alta (milhares de pF; valor exato não encontrado). A 500 kbps costuma passar; a 1 Mbps, medir as bordas.

---

## 6. PCB

### 6.1 Dados gerais

- Placa 71,9 × 63,4 mm, 2 camadas, componentes nas duas faces (MCU, portas lógicas e Micro-Fit na face inferior).
- Planos de GND nas duas faces, ≈ 66% da área cada.
- Trilhas de 0,30 mm em quase tudo, 0,50 mm em poucos trechos, 3,0 mm na entrada `12V_IN`. Adequado para as correntes do módulo (< 0,2 A, exceto a entrada).
- 98 vias com furo de 0,31 mm; 4 furos M3; 2 NPTH de 3 mm do Micro-Fit.
- Espaçamento entre redes ≈ 0,3 mm, suficiente para 12 V e surtos de ≈ 29 V.

### 6.2 Verificação layout × netlist (revisão 3)

Conectividade do cobre extraída dos Gerbers e comparada com a netlist pelas posições de pads do Pick&Place:

- 94 componentes de 2 pinos (incluindo R27, R28, C15 e C18 em 1206): sem curtos e sem aberturas ✅.
- Transistores e diodos SOT-23/323 (incluindo D15), MCU, U2, U3, U7, U8, U9, U10: 0 divergências ✅.
- D13.2 ↔ D15.1, D14.2 ↔ D15.2, D15.3 ↔ coletor do Q7: três redes separadas, sem contato com GND ou `IGN_x_PROT` ✅.
- Polaridade pela serigrafia: D6, D9, D13, D14 conferem ✅.
- Não verificados: U4, U5, U11, U12 e os conectores.

### 6.3 Trilhas relevantes

| Rede | Comprimento | Observação |
|---|---|---|
| `12V_IN` | 26 mm | 0,5–3,0 mm ✅ |
| `+5V` / `+3V3` | 131 / 289 mm | 0,3 mm, quedas desprezíveis ✅ |
| `VR+` | 51 mm | lado do sensor (baixa impedância); R33 junto do NCV1124 ✅ |
| `CKP_CAPTURE` | 61 mm | ⚠️ trecho em 4,7 kΩ; melhor R7 e clamp perto do PA0 |
| `MAPSIG` / `IATIN` | 60 mm cada | com filtros ✅ |
| `CANH` / `CANL` | 56 / 62 mm | não roteados como par; tolerável |
| `OSC_IN` / `OSC_OUT` | 11–12 mm | cristal a ≈ 6 mm do MCU ✅ |
| Nó SW do buck | 13 mm | aceitável |

### 6.4 Pontos físicos de atenção

- ⚠️ NPTH de 3 mm do Micro-Fit a ≈ 0,3 mm da borda inferior (fabricável, frágil).
- ⚠️ Numeração do Micro-Fit: o símbolo numera 1/2, 3/4… lado a lado; o padrão Molex costuma ser 1–6 numa fileira e 7–12 na outra. Conferir com o chicote.
- ⚠️ Módulo U11 (footprint próprio, sem código de peça): conferir a ordem VDD/SDA/GND/SCL com o módulo real.
- Layout espelhado frente/verso (D4/D5, Q3/Q4, R22/R23 etc. no mesmo XY): funcional, exige montagem nas duas faces.

---

## 7. Histórico de revisões

| Rev. | Mudança | Resultado |
|---|---|---|
| 1 | Versão inicial | Acoplamento entre saídas no modo 12 V (zeners D13/D14 com coletor do Q7 comum) |
| 2 | D15 (BAS21C) em série com os zeners; D13/D14 → MM3Z4V3 SOD-323 | Acoplamento eliminado; cobre mudou só na região do D13–D15/Q7 |
| 3 | R27/R28 → 120 kΩ 1206; C15/C18 → 1206 | Limitador em 6 ms; cobre mudou só na região desses quatro componentes |

---

## 8. Método e limites

- Esquemático lido do SVG renderizado e da netlist PADS. Datasheets consultados: CH32V203 (V2.7), NCV1124, 74HC123 (NXP), SN65HVD230, AP63205, família SMAJ, BAS21C (SXSEMI).
- Layout: Gerbers convertidos em geometria (gerbonara/shapely), camadas ligadas pelos furos, pads posicionados a partir do Pick&Place.
- Simulações: modelo DC simples para o grampo e o acoplamento.
- **Não foram feitos** simulação transitória, análise de EMC, DRC formal nem testes de bancada. O tW de 6 ms vem da fórmula do datasheet e precisa ser medido.
