#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ublox/UbloxProbe.hpp"


using namespace JimmyPaputto;

namespace
{

const std::string coreSwVersion = "EXT CORE 1.00 (61b2dd)";

}  // namespace


TEST(UbloxProbe, ModIdentifiesF9P)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=HPG 1.32",
        "PROTVER=27.31",
        "MOD=ZED-F9P",
        "GPS;GLO;GAL;BDS"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoF9P);
}

TEST(UbloxProbe, ModIdentifiesM9N)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=SPG 4.04",
        "PROTVER=32.01",
        "MOD=NEO-M9N",
        "GPS;GLO;GAL;BDS"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoM9N);
}

TEST(UbloxProbe, ModIdentifiesF10T)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=TIM 2.20",
        "PROTVER=50.20",
        "MOD=NEO-F10T",
        "GPS;GLO;GAL;BDS"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoF10T);
}

TEST(UbloxProbe, FirmwareFamilyIdentifiesF9PWhenModIsAbsent)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=HPG 1.13",
        "PROTVER=27.12"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoF9P);
}

TEST(UbloxProbe, FirmwareFamilyIdentifiesM9NWhenModIsAbsent)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=SPG 4.04",
        "PROTVER=32.01"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoM9N);
}

TEST(UbloxProbe, FirmwareFamilyIdentifiesF10TWhenModIsAbsent)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=TIM 2.20",
        "PROTVER=50.20"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoF10T);
}

// MOD= is authoritative: an F9P running standard precision firmware is still
// an F9P.
TEST(UbloxProbe, ModWinsOverFirmwareFamily)
{
    const std::vector<std::string> extensions = {
        "FWVER=SPG 4.04",
        "MOD=ZED-F9P"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::NeoF9P);
}

TEST(UbloxProbe, UnrecognisedReceiverStaysUnknown)
{
    const std::vector<std::string> extensions = {
        "ROM BASE 0x118B2060",
        "FWVER=ADR 4.21",
        "MOD=ZED-F9R"
    };

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, extensions),
        EUbloxModel::Unknown);
}

TEST(UbloxProbe, NoExtensionsStaysUnknown)
{
    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, {}),
        EUbloxModel::Unknown);
}

// Guards the reason the probe bypasses UbxParser: two calls must not leak into
// one another through shared state.
TEST(UbloxProbe, ConsecutiveCallsDoNotShareState)
{
    const std::vector<std::string> f9p = {"MOD=ZED-F9P", "FWVER=HPG 1.32"};
    const std::vector<std::string> m9n = {"MOD=NEO-M9N", "FWVER=SPG 4.04"};

    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, f9p),
        EUbloxModel::NeoF9P);
    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, m9n),
        EUbloxModel::NeoM9N);
    EXPECT_EQ(UbloxProbe::modelFromMonVer(coreSwVersion, {}),
        EUbloxModel::Unknown);
}
