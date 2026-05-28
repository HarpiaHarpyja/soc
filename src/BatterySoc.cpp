#include "BatterySoc.h"

#include <cmath>

namespace battery_soc {

namespace {

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

float absf(float value)
{
    return value < 0.0f ? -value : value;
}

} // namespace

BatterySoc::BatterySoc(const BatterySocConfig& config)
    : config_(config),
      snapshot_(),
      criticalTimerSeconds_(0.0f),
      recoveryTimerSeconds_(0.0f),
      stableVoltageTimerSeconds_(0.0f),
      previousFilteredVoltage_(config.initialSocPercent >= 100.0f ? config.voltageFull : config.voltageEmpty),
      criticalLatched_(false)
{
    const float initialVoltage = config_.voltageEmpty
        + (config_.voltageFull - config_.voltageEmpty) * clamp(config_.initialSocPercent, 0.0f, 100.0f) / 100.0f;
    reset(config_.initialSocPercent, initialVoltage);
}

BatterySocSnapshot BatterySoc::update(float voltage, float currentA, float deltaTimeSeconds)
{
    if (deltaTimeSeconds <= 0.0f || config_.capacityAh <= 0.0f) {
        return snapshot_;
    }

    const float signedCurrentA = signedBatteryCurrent(currentA);
    const bool charging = isCharging(signedCurrentA);
    const bool discharging = isDischarging(signedCurrentA);

    integrateCurrent(signedCurrentA, deltaTimeSeconds);
    filterVoltage(voltage);
    correctByStableVoltage(signedCurrentA, deltaTimeSeconds);
    updateCriticalLatch(deltaTimeSeconds);
    updateState(charging, discharging);

    snapshot_.charging = charging;
    snapshot_.discharging = discharging;
    snapshot_.lowPowerRecommended = snapshot_.state == EnergyState::LOWER || snapshot_.state == EnergyState::CRITICAL;

    return snapshot_;
}

void BatterySoc::reset(float socPercent, float voltage)
{
    snapshot_.socPercent = clamp(socPercent, 0.0f, 100.0f);
    snapshot_.filteredVoltage = voltage;
    snapshot_.state = EnergyState::NORMAL;
    snapshot_.cutoffRecommended = false;
    snapshot_.lowPowerRecommended = false;
    snapshot_.critical = false;
    snapshot_.charging = false;
    snapshot_.discharging = false;

    criticalTimerSeconds_ = 0.0f;
    recoveryTimerSeconds_ = 0.0f;
    stableVoltageTimerSeconds_ = 0.0f;
    previousFilteredVoltage_ = voltage;
    criticalLatched_ = false;
}

float BatterySoc::getPercentage() const
{
    return snapshot_.socPercent;
}

EnergyState BatterySoc::getState() const
{
    return snapshot_.state;
}

bool BatterySoc::getCutOff() const
{
    return snapshot_.cutoffRecommended;
}

bool BatterySoc::shouldEnterLowPower() const
{
    return snapshot_.lowPowerRecommended;
}

BatterySocSnapshot BatterySoc::getSnapshot() const
{
    return snapshot_;
}

const BatterySocConfig& BatterySoc::getConfig() const
{
    return config_;
}

float BatterySoc::signedBatteryCurrent(float currentA) const
{
    if (config_.currentDirection == CurrentDirection::POSITIVE_CHARGES) {
        return currentA;
    }
    return -currentA;
}

bool BatterySoc::isAtRest(float signedCurrentA) const
{
    return absf(signedCurrentA) <= config_.restCurrentThresholdA;
}

bool BatterySoc::isCharging(float signedCurrentA) const
{
    return signedCurrentA >= config_.chargeCurrentThresholdA;
}

bool BatterySoc::isDischarging(float signedCurrentA) const
{
    return signedCurrentA <= -config_.dischargeCurrentThresholdA;
}

float BatterySoc::voltageToSoc(float voltage) const
{
    if (config_.voltageFull <= config_.voltageEmpty) {
        return snapshot_.socPercent;
    }
    const float ratio = (voltage - config_.voltageEmpty) / (config_.voltageFull - config_.voltageEmpty);
    return clamp(ratio * 100.0f, 0.0f, 100.0f);
}

void BatterySoc::integrateCurrent(float signedCurrentA, float deltaTimeSeconds)
{
    const float deltaAh = signedCurrentA * deltaTimeSeconds / 3600.0f;
    const float deltaPercent = (deltaAh / config_.capacityAh) * 100.0f;
    snapshot_.socPercent = clamp(snapshot_.socPercent + deltaPercent, 0.0f, 100.0f);
}

void BatterySoc::filterVoltage(float voltage)
{
    const float alpha = clamp(config_.voltageFilterAlpha, 0.0f, 1.0f);
    previousFilteredVoltage_ = snapshot_.filteredVoltage;
    snapshot_.filteredVoltage = previousFilteredVoltage_ + alpha * (voltage - previousFilteredVoltage_);
}

void BatterySoc::correctByStableVoltage(float signedCurrentA, float deltaTimeSeconds)
{
    if (!isAtRest(signedCurrentA)) {
        stableVoltageTimerSeconds_ = 0.0f;
        return;
    }

    if (absf(snapshot_.filteredVoltage - previousFilteredVoltage_) <= config_.stableVoltageDelta) {
        stableVoltageTimerSeconds_ += deltaTimeSeconds;
    } else {
        stableVoltageTimerSeconds_ = 0.0f;
    }

    if (stableVoltageTimerSeconds_ < config_.stableVoltageTimeSeconds) {
        return;
    }

    const float targetSoc = voltageToSoc(snapshot_.filteredVoltage);
    const float correction = clamp(config_.voltageCorrectionGainPerSecond * deltaTimeSeconds, 0.0f, 1.0f);
    snapshot_.socPercent += (targetSoc - snapshot_.socPercent) * correction;
    snapshot_.socPercent = clamp(snapshot_.socPercent, 0.0f, 100.0f);
}

void BatterySoc::updateCriticalLatch(float deltaTimeSeconds)
{
    const bool rawCritical = snapshot_.socPercent <= config_.criticalSocPercent
        || snapshot_.filteredVoltage <= config_.voltageCritical;
    const bool recovered = snapshot_.socPercent >= config_.criticalSocPercent + config_.recoverySocHysteresisPercent
        && snapshot_.filteredVoltage >= config_.voltageCritical + config_.recoveryVoltageHysteresis;

    if (criticalLatched_) {
        if (recovered) {
            recoveryTimerSeconds_ += deltaTimeSeconds;
            if (recoveryTimerSeconds_ >= config_.recoveryDebounceSeconds) {
                criticalLatched_ = false;
                criticalTimerSeconds_ = 0.0f;
                recoveryTimerSeconds_ = 0.0f;
            }
        } else {
            recoveryTimerSeconds_ = 0.0f;
            criticalTimerSeconds_ += deltaTimeSeconds;
        }
    } else if (rawCritical) {
        criticalTimerSeconds_ += deltaTimeSeconds;
        if (criticalTimerSeconds_ >= config_.criticalDebounceSeconds) {
            criticalLatched_ = true;
        }
    } else {
        criticalTimerSeconds_ = 0.0f;
        recoveryTimerSeconds_ = 0.0f;
    }

    snapshot_.critical = criticalLatched_;
    snapshot_.cutoffRecommended = criticalLatched_ && criticalTimerSeconds_ >= config_.cutoffDelaySeconds;
}

void BatterySoc::updateState(bool charging, bool discharging)
{
    if (criticalLatched_) {
        snapshot_.state = EnergyState::CRITICAL;
    } else if (charging) {
        snapshot_.state = EnergyState::CHARGING;
    } else if (snapshot_.socPercent <= config_.lowSocPercent || snapshot_.filteredVoltage <= config_.voltageLow) {
        snapshot_.state = EnergyState::LOWER;
    } else if (discharging) {
        snapshot_.state = EnergyState::DISCHARGING;
    } else {
        snapshot_.state = EnergyState::NORMAL;
    }
}

const char* toString(EnergyState state)
{
    switch (state) {
    case EnergyState::NORMAL:
        return "NORMAL";
    case EnergyState::LOWER:
        return "LOWER";
    case EnergyState::CRITICAL:
        return "CRITICAL";
    case EnergyState::CHARGING:
        return "CHARGING";
    case EnergyState::DISCHARGING:
        return "DISCHARGING";
    default:
        return "UNKNOWN";
    }
}

} // namespace battery_soc
