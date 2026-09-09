/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_STARTUP_HPP_
#define JIMMY_PAPUTTO_STARTUP_HPP_

#include <span>
#include <vector>

#include "ublox/ICommDriver.hpp"
#include "ublox/IUbloxConfigRegistry.hpp"
#include "ublox/UbxParser.hpp"


namespace JimmyPaputto
{

class IStartupStrategy
{
public:
    virtual ~IStartupStrategy() = default;

    virtual bool execute() = 0;
};

class StartupBase
{
public:
    explicit StartupBase(ICommDriver& commDriver,
        IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser);
    virtual ~StartupBase() = default;

protected:
    virtual bool reconfigureCommPort() = 0;
    virtual int pollRxData(uint8_t* rxBuff, uint32_t size, int timeoutMs);

    void rate2Registers(const uint16_t measurementRate_Hz);
    void timepulsePinConfig2Registers(const TimepulsePinConfig& tpc);
    void baseConfig2Registers(const BaseConfig& baseConfig);
    // Populates expectedConfigValues_ for every CFG-NAVSPG-INFIL_* key
    // present in the filters struct. Returns the list of keys that were
    // populated so the caller can hand them to configure().
    std::vector<uint32_t> navigationFilters2Registers(
        const std::optional<GnssConfig::NavigationFilters>& filters);
    bool saveCurrentConfigToFlash();

    bool configure(std::span<const uint32_t> keys);

    bool awaitAck(std::span<const uint8_t> payload, EUbxMsg msgType);
    bool verifyConfig(std::span<const uint32_t> keys);
    // Issue #40: the receiver can come out of boot with the GNSS engine
    // stopped while the config interface answers normally, so startup
    // completes "successfully" on a dead receiver. CFG-RST "GNSS start" is
    // a no-op on a running engine and revives a stopped one.
    void sendGnssEngineStart();
    // Sends UBX-MON-VER poll and feeds the reply (or any pending data) to the
    // parser for up to `timeoutMs`. The MON-VER callback registered with
    // UbxParser populates Gnss::instance().swVersion()/hwVersion()/extensions.
    // Always returns true (best-effort: missing MON-VER must not abort
    // startup).
    bool pollMonVer(int timeoutMs = 500);
    std::vector<uint8_t> getExpectedValue(const uint32_t key);

    ICommDriver& commDriver_;
    IUbloxConfigRegistry& configRegistry_;
    UbxParser& ubxParser_;
    std::vector<uint32_t> timepulsePinConfigKeys_;
    std::vector<uint32_t> navigationFilterKeys_;
    static constexpr uint32_t rxBuffSize = 1024;
    std::vector<uint8_t> rxBuff_;
    static std::unordered_map<uint32_t, std::vector<uint8_t>>
        expectedConfigValues_;
};

class M9NStartup: public StartupBase, public IStartupStrategy
{
public:
    M9NStartup(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser);
    virtual ~M9NStartup() = default;

    bool execute() override;

protected:
    bool reconfigureCommPort() override;
};

class F10TStartup: public StartupBase, public IStartupStrategy
{
public:
    F10TStartup(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser);
    virtual ~F10TStartup() = default;

    bool execute() override;

private:
    bool reconfigureCommPort() override;
    int pollRxData(uint8_t* rxBuff, uint32_t size, int timeoutMs) override;
    bool timeBaseStartup();

    bool timeBaseEnabled_{false};
};

class F9PStartup: public M9NStartup
{
public:
    F9PStartup(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser);
    virtual ~F9PStartup() = default;

    bool execute() override;

private:
    bool rtkBaseStartup();
    bool rtkRoverStartup();

    bool base_{false};
    bool rover_{false};
};

// Reuses the M9N register set-up but drives the USB port instead of SPI. There
// is no TX-READY (CFG-TXREADY-INTERFACE only accepts I2C or SPI) and no
// timepulse pin reaches the host, so both key groups are left alone.
class M9NUsbStartup: public M9NStartup
{
public:
    M9NUsbStartup(ICommDriver& commDriver,
        IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser);
    virtual ~M9NUsbStartup() = default;

    bool execute() override;

protected:
    // CDC-ACM has no line speed and no clock phase to renegotiate.
    bool reconfigureCommPort() override { return true; }
    int pollRxData(uint8_t* rxBuff, uint32_t size, int timeoutMs) override;
};

class F9PUsbStartup: public M9NUsbStartup
{
public:
    F9PUsbStartup(ICommDriver& commDriver,
        IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser);
    virtual ~F9PUsbStartup() = default;

    bool execute() override;

private:
    bool rtkBaseStartup();

    bool base_{false};
    bool rover_{false};
};

}  // JimmyPaputto

#endif // JIMMY_PAPUTTO_STARTUP_HPP_
