/*
 * Jimmy Paputto 2025
 */

#include <cstdio>
#include <memory>

#include <jimmypaputto/GnssHat.hpp>


auto main() -> int
{
    // Under systemd stdout is a socket -> glibc fully buffers it and the
    // startup lines reach journald only at exit, with misleading timestamps.
    setvbuf(stdout, nullptr, _IOLBF, 0);

    std::unique_ptr<JimmyPaputto::IGnssHat> ubxHat(
        JimmyPaputto::IGnssHat::create());
    // Ctrl+C and systemctl stop end joinForwardForGpsd() below
    ubxHat->stopOnSignals();

    JimmyPaputto::GnssConfig config;
    config.measurementRate_Hz = 1;
    config.dynamicModel = JimmyPaputto::EDynamicModel::Portable;
    config.timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.1f },
        .pulseWhenNoFix = std::nullopt,
        .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    config.geofencing = std::nullopt;

    ubxHat->softResetUbloxSom_HotStart();

    printf("Starting GNSS module...\n");
    const bool isStartupDone = ubxHat->start(config);
    if (!isStartupDone)
    {
        printf("Failed to start GNSS module\n");
        return 1;
    }

    printf("Startup done succesfully\r\n");

    // Start NMEA forwarding for gpsd
    printf("Creating virtual serial port for gpsd...\r\n");
    if (!ubxHat->startForwardForGpsd())
    {
        printf("Failed to start NMEA forwarding\r\n");
        return 1;
    }

    printf("NMEA forwarding started!\r\n");
    printf("Virtual serial port: %s\r\n", ubxHat->getGpsdDevicePath().c_str());
    printf("To use with gpsd and PPS, run in another terminal:\r\n");
    printf("\tsudo gpsd -N -D5 %s\r\n", ubxHat->getGpsdDevicePath().c_str());
    printf("\tsudo gpsd -N -D5 -S 2222 %s /dev/pps0  # With PPS support\r\n",
        ubxHat->getGpsdDevicePath().c_str());
    printf("\tcgps  # To view gpsd data\r\n");
    printf("\tgpsmon  # To monitor gpsd and PPS\r\n");
    printf("Daemon is running... Press Ctrl+C to stop\r\n");

    ubxHat->joinForwardForGpsd();

    // The destructor removes the virtual device and releases the HAT
    printf("\nDaemon stopped\r\n");
    return 0;
}
