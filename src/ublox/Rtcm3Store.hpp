/*
 * Jimmy Paputto 2025
 */

#ifndef JP_RTCM3_STORE_HPP_
#define JP_RTCM3_STORE_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <vector>

#include "common/JPGuard.hpp"
#include "common/Notifier.hpp"


namespace JimmyPaputto
{

class Rtcm3Store final
{
public:
    // 3-byte header, up to 1023 bytes of payload, 3-byte CRC.
    static constexpr std::size_t maxFrameSize = 3 + 1023 + 3;

    explicit Rtcm3Store();

    void updateFrame(const uint16_t id, std::span<const uint8_t> newFrame);
    void updateFramesAndNotify(
        const std::vector<std::vector<uint8_t>>& newFrames
    );

    std::vector<uint8_t> getFrame(const uint16_t id) const;
    std::vector<std::vector<uint8_t>> getFrames(
        std::span<const uint16_t> ids) const;

    std::vector<std::vector<uint8_t>> waitForFrames();

    // Hands the next batch over by swapping it into `frames` rather than
    // copying it, so the waiting thread does not allocate. Returns false,
    // with `frames` empty, when stopped or the store stays locked.
    bool waitForFrames(std::stop_token stoken,
        std::vector<std::vector<uint8_t>>& frames);

private:
    std::unordered_map<uint16_t, std::vector<uint8_t>> frames_;
    std::vector<std::vector<uint8_t>> incomingFrames_;
    mutable JPGuard xSemaphore_;
    Notifier roverNotifier_;
};

}  // JimmyPaputto

#endif  // JP_RTCM3_STORE_HPP_
