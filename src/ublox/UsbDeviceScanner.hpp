/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_USB_DEVICE_SCANNER_HPP_
#define JIMMY_PAPUTTO_USB_DEVICE_SCANNER_HPP_

#include <string>
#include <vector>


namespace JimmyPaputto
{

struct UsbGnssDevice
{
    std::string devicePath{};   // /dev/ttyACM0
    std::string byPathAlias{};  // /dev/serial/by-path/... , empty when udev has none
    std::string product{};      // USB iProduct, e.g. "u-blox GNSS receiver"
};

// Finds u-blox CDC-ACM receivers by walking sysfs. /dev/serial/by-id is not
// usable for this: the modules ship without an iSerial, so two of them would
// collide on a single by-id name and only one symlink would survive. Deciding
// which of the results to open is ReceiverResolver's job.
class UsbDeviceScanner
{
public:
    explicit UsbDeviceScanner(std::string sysfsTtyRoot = "/sys/class/tty",
        std::string devRoot = "/dev",
        std::string serialByPathRoot = "/dev/serial/by-path");

    std::vector<UsbGnssDevice> scan() const;

    static constexpr const char* ubloxVendorId = "1546";

private:
    std::string sysfsTtyRoot_;
    std::string devRoot_;
    std::string serialByPathRoot_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_USB_DEVICE_SCANNER_HPP_
