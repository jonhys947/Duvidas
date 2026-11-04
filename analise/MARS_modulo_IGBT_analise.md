# MARS – Módulo de Ignição MOSFET/IGBT – Análise

**Arquivos:** `SCH/PCB_MARS_Modulo_Ignição_MOSFET_2026-11-04` (netlist, SVG, Gerbers, Pick&Place, BOM) e datasheet do STGB10NB37LZ (ST, jun/1999).
**Função:** estágio de potência para bobinas simples, comandado pelas saídas `IGN_A_PROT`/`IGN_B_PROT` do módulo principal.

Legenda: ✅ confere · ⚠️ ressalva · ❌ problema.

---

## 1. Resumo

| # | Item | Situação |
|---|---|---|
| 1 | **Isolação no lado de alta tensão:** os coletores chegam a até **425 V** (grampo do IGBT) e ficam a **0,18 mm** um do outro, 0,28–0,30 mm do GND e 0,30 mm do pad do parafuso SCREW1 | ❌ muito abaixo do necessário |
| 2 | Resistor de desligamento do gate ≈ 4 kΩ, enquanto o datasheet caracteriza tudo (atrasos, corrente de latch, chaveamento indutivo) com **1 kΩ** | ⚠️ trocar R2/R3 por 1 kΩ |
| 3 | Dissipação dos IGBTs só pelo cobre da placa (≈ 3,5 cm² por face) | ⚠️ medir; pode se aproximar dos 175 °C de junção com motor quente |
| 4 | Topologia, ligações, acionamento do gate e trilhas de corrente | ✅ |

---

## 2. Circuito

```
módulo principal           placa de potência
IGN_A_PROT ── R1 100 Ω ──┬── gate Q1 (STGB10NB37LZ)   coletor ── -BOBINA_A (CN1.4)
                         R2 10 kΩ                       emissor ── GND_STAR
GNDREF ───────────────── U4 (estrela) ── GND_STAR ── U2.1 (bateria −)
12V_FUSED (U2.2) ── U3 (resistor de balastro externo) ── 12V ── CN1.1/CN1.3 (+ das bobinas)
```

Canal B idêntico (R4, R3, Q2, CN1.6). CN1 (Micro-Fit 2×3): 12 V nos pinos 1/3, GND_STAR nos 2/5, coletores nos 4 e 6.

### 2.1 IGBT STGB10NB37LZ (datasheet)

| Parâmetro | Valor |
|---|---|
| Grampo coletor-emissor BV(CES) | 375 / 400 / 425 V (mín/típ/máx), Ic = 2 mA, −40 a 150 °C |
| Zener gate-emissor BV(GE) | 12–16 V (o gate é grampeado internamente) |
| Resistor interno gate-emissor RGE | 20 kΩ |
| Limiar VGE(th) | 0,6–2,4 V |
| VCE(sat) | 1,2 V típ / 1,8 V máx em VGE = 4,5 V, Ic = 10 A |
| Ic contínua / pulsada | 20 A / 60 A |
| Carga de gate Qg | 28 nC (VGE = 5 V) |
| Desligamento (RGE(off) = 1 kΩ) | td(off) 14,8 µs, tf 1,5 µs, Eoff 4,0 mJ (25 °C); 15,8 µs e 6,5 mJ a 125 °C |
| Energia de avalanche EAS | 215 mJ (início a 55 °C), 150 mJ (150 °C) |
| Teste funcional | 12 A com L = 3 mH, RGOFF = 1 kΩ |
| Proteção de bateria invertida VECR | 18 V |
| Térmica | Rθj-c 1,2 °C/W; Rθj-a 62,5 °C/W; Tj máx 175 °C |

A Digi-Key lista o STGB10NB37LZ como obsoleto; a versão T4 ainda aparece em distribuidores.

### 2.2 Ligações

- **Pinagem:** gate = pino 1, coletor = aba, emissor = pino 3. No cobre: gate isolado, aba ligada ao `-BOBINA`, emissor no `GND_STAR` ✅.
- **Terra em estrela:** `GNDREF` (referência do módulo principal) e `GND_STAR` ficam na mesma ilha de cobre em U4; a corrente das bobinas volta dos emissores para U2 sem passar pela referência do comando ✅.
- **Balastro externo** em U3 limita a corrente do primário ✅ (o IGBT não tem limitação de corrente própria).

---

## 3. Acionamento do gate

| Modo do módulo principal | Tensão no gate (estimada) |
|---|---|
| 5 V (Q7 ligado) | ≈ 4,6–4,9 V |
| 12 V (Q7 desligado) | ≈ 9–10 V |

- O VCE(sat) do datasheet é especificado em **VGE = 4,5 V**: o modo 5 V atende, com margem pequena.
- O modo 12 V também é seguro: o zener interno grampeia o gate em 12–16 V e a fonte de 1 kΩ do módulo principal limita a corrente ✅. A fuga do gate em 10 V pode chegar a 0,7 mA, tolerável.
- **Desligamento:** hoje a carga do gate sai por R2 10 kΩ ∥ RGE interno 20 kΩ ∥ R22 10 kΩ do módulo principal ≈ **4 kΩ**. O datasheet mede td(off) = 14,8 µs com 1 kΩ; com ≈ 4 kΩ o atraso deve ficar em ≈ 40–60 µs (≈ 1,5–2° a 6000 rpm) e variar com a temperatura. Além disso, corrente de latch, chaveamento indutivo e teste funcional só são garantidos com **RGOFF = 1 kΩ**.
- **Correção:** R2/R3 = **1 kΩ**. O gate continua em ≈ 4,6–4,8 V nos dois modos (o 1 kΩ do módulo principal forma divisor) e o desligamento volta às condições do datasheet. O módulo principal passa a fornecer ≈ 5–6 mA por canal.

---

## 4. PCB

- Placa 62,0 × 37,9 mm, 2 camadas, planos nas duas faces; 4 furos M3.
- Trilhas de 1,0 a 3,5 mm (3,5 mm no 12 V, 1,5–2,0 mm nos coletores, nas duas faces) ✅ para picos de 7–10 A.
- Aba de cada IGBT sobre ≈ 350 mm² de cobre por face, com 9 vias de 0,31 mm ligando as faces.

### 4.1 ❌ Isolação (o ponto principal)

Na abertura do primário o coletor sobe até o grampo do IGBT: **400 V típico, 425 V máximo**. Distâncias medidas nos Gerbers:

| Entre | Distância mínima | Onde |
|---|---|---|
| Coletor Q1 ↔ coletor Q2 | **0,18 mm** | trilhas chegando ao CN1, x ≈ 86,7 / y ≈ −19,6 mm (topo e fundo) |
| Coletor ↔ GND_STAR | 0,28–0,30 mm | perto do CN1 e ao redor das áreas de cobre das abas |
| Coletor Q1 ↔ pad do SCREW1 | 0,30 mm | x ≈ 83,1 / y ≈ −32,0 mm; o parafuso pode estar no chassi |

Referência (IPC-2221, 301–500 V): ≈ 2,5 mm em camada externa sem revestimento; ≈ 0,8 mm com revestimento permanente (verniz conformal). 0,18–0,30 mm com só máscara de solda é sujeito a arco e trilhamento, principalmente com umidade e sujeira de motor.

Correções:
- Afastar as duas trilhas de coletor entre si e do GND/12 V: **≥ 2,5 mm** sem verniz, ou ≥ 0,8 mm com verniz conformal em toda a área de alta tensão.
- Recuar o plano de GND em volta das áreas de cobre das abas e das trilhas de coletor (aumentar o afastamento do pour só nessas redes).
- Afastar a área de cobre do Q1 do furo/pad do SCREW1, ou usar furo sem pad (NPTH) ali.
- Não passar trilha de coletor entre pinos do CN1; chegar a cada pino pelo lado de fora.

### 4.2 ⚠️ Térmica e energia

Estimativa (depende da bobina e do balastro): corrente média ≈ 3,5 A durante o dwell, VCE(sat) ≈ 1,2–1,5 V → ≈ 4–5 W enquanto conduz, mais ≈ 4–6,5 mJ de perda no desligamento a cada disparo (Eoff do datasheet a 10 A; menor com corrente menor). Com dwell de 3–4,8 ms por volta (cada canal dispara uma vez por volta em centelha perdida), a ciclagem fica em 30–55% → **≈ 1,5–3 W médios por IGBT** a 6000 rpm. Com ≈ 3,5 cm² de cobre por face, algo como 40–50 °C/W: elevação de 60–150 °C sobre o ambiente. Num cofre a 85 °C pode chegar perto dos 175 °C de junção.

- Medir a temperatura da aba na bancada, na pior rotação e com o maior dwell.
- Se necessário: dissipador encostado na aba/área de cobre, caixa metálica com pad térmico, ou mais área de cobre.
- **Secundário aberto** (vela desconectada): a energia da bobina (½·L·I²) vai para o grampo a cada disparo. O EAS é de 215 mJ (55 °C) e 150 mJ (150 °C) por pulso único; uma bobina de 3 mH a 7 A tem ≈ 74 mJ, 6 mH a 7 A ≈ 147 mJ. Pulsos repetidos nessa condição não são garantidos pelo datasheet: o firmware deve evitar disparar canal sem vela.

### 4.3 Conector CN1

Micro-Fit 3.0 (≈ 5–8,5 A por terminal conforme bitola). Cada coletor usa um pino só e o 12 V usa dois para as duas bobinas. Com picos de 7–10 A e ciclagem abaixo de 50%, a corrente eficaz fica perto do limite: use terminais e fio adequados (18 AWG ou maior).

---

## 5. Ligação com o módulo principal

- `IGN_A_PROT`/`IGN_B_PROT` passam a ser comando de gate: funcionam nos dois modos (seção 3).
- O limitador de dwell do módulo principal (≈ 6 ms) passa a proteger também o IGBT e a bobina contra nível alto preso.
- O D4/D5 (SMAJ18A) do módulo principal fica no nó de gate, longe do coletor: não interfere.
- `GNDREF` deve vir do GND do módulo principal pelo mesmo chicote dos sinais.

---

## 6. Método e limites

Conectividade e distâncias extraídas dos Gerbers (gerbonara/shapely), ilhas ligadas pelos furos. Valores do IGBT do datasheet ST (jun/1999) fornecido pelo projetista. Estimativas térmicas e de atraso de gate são de ordem de grandeza.
