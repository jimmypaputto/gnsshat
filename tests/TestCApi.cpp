#include <gtest/gtest.h>

#include "GnssHat.h"


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
