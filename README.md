Objetivo

Como sistema embarcado IoT, quero interpretar o estado energético da bateria de forma inteligente e desacoplada do hardware para aumentar a confiabilidade da medição, proteger a bateria contra descarga profunda e permitir reutilização da solução entre ESP-IDF e Arduino.

Descrição detalhada (o que deve ser feito?)

Desenvolver uma biblioteca em C/C++ responsável pela interpretação energética da bateria baseada nas leituras de tensão e corrente provenientes do INA226.

A biblioteca deverá ser desacoplada:

do driver INA226

do ESP-IDF

do Arduino

de qualquer HAL específica

Seu objetivo será centralizar toda a lógica de:

cálculo de SOC (State of Charge)

filtragem energética

histerese

debounce temporal

estados energéticos

recomendação de cutoff

A solução deverá utilizar:

integração de corrente (Coulomb Counter simplificado)

correção gradual por tensão estabilizada

proteção contra oscilações transitórias de carga

A biblioteca não deverá executar ações de hardware diretamente, apenas recomendar estados e ações ao firmware principal.

Exemplo:

recomendar cutoff

recomendar estado crítico

recomendar modo de baixo consumo

A lógica deverá funcionar igualmente em:

ESP-IDF

Arduino

permitindo reaproveitamento entre projetos.

Requisitos / Critérios de Aceite

Critérios funcionais

SOC baseado em corrente integrada

Dado que o sistema esteja operando normalmente, quando houver consumo de corrente ao longo do tempo, então o SOC deverá ser atualizado através da integração da corrente medida.

Correção gradual por tensão

Dado que o sistema esteja em baixa carga ou repouso, quando a tensão estabilizar, então o SOC deverá ser corrigido gradualmente utilizando a tensão da bateria.

Filtragem de oscilações transitórias

Dado que ocorram quedas momentâneas de tensão causadas por buzzer, rádio ou Wi-Fi, quando essas oscilações forem transitórias, então o sistema não deverá interpretar imediatamente como bateria crítica.

Estados energéticos

Dado que a bateria esteja operando em diferentes níveis energéticos, então a biblioteca deverá informar estados mínimos:

NORMAL

LOW

CRITICAL

CHARGING

DISCHARGING

Recomendação de cutoff

Dado que a bateria permaneça abaixo do limite crítico por período contínuo configurado, então a biblioteca deverá retornar recomendação de cutoff ao firmware.

Histerese de recuperação

Dado que o sistema entre em estado crítico, quando a tensão retornar para faixa segura, então a recuperação deverá ocorrer apenas após margem configurada de histerese.

Desacoplamento de hardware

Dado que a biblioteca seja utilizada em ESP-IDF ou Arduino, então nenhuma dependência direta de hardware, I2C ou SDK específico deverá existir no núcleo SOC.

Interface padronizada

Dado que o firmware envie:

tensão

corrente

delta de tempo

quando a biblioteca processar os dados, então ela deverá retornar:

percentual de SOC

estado energético

recomendação de cutoff

DEFINED to READY

Descrição geral (COMO deve ser feito?)

API esperada

Exemplo:

soc.update(
    voltage,
    current,
    delta_time_seconds
);

soc.getPercentage();
soc.getState();
soc.getCutOff();

