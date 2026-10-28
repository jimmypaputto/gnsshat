/*
 * Jimmy Paputto 2025
 */

#include <csignal>
#include <cstdio>
#include <memory>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

void print(const Navigation& navigation)
{
    printf("Time from GNSS: %s\r\n",
           Utils::utcTimeFromGnss_ISO8601(navigation.pvt).c_str());
    printf("Lat: %f, Lon: %f, Alt: %f\r\n",
           navigation.pvt.latitude,
           navigation.pvt.longitude,
           navigation.pvt.altitude);
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

    GnssConfig config;
    config.measurementRate_Hz = 1;
    config.dynamicModel = EDynamicModel::Portable;
    config.timepulsePinConfig = TimepulsePinConfig {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.1f },
        .pulseWhenNoFix = std::nullopt,
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    config.geofencing = std::nullopt;

    ubxHat->softResetUbloxSom_HotStart();
    const bool isStartupDone = ubxHat->start(config);
    if (!isStartupDone)
    {
        printf("Failed to start GNSS module\r\n");
        return 1;
    }

    printf("Startup done succesfully\r\n");

    if (ubxHat->startForwardForGpsd())
    {
        printf("NMEA forwarding started!\n");
        printf("Virtual serial port: %s\n", ubxHat->getGpsdDevicePath().c_str());
        printf("\nTo use with gpsd, run:\n");
        printf("  sudo gpsd %s\n", ubxHat->getGpsdDevicePath().c_str());
        printf("  cgps  # To view gpsd data\n");
    }
    else
    {
        printf("Failed to start NMEA forwarding\n");
        return 1;
    }

    auto signalsInterrupt = signalsHandler(*ubxHat);

    // You can do whatever you want here, nmea forwarder is running
    // in background without collisions with your application,
    // you can use gpsd simultaneously.
    while (const auto navigation = ubxHat->waitAndGetFreshNavigation())
        print(*navigation);

    // Wakes sigwait if the loop ends without a signal (e.g. a break added
    // to the loop or a future device error).
    pthread_kill(signalsInterrupt.native_handle(), SIGINT);
    signalsInterrupt.join();

    printf("\nShutting down...\r\n");
    return 0;  // the destructor stops the forwarder and removes its device
}
