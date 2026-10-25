#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "GnssHat.h"
#include "GnssHat.hpp"


TEST(CApi, CorrectionsWithoutHatAreNull)
{
    EXPECT_EQ(jp_gnss_rtk_get_full_corrections(nullptr), nullptr);
    EXPECT_EQ(jp_gnss_rtk_get_tiny_corrections(nullptr), nullptr);
    jp_gnss_rtk_corrections_free(nullptr);
}

TEST(CApi, ConfigInitLeavesTimepulseInactive)
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    EXPECT_FALSE(config.timepulse_pin_config.active);
    EXPECT_EQ(config.timepulse_pin_config.fixed_pulse.frequency, 1u);
    EXPECT_FLOAT_EQ(config.timepulse_pin_config.fixed_pulse.pulse_width, 0.1f);
    EXPECT_EQ(config.timepulse_pin_config.polarity,
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE);
}

// Requires a real HAT, so it only runs with GNSSHAT_HW_TESTS set.
TEST(CApi, ShutdownMakesWaitsReturnFalse)
{
    if (std::getenv("GNSSHAT_HW_TESTS") == nullptr)
        GTEST_SKIP() << "set GNSSHAT_HW_TESTS=1 to run on a real HAT";

    const std::string product = JimmyPaputto::Hat::detectProduct();
    if (product.empty())
        GTEST_SKIP() << "no HAT detected";

    jp_gnss_hat_t* hat = jp_gnss_hat_create();
    jp_gnss_hat_shutdown(hat);

    jp_gnss_navigation_t navigation;
    EXPECT_FALSE(jp_gnss_hat_wait_and_get_fresh_navigation(hat, &navigation));
    if (product == "L1/L5 GNSS TIME HAT")
    {
        jp_gnss_time_mark_t timeMark;
        EXPECT_FALSE(jp_gnss_hat_wait_and_get_fresh_time_mark(hat, &timeMark));
    }
    EXPECT_FALSE(jp_gnss_hat_timepulse(hat));

    jp_gnss_hat_destroy(hat);
}

// Requires a real HAT, so it only runs with GNSSHAT_HW_TESTS set.
TEST(CApi, DestroyWakesBlockedWait)
{
    if (std::getenv("GNSSHAT_HW_TESTS") == nullptr)
        GTEST_SKIP() << "set GNSSHAT_HW_TESTS=1 to run on a real HAT";

    if (JimmyPaputto::Hat::detectProduct().empty())
        GTEST_SKIP() << "no HAT detected";

    jp_gnss_hat_t* hat = jp_gnss_hat_create();

    // Not started, so the wait blocks until the handle is destroyed.
    std::atomic<int> result{-1};
    std::thread waiter([hat, &result] {
        jp_gnss_navigation_t navigation;
        result = jp_gnss_hat_wait_and_get_fresh_navigation(hat, &navigation);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(result.load(), -1);

    jp_gnss_hat_destroy(hat);
    waiter.join();
    EXPECT_EQ(result.load(), 0);
}
