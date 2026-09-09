/*
 * Jimmy Paputto 2026
 */

#include "ublox/UbxRtcmDemux.hpp"


namespace JimmyPaputto
{

namespace
{

constexpr uint8_t ubxSyncChar1 = 0xB5;
constexpr uint8_t ubxSyncChar2 = 0x62;
constexpr size_t ubxHeaderSize = 6;
constexpr size_t ubxChecksumSize = 2;

constexpr uint8_t rtcm3Preamble = 0xD3;
constexpr size_t rtcm3HeaderSize = 3;
constexpr size_t rtcm3CrcSize = 3;
// The six bits above the 10-bit length field are reserved and always zero,
// which is what keeps a stray 0xD3 inside a UBX payload from being taken for
// the start of an RTCM3 frame.
constexpr uint8_t rtcm3ReservedMask = 0xFC;

void append(std::vector<uint8_t>& sink, std::span<const uint8_t> frame)
{
    sink.insert(sink.end(), frame.begin(), frame.end());
}

}  // namespace

UbxRtcmDemux::Result UbxRtcmDemux::split(std::span<const uint8_t> buffer)
{
    Result result;
    size_t i = 0;

    while (i < buffer.size())
    {
        if (buffer[i] == ubxSyncChar1)
        {
            if (i + 1 >= buffer.size())
                break;

            if (buffer[i + 1] == ubxSyncChar2)
            {
                if (i + ubxHeaderSize > buffer.size())
                    break;

                const size_t payloadLen =
                    static_cast<size_t>(buffer[i + 4]) |
                    (static_cast<size_t>(buffer[i + 5]) << 8);
                const size_t frameLen =
                    ubxHeaderSize + payloadLen + ubxChecksumSize;

                if (i + frameLen > buffer.size())
                    break;

                append(result.ubx, buffer.subspan(i, frameLen));
                i += frameLen;
                continue;
            }
        }
        else if (buffer[i] == rtcm3Preamble)
        {
            if (i + rtcm3HeaderSize > buffer.size())
                break;

            if ((buffer[i + 1] & rtcm3ReservedMask) == 0)
            {
                const size_t payloadLen =
                    (static_cast<size_t>(buffer[i + 1] & 0x03) << 8) |
                    static_cast<size_t>(buffer[i + 2]);
                const size_t frameLen =
                    rtcm3HeaderSize + payloadLen + rtcm3CrcSize;

                if (i + frameLen > buffer.size())
                    break;

                append(result.rtcm3, buffer.subspan(i, frameLen));
                i += frameLen;
                continue;
            }
        }

        ++i;
    }

    append(result.pending, buffer.subspan(i));
    return result;
}

}  // JimmyPaputto
