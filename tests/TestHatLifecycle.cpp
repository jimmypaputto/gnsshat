#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <stop_token>
#include <thread>

#include <unistd.h>

#include "GnssHat.hpp"

using namespace JimmyPaputto;


namespace
{

bool hardwareTestsEnabled()
{
    return std::getenv("GNSSHAT_HW_TESTS") != nullptr;
}

bool supportsGeofencing(const std::string& product)
{
    return product != "L1/L5 GNSS TIME HAT";
}

GnssConfig configWithTwoGeofences()
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
        .geofencing = GnssConfig::Geofencing {
            .geofences = {
                Geofence { .lat = 50.8673, .lon = 15.6038, .radius = 200 },
                Geofence { .lat = 51.3170, .lon = 15.6038, .radius = 200 }
            },
            .confidenceLevel = 3,
            .pioPinPolarity = std::nullopt
        }
    };
}

}  // namespace

// Requires a real HAT, so it only runs with GNSSHAT_HW_TESTS set.
TEST(HatLifecycle, NewInstanceStartsClean)
{
    if (!hardwareTestsEnabled())
        GTEST_SKIP() << "set GNSSHAT_HW_TESTS=1 to run on a real HAT";

    const std::string product = Hat::detectProduct();
    if (product.empty())
        GTEST_SKIP() << "no HAT detected";

    const bool geofencing = supportsGeofencing(product);
    auto config = configWithTwoGeofences();
    if (!geofencing)
        config.geofencing = std::nullopt;

    {
        std::unique_ptr<IGnssHat> hat(IGnssHat::create());
        ASSERT_TRUE(hat->start(config));
        ASSERT_FALSE(hat->swVersion().empty());
        if (geofencing)
        {
            ASSERT_EQ(hat->navigation().geofencing.cfg.geofences.size(), 2u);
        }
    }

    std::unique_ptr<IGnssHat> hat(IGnssHat::create());
    EXPECT_TRUE(hat->swVersion().empty())
        << "MON-VER leaked from the previous instance: " << hat->swVersion();
    EXPECT_TRUE(hat->hwVersion().empty());
    EXPECT_TRUE(hat->monVerExtensions().empty());
    EXPECT_TRUE(hat->navigation().geofencing.cfg.geofences.empty())
        << "geofences leaked from the previous instance";
}

// Requires a real HAT, so it only runs with GNSSHAT_HW_TESTS set.
TEST(HatLifecycle, DeleteWakesBlockedThreads)
{
    if (!hardwareTestsEnabled())
        GTEST_SKIP() << "set GNSSHAT_HW_TESTS=1 to run on a real HAT";

    if (Hat::detectProduct().empty())
        GTEST_SKIP() << "no HAT detected";

    IGnssHat* hat = IGnssHat::create();
    // gpsd forwarding needs root for /dev/jimmypaputto; without it only the
    // navigation wait is covered.
    const bool forwarding = hat->startForwardForGpsd();

    // The receiver is not started, so no epoch arrives and both calls block
    // until the HAT is deleted. The threads must not touch it afterwards.
    std::atomic<int> woken{0};
    std::thread navigationWaiter([hat, &woken] {
        if (!hat->waitAndGetFreshNavigation())
            ++woken;
    });
    std::thread gpsdJoiner;
    if (forwarding)
    {
        gpsdJoiner = std::thread([hat, &woken] {
            hat->joinForwardForGpsd();
            ++woken;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(woken.load(), 0);

    delete hat;
    navigationWaiter.join();
    if (gpsdJoiner.joinable())
        gpsdJoiner.join();

    EXPECT_EQ(woken.load(), forwarding ? 2 : 1);
}

// Requires a real HAT, so it only runs with GNSSHAT_HW_TESTS set.
TEST(HatLifecycle, StopTokenEndsOnlyItsOwnWait)
{
    if (!hardwareTestsEnabled())
        GTEST_SKIP() << "set GNSSHAT_HW_TESTS=1 to run on a real HAT";

    if (Hat::detectProduct().empty())
        GTEST_SKIP() << "no HAT detected";

    std::unique_ptr<IGnssHat> hat(IGnssHat::create());
    EXPECT_FALSE(hat->timepulse()) << "timepulse is not enabled";

    // Not started, so no epoch arrives and only the token ends the waits.
    std::atomic<bool> firstStopped{false};
    std::jthread first([&](std::stop_token stop) {
        firstStopped = !hat->waitAndGetFreshNavigation(stop);
    });
    std::atomic<bool> secondReturned{false};
    std::jthread second([&](std::stop_token stop) {
        EXPECT_FALSE(hat->waitAndGetFreshNavigation(stop));
        secondReturned = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    first.request_stop();
    first.join();
    EXPECT_TRUE(firstStopped);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_FALSE(secondReturned) << "a stopped token must not end other waits";
    second.request_stop();
    second.join();
    EXPECT_TRUE(secondReturned);
}

// Requires a real HAT, so it only runs with GNSSHAT_HW_TESTS set.
TEST(HatLifecycle, StopOnSignalsEndsWaits)
{
    if (!hardwareTestsEnabled())
        GTEST_SKIP() << "set GNSSHAT_HW_TESTS=1 to run on a real HAT";

    if (Hat::detectProduct().empty())
        GTEST_SKIP() << "no HAT detected";

    std::unique_ptr<IGnssHat> hat(IGnssHat::create());
    ASSERT_TRUE(hat->stopOnSignals());

    // Not started, so no epoch arrives and only the signal ends the wait.
    std::atomic<bool> stopped{false};
    std::thread waiter([&] {
        stopped = !hat->waitAndGetFreshNavigation();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_FALSE(stopped);
    ::kill(::getpid(), SIGINT);
    waiter.join();

    EXPECT_TRUE(stopped);
    EXPECT_FALSE(hat->waitAndGetFreshNavigation()) << "the stop is permanent";
}
