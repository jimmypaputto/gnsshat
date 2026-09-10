#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ublox/ReceiverResolver.hpp"


using namespace JimmyPaputto;

namespace
{

UsbGnssDevice usbDevice(const std::string& devicePath)
{
    return UsbGnssDevice {
        .devicePath = devicePath,
        .byPathAlias = "/dev/serial/by-path/platform-xhci-hcd.1-usb-0:1:1.0",
        .product = "u-blox GNSS receiver"
    };
}

const std::string hatOnHeader{HatProduct::gnssL1};
const std::string noHat;

ReceiverSelection resolve(const ReceiverRequest& request,
    const std::string& hatProduct,
    const std::vector<UsbGnssDevice>& usbDevices)
{
    return resolveReceiver(request, hatProduct, usbDevices);
}

}  // namespace


TEST(ReceiverResolver, KnowsTheProductsWeDrive)
{
    EXPECT_TRUE(isSupportedHatProduct(HatProduct::gnssL1));
    EXPECT_TRUE(isSupportedHatProduct(HatProduct::gnssL1L5Time));
    EXPECT_TRUE(isSupportedHatProduct(HatProduct::gnssL1L5Rtk));
    EXPECT_FALSE(isSupportedHatProduct(""));
    EXPECT_FALSE(isSupportedHatProduct("Some Other HAT"));
}

TEST(ReceiverResolver, PrefersTheHatWhenNothingElseIsAsked)
{
    const auto selection = resolve({}, hatOnHeader, {});

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Hat);
    EXPECT_EQ(selection.hatProduct, hatOnHeader);
    EXPECT_FALSE(selection.usbAlsoAttached);
}

// The L1 and RTK HATs expose the module's own USB port, so a receiver on USB
// next to a HAT is common enough to deserve a heads-up rather than a failure.
TEST(ReceiverResolver, KeepsTheHatButFlagsTheIdleUsbReceiver)
{
    const auto selection = resolve(
        {}, hatOnHeader, { usbDevice("/dev/ttyACM0") });

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Hat);
    EXPECT_TRUE(selection.usbAlsoAttached);
}

TEST(ReceiverResolver, FallsBackToUsbWhenNoHatIsSeated)
{
    const auto selection = resolve(
        {}, noHat, { usbDevice("/dev/ttyACM0") });

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Usb);
    EXPECT_EQ(selection.usbDevicePath, "/dev/ttyACM0");
    EXPECT_FALSE(selection.unknownHatIgnored);
}

TEST(ReceiverResolver, FallsBackToUsbOverAnUnknownHatAndSaysSo)
{
    const auto selection = resolve(
        {}, "Some Other HAT", { usbDevice("/dev/ttyACM0") });

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Usb);
    EXPECT_TRUE(selection.unknownHatIgnored);
}

TEST(ReceiverResolver, UsbRequestBeatsASeatedHat)
{
    const auto selection = resolve(
        { .useUsb = true }, hatOnHeader, { usbDevice("/dev/ttyACM0") });

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Usb);
    EXPECT_EQ(selection.usbDevicePath, "/dev/ttyACM0");
    EXPECT_FALSE(selection.usbAlsoAttached);
}

// Asking for USB is an order, not a preference: silently opening the HAT
// instead would hand back a different receiver than the caller asked for.
TEST(ReceiverResolver, UsbRequestWithoutAUsbReceiverIsFatal)
{
    const auto selection = resolve({ .useUsb = true }, hatOnHeader, {});

    EXPECT_EQ(selection.resolution,
        EReceiverResolution::UsbRequestedButAbsent);
}

TEST(ReceiverResolver, ExplicitPathWinsOverEverything)
{
    const auto selection = resolve(
        { .usbDevicePath = "/dev/serial/by-path/pinned" },
        hatOnHeader,
        { usbDevice("/dev/ttyACM0"), usbDevice("/dev/ttyACM1") }
    );

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Usb);
    EXPECT_EQ(selection.usbDevicePath, "/dev/serial/by-path/pinned");
}

// A path may name a receiver the scan cannot see, so it is honoured even when
// sysfs turns up nothing.
TEST(ReceiverResolver, ExplicitPathIsHonouredWithoutAnyScanResult)
{
    const auto selection = resolve(
        { .usbDevicePath = "/dev/ttyACM7" }, noHat, {});

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Usb);
    EXPECT_EQ(selection.usbDevicePath, "/dev/ttyACM7");
}

TEST(ReceiverResolver, RefusesToGuessBetweenUsbReceivers)
{
    const auto selection = resolve(
        {}, noHat, { usbDevice("/dev/ttyACM0"), usbDevice("/dev/ttyACM1") });

    EXPECT_EQ(selection.resolution, EReceiverResolution::AmbiguousUsb);
}

TEST(ReceiverResolver, RefusesToGuessBetweenUsbReceiversWhenUsbIsRequested)
{
    const auto selection = resolve(
        { .useUsb = true },
        hatOnHeader,
        { usbDevice("/dev/ttyACM0"), usbDevice("/dev/ttyACM1") }
    );

    EXPECT_EQ(selection.resolution, EReceiverResolution::AmbiguousUsb);
}

// Several receivers are harmless as long as a supported HAT wins on its own.
TEST(ReceiverResolver, SeveralUsbReceiversDoNotDisturbTheHat)
{
    const auto selection = resolve(
        {}, hatOnHeader,
        { usbDevice("/dev/ttyACM0"), usbDevice("/dev/ttyACM1") });

    EXPECT_EQ(selection.resolution, EReceiverResolution::Ok);
    EXPECT_EQ(selection.transport, EReceiverTransport::Hat);
    EXPECT_TRUE(selection.usbAlsoAttached);
}

TEST(ReceiverResolver, NothingAnywhereIsFatal)
{
    const auto selection = resolve({}, noHat, {});

    EXPECT_EQ(selection.resolution, EReceiverResolution::NoReceiver);
}

TEST(ReceiverResolver, UnknownHatWithoutUsbKeepsItsOwnDiagnostic)
{
    const auto selection = resolve({}, "Some Other HAT", {});

    EXPECT_EQ(selection.resolution, EReceiverResolution::UnknownHat);
    EXPECT_EQ(selection.hatProduct, "Some Other HAT");
}
