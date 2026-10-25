/*
 * Jimmy Paputto 2026
 */

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

const char* timeBase2string(ETimeMarkTimeBase tb)
{
    switch (tb)
    {
        case ETimeMarkTimeBase::ReceiverTime: return "Receiver";
        case ETimeMarkTimeBase::GnssTime:     return "GNSS";
        case ETimeMarkTimeBase::UTC:          return "UTC";
    }
    return "Unknown";
}

const char* mode2string(ETimeMarkMode m)
{
    switch (m)
    {
        case ETimeMarkMode::Single:  return "Single";
        case ETimeMarkMode::Running: return "Running";
    }
    return "Unknown";
}

const char* run2string(ETimeMarkRun r)
{
    switch (r)
    {
        case ETimeMarkRun::Armed:   return "Armed";
        case ETimeMarkRun::Stopped: return "Stopped";
    }
    return "Unknown";
}

void printTimeMark(const TimeMark& tm)
{
    printf("--- New TimeMark event ---\r\n");
    printf("  channel:          %u\r\n", tm.channel);
    printf("  mode:             %s\r\n", mode2string(tm.mode));
    printf("  run:              %s\r\n", run2string(tm.run));
    printf("  timeBase:         %s\r\n", timeBase2string(tm.timeBase));
    printf("  timeValid:        %s\r\n", tm.timeValid ? "yes" : "no");
    printf("  utcAvailable:     %s\r\n", tm.utcAvailable ? "yes" : "no");
    printf("  newRisingEdge:    %s\r\n", tm.newRisingEdge ? "yes" : "no");
    printf("  newFallingEdge:   %s\r\n", tm.newFallingEdge ? "yes" : "no");
    printf("  count:            %u\r\n", tm.count);
    printf("  rising  WN: %u  TOW: %u ms + %u ns\r\n",
        tm.weekNumberRising, tm.towRising_ms, tm.towSubRising_ns);
    printf("  falling WN: %u  TOW: %u ms + %u ns\r\n",
        tm.weekNumberFalling, tm.towFalling_ms, tm.towSubFalling_ns);
    printf("  accuracy:         %u ns\r\n", tm.accuracyEstimate_ns);
    printf("\r\n");
}

GnssConfig createConfig()
{
    return GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = std::nullopt,
        .timing = TimingConfig {
            .enableTimeMark = true
        }
    };
}

auto main() -> int
{
    std::unique_ptr<IGnssHat> ubxHat(IGnssHat::create());
    ubxHat->stopOnSignals();
    ubxHat->softResetUbloxSom_HotStart();

    const bool isStartupDone = ubxHat->start(createConfig());
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }
    printf("Startup done, ublox configured\r\n");

    ubxHat->enableTimeMarkTrigger();
    printf("TimeMark trigger enabled, toggling EXTINT every 5s\r\n\r\n");

    // Declared after the HAT, so it is stopped and joined before the HAT is
    // destroyed.
    std::jthread toggleThread([&ubxHat](std::stop_token stop) {
        while (!stop.stop_requested())
        {
            ubxHat->triggerTimeMark();
            printf("[EXTINT] toggled\r\n");

            for (int i = 0; i < 50 && !stop.stop_requested(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    while (const auto timeMark = ubxHat->waitAndGetFreshTimeMark())
        printTimeMark(*timeMark);

    printf("Exiting...\r\n");
    return 0;
}
