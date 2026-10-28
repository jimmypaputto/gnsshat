/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <csignal>
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

// Returns false if the wait is stopped first.
static bool waitForEpochs(JimmyPaputto::IGnssHat& hat, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (!hat.waitAndGetFreshNavigation())
            return false;
    }
    return true;
}

static void measureStartups(JimmyPaputto::IGnssHat& ubxHat,
    int measurementRate_Hz)
{
    ubxHat.hardResetUbloxSom_ColdStart();
    auto start = std::chrono::high_resolution_clock::now();
    if (!waitForFix(ubxHat))
        return;
    auto stop = std::chrono::high_resolution_clock::now();
    auto time2fix =
        std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    printf("Cold start took %ld ms\r\n", time2fix);

    printf("Wait 40s to collect data for hot start\r\n");
    if (!waitForEpochs(ubxHat, 40 * measurementRate_Hz))
        return;
    printf("Performing hot start\r\n");

    ubxHat.softResetUbloxSom_HotStart();
    start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    if (!waitForFix(ubxHat))
        return;
    stop = std::chrono::high_resolution_clock::now();
    time2fix =
        std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    printf("Hot start took %ld ms\r\n", time2fix);
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
    const auto gnssConfig = createDefaultConfig();
    std::unique_ptr<JimmyPaputto::IGnssHat> ubxHat(
        JimmyPaputto::IGnssHat::create());
    ubxHat->hardResetUbloxSom_ColdStart();
    const auto isStartupDone = ubxHat->start(gnssConfig);
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }
    printf("Startup done, ublox configured\r\n");

    auto signalsInterrupt = signalsHandler(*ubxHat);

    measureStartups(*ubxHat, gnssConfig.measurementRate_Hz);

    // Wakes sigwait when the measurement ends without a signal.
    pthread_kill(signalsInterrupt.native_handle(), SIGINT);
    signalsInterrupt.join();
    return 0;
}
