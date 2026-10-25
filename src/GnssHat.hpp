/*
 * Jimmy Paputto 2024
 */

#ifndef GNSS_HAT_HPP_
#define GNSS_HAT_HPP_

#include "Version.hpp"

#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

#include "ublox/GnssConfig.hpp"
#include "ublox/Navigation.hpp"
#include "ublox/RTK.hpp"
#include "ublox/SystemHealth.hpp"
#include "ublox/TimeMark.hpp"

#include "ntrip/NtripCaster.hpp"
#include "ntrip/NtripClient.hpp"
#include "ntrip/NtripServer.hpp"

namespace JimmyPaputto
{

class IGnssHat
{
public:
    virtual bool start(const GnssConfig& config) = 0;

    virtual std::string_view name() const = 0;

    virtual Navigation navigation() const = 0;
    // Blocks until the next navigation epoch. Returns std::nullopt once the
    // wait is stopped: by `stop`, by a signal after stopOnSignals(), or by
    // the destruction of this instance.
    virtual std::optional<Navigation> waitAndGetFreshNavigation(
        std::stop_token stop = {}) = 0;

    virtual void hardResetUbloxSom_ColdStart() const = 0;
    virtual void softResetUbloxSom_HotStart() = 0;

    virtual IRtk* rtk() = 0;

    virtual bool startForwardForGpsd() = 0;
    virtual void stopForwardForGpsd() = 0;
    // Blocks until gpsd forwarding is stopped: by stopForwardForGpsd(), by a
    // signal after stopOnSignals(), or by the destruction of this instance.
    // Useful to park the main thread of a gpsd-only daemon; cleanup does not
    // need it, because destruction stops the forwarder.
    virtual void joinForwardForGpsd() = 0;
    virtual std::string getGpsdDevicePath() const = 0;

    virtual bool enableTimepulse() = 0;
    virtual void disableTimepulse() = 0;
    // Blocks until the next timepulse. Returns false when the timepulse is
    // not enabled or the wait is stopped, as in waitAndGetFreshNavigation().
    virtual bool timepulse(std::stop_token stop = {}) = 0;

    virtual std::optional<TimeMark> timeMark() const = 0;
    // Blocks until the next time mark event. Returns std::nullopt when the
    // HAT has no time mark input or the wait is stopped, as in
    // waitAndGetFreshNavigation().
    virtual std::optional<TimeMark> waitAndGetFreshTimeMark(
        std::stop_token stop = {}) = 0;

    virtual SystemHealth systemHealth() const = 0;
    virtual std::string swVersion() const = 0;
    virtual std::string hwVersion() const = 0;
    virtual std::vector<std::string> monVerExtensions() const = 0;

    virtual bool enableTimeMarkTrigger() = 0;
    virtual void disableTimeMarkTrigger() = 0;
    virtual void triggerTimeMark(
        ETimeMarkTriggerEdge edge = ETimeMarkTriggerEdge::Toggle) = 0;

    // SIGINT and SIGTERM stop every wait of this instance for good, so a loop
    // on the waits ends and destructors clean up. Replaces the process'
    // handlers of these signals until the last instance that called this is
    // destroyed; after the first signal a second one terminates the process
    // at once. Returns false if the handlers could not be installed.
    virtual bool stopOnSignals() = 0;

    // Never returns nullptr: an unknown or missing HAT terminates the process.
    // Use Hat::detectProduct() first to fail gracefully.
    static IGnssHat* create();

    // Deleting stops the waits, wakes joinForwardForGpsd() and waits until
    // blocked calls have returned. No thread may start a new call once
    // deletion has begun.
    virtual ~IGnssHat() = default;
};

namespace Hat
{
// Reads /proc/device-tree/hat/<field>. Returns empty string if the field
// is missing or the HAT EEPROM device-tree entry is not present. Does NOT
// touch hardware.
std::string readEepromField(const std::string& field);

// Convenience: returns the HAT product name (e.g. "L1/L5 GNSS RTK HAT"),
// or an empty string if no HAT is detected.
std::string detectProduct();
}

namespace Utils
{

std::string eBand2string(const EGnssBand e);
std::string eFixQuality2string(const EFixQuality e);
std::string eFixStatus2string(const EFixStatus e);
std::string eFixType2string(const EFixType e);
std::string jammingState2string(const EJammingState e);
std::string antennaStatus2string(const EAntennaStatus e);
std::string antennaPower2string(const EAntennaPower e);
std::string geofencingStatus2string(const EGeofencingStatus e);
std::string geofenceStatus2string(const EGeofenceStatus e);
std::string gnssId2string(const EGnssId e);
std::string svQuality2string(const ESvQuality e);
std::string timeMarkMode2string(const ETimeMarkMode e);
std::string timeMarkRun2string(const ETimeMarkRun e);
std::string timeMarkTimeBase2string(const ETimeMarkTimeBase e);

std::string utcTimeFromGnss_ISO8601(const PositionVelocityTime& pvt);

}  // Utils

}  // JimmyPaputto

#endif  // GNSS_HAT_HPP_
