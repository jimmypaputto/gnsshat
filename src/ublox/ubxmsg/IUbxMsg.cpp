/*
 * Jimmy Paputto 2022
 */

#include "IUbxMsg.hpp"

#include "ublox/UbxParser.hpp"


namespace JimmyPaputto::ubxmsg
{

std::vector<uint8_t> IUbxMsg::buildFrame(std::vector<uint8_t> frame)
{
    JimmyPaputto::UbxParser::addChecksum(frame);
    return frame;
}

std::size_t IUbxMsg::payloadLength(std::span<const uint8_t> frame)
{
    constexpr std::size_t checksumSize = 2;
    if (frame.size() < payloadOffset + checksumSize)
        return 0;

    const std::size_t length = frame[4] | (frame[5] << 8);
    if (frame.size() != payloadOffset + length + checksumSize)
        return 0;

    return length;
}

}  // JimmyPaputto::ubxmsg
