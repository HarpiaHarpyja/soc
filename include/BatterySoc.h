#ifndef BATTERY_SOC_H
#define BATTERY_SOC_H

#include <stdint.h>

namespace battery_soc {

enum class EnergyState {
    NORMAL,
    LOWER,
    CRITICAL,
    CHARGING,
    DISCHARGING
};

enum class CurrentDirection {
    POSITIVE_DISCHARGES,
    POSITIVE_CHARGES
};

struct BatterySocConfig {
    float capacityAh = 2.0f;
    float initialSocPercent = 100.0f;

    float voltageEmpty = 3.20f;
    float voltageLow = 3.45f;
    float voltageCritical = 3.30f;
    float voltageFull = 4.20f;

    float lowSocPercent = 25.0f;
    float criticalSocPercent = 10.0f;
    float recoverySocHysteresisPercent = 5.0f;
    float recoveryVoltageHysteresis = 0.08f;

    float restCurrentThresholdA = 0.05f;
    float chargeCurrentThresholdA = 0.05f;
    float dischargeCurrentThresholdA = 0.05f;

    float voltageFilterAlpha = 0.15f;
    float stableVoltageDelta = 0.01f;
    float stableVoltageTimeSeconds = 30.0f;
    float voltageCorrectionGainPerSecond = 0.002f;

    float criticalDebounceSeconds = 10.0f;
    float recoveryDebounceSeconds = 10.0f;
    float cutoffDelaySeconds = 60.0f;

    CurrentDirection currentDirection = CurrentDirection::POSITIVE_DISCHARGES;
};

struct BatterySocSnapshot {
    float socPercent;
    float filteredVoltage;
    EnergyState state;
    bool cutoffRecommended;
    bool lowPowerRecommended;
    bool critical;
    bool charging;
    bool discharging;
};

class BatterySoc {
public:
    explicit BatterySoc(const BatterySocConfig& config = BatterySocConfig());

    BatterySocSnapshot update(float voltage, float currentA, float deltaTimeSeconds);
    void reset(float socPercent, float voltage);

    float getPercentage() const;
    EnergyState getState() const;
    bool getCutOff() const;
    bool shouldEnterLowPower() const;
    BatterySocSnapshot getSnapshot() const;
    const BatterySocConfig& getConfig() const;

private:
    BatterySocConfig config_;
    BatterySocSnapshot snapshot_;

    float criticalTimerSeconds_;
    float recoveryTimerSeconds_;
    float stableVoltageTimerSeconds_;
    float previousFilteredVoltage_;
    bool criticalLatched_;

    float signedBatteryCurrent(float currentA) const;
    bool isAtRest(float signedCurrentA) const;
    bool isCharging(float signedCurrentA) const;
    bool isDischarging(float signedCurrentA) const;
    float voltageToSoc(float voltage) const;
    void integrateCurrent(float signedCurrentA, float deltaTimeSeconds);
    void filterVoltage(float voltage);
    void correctByStableVoltage(float signedCurrentA, float deltaTimeSeconds);
    void updateCriticalLatch(float deltaTimeSeconds);
    void updateState(bool charging, bool discharging);
};

const char* toString(EnergyState state);

} // namespace battery_soc

#endif // BATTERY_SOC_H
