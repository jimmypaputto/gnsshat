/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <csignal>
#include <cstdio>
#include <memory>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


void printNavigation(const JimmyPaputto::Navigation& navigation)
{
    printf("GNSS Navigation Data:\r\n");
    printf("\tFix Quality: %s\r\n", JimmyPaputto::Utils::eFixQuality2string(navigation.pvt.fixQuality).c_str());
    printf("\tFix Status: %s\r\n", JimmyPaputto::Utils::eFixStatus2string(navigation.pvt.fixStatus).c_str());
    printf("\tFix Type: %s\r\n", JimmyPaputto::Utils::eFixType2string(navigation.pvt.fixType).c_str());
    printf("\tVisible Satellites: %d\n", navigation.pvt.visibleSatellites);
    printf("\tLatitude: %.6f°\n", navigation.pvt.latitude);
    printf("\tLongitude: %.6f°\n", navigation.pvt.longitude);
    printf("\tAltitude: %.2f m\n", navigation.pvt.altitude);
    printf("\tTime: %s\n", JimmyPaputto::Utils::utcTimeFromGnss_ISO8601(navigation.pvt).c_str());
    printf("\tTime accuracy: %d nanoseconds\n", navigation.pvt.utc.accuracy);
    printf("\tDate valid: %s\n", navigation.pvt.date.valid ? "true" : "false");
}

JimmyPaputto::GnssConfig createDefaultConfig()
{
    return JimmyPaputto::GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = JimmyPaputto::EDynamicModel::Stationary,
        .timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
            .active = true,
            .fixedPulse = JimmyPaputto::TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = std::nullopt
    };
}

std::thread signalsHandler(JimmyPaputto::IGnssHat& ubxHat)
{
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    // Blocks the signals in the calling thread (main); the new thread
    // inherits the mask. Library threads block them on their own.
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    return std::thread([&ubxHat, signals] {
        int s;
        sigwait(&signals, &s);
        ubxHat.shutdown();
    });
}

auto main() -> int
{
    std::unique_ptr<JimmyPaputto::IGnssHat> ubxHat(
        JimmyPaputto::IGnssHat::create());
    ubxHat->softResetUbloxSom_HotStart();
    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    auto signalsInterrupt = signalsHandler(*ubxHat);

    while (const auto navigation = ubxHat->waitAndGetFreshNavigation())
        printNavigation(*navigation);

    signalsInterrupt.join();
    return 0;
}
