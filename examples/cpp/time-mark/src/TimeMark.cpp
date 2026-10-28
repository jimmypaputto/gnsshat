/*
 * Jimmy Paputto 2026
 */

#include <csignal>
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

    // Before toggleThread: it must inherit the blocked signals.
    auto signalsInterrupt = signalsHandler(*ubxHat);

    // Toggles EXTINT every 5 navigation epochs (5 s at 1 Hz) and ends when
    // shutdown() stops its wait.
    std::thread toggleThread([&ubxHat] {
        int epoch = 0;
        while (ubxHat->waitAndGetFreshNavigation())
        {
            if (epoch++ % 5 != 0)
                continue;
            ubxHat->triggerTimeMark();
            printf("[EXTINT] toggled\r\n");
        }
    });

    while (const auto timeMark = ubxHat->waitAndGetFreshTimeMark())
        printTimeMark(*timeMark);

    // Wakes sigwait if the loop ends without a signal (e.g. a break added
    // to the loop or a future device error).
    pthread_kill(signalsInterrupt.native_handle(), SIGINT);
    signalsInterrupt.join();
    toggleThread.join();

    printf("Exiting...\r\n");
    return 0;
}
