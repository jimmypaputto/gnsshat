/*
 * Jimmy Paputto 2021
 */

#ifndef JP_UBX_PARSER_HPP_
#define JP_UBX_PARSER_HPP_

#include <array>
#include <cstddef>
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
    static constexpr std::size_t maxFrameSize = 4096;

    explicit UbxParser(IUbloxConfigRegistry& configRegistry,
        NavigationStore& navigationStore, Notifier& timeMarkNotifier);

    // Dispatches every complete frame in `buffer` and returns the frame cut
    // off at its end, as a view into `buffer` (empty when there is none).
    // Frames are read in place, so parsing does not allocate.
    std::span<const uint8_t> parse(std::span<const uint8_t> buffer);

    // Moves the unfinished frame returned by parse() to the front of the
    // buffer it points into and returns its length, i.e. the offset at which
    // the next read continues the frame.
    static std::size_t moveUnfinishedToFront(
        std::span<const uint8_t> unfinished, std::span<uint8_t> buffer);

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
    std::span<const uint8_t> extractFrames(std::span<const uint8_t> buffer);

    // Views into the buffer being parsed, valid only during parse(); a frame
    // that failed its checksum is left empty.
    constexpr static uint16_t maxNumberOfFrames_ = 300;
    std::array<std::span<const uint8_t>, maxNumberOfFrames_> frames_;
    std::array<std::span<const uint8_t>, maxNumberOfFrames_>::iterator endFrameIt_;
    IUbloxConfigRegistry& configRegistry_;
    UbxCallbacks ubxCallbacks_;
    UbxFactory ubxFactory_;
};

}  // JimmyPaputto

#endif  // JP_UBX_PARSER_HPP_
