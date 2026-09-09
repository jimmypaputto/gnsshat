#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "ublox/UsbDeviceScanner.hpp"


using namespace JimmyPaputto;

namespace fs = std::filesystem;

namespace
{

// Mirrors the layout the scanner walks: /sys/class/tty/ttyACMx/device is a
// symlink to the USB interface, and idVendor lives on its parent.
class FakeSysfs
{
public:
    FakeSysfs()
    :   root_(fs::temp_directory_path() /
            ("gnsshat-scan-" + std::to_string(::getpid()) + "-" +
             std::to_string(counter_++)))
    {
        fs::create_directories(ttyRoot());
        fs::create_directories(devRoot());
        fs::create_directories(byPathRoot());
    }

    ~FakeSysfs()
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void addSerialDevice(const std::string& ttyName,
        const std::string& vendorId, const std::string& product,
        const std::string& busId)
    {
        const fs::path usbDevice = root_ / "devices" / busId;
        const fs::path usbInterface = usbDevice / (busId + ":1.0");
        fs::create_directories(usbInterface);

        writeFile(usbDevice / "idVendor", vendorId);
        writeFile(usbDevice / "product", product);

        const fs::path ttyDir = ttyRoot() / ttyName;
        fs::create_directories(ttyDir);
        fs::create_directory_symlink(usbInterface, ttyDir / "device");

        writeFile(devRoot() / ttyName, "");
    }

    // A non-USB tty such as the on-board UART has no device symlink at all.
    void addPlatformTty(const std::string& ttyName)
    {
        fs::create_directories(ttyRoot() / ttyName);
    }

    void addByPathAlias(const std::string& alias, const std::string& ttyName)
    {
        fs::create_symlink(devRoot() / ttyName, byPathRoot() / alias);
    }

    UsbDeviceScanner scanner() const
    {
        return UsbDeviceScanner(
            ttyRoot().string(), devRoot().string(), byPathRoot().string()
        );
    }

    fs::path devRoot() const { return root_ / "dev"; }

private:
    static void writeFile(const fs::path& path, const std::string& content)
    {
        std::ofstream file(path);
        file << content << "\n";
    }

    fs::path ttyRoot() const { return root_ / "sys" / "class" / "tty"; }
    fs::path byPathRoot() const { return root_ / "dev" / "serial" / "by-path"; }

    fs::path root_;
    static inline int counter_ = 0;
};

constexpr const char* ubloxProduct = "u-blox GNSS receiver";

}  // namespace


TEST(UsbDeviceScanner, FindsNothingWhenNoTtyIsPresent)
{
    FakeSysfs sysfs;

    EXPECT_TRUE(sysfs.scanner().scan().empty());
}

TEST(UsbDeviceScanner, FindsUbloxReceiver)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");

    const auto devices = sysfs.scanner().scan();

    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].devicePath, (sysfs.devRoot() / "ttyACM0").string());
    EXPECT_EQ(devices[0].product, ubloxProduct);
}

TEST(UsbDeviceScanner, IgnoresOtherVendors)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM0", "0403", "FTDI adapter", "3-1");
    sysfs.addSerialDevice("ttyACM1", "2341", "Arduino", "3-2");

    EXPECT_TRUE(sysfs.scanner().scan().empty());
}

TEST(UsbDeviceScanner, IgnoresNonUsbTtys)
{
    FakeSysfs sysfs;
    sysfs.addPlatformTty("ttyAMA0");
    sysfs.addPlatformTty("ttyS0");
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");

    const auto devices = sysfs.scanner().scan();

    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].devicePath, (sysfs.devRoot() / "ttyACM0").string());
}

TEST(UsbDeviceScanner, ResultIsSortedForDeterminism)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM2", "1546", ubloxProduct, "3-3");
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");
    sysfs.addSerialDevice("ttyACM1", "1546", ubloxProduct, "3-2");

    const auto devices = sysfs.scanner().scan();

    ASSERT_EQ(devices.size(), 3u);
    EXPECT_EQ(devices[0].devicePath, (sysfs.devRoot() / "ttyACM0").string());
    EXPECT_EQ(devices[1].devicePath, (sysfs.devRoot() / "ttyACM1").string());
    EXPECT_EQ(devices[2].devicePath, (sysfs.devRoot() / "ttyACM2").string());
}

// udev publishes the same port as both usb- and usbv2-; the reported alias has
// to be stable rather than whichever the directory happens to yield first.
TEST(UsbDeviceScanner, ReportsSingleStableByPathAlias)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");
    sysfs.addByPathAlias("platform-xhci-hcd.1-usbv2-0:1:1.0", "ttyACM0");
    sysfs.addByPathAlias("platform-xhci-hcd.1-usb-0:1:1.0", "ttyACM0");

    const auto devices = sysfs.scanner().scan();

    ASSERT_EQ(devices.size(), 1u);
    EXPECT_TRUE(devices[0].byPathAlias.ends_with(
        "platform-xhci-hcd.1-usb-0:1:1.0"));
}

TEST(UsbDeviceScanner, MissingByPathDirectoryLeavesAliasEmpty)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");

    const auto devices = sysfs.scanner().scan();

    ASSERT_EQ(devices.size(), 1u);
    EXPECT_TRUE(devices[0].byPathAlias.empty());
}

TEST(UsbDeviceScanner, ExplicitPathIsReturnedWithoutScanning)
{
    FakeSysfs sysfs;

    EXPECT_EQ(sysfs.scanner().resolveSingle("/dev/ttyACM7"), "/dev/ttyACM7");
}

TEST(UsbDeviceScanner, ResolvesTheSoleReceiver)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");

    EXPECT_EQ(sysfs.scanner().resolveSingle(nullptr),
        (sysfs.devRoot() / "ttyACM0").string());
}

TEST(UsbDeviceScannerDeathTest, TerminatesWhenNoReceiverIsPresent)
{
    FakeSysfs sysfs;

    EXPECT_DEATH(sysfs.scanner().resolveSingle(nullptr),
        "No u-blox USB receiver found");
}

TEST(UsbDeviceScannerDeathTest, TerminatesRatherThanGuessBetweenReceivers)
{
    FakeSysfs sysfs;
    sysfs.addSerialDevice("ttyACM0", "1546", ubloxProduct, "3-1");
    sysfs.addSerialDevice("ttyACM1", "1546", ubloxProduct, "3-2");

    EXPECT_DEATH(sysfs.scanner().resolveSingle(nullptr),
        "explicit device path");
}
