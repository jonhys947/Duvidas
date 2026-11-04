# Firmware – MARS Módulo de Ignição (CH32V203K8T6)

Motor de 4 cilindros, ordem 1-3-4-2, centelha perdida; roda fônica **60-2** com sensor **VR** (NCV1124 → PB3).

## Estado

| Parte | Situação |
|---|---|
| Núcleo (`core/`): decodificador 60-2, avanço/dwell, agendamento, tabelas, configuração, sensores | ✅ escrito e testado no PC (`tests/`) |
| Camada de hardware CH32V203 (timers, GPIO, ADC, I2C, CAN, EEPROM) | ⏳ próxima etapa, quando houver acesso ao PC Linux com o WCH-LinkE |
| Protocolo com o ESP32-C3 (I2C) | ⏳ proposta abaixo, a confirmar |
| Mensagens CAN | ⏳ a definir quando os consumidores forem conhecidos |

## Testes no PC

```
cd firmware/tests && make test
```

Simula a roda 60-2 com rotação constante, partida acelerando (150 → 900 rpm), dente perdido, borda de ruído e início em ângulo qualquer. Confere sincronismo em ≤ 2 voltas, leitura de rpm, ângulo das centelhas (< 0,3°), duração do dwell (< 3%), limitador de rotação, watchdog de dwell, tabelas, CRC da configuração e conversão de MAP/IAT.

## Arquitetura

```
borda do VR (captura de timer, PB3) ──► decoder_on_edge() ──► ignition_on_tooth() ──► eventos com instante absoluto
                                                                                       │
                                     comparador de timer dispara no instante ◄─────────┘
                                     └─► ignition_output() ──► PA6/PA7 (IGN_A/IGN_B)

laço principal (≈ 5 ms): ADC (MAP, IAT) ──► ignition_update() (avanço, dwell, limitador)
                         ignition_watchdog(), decoder_check_timeout(), IGN_ENABLE
```

- **Ângulos** em décimos de grau após o PMS do cilindro 1 (0–3599). Saída A = cilindros 1/4 (PMS em 0°), saída B = cilindros 3/2 (PMS em 180°).
- **Agendamento:** a cada dente, os eventos que caem antes da próxima borda são convertidos em tempo com o período do dente atual (antes da falha, o vão vale 3 dentes).
- **Sincronismo:** duas falhas separadas por exatamente 58 dentes. Falha fora do lugar ou 58 dentes sem falha derrubam o sincronismo; bordas com intervalo < 1/4 do período são ruído.
- **Proteções em software** (além do limitador de hardware de ≈ 6 ms): dwell máximo de 4,8 ms, tempo mínimo desligado entre centelhas, corte por rotação com histerese, watchdog que força a centelha se o dwell passar do máximo, e `IGN_ENABLE` só com sincronismo.

## Pinagem usada (do esquemático)

| Função | Pino |
|---|---|
| VR condicionado (NCV1124 OUT1) | PB3 (TIM2_CH2, remap) |
| Hall condicionado | PA15 |
| CKP de outra ECU | PA0 |
| IGN_ENABLE (habilita as duas saídas) | PA1 |
| SEL_5V (modo 5 V da saída) | PA5 |
| IGN_A / IGN_B | PA6 / PA7 |
| MAP / IAT (ADC) | PA2 (IN2) / PA3 (IN3) |
| I2C1 SCL / SDA (24C256 0x50, AHT20 0x38, BMP280 0x76/77) | PB6 / PB7 |
| CAN RX / TX | PA11 / PA12 |
| UART1 TX / RX (J3) | PA9 / PA10 |
| Botão SELECT | PA4 (pull-up interno) |
| LED bicolor | PB0 (via 1 kΩ) / PB1 |

## Configuração (`core/config.h`)

Estrutura única com mágico, versão, tamanho e CRC-16, gravada na 24C256. Contém: ângulo do dente 0, tipo de bobina, limites (corte, partida, dwell máximo), mapa de avanço 16 (rpm) × 12 (MAP), correção por IAT, dwell por tensão com teto por rotação e calibração do MAP e do NTC.

Os valores padrão servem só para bancada. **O ângulo do dente 0 (`tooth0_deg10`) precisa ser medido no motor** com lâmpada de ponto.

## Decisões e pendências

- **Tensão da bateria:** virá de um **INA219** no mesmo I2C (endereço 0x40–0x4F) e também pelo CAN. O dwell sai de uma tabela por tensão, limitado por um teto por rotação e pelo máximo de 4,8 ms. Sem nenhuma leitura, usa `vbat_default_mv` (12,0 V, conservador). Prioridade entre INA219 e CAN a definir na camada de hardware.
- **ESP32-C3 no I2C (decidido):** o **CH32 é o único mestre** e o ESP32-C3 é **escravo I2C**. O CH32 consulta o ESP periodicamente, envia dados ao vivo, recebe blocos de mapa, confere o CRC e grava na 24C256. O endereço do ESP precisa ficar fora de 0x38 (AHT20), 0x40–0x4F (INA219), 0x50–0x57 (24C256) e 0x76/0x77 (BMP280); sugestão **0x30**. Falta definir onde o ESP se liga fisicamente no SDA/SCL.
- **Endereços no I2C:** 0x30 ESP32-C3 (proposto), 0x38 AHT20, 0x40 INA219 (padrão, A0/A1 em GND), 0x50 24C256, 0x76 BMP280.
- **CAN:** camada separada; tabela de mensagens fica para quando os consumidores forem definidos.
- **Compilação/gravação:** toolchain RISC-V + SDK da WCH na linha de comando, gravação pelo WCH-LinkE, no PC Linux que será disponibilizado.
