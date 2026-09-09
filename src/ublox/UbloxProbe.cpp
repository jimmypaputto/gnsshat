/*
 * Jimmy Paputto 2026
 */

#include "ublox/UbloxProbe.hpp"

#include <chrono>
#include <span>

#include "ublox/UbxParser.hpp"
#include "ublox/ubxmsg/UBX_MON_VER.hpp"


namespace JimmyPaputto
{

namespace
{

constexpr uint8_t ubxSyncChar1 = 0xB5;
constexpr uint8_t ubxSyncChar2 = 0x62;
constexpr uint8_t monVerClass = 0x0A;
constexpr uint8_t monVerId = 0x04;
constexpr size_t ubxHeaderSize = 6;
constexpr size_t ubxChecksumSize = 2;

std::span<const uint8_t> findMonVerFrame(std::span<const uint8_t> buffer)
{
    for (size_t i = 0;
        i + ubxHeaderSize + ubxChecksumSize <= buffer.size(); ++i)
    {
        if (buffer[i] != ubxSyncChar1 || buffer[i + 1] != ubxSyncChar2)
            continue;
        if (buffer[i + 2] != monVerClass || buffer[i + 3] != monVerId)
            continue;

        const size_t payloadLen =
            static_cast<size_t>(buffer[i + 4]) |
            (static_cast<size_t>(buffer[i + 5]) << 8);
        const size_t frameLen = ubxHeaderSize + payloadLen + ubxChecksumSize;

        if (i + frameLen > buffer.size())
            return {};  // still arriving

        const auto frame = buffer.subspan(i, frameLen);
        if (UbxParser::checkFrame(frame))
            return frame;
    }

    return {};
}

EUbloxModel modelFromProtocolFamily(const std::string& text)
{
    if (text.find("HPG") != std::string::npos)
        return EUbloxModel::NeoF9P;
    if (text.find("SPG") != std::string::npos)
        return EUbloxModel::NeoM9N;
    return EUbloxModel::Unknown;
}

}  // namespace

EUbloxModel UbloxProbe::modelFromMonVer(const std::string& swVersion,
    const std::vector<std::string>& extensions)
{
    for (const auto& extension : extensions)
    {
        if (!extension.starts_with("MOD="))
            continue;
        if (extension.find("F9P") != std::string::npos)
            return EUbloxModel::NeoF9P;
        if (extension.find("M9N") != std::string::npos)
            return EUbloxModel::NeoM9N;
    }

    // Firmware that omits MOD= still names the protocol family, and high
    // precision (HPG) only ships on the F9 line.
    for (const auto& extension : extensions)
    {
        if (!extension.starts_with("FWVER="))
            continue;
        const auto model = modelFromProtocolFamily(extension);
        if (model != EUbloxModel::Unknown)
            return model;
    }

    return modelFromProtocolFamily(swVersion);
}

UbloxIdentity UbloxProbe::identify(SerialDriver& driver, int timeoutMs)
{
    UbloxIdentity identity;

    if (!driver.isOpen())
        return identity;

    driver.transmit(ubxmsg::UBX_MON_VER::poll());

    constexpr uint32_t rxBuffSize = 4096;
    std::vector<uint8_t> rxBuff(rxBuffSize);
    uint32_t offset = 0;

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeoutMs);

    while (offset < rxBuffSize)
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            break;

        const auto remainingMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now).count();

        const int bytesRead = driver.epoll(
            rxBuff.data() + offset,
            rxBuffSize - offset,
            static_cast<int>(remainingMs)
        );

        if (bytesRead < 0)
            break;
        if (bytesRead == 0)
            continue;

        offset += static_cast<uint32_t>(bytesRead);

        const auto frame = findMonVerFrame(
            std::span<const uint8_t>(rxBuff.data(), offset)
        );
        if (frame.empty())
            continue;

        const ubxmsg::UBX_MON_VER monVer(frame);
        identity.swVersion = monVer.swVersion();
        identity.hwVersion = monVer.hwVersion();
        identity.extensions = monVer.extensions();
        identity.model = modelFromMonVer(
            identity.swVersion, identity.extensions
        );
        return identity;
    }

    return identity;
}

}  // JimmyPaputto
