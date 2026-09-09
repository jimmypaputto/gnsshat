/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_UBX_RTCM_DEMUX_HPP_
#define JIMMY_PAPUTTO_UBX_RTCM_DEMUX_HPP_

#include <cstdint>
#include <span>
#include <vector>


namespace JimmyPaputto
{

// SPI and UART keep UBX and RTCM3 on separate ports; a USB receiver interleaves
// them on the single CDC-ACM endpoint, so the byte stream has to be split
// before either parser sees it.
class UbxRtcmDemux
{
public:
    struct Result
    {
        std::vector<uint8_t> ubx;
        std::vector<uint8_t> rtcm3;
        std::vector<uint8_t> pending;  // trailing frame still arriving
    };

    // Bytes matching neither framing are dropped one at a time to resync.
    // Frame integrity is left to the downstream parsers.
    static Result split(std::span<const uint8_t> buffer);
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_UBX_RTCM_DEMUX_HPP_
