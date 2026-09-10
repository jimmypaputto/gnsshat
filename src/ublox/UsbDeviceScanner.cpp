/*
 * Jimmy Paputto 2026
 */

#include "ublox/UsbDeviceScanner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <utility>

namespace fs = std::filesystem;


namespace JimmyPaputto
{

namespace
{

std::string readSysfsString(const fs::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return {};

    std::string value;
    std::getline(file, value);
    while (!value.empty() &&
        (value.back() == '\n' || value.back() == '\r' || value.back() == ' '))
    {
        value.pop_back();
    }
    return value;
}

// /sys/class/tty/ttyACMx/device points at the CDC interface; idVendor lives on
// the USB device node one level up.
fs::path usbDeviceDir(const fs::path& ttyDir)
{
    std::error_code ec;
    const fs::path interfaceDir = fs::canonical(ttyDir / "device", ec);
    if (ec)
        return {};
    return interfaceDir.parent_path();
}

// Maps tty name -> by-path alias. udev publishes the same port twice
// (usb- and usbv2-), so the candidates are sorted and only the first kept.
std::map<std::string, std::string> byPathAliases(const fs::path& root)
{
    std::map<std::string, std::string> aliases;
    std::error_code ec;

    if (!fs::is_directory(root, ec))
        return aliases;

    std::vector<std::pair<std::string, std::string>> candidates;
    for (const auto& entry : fs::directory_iterator(root, ec))
    {
        std::error_code targetEc;
        const fs::path target = fs::canonical(entry.path(), targetEc);
        if (targetEc)
            continue;
        candidates.emplace_back(
            target.filename().string(), entry.path().string()
        );
    }

    std::ranges::sort(candidates);
    for (auto& [ttyName, alias] : candidates)
        aliases.emplace(ttyName, std::move(alias));

    return aliases;
}

}  // namespace

UsbDeviceScanner::UsbDeviceScanner(std::string sysfsTtyRoot,
    std::string devRoot, std::string serialByPathRoot)
:   sysfsTtyRoot_(std::move(sysfsTtyRoot)),
    devRoot_(std::move(devRoot)),
    serialByPathRoot_(std::move(serialByPathRoot))
{
}

std::vector<UsbGnssDevice> UsbDeviceScanner::scan() const
{
    std::vector<UsbGnssDevice> devices;
    std::error_code ec;

    if (!fs::is_directory(sysfsTtyRoot_, ec))
        return devices;

    const auto aliases = byPathAliases(serialByPathRoot_);

    for (const auto& entry : fs::directory_iterator(sysfsTtyRoot_, ec))
    {
        const std::string name = entry.path().filename().string();
        if (!name.starts_with("ttyACM"))
            continue;

        const fs::path usbDir = usbDeviceDir(entry.path());
        if (usbDir.empty())
            continue;

        if (readSysfsString(usbDir / "idVendor") != ubloxVendorId)
            continue;

        UsbGnssDevice device;
        device.devicePath = (fs::path(devRoot_) / name).string();
        device.product = readSysfsString(usbDir / "product");
        if (const auto it = aliases.find(name); it != aliases.end())
            device.byPathAlias = it->second;

        devices.push_back(std::move(device));
    }

    std::ranges::sort(devices, {}, &UsbGnssDevice::devicePath);
    return devices;
}

}  // JimmyPaputto
