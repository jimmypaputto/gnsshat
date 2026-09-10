/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_E_UBLOX_MODEL_HPP_
#define JIMMY_PAPUTTO_E_UBLOX_MODEL_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EUbloxModel : uint8_t
{
    Unknown = 0,
    NeoM9N,
    NeoF9P,
    NeoF10T
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_E_UBLOX_MODEL_HPP_
