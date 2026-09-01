/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_CFG_RST_HPP_
#define UBX_CFG_RST_HPP_

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_CFG_RST: public IUbxMsg  // output-only; the receiver never ACKs CFG-RST
{
public:
    explicit UBX_CFG_RST() = default;

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    void deserialize(std::span<const uint8_t>) override
    {

    }

    // navBbrMask=0x0000 (hot start), resetMode=0x01 (controlled SW reset)
    static std::vector<uint8_t> hotStart()
    {
        return buildFrame( {
            0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00
        });
    }

    // resetMode=0x09 (controlled GNSS start): no-op on a running engine,
    // revives one left in the "GNSS stopped" state (issue #40)
    static std::vector<uint8_t> startGnss()
    {
        return buildFrame( {
            0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00, 0x09, 0x00
        });
    }
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_RST_HPP_
