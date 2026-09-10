/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_SERIAL_DRIVER_HPP_
#define JIMMY_PAPUTTO_SERIAL_DRIVER_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "ublox/ICommDriver.hpp"


namespace JimmyPaputto
{

// Raw-mode tty transport shared by the on-board UART and the USB CDC-ACM
// receivers. CDC-ACM ignores the line speed, but the termios setup is still
// required so the binary UBX stream is not mangled by line discipline.
class SerialDriver: public ICommDriver
{
public:
    SerialDriver(std::string devicePath, const uint32_t baudrate,
        const char* logTag);
    ~SerialDriver() override;

    void transmitReceive(std::span<const uint8_t> txBuff,
        std::vector<uint8_t>& rxBuff) override;
    void getRxBuff(uint8_t* rxBuff, const uint32_t size) override;

    void transmit(std::span<const uint8_t> txBuff);
    int epoll(uint8_t* rxBuff, const uint32_t size, int timeoutMs = -1);

    bool isOpen() const { return fd_ >= 0; }
    const std::string& devicePath() const override { return devicePath_; }
    uint32_t currentBaudrate() const { return baudrate_; }

protected:
    void reopen(const uint32_t baudrate);

private:
    void init(const uint32_t baudrate);
    void deinit();
    void initEpoll();
    void deinitEpoll();

    const char* logTag_;
    std::string devicePath_;
    uint32_t baudrate_;
    int fd_;
    int epollFd_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_SERIAL_DRIVER_HPP_
