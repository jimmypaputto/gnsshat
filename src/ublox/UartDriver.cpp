/*
 * Jimmy Paputto 2025
 */

#include "ublox/UartDriver.hpp"

namespace JimmyPaputto
{

UartDriver::UartDriver()
:   SerialDriver(UBX_UART_DEV, expectedBaudrate, "[UART]")
{
}

void UartDriver::reinit(const uint32_t baudrate)
{
    reopen(baudrate);
}

}  // JimmyPaputto
