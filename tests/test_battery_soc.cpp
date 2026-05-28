#include "BatterySoc.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

bool near(float actual, float expected, float tolerance)
{
    return std::fabs(actual - expected) <= tolerance;
}

void test_coulomb_counter_discharge()
{
    battery_soc::BatterySocConfig config;
    config.capacityAh = 1.0f;
    config.initialSocPercent = 100.0f;
    config.currentDirection = battery_soc::CurrentDirection::POSITIVE_DISCHARGES;
    config.voltageFilterAlpha = 1.0f;

    battery_soc::BatterySoc soc(config);
    soc.update(4.0f, 1.0f, 1800.0f);

    require(near(soc.getPercentage(), 50.0f, 0.01f), "1 A for 30 min on 1 Ah must remove 50% SOC");
    require(soc.getState() == battery_soc::EnergyState::DISCHARGING, "discharge must be reported");
}

void test_coulomb_counter_charge()
{
    battery_soc::BatterySocConfig config;
    config.capacityAh = 1.0f;
    config.initialSocPercent = 25.0f;
    config.currentDirection = battery_soc::CurrentDirection::POSITIVE_CHARGES;
    config.voltageFilterAlpha = 1.0f;

    battery_soc::BatterySoc soc(config);
    soc.update(3.9f, 0.5f, 1800.0f);

    require(near(soc.getPercentage(), 50.0f, 0.01f), "0.5 A charge for 30 min on 1 Ah must add 25% SOC");
    require(soc.getState() == battery_soc::EnergyState::CHARGING, "charge must be reported");
}

void test_voltage_correction_requires_rest_and_stability()
{
    battery_soc::BatterySocConfig config;
    config.capacityAh = 2.0f;
    config.initialSocPercent = 20.0f;
    config.currentDirection = battery_soc::CurrentDirection::POSITIVE_DISCHARGES;
    config.voltageFilterAlpha = 1.0f;
    config.stableVoltageTimeSeconds = 2.0f;
    config.voltageCorrectionGainPerSecond = 0.5f;

    battery_soc::BatterySoc soc(config);
    soc.update(4.2f, 0.0f, 1.0f);
    const float beforeStable = soc.getPercentage();
    soc.update(4.2f, 0.0f, 1.0f);
    soc.update(4.2f, 0.0f, 1.0f);
    const float afterStable = soc.getPercentage();

    require(near(beforeStable, 20.0f, 0.01f), "SOC must not be voltage-corrected before stability time");
    require(afterStable > beforeStable, "stable rest voltage must gradually correct SOC upward");
    require(afterStable < 100.0f, "voltage correction must be gradual");
}

void test_transient_voltage_drop_does_not_trigger_cutoff()
{
    battery_soc::BatterySocConfig config;
    config.initialSocPercent = 80.0f;
    config.voltageFilterAlpha = 1.0f;
    config.criticalDebounceSeconds = 5.0f;
    config.cutoffDelaySeconds = 10.0f;

    battery_soc::BatterySoc soc(config);
    soc.update(3.0f, 0.2f, 1.0f);
    soc.update(3.9f, 0.2f, 1.0f);

    require(soc.getState() != battery_soc::EnergyState::CRITICAL, "short voltage transient must not latch critical state");
    require(!soc.getCutOff(), "short voltage transient must not recommend cutoff");
}

void test_cutoff_after_continuous_critical_period()
{
    battery_soc::BatterySocConfig config;
    config.initialSocPercent = 50.0f;
    config.voltageFilterAlpha = 1.0f;
    config.criticalDebounceSeconds = 2.0f;
    config.cutoffDelaySeconds = 5.0f;

    battery_soc::BatterySoc soc(config);
    for (int i = 0; i < 5; ++i) {
        soc.update(3.0f, 0.0f, 1.0f);
    }

    require(soc.getState() == battery_soc::EnergyState::CRITICAL, "continuous critical voltage must latch critical state");
    require(soc.getCutOff(), "continuous critical voltage must recommend cutoff after configured delay");
}

void test_recovery_hysteresis()
{
    battery_soc::BatterySocConfig config;
    config.initialSocPercent = 50.0f;
    config.voltageFilterAlpha = 1.0f;
    config.criticalDebounceSeconds = 1.0f;
    config.recoveryDebounceSeconds = 2.0f;
    config.cutoffDelaySeconds = 20.0f;

    battery_soc::BatterySoc soc(config);
    soc.update(3.0f, 0.0f, 1.0f);
    require(soc.getState() == battery_soc::EnergyState::CRITICAL, "critical must latch before recovery test");

    soc.update(3.34f, 0.0f, 1.0f);
    require(soc.getState() == battery_soc::EnergyState::CRITICAL, "voltage below hysteresis margin must not recover");

    soc.update(3.5f, 0.0f, 1.0f);
    soc.update(3.5f, 0.0f, 1.0f);
    require(soc.getState() != battery_soc::EnergyState::CRITICAL, "safe voltage held through debounce must recover");
}

} // namespace

int main()
{
    test_coulomb_counter_discharge();
    test_coulomb_counter_charge();
    test_voltage_correction_requires_rest_and_stability();
    test_transient_voltage_drop_does_not_trigger_cutoff();
    test_cutoff_after_continuous_critical_period();
    test_recovery_hysteresis();

    std::cout << "All battery_soc tests passed\n";
    return 0;
}
