/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


JimmyPaputto::GnssConfig createDefaultConfig()
{
    return JimmyPaputto::GnssConfig {
        .measurementRate_Hz = 10,
        .dynamicModel = JimmyPaputto::EDynamicModel::Stationary,
        .timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
            .active = true,
            .fixedPulse = JimmyPaputto::TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = std::nullopt,
        .timing = std::nullopt
    };
}

// Returns false if the wait is stopped before the fix.
static bool waitForFix(JimmyPaputto::IGnssHat& hat)
{
    while (const auto navigation = hat.waitAndGetFreshNavigation())
    {
        if (navigation->pvt.fixStatus == JimmyPaputto::EFixStatus::Active)
            return true;
    }
    return false;
}

auto main() -> int
{
    const auto gnssConfig = createDefaultConfig();
    std::unique_ptr<JimmyPaputto::IGnssHat> ubxHat(
        JimmyPaputto::IGnssHat::create());
    ubxHat->stopOnSignals();
    ubxHat->hardResetUbloxSom_ColdStart();
    const auto isStartupDone = ubxHat->start(gnssConfig);
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }
    printf("Startup done, ublox configured\r\n");

    ubxHat->hardResetUbloxSom_ColdStart();
    auto start = std::chrono::high_resolution_clock::now();
    if (!waitForFix(*ubxHat))
        return 0;
    auto stop = std::chrono::high_resolution_clock::now();
    auto time2fix =
        std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    printf("Cold start took %ld ms\r\n", time2fix);

    printf("Wait 40s to collect data for hot start\r\n");
    std::this_thread::sleep_for(std::chrono::seconds(40));
    printf("Performing hot start\r\n");

    ubxHat->softResetUbloxSom_HotStart();
    start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    if (!waitForFix(*ubxHat))
        return 0;
    stop = std::chrono::high_resolution_clock::now();
    time2fix =
        std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    printf("Hot start took %ld ms\r\n", time2fix);

    return 0;
}
