/*
 * Jimmy Paputto 2026
 */

#ifndef NAVIGATION_STORE_HPP_
#define NAVIGATION_STORE_HPP_

#include "ublox/Navigation.hpp"
#include "ublox/SystemHealth.hpp"
#include "ublox/TimeMark.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <vector>


namespace JimmyPaputto
{

class NavigationStore final
{
public:
    void dop(const DilutionOverPrecision& dop);
    void pvt(const PositionVelocityTime& pvt);
    void geofencingCfg(const Geofencing::Cfg& cfg);
    void geofencingNav(const Geofencing::Nav& nav);
    void rfBlocks(const std::vector<RfBlock>& rfBlocks);
    void rfBlocksSpectrumData(
        const std::vector<RfBlockSpectrumData>& rfBlocksSpectrumData);
    void satellites(const std::vector<SatelliteInfo>& satellites);

    void monVer(const std::string& swVersion, const std::string& hwVersion,
                const std::vector<std::string>& extensions);
    std::string swVersion() const;
    std::string hwVersion() const;
    std::vector<std::string> monVerExtensions() const;

    void systemHealth(const SystemHealth& systemHealth);
    SystemHealth systemHealth() const;

    void timeMark(const TimeMark& timeMark);
    std::optional<TimeMark> timeMark() const;

    Navigation navigation() const;

private:
    mutable std::mutex mutex_;
    Navigation navigation_;
    std::optional<TimeMark> timeMark_;
    std::string swVersion_;
    std::string hwVersion_;
    std::vector<std::string> monVerExtensions_;
    SystemHealth systemHealth_;
};

}  // JimmyPaputto

#endif  // NAVIGATION_STORE_HPP_
