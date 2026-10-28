/*
 * Jimmy Paputto 2025
 */

#include <csignal>
#include <cstdio>
#include <memory>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

GnssConfig createDefaultConfig()
{
    return GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        }
    };
}

void printRfBlock(const RfBlock& rfBlock)
{
    printf("Band %s\r\n", Utils::eBand2string(rfBlock.gnssBand).c_str());
    printf("    Noise per ms: %d\r\n", rfBlock.noisePerMS);
    printf("    AGC monitor, percentage of max gain: %.2f%%\r\n",
        rfBlock.agcMonitor);
    printf("    Antenna status: %s\r\n",
        Utils::antennaStatus2string(rfBlock.antennaStatus).c_str());
    printf("    JammingState: %s\r\n",
        Utils::jammingState2string(rfBlock.jammingState).c_str());
    printf("    CW interference suppression level: %.2f%%\r\n",
        rfBlock.cwInterferenceSuppressionLevel);
}

void analyzeRfBlock(const RfBlock& rfBlock)
{
    constexpr float jammingTreshold = 40;
    if (rfBlock.cwInterferenceSuppressionLevel < jammingTreshold)
    {
        printf(
            "CW interference suppression level is below %.f%%, "
            "no jamming\r\n",
            jammingTreshold
        );
    }
    else
    {
        printf(
            "CW interference suppression level is above %.f%%, "
            "jamming detected\r\n",
            jammingTreshold
        );
    }
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
    const auto gnssConfig = createDefaultConfig();
    std::unique_ptr<IGnssHat> ubxHat(IGnssHat::create());
    ubxHat->softResetUbloxSom_HotStart();
    const auto isStartupDone = ubxHat->start(gnssConfig);
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }

    auto signalsInterrupt = signalsHandler(*ubxHat);

    while (const auto navigation = ubxHat->waitAndGetFreshNavigation())
    {
        const auto& rfBlocks = navigation->rfBlocks;
        if (rfBlocks.empty())
            continue;
        printf("============================================\r\n");
        for (const auto& rfBlock : rfBlocks)
        {
            printRfBlock(rfBlock);
            analyzeRfBlock(rfBlock);
        }
    }

    // Wakes sigwait if the loop ends without a signal (e.g. a break added
    // to the loop or a future device error).
    pthread_kill(signalsInterrupt.native_handle(), SIGINT);
    signalsInterrupt.join();
    return 0;
}
