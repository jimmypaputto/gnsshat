/*
 * Jimmy Paputto 2025
 */

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "GnssHat.hpp"
#include "ublox/SpiDriver.hpp"
#include "ublox/UartDriver.hpp"
#include "ublox/UbloxProbe.hpp"
#include "ublox/UbxParser.hpp"
#include "ublox/UsbDeviceScanner.hpp"
#include "ublox/UsbDriver.hpp"
#include "ublox/ubxmsg/UBX_MON_VER.hpp"

static const char* modelName(const JimmyPaputto::EUbloxModel model)
{
    switch (model)
    {
        case JimmyPaputto::EUbloxModel::NeoM9N: return "NEO-M9N";
        case JimmyPaputto::EUbloxModel::NeoF9P: return "NEO-F9P";
        case JimmyPaputto::EUbloxModel::Unknown: break;
    }
    return "Unknown";
}

// Unlike IGnssUsb::create(), this lists every receiver instead of refusing to
// choose - showing the paths is the whole point of the tool.
static int probeUsbReceivers()
{
    const JimmyPaputto::UsbDeviceScanner scanner;
    const auto devices = scanner.scan();

    if (devices.empty())
    {
        printf("No u-blox USB receiver detected (USB vendor %s).\n",
            JimmyPaputto::UsbDeviceScanner::ubloxVendorId);
        return 1;
    }

    printf("Found %zu u-blox USB receiver(s):\n\n", devices.size());

    for (const auto& device : devices)
    {
        printf("Device:      %s\n", device.devicePath.c_str());
        printf("Product:     %s\n", device.product.c_str());
        if (!device.byPathAlias.empty())
            printf("Stable path: %s\n", device.byPathAlias.c_str());

        JimmyPaputto::UsbDriver driver(device.devicePath);
        if (!driver.isOpen())
        {
            printf("Model:       <cannot open device>\n\n");
            continue;
        }

        const auto identity = JimmyPaputto::UbloxProbe::identify(driver);
        printf("Model:       %s\n", modelName(identity.model));

        if (!identity.swVersion.empty())
        {
            printf("Software:    %s\n", identity.swVersion.c_str());
            printf("Hardware:    %s\n", identity.hwVersion.c_str());
            for (const auto& extension : identity.extensions)
                printf("  %s\n", extension.c_str());
        }
        printf("\n");
    }

    return 0;
}

static std::map<std::string, std::string> parseCustom0(const std::string& raw)
{
    std::map<std::string, std::string> result;
    size_t pos = 0;
    while (pos < raw.size())
    {
        size_t sep = raw.find(';', pos);
        std::string token = (sep == std::string::npos)
            ? raw.substr(pos) : raw.substr(pos, sep - pos);

        size_t eq = token.find(':');
        if (eq != std::string::npos)
            result[token.substr(0, eq)] = token.substr(eq + 1);

        if (sep == std::string::npos) break;
        pos = sep + 1;
    }
    return result;
}

static bool pollMonVer(JimmyPaputto::ICommDriver& driver)
{
    auto pollFrame = JimmyPaputto::ubxmsg::UBX_MON_VER::poll();

    std::vector<uint8_t> rxBuff(1200, 0);
    driver.transmitReceive(pollFrame, rxBuff);

    // Search for MON-VER response header: 0xB5 0x62 0x0A 0x04
    for (size_t i = 0; i + 6 < rxBuff.size(); i++)
    {
        if (rxBuff[i] == 0xB5 && rxBuff[i + 1] == 0x62 &&
            rxBuff[i + 2] == 0x0A && rxBuff[i + 3] == 0x04)
        {
            uint16_t payloadLen =
                static_cast<uint16_t>(rxBuff[i + 4]) |
                (static_cast<uint16_t>(rxBuff[i + 5]) << 8);

            size_t frameLen = 6 + payloadLen + 2;  // header + payload + checksum
            if (i + frameLen > rxBuff.size())
            {
                printf("  MON-VER response truncated\n");
                return false;
            }

            JimmyPaputto::ubxmsg::UBX_MON_VER monVer;
            monVer.deserialize(
                std::span<const uint8_t>(rxBuff.data() + i, frameLen));

            printf("\nFirmware:\n");
            printf("  Software: %s\n", monVer.swVersion().c_str());
            printf("  Hardware: %s\n", monVer.hwVersion().c_str());
            for (const auto& ext : monVer.extensions())
                printf("  %s\n", ext.c_str());

            return true;
        }
    }

    printf("  No MON-VER response received\n");
    return false;
}

int main(int argc, char* argv[])
{
    bool liveMode = false;
    bool usbMode = false;
    std::string forceComm;
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--live") == 0)
            liveMode = true;
        else if (std::strcmp(argv[i], "--usb") == 0)
            usbMode = true;
        else if (std::strcmp(argv[i], "--spi") == 0)
            forceComm = "SPI";
        else if (std::strcmp(argv[i], "--uart") == 0)
            forceComm = "UART";
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h") == 0)
        {
            printf("Usage: gnsshat-probe [--live] [--spi|--uart] [--usb]\n");
            printf("  --live  Query the u-blox receiver for firmware version\n");
            printf("  --spi   Force SPI interface for --live mode\n");
            printf("  --uart  Force UART interface for --live mode\n");
            printf("  --usb   List u-blox receivers attached over USB\n");
            return 0;
        }
    }

    if (usbMode)
        return probeUsbReceivers();

    const std::string product = JimmyPaputto::Hat::readEepromField("product");
    if (product.empty())
    {
        printf("No GNSS HAT detected (device-tree entry not found).\n");
        return 1;
    }

    const std::string vendor = JimmyPaputto::Hat::readEepromField("vendor");
    const std::string productId = JimmyPaputto::Hat::readEepromField("product_id");
    const std::string productVer = JimmyPaputto::Hat::readEepromField("product_ver");
    const std::string uuid = JimmyPaputto::Hat::readEepromField("uuid");
    const std::string custom0 = JimmyPaputto::Hat::readEepromField("custom_0");

    printf("Product:     %s\n", product.c_str());
    printf("Vendor:      %s\n", vendor.c_str());
    printf("Product ID:  %s\n", productId.c_str());
    printf("Product Ver: %s\n", productVer.c_str());
    printf("UUID:        %s\n", uuid.c_str());

    auto params = parseCustom0(custom0);
    if (!params.empty())
    {
        printf("\nHardware:\n");
        const auto commIt = params.find("COMM_INTERFACE");
        if (commIt != params.end())
            printf("  Communication: %s\n", commIt->second.c_str());

        const auto txIt = params.find("TX_READY");
        if (txIt != params.end())
            printf("  TX Ready:      %s\n", txIt->second.c_str());

        const auto resetIt = params.find("RESET");
        if (resetIt != params.end())
            printf("  Reset:         %s\n", resetIt->second.c_str());

        const auto tpIt = params.find("TIMEPULSE");
        if (tpIt != params.end())
            printf("  Timepulse:     %s\n", tpIt->second.c_str());
    }

    if (liveMode)
    {
        std::string comm = forceComm;
        if (comm.empty())
        {
            const auto commIt = params.find("COMM_INTERFACE");
            comm = (commIt != params.end()) ? commIt->second : "";
        }

        if (comm.find("SPI") != std::string::npos)
        {
            JimmyPaputto::SpiDriver spi;
            pollMonVer(spi);
        }
        else if (comm.find("UART") != std::string::npos)
        {
            JimmyPaputto::UartDriver uart;
            pollMonVer(uart);
        }
        else
        {
            printf("\nCannot determine communication interface for --live mode.\n");
            return 1;
        }
    }

    return 0;
}
