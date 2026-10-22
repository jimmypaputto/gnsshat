/*
 * Jimmy Paputto 2026
 */

#ifndef GNSS_HAT_INTERNAL_HPP_
#define GNSS_HAT_INTERNAL_HPP_

#include <optional>

#include "GnssHat.hpp"

// Library-internal entry points for the C wrapper. This header is not
// installed; `hat` must come from IGnssHat::create().
namespace JimmyPaputto::Internal
{

// Wakes threads blocked in waitAndGetFresh*(), timepulse() and
// joinForwardForGpsd() and stops gpsd forwarding. After this the waits return
// at once and the instance can only be deleted.
void shutdown(IGnssHat& hat);

// std::nullopt when the wait was ended by shutdown() or by deleting the HAT.
std::optional<Navigation> waitForNavigation(IGnssHat& hat);
std::optional<TimeMark> waitForTimeMark(IGnssHat& hat);

}  // JimmyPaputto::Internal

#endif  // GNSS_HAT_INTERNAL_HPP_
