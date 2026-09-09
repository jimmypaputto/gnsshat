/*
 * Jimmy Paputto 2026
 */

#include "ublox/UsbDriver.hpp"


namespace JimmyPaputto
{

UsbDriver::UsbDriver(const std::string& devicePath)
:   SerialDriver(devicePath, nominalBaudrate, "[USB]")
{
}

}  // JimmyPaputto
