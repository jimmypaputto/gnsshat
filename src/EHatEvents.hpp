/*
 * Jimmy Paputto 2026
 */

#ifndef E_HAT_EVENTS_HPP_
#define E_HAT_EVENTS_HPP_

#include <cstdint>


namespace JimmyPaputto
{

// What IGnssHat::waitForEvent() waits for.
enum class EHatEvent : uint8_t
{
    Navigation,
    TimeMark,
    Timepulse
};

// How IGnssHat::waitForEvent() ended.
enum class EWaitResult : uint8_t
{
    Ready,
    Timeout,
    Stopped
};

}  // JimmyPaputto

#endif  // E_HAT_EVENTS_HPP_
