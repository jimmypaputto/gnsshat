/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_UBLOX_PROBE_HPP_
#define JIMMY_PAPUTTO_UBLOX_PROBE_HPP_

#include <string>
#include <vector>

#include "ublox/EUbloxModel.hpp"
#include "ublox/SerialDriver.hpp"


namespace JimmyPaputto
{

struct UbloxIdentity
{
    EUbloxModel model{EUbloxModel::Unknown};
    std::string swVersion;
    std::string hwVersion;
    std::vector<std::string> extensions;
};

// Identifies the receiver behind an already open transport by polling
// UBX-MON-VER. The reply is decoded here rather than through UbxParser on
// purpose: UbxParser publishes into the Gnss singleton, so probing a second
// receiver would report the first one's firmware.
class UbloxProbe
{
public:
    static UbloxIdentity identify(SerialDriver& driver, int timeoutMs = 1000);

    static EUbloxModel modelFromMonVer(const std::string& swVersion,
        const std::vector<std::string>& extensions);
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_UBLOX_PROBE_HPP_
