#include "BatterySoc.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

int stateCode(battery_soc::EnergyState state)
{
    switch (state) {
    case battery_soc::EnergyState::NORMAL:
        return 0;
    case battery_soc::EnergyState::LOWER:
        return 1;
    case battery_soc::EnergyState::CRITICAL:
        return 2;
    case battery_soc::EnergyState::CHARGING:
        return 3;
    case battery_soc::EnergyState::DISCHARGING:
        return 4;
    default:
        return -1;
    }
}

float clamp(float value, float minValue, float maxValue)
{
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

float estimateOpenCircuitVoltage(float actualSocPercent)
{
    const float soc = clamp(actualSocPercent, 0.0f, 100.0f);

    if (soc <= 10.0f) {
        return 3.20f + (3.35f - 3.20f) * (soc / 10.0f);
    }
    if (soc <= 50.0f) {
        return 3.35f + (3.70f - 3.35f) * ((soc - 10.0f) / 40.0f);
    }
    if (soc <= 90.0f) {
        return 3.70f + (4.00f - 3.70f) * ((soc - 50.0f) / 40.0f);
    }
    return 4.00f + (4.10f - 4.00f) * ((soc - 90.0f) / 10.0f);
}

bool buzzerIsActive(float timeSeconds)
{
    if (timeSeconds <= 0.0f) {
        return false;
    }

    const int eventIndex = static_cast<int>(timeSeconds / 1800.0f);
    const float timeInsideEventWindow = timeSeconds - static_cast<float>(eventIndex) * 1800.0f;
    const float durationSeconds = 10.0f + static_cast<float>(eventIndex % 3) * 10.0f;

    return timeInsideEventWindow < durationSeconds;
}

} // namespace

int main()
{
    const float batteryCapacityAh = 1.8f;
    const float baseCurrentA = 0.09659f;
    const float buzzerAdditionalCurrentA = 0.120f;
    const float samplePeriodSeconds = 0.5f;
    const float maxSimulationSeconds = 22.0f * 3600.0f;
    const char* csvPath = "battery_soc_1800mah_profile.csv";

    battery_soc::BatterySocConfig config;
    config.capacityAh = batteryCapacityAh;
    config.initialSocPercent = 100.0f;
    config.currentDirection = battery_soc::CurrentDirection::POSITIVE_DISCHARGES;
    config.voltageFull = 4.10f;
    config.voltageLow = 3.45f;
    config.voltageCritical = 3.30f;
    config.voltageEmpty = 3.20f;
    config.lowSocPercent = 25.0f;
    config.criticalSocPercent = 10.0f;
    config.restCurrentThresholdA = 0.020f;
    config.voltageFilterAlpha = 0.12f;
    config.criticalDebounceSeconds = 30.0f;
    config.cutoffDelaySeconds = 120.0f;
    config.recoveryDebounceSeconds = 30.0f;

    battery_soc::BatterySoc soc(config);
    std::ofstream csv(csvPath);
    require(csv.is_open(), "must create simulation CSV");

    csv << "time_s,voltage_v,soc_percent,current_ma,state_code,state,cutoff\n";
    csv << std::fixed << std::setprecision(3);

    float actualRemainingAh = batteryCapacityAh;
    bool sawBuzzer = false;
    bool sawLow = false;
    bool sawCritical = false;
    bool sawCutoff = false;
    float lastSoc = 100.0f;

    for (float timeSeconds = 0.0f; timeSeconds <= maxSimulationSeconds; timeSeconds += samplePeriodSeconds) {
        const bool buzzerActive = buzzerIsActive(timeSeconds);
        const float currentA = baseCurrentA + (buzzerActive ? buzzerAdditionalCurrentA : 0.0f);

        const float actualSocPercent = (actualRemainingAh / batteryCapacityAh) * 100.0f;
        const float loadSagV = currentA * 0.18f;
        const float buzzerTransientDropV = buzzerActive ? 0.035f : 0.0f;
        const float voltage = estimateOpenCircuitVoltage(actualSocPercent) - loadSagV - buzzerTransientDropV;

        const battery_soc::BatterySocSnapshot snapshot = soc.update(voltage, currentA, samplePeriodSeconds);

        csv << timeSeconds << ","
            << voltage << ","
            << snapshot.socPercent << ","
            << currentA * 1000.0f << ","
            << stateCode(snapshot.state) << ","
            << battery_soc::toString(snapshot.state) << ","
            << (snapshot.cutoffRecommended ? 1 : 0) << "\n";

        actualRemainingAh -= currentA * samplePeriodSeconds / 3600.0f;
        actualRemainingAh = clamp(actualRemainingAh, 0.0f, batteryCapacityAh);

        sawBuzzer = sawBuzzer || buzzerActive;
        sawLow = sawLow || snapshot.state == battery_soc::EnergyState::LOWER;
        sawCritical = sawCritical || snapshot.state == battery_soc::EnergyState::CRITICAL;
        sawCutoff = sawCutoff || snapshot.cutoffRecommended;
        lastSoc = snapshot.socPercent;

        if (snapshot.cutoffRecommended && timeSeconds > 1800.0f) {
            break;
        }
    }

    csv.close();

    require(sawBuzzer, "simulation must include buzzer current bursts");
    require(sawLow, "simulation must reach LOWER state");
    require(sawCritical, "simulation must reach CRITICAL state");
    require(sawCutoff, "simulation must recommend cutoff");
    require(lastSoc < 15.0f, "simulation must discharge battery close to critical SOC");

    std::cout << "Generated " << csvPath << "\n";
    return 0;
}
