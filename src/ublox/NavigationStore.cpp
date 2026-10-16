/*
 * Jimmy Paputto 2021
 */

#include "ublox/NavigationStore.hpp"


namespace JimmyPaputto
{

void NavigationStore::dop(const DilutionOverPrecision& dop)
{
    std::scoped_lock lock(mutex_);
    navigation_.dop = dop;
}

void NavigationStore::pvt(const PositionVelocityTime& pvt)
{
    std::scoped_lock lock(mutex_);
    navigation_.pvt = pvt;
}

void NavigationStore::geofencingCfg(const Geofencing::Cfg& cfg)
{
    std::scoped_lock lock(mutex_);
    navigation_.geofencing.cfg = cfg;
}

void NavigationStore::geofencingNav(const Geofencing::Nav& nav)
{
    std::scoped_lock lock(mutex_);
    navigation_.geofencing.nav = nav;
}

void NavigationStore::rfBlocks(const std::vector<RfBlock>& rfBlocks)
{
    std::scoped_lock lock(mutex_);
    navigation_.rfBlocks = rfBlocks;
}

void NavigationStore::rfBlocksSpectrumData(
    const std::vector<RfBlockSpectrumData>& rfBlocksSpectrumData)
{
    std::scoped_lock lock(mutex_);
    navigation_.rfBlocksSpectrumData = rfBlocksSpectrumData;
}

void NavigationStore::satellites(const std::vector<SatelliteInfo>& satellites)
{
    std::scoped_lock lock(mutex_);
    navigation_.satellites = satellites;
}

void NavigationStore::monVer(const std::string& swVersion,
    const std::string& hwVersion, const std::vector<std::string>& extensions)
{
    std::scoped_lock lock(mutex_);
    swVersion_ = swVersion;
    hwVersion_ = hwVersion;
    monVerExtensions_ = extensions;
}

std::string NavigationStore::swVersion() const
{
    std::scoped_lock lock(mutex_);
    return swVersion_;
}

std::string NavigationStore::hwVersion() const
{
    std::scoped_lock lock(mutex_);
    return hwVersion_;
}

std::vector<std::string> NavigationStore::monVerExtensions() const
{
    std::scoped_lock lock(mutex_);
    return monVerExtensions_;
}

void NavigationStore::systemHealth(const SystemHealth& systemHealth)
{
    std::scoped_lock lock(mutex_);
    systemHealth_ = systemHealth;
}

SystemHealth NavigationStore::systemHealth() const
{
    std::scoped_lock lock(mutex_);
    return systemHealth_;
}

void NavigationStore::timeMark(const TimeMark& timeMark)
{
    std::scoped_lock lock(mutex_);
    timeMark_ = timeMark;
}

std::optional<TimeMark> NavigationStore::timeMark() const
{
    std::scoped_lock lock(mutex_);
    return timeMark_;
}

Navigation NavigationStore::navigation() const
{
    std::scoped_lock lock(mutex_);
    return navigation_;
}

}  // JimmyPaputto
