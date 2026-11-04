# MARS – Módulo de Ignição (CH32V203K8T6) – Análise de circuito e PCB

Arquivos analisados: esquemático (SVG + netlist PADS), BOM, Pick&Place, Gerbers v1.1 (2 camadas).
Data dos arquivos: 2026-11-04. Título no esquemático: "STM32 IGNITION MODULE" (o MCU real é o **WCH CH32V203K8T6**, RISC-V).

Legenda: ✅ funciona / confere · ⚠️ funciona, mas com ressalva ou precisa ser verificado · ❌ falha ou risco real.

---

## 1. Veredito

O projeto é coerente e a maior parte funciona. O layout (PCB) **bate com a netlist** nos componentes verificados, sem curtos nem abertos. Os pontos que realmente precisam de atenção são poucos:

| # | Item | Gravidade |
|---|---|---|
| 1 | Saídas de ignição A e B se **acoplam no modo 12 V** (zeners D13/D14 com coletor do Q7 flutuando) | ✅ **corrigido na revisão 2** (BAS21C em D15, ver seção 10) |
| 2 | Limitador de dwell (74HC123) fixado em ≈ 4,5–5,5 ms, com tolerância ampla | ⚠️ pode cortar dwell na partida/bateria baixa |
| 3 | TVS de entrada SMCJ18A (VWM = 18 V) | ⚠️ não suporta jump-start de 24 V; em polaridade invertida o TVS entra em curto |
| 4 | Módulo AHT20+BMP280 (U11) e conector Micro-Fit (U12) sem confirmação de pinagem física | ⚠️ conferir antes de ligar |
| 5 | Proteção do CAN (SMBJ24CA) não protege o SN65HVD230 (máx. absoluto 16 V no barramento) | ⚠️ baixo para 12 V, ruim para 24 V |
| 6 | Entrada de IAT com pull-up de 2,49 kΩ serve para ar (−20…100 °C), não para 100–180 °C | ⚠️ depende do uso |

---

## 2. O que o circuito faz (visão geral)

Módulo de ignição autônomo para motor de 4 cilindros em centelha perdida (duas saídas: cilindros 1-4 e 2-3).

```
Conector Micro-Fit 12P ──┬─ 12V_IN ── SS56U (diodo série) ── +12V ── AP63205 (buck 5 V) ── AMS1117 (3,3 V)
                         ├─ VR+ ──► NCV1124 (condicionador VR) ──► PB3
                         ├─ HALLIN ──► Q5/Q6 (comparador a transistor) ──► PA15
                         ├─ CKPIN ──► R 4,7k + clamp ──► PA0
                         ├─ MAPSIG ──► divisor 10k/15k ──► PA2 (ADC)
                         ├─ IATIN ──► NTC com pull-up 2,49k ──► PA3 (ADC)
                         └─ CANH/CANL ──► SN65HVD230 ──► PA11/PA12
CH32V203 ── PA6/PA7 (IGN_A/B) ─ AND com PA1 (IGN_ENABLE) ─ AND com janela do 74HC123 ─ NPN+PNP ─ 1 kΩ ─ saída
```

O MCU lê posição do virabrequim (VR ou Hall), MAP e IAT, calcula avanço/dwell e gera os dois pulsos de ignição. A saída **não é um estágio de potência**: é um sinal de comando (12 V ou 5 V, impedância 1 kΩ) para bobinas inteligentes ou módulos igniters externos.

---

## 3. Cadeia de ignição e dwell

### 3.1 Caminho do sinal (canal A; o B é idêntico)

1. **PA6 (IGN_A)** e **PA1 (IGN_ENABLE)** entram em `U2` (74LVC2G08, AND). Saída: `IGN_A_ENABLED`.
   - R9/R11/R12/R13 (47 kΩ) puxam as entradas para GND. No reset ou travamento o canal fica desligado ✅.
2. `IGN_A_ENABLED` passa por R29 (4,7 kΩ) + C28 (1 nF) e dispara a entrada **1B do 74HC123 (U9)**. A entrada 1A# está em GND, então dispara na borda de subida de 1B.
   - Atraso do filtro RC ≈ 3 µs (só no início do dwell; não afeta o ponto de centelha).
3. A saída **1Q** (`DWELL_WINDOW_A`) fica em nível alto durante o tempo do monoestável.
4. `U10` (AND) faz `IGN_A_LIMITED = IGN_A_ENABLED AND DWELL_WINDOW_A`.
5. `Q1` (BC817) inverte e aciona `Q3` (BC807, PNP de alto lado, emissor em +12 V). Saída `IGN_A_RAW`.
6. `D2` (BAS21C) + `R21` (1 kΩ) levam o sinal ao `IGN_A_PROT` (conector). `R22` (10 kΩ) mantém a saída em 0 V quando desligada. `D5` (SMAJ18A) protege contra surtos.
7. **Modo de tensão:** `PA5 (SEL_5V)` liga o `Q7`. Com Q7 ligado, os zeners de 4,7 V (D13/D14) limitam a saída a ≈ 4,9 V. Com Q7 desligado a saída vai a ≈ 9–10 V (carga de 10 kΩ) ou até ≈ 11 V.

### 3.2 O limitador de dwell

Rext = 100 kΩ (R27/R28) a +3V3, Cext = 100 nF (C15/C18), Vcc = 3,3 V.

- Fórmula do 74HC123 (Cext > 10 nF): `tW = K · Rext · Cext`, com K ≈ 0,45 (5 V) a 0,55 (2 V). A 3,3 V, K ≈ 0,5.
- **tW ≈ 0,5 × 100 kΩ × 100 nF ≈ 5 ms** (faixa 4,5–5,5 ms típica).
- Tolerância real: C X7R ±5% inicial (Walsin 0805B104J101CT) mais ±15% com temperatura, resistor 1%, K variando com Vcc e com o fabricante. Esperar **≈ 3,6 a 6,6 ms** no pior caso.
- O 74HC123 do BOM é **SN74HC123DR(LX)** (clone da Lingxing, LCSC C22449638): o K pode diferir do da TI. Meça com osciloscópio.

Consequências:

- Se o firmware pedir dwell acima de ≈ tW, o canal desliga em tW, **a centelha ocorre cedo** (no fim do limitador) e não no instante calculado.
- Dwell típico de bobina inteligente é 2–4 ms a 13,5 V. Na partida (bateria 8–10 V) algumas bobinas pedem mais. O limite fixo ≈ 5 ms pode ser curto.
- Recomendação: manter o dwell do firmware ≤ ≈ 3,5 ms, ou trocar R27/R28 por um valor/trimpot para ajustar o limite à bobina real.
- Failsafe ✅: com o MCU travado em nível alto, a saída cai sozinha depois de tW, e no reset o canal fica desligado.

### 3.3 Atraso e velocidade

- Subida: IGN_A → saída ≈ 3–4 µs (filtro RC + portas + transistores). Irrelevante para dwell.
- Descida (centelha): corte imediato pela porta AND. O BC807 em saturação tem tempo de armazenamento de ~1–2 µs. Irrelevante para o avanço.
- Em 8000 rpm cada saída dispara uma vez por volta (7,5 ms). Com dwell máximo de 5 ms sobram ≥ 2,5 ms de recuperação do 74HC123 ✅.

### 3.4 ❌ Acoplamento entre as saídas A e B (modo 12 V)

`D13` e `D14` (zeners 4,7 V, catodos em `IGN_B_PROT`/`IGN_A_PROT`) têm os ânodos ligados ao **mesmo nó**, que é o coletor do `Q7`. Com Q7 **desligado** (modo 12 V), esse nó fica flutuando:

- Quando A dispara (≈ 9–10 V), D14 conduz em ruptura (4,7 V) e eleva o nó comum.
- D13 conduz em **direta** (0,7 V) para o `IGN_B_PROT`, que deveria ficar em 0 V.

Simulação DC (modelo simples; A disparado, B desligado, modo 12 V):

| Impedância de entrada da bobina | V_A | V_B (deveria ser 0 V) |
|---|---|---|
| sem carga (só os 10 kΩ) | 9,3 V | **3,9 V** |
| 5 kΩ | 7,6 V | **2,2 V** |
| 2 kΩ | 6,3 V | **0,9 V** |
| ≤ 1 kΩ | ≤ 5 V | ≈ 0 V |

No modo 5 V (Q7 ligado) não há problema: o nó comum fica em ≈ 0,15 V.

Impacto: uma entrada de bobina com limiar de 2–3 V e impedância alta pode ser disparada pelo outro canal.

**Correção recomendada: um BAS21C (catodo comum, o mesmo componente já usado em D2/D3/D8) em série com os dois zeners.** Ligação: pino 1 no ânodo do D14, pino 2 no ânodo do D13, pino 3 (catodo comum) no coletor do Q7. Com Q7 desligado, o diodo do canal que não disparou fica em reversa e bloqueia o acoplamento (V_B ≈ 0 V). O grampo do modo 5 V sobe cerca de 0,65–0,8 V, então troque os zeners:

| Zener | Nível da saída no modo 5 V (simulado) |
|---|---|
| 4,7 V (atual, sem diodo) | ≈ 4,9 V |
| 4,7 V + BAS21C | ≈ 5,6 V |
| 4,3 V + BAS21C | ≈ 5,2 V |
| 3,9 V + BAS21C | ≈ 4,8 V |

A numeração 1/2 = ânodos e 3 = catodo comum é a do símbolo do projeto; confirme no datasheet do BAS21C-JS3 (ZRE, LCSC C779748) antes de desenhar. Na placa v1.1 é preciso separar o nó que liga os dois ânodos ao coletor do Q7 (retrabalho com fios); na próxima revisão, ajustar o layout. Alternativa mais cara: um transistor de clamp por canal.

### 3.5 Capacidade de acionamento

- Fonte de 1 kΩ: corrente de curto ≤ ≈ 11 mA. Serve para entradas de comando de bobinas inteligentes e módulos igniters. **Não aciona bobina simples nem entrada de baixa impedância.**
- Confirme no datasheet da bobina: limiar de entrada, corrente de entrada e se aceita 12 V através de 1 kΩ (ou 5 V no modo clamp).
- Modo padrão no reset = 12 V (Q7 desligado). Seguro, porque o canal está desabilitado até o MCU subir.

---

## 4. Entradas de sensores

| Entrada | Circuito | Avaliação |
|---|---|---|
| **VR+** | R33 22 kΩ → IN1 do NCV1124; IN2 em GND; VR− no GND do conector; C33 47 pF | ✅ segue o datasheet (R1 = 22 kΩ). Entrada ±250 V, 5 V ±10%. Saída push-pull 0–5 V → divisor 10k/20k + 330 Ω → 3,33 V em PB3 (FT) |
| Limiar VR | RAdj = R31 20 k + trimpot 0–5 k (20–25 k), INP1 ≈ 11 µA, VHYS = 160 mV | ✅ `VRS(+TRP) ≈ INP1·(RAdj − R1 − RRS) + VHYS` → **≈ 120–190 mV** ajustável (com RRS 0,5–2 kΩ) |
| DIAG do NCV1124 | R34 10 kΩ para GND | ✅ modo normal |
| **HALLIN** | 5 V → R44 10k → D8 → HALLIN; R42 33k → base de Q5; R43 47k; Q5/Q6 invertem duas vezes | ✅ aceita coletor aberto e saída até ~12–24 V (D8 bloqueia retorno). Limiar ≈ 1 V, **sem histerese** (ruído pode causar chaveamento duplo em sinal lento) |
| **CKPIN** | R7 4,7 kΩ + clamp BAT54SW (3V3/GND) → PA0 | ✅ entrada lógica protegida. Sem pull-down: fica flutuando se desconectada |
| **MAPSIG** | R3 10k + R4 15k (fator 0,6) + R6 1k + C8 100 nF + clamp | ✅ sensor 0,2–4,7 V (como o MAP FuelTech da tabela) → 0,12–2,82 V no ADC |
| **IATIN** | R25 2,49 kΩ a 3V3, R26 10k, C11/C23 100 nF, clamp | ✅ para NTC de ar (≈ 2,5 kΩ a 20–25 °C). ⚠️ Não serve para 100–180 °C (ver análise do NTC anterior) |
| I2C | U8 24C256 (0x50), U11 AHT20 (0x38)/BMP280 (0x76/0x77), pull-ups 4,7 kΩ | ✅ sem conflito de endereços |

Os pinos PA15 e PB3 exigem remap do TIM2 para usar captura de entrada, e PA0 (TIM2_CH1 no mapeamento padrão) pode disputar o mesmo canal com o PA15 conforme o remap escolhido. Isso é decisão de firmware, mas vale planejar antes de fixar o pinout.

---

## 5. Alimentação e proteção

| Bloco | Avaliação |
|---|---|
| **D6 SS56U** em série (proteção de inversão) | ✅ queda ≈ 0,4–0,55 V |
| **D7 SMCJ18A** (TVS, VWM 18 V, VBR 20–22 V) | ⚠️ Na tensão de jump-start (24 V) o TVS conduz forte e pode queimar. Na **inversão de polaridade**, o TVS (antes do D6) fica em direta e **curto-circuita a bateria**: precisa de fusível externo. Alternativa: SMCJ24A, mas o grampo dele (VC = 38,9 V) fica acima dos 32 V de operação do AP63205; conferir o máximo absoluto do regulador antes de escolher. Colocar o TVS depois do diodo evita o curto em inversão de polaridade |
| **U5 AP63205** (5 V, 2 A) | ✅ FB ligado ao 5 V (versão fixa), EN em VIN, L1 4,7 µH, 2× 22 µF, BST 100 nF. É o circuito típico do datasheet (VIN 3,8–32 V). C29/C30 são 22 µF/16 V X5R: perdem capacitância com 5 V, mas ficam acima do mínimo útil |
| **U4 AMS1117-3.3** | ✅ queda 1,7 V × ~0,12 A ≈ 0,2 W. C5 eletrolítico (ESR adequada ao LDO) |
| **VDDA** | ✅ R5 10 Ω + 4,7 µF + 100 nF + 1 µF |
| **Reset/BOOT** | ✅ R8 10k + C16; BOOT0 com R10 10k + chave |
| **Cristal 8 MHz** (ABM3, CL ≈ 18 pF) com 20 pF | ✅ CL efetiva ≈ 14–16 pF → pequeno desvio de frequência, sem importância |
| **CAN** SN65HVD230 + R2 10k (slope control, 15 V/µs) + termino. 120 Ω via SB1 | ✅ funcional. ⚠️ D1 SMBJ24CA não protege: o máximo absoluto no barramento é 16 V (a TVS só começa a limitar em 24 V). Capacitância da TVS pode arredondar bordas a 1 Mbps |
| **Programação** CN2 (3V3, SWCLK PA14, SWDIO PA13, RESET, GND) | ✅ compatível com WCH-LinkE |
| **J3** UART (PA9/PA10, 100 Ω em série) | ✅ |

---

## 6. Análise física (PCB)

### 6.1 Dados gerais (extraídos dos Gerbers)

- Placa **71,9 × 63,4 mm**, 2 camadas, componentes nas duas faces.
- **Planos de GND** nas duas faces: ≈ 3051 mm² (topo) e 2995 mm² (fundo), cerca de 66–67% da área.
- Trilhas: **0,30 mm** em quase tudo (≈ 919 mm no topo e 866 mm no fundo). 0,50 mm em 7 segmentos. **3,0 mm** em 5 segmentos (a entrada `12V_IN`).
- 98 vias com furo de 0,31 mm. Furos PTH 0,70 a 1,20 mm; 4 furos M3 (3,2 mm); 2 NPTH de 3,0 mm (pinos de posicionamento do Micro-Fit).

### 6.2 Layout × netlist (verificação automática)

Extraí a conectividade do cobre e comparei com a netlist usando a posição dos pads (Pick&Place):

- **94 componentes de 2 pinos** (resistores, capacitores, diodos, indutor): todos os 188 pads estão sobre cobre, **nenhuma rede de cobre carrega dois nomes da netlist (sem curtos) e nenhuma rede da netlist fica dividida (sem abertos)** ✅.
- Transistores e diodos SOT-23/323, MCU (U1), U2, U3, U7, U8, U9, U10: **0 divergências** nos pinos verificáveis ✅.
- Não verificados: U4, U5, U11, U12 e demais conectores (geometria de pads não confiável a partir do Pick&Place).

Polaridade dos diodos (serigrafia × netlist): D6, D13, D14 e D9 conferem ✅. O D9 (BAT42W no circuito do Hall) tem o catodo para a base de Q5, como no esquema.

### 6.3 Comprimentos e larguras de trilhas relevantes

| Rede | Comprimento | Observação |
|---|---|---|
| `12V_IN` | 26 mm | 0,5–3,0 mm ✅ (corrente de entrada ≈ 0,1 A, sobra margem) |
| `+12V` | 69 mm | 0,3–0,5 mm ✅ |
| `+5V` | 131 mm | 0,3 mm ✅ (≈ 0,14 A, queda < 40 mV) |
| `+3V3` | 289 mm | 0,3 mm ✅ |
| `VR+` | 51 mm (16 topo, 35 fundo) | longo, mas é lado do sensor (baixa impedância); o R33 22 kΩ fica junto do NCV1124 ✅ |
| `CKP_CAPTURE` | 61 mm | ⚠️ trecho de 4,7 kΩ de impedância percorre ~50 mm: sensível a ruído; melhor R7 e clamp junto ao MCU |
| `MAPSIG` / `IATIN` | 60 mm cada | sinais de sensor, com filtros (C8, C11) ✅ |
| `CANH` / `CANL` | 56 / 62 mm | par não roteado junto, mas o transceiver é diferencial e tolerante |
| `OSC_IN` / `OSC_OUT` | 11–12 mm | ✅ cristal a ≈ 6 mm do MCU |
| `IGN_A_PROT` / `IGN_B_PROT` | 29 / 43 mm | corrente baixa (≤ 11 mA) ✅ |
| `U5_5` (nó SW do buck) | 13 mm | 0,3 mm, aceitável pela corrente média baixa; pode irradiar mais que o necessário |

### 6.4 Pontos de atenção físicos

- **Larguras de 0,3 mm** suportam as correntes deste módulo (tudo abaixo de ≈ 0,2 A, exceto picos do buck). Não há problema de queda ou aquecimento.
- Espaçamento ≈ 0,3 mm entre redes: aceitável para 12 V e picos de ≈ 29 V (clamp do TVS).
- **NPTH de 3 mm do Micro-Fit** fica a ≈ 0,3 mm da borda inferior da placa: fabricável, mas frágil. Considere afastar 0,5 mm ou mais.
- Layout espelhado frente/verso (D4/D5, Q3/Q4, R22/R23 etc. no mesmo XY): estilo funcional, mas exige montagem nas duas faces (custo maior).
- O MCU, as portas lógicas e o Micro-Fit ficam na **face inferior**. A placa fica "de cabeça para baixo" na caixa.
- Sem fusível no esquema (necessário por causa do TVS antes do diodo).

---

## 7. Tabelas finais

### 7.1 O que vai funcionar ✅

- Alimentação: diodo série, AP63205 → 5 V, AMS1117 → 3,3 V.
- MCU: pinagem do símbolo confere com o datasheet do CH32V203K8T6 (LQFP32), cristal, reset, boot, SWD, UART, CAN e I2C.
- CAN com SN65HVD230 e terminação selecionável.
- Condicionador de VR (NCV1124) com limiar ajustável ≈ 120–190 mV.
- Condicionador Hall (com ressalva de histerese).
- Leitura de MAP e IAT (ar).
- Cadeia de ignição lógica: habilitação por hardware, limitador de dwell, drive PNP, modos 12 V/5 V (com a ressalva do acoplamento), failsafe no reset.
- Roteamento coerente com a netlist (sem curtos/abertos nos pinos verificados).

### 7.2 O que não vai funcionar como projetado ❌

1. ~~Saídas A/B acopladas no modo 12 V (zeners D13/D14, item 3.4)~~: **resolvido na revisão 2** (seção 10). Nenhum item do circuito falha mais por projeto; restam apenas os riscos da seção 7.3.

### 7.3 O que pode falhar na prática ⚠️

1. Dwell limitado a ≈ 5 ms (tolerância ±20%): pode cortar dwell em baixa tensão e adiantar a centelha.
2. TVS D7 18 V: jump-start de 24 V e inversão de polaridade (curto via TVS) exigem fusível/TVS adequado.
3. Pinagem física de U11 (módulo AHT20+BMP280, sem MPN no BOM) e de U12 (Micro-Fit 2×6): conferir contra a peça e o chicote antes de ligar. O símbolo numera 1/2, 3/4... lado a lado; o Micro-Fit da Molex costuma numerar 1–6 em uma fileira e 7–12 na outra, então a numeração do footprint pode não coincidir com a do chicote.
4. Proteção do CAN não protege o transceiver acima de 16 V.
5. Hall sem histerese; CKP_CAPTURE com ~50 mm em 4,7 kΩ.
6. BOM incompleto (U3, D7, U11, SW1 sem número de peça; BOBINA-OUT na Robu, resto LCSC): fecha a montagem só depois de completar.

---

## 8. Correções recomendadas (ordem de prioridade)

1. **D13/D14**: um clamp por canal, ou diodo de sinal em série com cada zener (e zener 4,3 V).
2. **Dwell**: ajustar R27/R28 (trimpot) para o limite que a sua bobina precisa; medir tW real do 74HC123 (LX) com osciloscópio.
3. **Entrada 12 V**: TVS SMCJ24A depois do D6 (ou mantê-lo e adicionar fusível/PTC antes); fusível de 3 A no chicote.
4. **Conferir pinagem** do U11 e do U12 contra o datasheet/chicote.
5. CAN: TVS de baixa capacitância própria para CAN (ex.: PESD2CAN), com ligação ao GND.
6. Hall: adicionar histerese (Schmitt) se o sinal for lento.
7. Mover R7 + D12 para perto do PA0 ou reduzir a impedância do trecho longo.
8. Afastar o NPTH de 3 mm da borda.

---

## 9. Método e limites desta análise

- Esquemático lido a partir do SVG (renderizado) e da netlist PADS; datasheets consultados: CH32V203 (V2.7), NCV1124, 74HC123 (fórmula do K), SN65HVD230, AP63205.
- Layout: Gerbers lidos com gerbonara; cobre convertido em geometria; ligações camada a camada pelos furos; posições de pads inferidas do Pick&Place (pad 1 + passo). A verificação cobre passivos, SOT-23/323 e CIs principais; não cobre conectores nem U4/U5/U11.
- Simulação do acoplamento (3.4) é um modelo DC simples (zener 4,7 V, queda direta 0,7 V). A faixa real depende da entrada da bobina.
- **Não foi feita** simulação transitória, análise de EMC, medição real nem teste de bancada. O dwell (≈ 5 ms) vem da fórmula do datasheet e precisa de confirmação no osciloscópio.

---

## 10. Revisão 2 (arquivos `*_2`, Gerbers gerados às 11:08)

### 10.1 O que mudou (comparado com a revisão 1)

| Item | Antes | Agora |
|---|---|---|
| D13, D14 (zeners) | MM1Z4V7, SOD-123 | **MM3Z4V3** (ST, LCSC C61849), SOD-323 |
| D15 | não existia | **BAS21C-JS3** (ZRE, C779748), SOT-23, catodo comum |
| Rede `Q7_3` | ânodo D13 + ânodo D14 + coletor Q7 | separada em `D13_2` (D13.2 + D15.1), `D14_2` (D14.2 + D15.2) e `Q7_3` (Q7.3 + D15.3) |
| Demais componentes e redes | – | **idênticos** (diferenças no BOM/PnP são só de nome: BAT54SW, BAT42W, SMAJ18A etc.) |

Os Gerbers mudaram **somente** na região x 41–53 mm, y −52…−41 mm (zona de D13/D14/D15/Q7): 1 via removida, 2 vias adicionadas, retraçado local.

### 10.2 Verificação

- **Netlist:** única mudança é a descrita acima.
- **Cobre × netlist (Gerbers v2):** D13.2 = D15.1, D14.2 = D15.2, D15.3 = Q7.3; as três redes são distintas entre si e não tocam GND nem as redes `IGN_x_PROT`. D13.1 está em `IGN_B_PROT`, D14.1 em `IGN_A_PROT`, Q7.2 em GND ✅.
- **Verificação global:** 94 componentes de 2 pinos e os transistores/CIs verificáveis continuam sem curtos nem abertos ✅.
- **Polaridade dos zeners novos:** a serigrafia do D13 (topo) e do D14 (fundo) marca o catodo no pad 1, que vai para `IGN_x_PROT` ✅.
- **Pinagem do BAS21C:** o datasheet da SXSEMI (marcação JS3, a mesma do código `BAS21C-JS3`) mostra pinos 1 e 2 como ânodos e pino 3 como catodo comum; VRRM 250 V, fuga ≤ 0,1 µA a 200 V, trr ≤ 50 ns ✅. Não achei o datasheet da ZRE, só o da SXSEMI com a mesma marcação.
- **Simulação DC (modo 12 V):** com o BAS21C bloqueando, o canal desligado fica em ≈ 0 V para qualquer impedância de entrada da bobina. O acoplamento de até 3,9 V da revisão 1 desaparece.

### 10.3 Nível da saída no modo 5 V com os novos zeners

Zener 4,3 V + BAS21C + Vce(sat) do Q7 → **≈ 5,1 V** típico. Pela listagem da LCSC o MM3Z4V3 (ST) tem Vz 4,0–4,6 V, então o nível real fica entre **≈ 4,8 e 5,4 V**. Confira se a entrada da bobina aceita até 5,4 V. Zzt listada: 130 Ω (a 5 mA; o grampo opera em ≈ 5–6 mA).

### 10.4 Pendências (não mudaram)

1. Dwell limitado a ≈ 5 ms (R27/R28 100 kΩ, C15/C18 100 nF X7R, clone LX).
2. D7 SMCJ18A (jump-start de 24 V; inversão de polaridade via TVS). Ver nota sobre o clamp do SMCJ24A na seção 5.
3. D4/D5 continuam SMAJ18A (VC = 29,2 V). SMAJ15A (VC = 24,4 V) ou SMAJ16A (26,0 V) protegem melhor o nó de saída; ver tabela da família SMAJ.
4. D13/D14 agora são SOD-323 (listagens de 200 a 300 mW): menos robustos a surtos do chicote do que o SOD-123 de 500 mW.
5. Pinagem física do módulo U11 (AHT20+BMP280) e do conector U12 (Micro-Fit).
6. Proteção do CAN (D1 SMBJ24CA) acima do máximo absoluto do SN65HVD230 (16 V).
7. Hall sem histerese; `CKP_CAPTURE` com ≈ 50 mm de trilha em 4,7 kΩ; NPTH de 3 mm a ≈ 0,3 mm da borda.
