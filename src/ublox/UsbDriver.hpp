/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_USB_DRIVER_HPP_
#define JIMMY_PAPUTTO_USB_DRIVER_HPP_

#include <cstdint>
#include <string>

#include "ublox/SerialDriver.hpp"


namespace JimmyPaputto
{

class UsbDriver: public SerialDriver
{
public:
    explicit UsbDriver(const std::string& devicePath);

    // CDC-ACM runs at USB speed and ignores the line rate, but termios still
    // demands a legal value.
    static constexpr uint32_t nominalBaudrate = 115200;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_USB_DRIVER_HPP_
