/*
 * Jimmy Paputto 2021
 */

#ifndef JP_UBX_PARSER_HPP_
#define JP_UBX_PARSER_HPP_

#include <array>
#include <memory>
#include <span>
#include <vector>

#include "IUbloxConfigRegistry.hpp"
#include "NavigationStore.hpp"
#include "UbxCallbacks.hpp"
#include "UbxFactory.hpp"
#include "common/Notifier.hpp"
#include "ubxmsg/IUbxMsg.hpp"


namespace JimmyPaputto
{

class UbxParser final
{
public:
    explicit UbxParser(IUbloxConfigRegistry& configRegistry,
        NavigationStore& navigationStore, Notifier& timeMarkNotifier);

    std::vector<uint8_t> parse(std::span<const uint8_t> buffer);

    // True when the buffer just parsed carried a NAV-PVT frame, i.e. a new
    // navigation epoch reached the NavigationStore.
    bool consumeNavigationEpoch()
    {
        return ubxCallbacks_.consumeNavigationEpoch();
    }

    // True when a MON-VER frame was parsed since the last call.
    bool consumeMonVer()
    {
        return ubxCallbacks_.consumeMonVer();
    }

    static void addChecksum(std::vector<uint8_t>& frame);
    static std::array<uint8_t, 2> checksum(std::span<const uint8_t> frame,
        uint8_t offset = 2);
    static bool checkFrame(std::span<const uint8_t> frame);

private:
    void extractFrames(std::span<const uint8_t> buffer);

    constexpr static uint16_t maxNumberOfFrames_ = 300;
    std::array<std::vector<uint8_t>, maxNumberOfFrames_> frames_;
    std::array<std::vector<uint8_t>, maxNumberOfFrames_>::iterator endFrameIt_;
    std::vector<uint8_t> unfinishedFrameFromBuffer_;
    IUbloxConfigRegistry& configRegistry_;
    UbxCallbacks ubxCallbacks_;
    UbxFactory ubxFactory_;
};

}  // JimmyPaputto

#endif  // JP_UBX_PARSER_HPP_
