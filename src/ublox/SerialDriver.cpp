/*
 * Jimmy Paputto 2026
 */

#include "ublox/SerialDriver.hpp"

#include <cstring>
#include <utility>

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/epoll.h>


namespace JimmyPaputto
{

SerialDriver::SerialDriver(std::string devicePath, const uint32_t baudrate,
    const char* logTag)
:   logTag_(logTag),
    devicePath_(std::move(devicePath)),
    baudrate_(baudrate),
    fd_(-1),
    epollFd_(-1)
{
    init(baudrate_);
    initEpoll();
}

SerialDriver::~SerialDriver()
{
    deinitEpoll();
    deinit();
}

void SerialDriver::reopen(const uint32_t baudrate)
{
    deinitEpoll();
    deinit();
    baudrate_ = baudrate;
    init(baudrate_);
    initEpoll();
}

void SerialDriver::transmitReceive(std::span<const uint8_t> txBuff,
    std::vector<uint8_t>& rxBuff)
{
    if (fd_ < 0)
    {
        printf("%s Error: device not initialized\r\n", logTag_);
        return;
    }

    if (txBuff.empty())
    {
        return;
    }

    tcflush(fd_, TCIFLUSH);

    const ssize_t written = write(fd_, txBuff.data(), txBuff.size());
    if (written != static_cast<ssize_t>(txBuff.size()))
    {
        fprintf(
            stderr,
            "%s Error: Failed to write all data, expected: %zu, "
            "written: %ld\n",
            logTag_,
            txBuff.size(),
            written
        );
        if (written < 0)
            perror("[SERIAL] write");
    }

    getRxBuff(rxBuff.data(), rxBuff.size());
}

void SerialDriver::getRxBuff(uint8_t* rxBuff, const uint32_t size)
{
    if (fd_ < 0 || !rxBuff)
        return;

    const ssize_t readed = read(fd_, rxBuff, size);

    if (readed < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            fprintf(
                stderr,
                "%s getRxBuff Error: Failed to read data\r\n",
                logTag_
            );
            perror("[SERIAL] getRxBuff perror");
        }
        std::memset(rxBuff, 0xFF, size);
    }
    else if (readed == 0)
    {
        std::memset(rxBuff, 0xFF, size);
    }
    else if (readed < static_cast<ssize_t>(size))
    {
        std::memset(rxBuff + readed, 0xFF, size - readed);
    }
}

void SerialDriver::init(const uint32_t baudrate)
{
    fd_ = open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0)
    {
        fprintf(stderr, "%s Failed to open %s\r\n", logTag_,
            devicePath_.c_str());
        perror("[SERIAL] open");
        return;
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0)
    {
        perror("[SERIAL] Failed to get terminal attributes");
        close(fd_);
        fd_ = -1;
        return;
    }

    speed_t speed;
    switch (baudrate)
    {
        case 9600U:   speed = B9600;   break;
        case 19200U:  speed = B19200;  break;
        case 38400U:  speed = B38400;  break;
        case 57600U:  speed = B57600;  break;
        case 115200U: speed = B115200; break;
        default:
            printf("%s Error: Unsupported baud rate: %u\n", logTag_, baudrate);
            close(fd_);
            fd_ = -1;
            return;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Configure 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;         // Clear size mask
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable read, ignore control lines

    // Configure input flags - clear ALL processing for raw binary protocol
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                      INLCR | IGNCR | ICRNL |
                      IXON | IXOFF | IXANY);

    // Configure output flags
    tty.c_oflag &= ~OPOST;         // Raw output

    // Configure line discipline
    tty.c_lflag &= ~(ICANON | ECHO | ECHONL | ECHOE | ISIG | IEXTEN);

    // Set timeouts
    tty.c_cc[VMIN] = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 1;  // 0.1 second timeout

    // Apply terminal attributes
    if (tcsetattr(fd_, TCSANOW, &tty) != 0)
    {
        perror("[SERIAL] Error: Failed to set terminal attributes");
        close(fd_);
        fd_ = -1;
        return;
    }

    // Flush any existing data
    tcflush(fd_, TCIOFLUSH);

    baudrate_ = baudrate;
}

void SerialDriver::deinit()
{
    if (fd_ >= 0)
    {
        close(fd_);
        fd_ = -1;
    }
}

void SerialDriver::transmit(std::span<const uint8_t> txBuff)
{
    if (fd_ < 0)
    {
        printf("%s Error: device not initialized\r\n", logTag_);
        return;
    }

    if (txBuff.empty())
    {
        return;
    }

    const ssize_t written = write(fd_, txBuff.data(), txBuff.size());
    if (written != static_cast<ssize_t>(txBuff.size()))
    {
        fprintf(
            stderr,
            "%s Error: Failed to write all data, expected: %zu, "
            "written: %ld\n",
            logTag_,
            txBuff.size(),
            written
        );
        if (written < 0)
            perror("[SERIAL] write");
    }
}

void SerialDriver::initEpoll()
{
    if (fd_ < 0)
    {
        printf("%s Cannot init epoll - device not initialized\n", logTag_);
        return;
    }

    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0)
    {
        perror("[SERIAL] Failed to create epoll instance");
        return;
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd_;

    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd_, &event) < 0)
    {
        perror("[SERIAL] Failed to add fd to epoll");
        close(epollFd_);
        epollFd_ = -1;
        return;
    }
}

void SerialDriver::deinitEpoll()
{
    if (epollFd_ >= 0)
    {
        close(epollFd_);
        epollFd_ = -1;
    }
}

int SerialDriver::epoll(uint8_t* rxBuff, const uint32_t size, int timeoutMs)
{
    if (epollFd_ < 0 || fd_ < 0)
    {
        printf("%s epoll or device not initialized\n", logTag_);
        return -1;
    }

    constexpr int maxEvents = 12;
    struct epoll_event events[maxEvents];
    const int numEvents = epoll_wait(epollFd_, events, maxEvents, timeoutMs);

    if (numEvents < 0)
    {
        if (errno != EINTR)
            perror("[SERIAL] epoll_wait failed");
        return -1;
    }

    if (numEvents == 0)
        return 0;

    ssize_t totalBytesRead = 0;
    if (events[0].data.fd == fd_ && (events[0].events & EPOLLIN))
    {
        while (static_cast<uint32_t>(totalBytesRead) < size)
        {
            const auto bytesRead = read(
                fd_,
                rxBuff + totalBytesRead,
                size - totalBytesRead
            );

            if (bytesRead > 0)
            {
                totalBytesRead += bytesRead;
                continue;
            }

            if (bytesRead == 0)
                break;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            perror("[SERIAL] Read error in epoll");
            break;
        }
    }

    return totalBytesRead;
}

}  // JimmyPaputto
