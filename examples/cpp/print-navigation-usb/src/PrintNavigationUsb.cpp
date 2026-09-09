/*
 * Jimmy Paputto 2026
 */

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


void printNavigation(const JimmyPaputto::Navigation& navigation)
{
    printf("GNSS Navigation Data:\r\n");
    printf("\tFix Quality: %s\r\n", JimmyPaputto::Utils::eFixQuality2string(navigation.pvt.fixQuality).c_str());
    printf("\tFix Status: %s\r\n", JimmyPaputto::Utils::eFixStatus2string(navigation.pvt.fixStatus).c_str());
    printf("\tFix Type: %s\r\n", JimmyPaputto::Utils::eFixType2string(navigation.pvt.fixType).c_str());
    printf("\tVisible Satellites: %d\n", navigation.pvt.visibleSatellites);
    printf("\tLatitude: %.7f°\n", navigation.pvt.latitude);
    printf("\tLongitude: %.7f°\n", navigation.pvt.longitude);
    printf("\tAltitude: %.2f m\n", navigation.pvt.altitude);
    printf("\tTime: %s\n", JimmyPaputto::Utils::utcTimeFromGnss_ISO8601(navigation.pvt).c_str());
}

JimmyPaputto::GnssConfig createDefaultConfig()
{
    return JimmyPaputto::GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = JimmyPaputto::EDynamicModel::Portable,
        // TP1 still drives the board LED over USB; only the host-side
        // interrupt needs the HAT header.
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

const char* modelName(const JimmyPaputto::EUbloxModel model)
{
    switch (model)
    {
        case JimmyPaputto::EUbloxModel::NeoM9N: return "NEO-M9N";
        case JimmyPaputto::EUbloxModel::NeoF9P: return "NEO-F9P";
        case JimmyPaputto::EUbloxModel::Unknown: break;
    }
    return "Unknown";
}

auto main(int argc, char** argv) -> int
{
    // Without an argument the library picks the only attached receiver and
    // aborts if there is more than one.
    const char* devicePath = (argc > 1) ? argv[1] : nullptr;

    auto* receiver = JimmyPaputto::IGnssUsb::create(devicePath);

    printf("Opened %s on %s\r\n",
        modelName(receiver->model()), receiver->devicePath().c_str());

    receiver->softResetUbloxSom_HotStart();
    if (!receiver->start(createDefaultConfig()))
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    while (true)
    {
        const auto navigation = receiver->waitAndGetFreshNavigation();
        printNavigation(navigation);
    }

    return 0;
}
