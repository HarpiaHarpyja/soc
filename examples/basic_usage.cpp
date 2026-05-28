#include "BatterySoc.h"

#include <iostream>

int main()
{
    battery_soc::BatterySocConfig config;
    config.capacityAh = 2.2f;
    config.initialSocPercent = 80.0f;
    config.currentDirection = battery_soc::CurrentDirection::POSITIVE_DISCHARGES;

    battery_soc::BatterySoc soc(config);

    const float voltage = 3.82f;
    const float currentA = 0.18f;
    const float deltaTimeSeconds = 1.0f;

    const battery_soc::BatterySocSnapshot snapshot = soc.update(voltage, currentA, deltaTimeSeconds);

    std::cout << "SOC: " << snapshot.socPercent << "%\n";
    std::cout << "State: " << battery_soc::toString(snapshot.state) << "\n";
    std::cout << "Cutoff: " << (snapshot.cutoffRecommended ? "yes" : "no") << "\n";

    return 0;
}
