/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_UART_DRIVER_HPP_
#define JIMMY_PAPUTTO_UART_DRIVER_HPP_

#include <cstdint>

#include "ublox/SerialDriver.hpp"
#include "ublox/ubxmsg/UBX_CFG_PRT.hpp"

#define UBX_UART_DEV "/dev/ttyAMA0"


namespace JimmyPaputto
{

class UartDriver: public SerialDriver
{
public:
    explicit UartDriver();

    void reinit(const uint32_t baudrate);

    static constexpr uint32_t expectedBaudrate = 115200;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_UART_DRIVER_HPP_
