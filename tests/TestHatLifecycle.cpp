#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>

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
