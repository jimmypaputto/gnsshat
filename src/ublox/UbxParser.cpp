/*
 * Jimmy Paputto 2022
 */

#include "UbxParser.hpp"

#include <algorithm>
#include <cstring>

#include "EUbxMsg.hpp"
#include "UbxClassMsgId.hpp"
#include "UbxFactory.hpp"
#include "Ublox.hpp"

#include "ubxmsg/UBX_ACK_ACK.hpp"
#include "ubxmsg/UBX_ACK_NAK.hpp"
#include "ubxmsg/UBX_NAV_DOP.hpp"
#include "ubxmsg/UBX_NAV_PVT.hpp"

#include "common/Utils.hpp"


namespace JimmyPaputto
{

UbxParser::UbxParser(IUbloxConfigRegistry& configRegistry,
    NavigationStore& navigationStore, Notifier& timeMarkNotifier)
:   endFrameIt_(frames_.begin()),
    configRegistry_(configRegistry),
    ubxCallbacks_(configRegistry, navigationStore, timeMarkNotifier)
{
}

std::span<const uint8_t> UbxParser::parse(std::span<const uint8_t> buffer)
{
    const auto unfinishedFrame = extractFrames(buffer);
    for (auto frameIt = frames_.begin(); frameIt != endFrameIt_; frameIt++)
    {
        if (frameIt->empty())
            continue;

        const auto frame = *frameIt;
        const auto eUbxMsg = UbxClassMsgId::translate(
            { frame[2], frame[3] }
        );
        if (auto* ubxMsg = ubxFactory_.create(eUbxMsg, frame))
        {
            ubxCallbacks_.run(*ubxMsg, eUbxMsg);
        }
    }
    return unfinishedFrame;
}

std::size_t UbxParser::moveUnfinishedToFront(
    std::span<const uint8_t> unfinished, std::span<uint8_t> buffer)
{
    // The frame lies inside `buffer`, so source and destination may overlap.
    if (!unfinished.empty())
        std::memmove(buffer.data(), unfinished.data(), unfinished.size());
    return unfinished.size();
}

std::span<const uint8_t> UbxParser::extractFrames(
    std::span<const uint8_t> buffer)
{
    constexpr std::array<uint8_t, 2> pattern {0xB5, 0x62};
    constexpr std::ptrdiff_t headerSize = 6;
    constexpr std::ptrdiff_t checksumSize = 2;

    endFrameIt_ = frames_.begin();
    auto searchFrom = buffer.begin();

    while (endFrameIt_ != frames_.end())
    {
        const auto frameBegin = std::search(
            searchFrom, buffer.end(), pattern.begin(), pattern.end()
        );
        const auto available = std::distance(frameBegin, buffer.end());
        if (available == 0)
            break;

        if (available < headerSize)
            return { frameBegin, buffer.end() };

        const std::ptrdiff_t payloadSize = frameBegin[4] | (frameBegin[5] << 8);
        const std::ptrdiff_t frameSize = headerSize + payloadSize + checksumSize;
        if (available < frameSize)
            return { frameBegin, buffer.end() };

        const std::span<const uint8_t> frame(frameBegin, frameSize);
        *endFrameIt_++ = checkFrame(frame) ? frame : std::span<const uint8_t>{};
        searchFrom = frameBegin + frameSize;
    }
    return {};
}

void UbxParser::addChecksum(std::vector<uint8_t>& frame)
{
    const auto ck = UbxParser::checksum(frame, 0);
    frame.insert(frame.end(), ck.begin(), ck.end());

    if (!UbxParser::checkFrame(frame))
    {
        frame = { 0xFF };
    }
}

std::array<uint8_t, 2> UbxParser::checksum(std::span<const uint8_t> frame,
    uint8_t offset)
{
    uint8_t cka = 0;
    uint8_t ckb = 0;

    for (std::size_t i = 2; i < frame.size() - offset; i++)
    {
        cka += frame[i];
        ckb += cka;
    }

    return { cka, ckb };
}

bool UbxParser::checkFrame(std::span<const uint8_t> frame)
{
    uint8_t cka = 0;
    uint8_t ckb = 0;

    for (std::size_t i = 2; i < frame.size() - 2; i++)
    {
        cka += frame[i];
        ckb += cka;
    }

    if (cka == frame[frame.size() - 2] && ckb == frame[frame.size() - 1])
    {
        return true;
    }

    return false;
}

}  // JimmyPaputto
