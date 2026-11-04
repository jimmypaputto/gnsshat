/*
 * Jimmy Paputto 2025
 */

#include <csignal>
#include <cstdio>
#include <memory>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>

#define PULSE_RATE_HZ 5
// Part of the pulse period the pulse lasts: 10% of 200 ms is 20 ms.
#define PULSE_WIDTH_RATIO 0.1f


using namespace JimmyPaputto;

GnssConfig createDefaultConfig()
{
    const auto pulse = TimepulsePinConfig::Pulse {
        PULSE_RATE_HZ, PULSE_WIDTH_RATIO
    };
    return GnssConfig {
        .measurementRate_Hz = PULSE_RATE_HZ,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = pulse,
            .pulseWhenNoFix = pulse,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt
    };
}

std::thread signalsHandler(IGnssHat& ubxHat)
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
    std::unique_ptr<IGnssHat> ubxHat(IGnssHat::create());
    ubxHat->hardResetUbloxSom_ColdStart();
    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }

    printf("Startup done, ublox configured\r\n");

    // Fails while the kernel PPS driver holds GPIO 5 (dtoverlay=pps-gpio).
    if (!ubxHat->enableTimepulse())
    {
        printf("Failed to enable timepulse, exit\r\n");
        return -1;
    }

    auto signalsInterrupt = signalsHandler(*ubxHat);

    uint32_t counter = 0;
    while (ubxHat->timepulse())
    {
        const auto time = ubxHat->navigation().pvt.utc;
        printf("Timepulse: %u, %02d:%02d:%02d\r\n",
            counter++, time.hh, time.mm, time.ss);
    }

    signalsInterrupt.join();
    return 0;
}
