# Battery SOC

Biblioteca C++11 portavel para interpretar o estado energetico de uma bateria a partir de leituras externas de tensao e corrente, como as fornecidas por um INA226.

O nucleo nao depende de INA226, ESP-IDF, Arduino, I2C ou qualquer HAL. O firmware principal fica responsavel por ler o hardware e chamar:

```cpp
soc.update(voltage, current, delta_time_seconds);
```

A biblioteca retorna percentual de SOC, estado energetico e recomendacoes para cutoff ou baixo consumo.

## Recursos

- SOC por integracao de corrente, em modelo simplificado de Coulomb Counter.
- Correcao gradual por tensao apenas quando a bateria esta em repouso ou baixa carga.
- Filtro IIR para tensao, reduzindo impacto de quedas transitorias causadas por radio, Wi-Fi, buzzer ou cargas de pico.
- Debounce temporal para estado critico e recomendacao de cutoff.
- Histerese de recuperacao apos estado critico.
- Estados minimos: `NORMAL`, `LOWER`, `CRITICAL`, `CHARGING` e `DISCHARGING`.
- Configuracao do sentido da corrente, porque projetos com INA226 podem adotar convencoes diferentes.

## Estrutura

```text
include/BatterySoc.h      API publica
src/BatterySoc.cpp        Implementacao portavel
examples/basic_usage.cpp  Exemplo minimo
tests/test_battery_soc.cpp Testes sem framework externo
CMakeLists.txt            Build opcional por CMake
```

## API basica

```cpp
#include "BatterySoc.h"

battery_soc::BatterySocConfig config;
config.capacityAh = 2.2f;
config.initialSocPercent = 80.0f;
config.currentDirection = battery_soc::CurrentDirection::POSITIVE_DISCHARGES;

battery_soc::BatterySoc soc(config);

auto snapshot = soc.update(3.82f, 0.18f, 1.0f);

float percentage = soc.getPercentage();
battery_soc::EnergyState state = soc.getState();
bool cutoff = soc.getCutOff();
```

## Convencao de corrente

A integracao usa uma corrente interna assinada:

- corrente positiva interna carrega a bateria;
- corrente negativa interna descarrega a bateria.

Como a polaridade medida depende do hardware e do shunt, a configuracao aceita:

```cpp
config.currentDirection = battery_soc::CurrentDirection::POSITIVE_DISCHARGES;
```

Use `POSITIVE_DISCHARGES` quando a leitura positiva representa consumo da bateria. Use `POSITIVE_CHARGES` quando a leitura positiva representa carga entrando na bateria.

## Principais parametros

```cpp
config.capacityAh = 2.0f;
config.voltageEmpty = 3.20f;
config.voltageLow = 3.45f;
config.voltageCritical = 3.30f;
config.voltageFull = 4.20f;

config.lowSocPercent = 25.0f;
config.criticalSocPercent = 10.0f;

config.restCurrentThresholdA = 0.05f;
config.voltageFilterAlpha = 0.15f;
config.stableVoltageTimeSeconds = 30.0f;
config.voltageCorrectionGainPerSecond = 0.002f;

config.criticalDebounceSeconds = 10.0f;
config.cutoffDelaySeconds = 60.0f;
config.recoveryVoltageHysteresis = 0.08f;
config.recoverySocHysteresisPercent = 5.0f;
```

## Comportamento

1. A cada `update`, a corrente e integrada ao longo de `delta_time_seconds`.
2. A tensao e filtrada por IIR usando `voltageFilterAlpha`.
3. Se a bateria estiver em repouso e a tensao permanecer estavel por `stableVoltageTimeSeconds`, o SOC e corrigido gradualmente para o SOC estimado pela tensao.
4. O estado `CRITICAL` so e travado apos `criticalDebounceSeconds` em condicao critica.
5. `getCutOff()` so retorna `true` quando a condicao critica permanece por `cutoffDelaySeconds`.
6. A recuperacao de `CRITICAL` exige margem de histerese em tensao e SOC durante `recoveryDebounceSeconds`.

## Build e testes

Com CMake:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build
```

Compilacao direta:

```bash
g++ -std=c++11 -I include src/BatterySoc.cpp tests/test_battery_soc.cpp -o build/battery_soc_tests.exe
build/battery_soc_tests.exe
```

## Simulacao CSV para grafico

O teste `battery_soc_profile_simulation` simula uma bateria de 1800 mAh, 3.7 V nominal e 4.1 V como 100% de carga. O consumo continuo usado no perfil e 96.59 mA, com acionamento de buzzer a cada 30 minutos. Cada evento de buzzer dura 10, 20 ou 30 segundos em ciclo, adicionando 120 mA ao consumo base.

Execute:

```bash
cmake --build build-mingw
build-mingw/battery_soc_profile_simulation.exe
```

Ou via CTest:

```bash
ctest --test-dir build-mingw -R battery_soc_profile_simulation
```

O arquivo gerado fica no diretorio de execucao com o nome:

```text
battery_soc_1800mah_profile.csv
```

Colunas do CSV:

```text
time_s,voltage_v,soc_percent,current_ma,state_code,state,cutoff
```

Para um unico grafico com eixo X em tempo, use:

- `voltage_v`: tensao em V.
- `soc_percent`: percentual de SOC.
- `current_ma`: corrente em mA.
- `state_code`: estado numerico para plotagem (`0=NORMAL`, `1=LOWER`, `2=CRITICAL`, `3=CHARGING`, `4=DISCHARGING`).
- `cutoff`: recomendacao de cutoff (`0=false`, `1=true`).

## Integracao com ESP-IDF ou Arduino

O firmware deve apenas coletar as leituras e passar os valores para a biblioteca:

```cpp
float voltage = ina226_read_voltage();
float current = ina226_read_current();
float dt = seconds_since_last_sample();

auto snapshot = soc.update(voltage, current, dt);

if (snapshot.cutoffRecommended) {
    // Firmware decide como desligar cargas, salvar estado ou entrar em protecao.
}
```

A biblioteca nao executa nenhuma acao de hardware diretamente.
