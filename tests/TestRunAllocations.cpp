/*
 * Jimmy Paputto 2026
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <span>
#include <stop_token>
#include <vector>

#include "common/Notifier.hpp"
#include "ublox/GnssConfig.hpp"
#include "ublox/ICommDriver.hpp"
#include "ublox/NavigationStore.hpp"
#include "ublox/Rtcm3Parser.hpp"
#include "ublox/Rtcm3Store.hpp"
#include "ublox/Run.hpp"
#include "ublox/UbloxConfigRegistry.hpp"
#include "ublox/UbxParser.hpp"


using namespace JimmyPaputto;

namespace
{

thread_local bool countingAllocations = false;
thread_local std::size_t allocationCount = 0;

// Counts the operator new calls made on this thread while alive. Take the
// count with stop() before asserting: gtest allocates on its own.
class AllocationCounter
{
public:
    AllocationCounter()
    {
        allocationCount = 0;
        countingAllocations = true;
    }

    ~AllocationCounter()
    {
        countingAllocations = false;
    }

    std::size_t stop()
    {
        countingAllocations = false;
        return allocationCount;
    }
};

}  // namespace

// Replaces the global allocation functions of the whole test binary, gnsshat
// included; outside an AllocationCounter they only forward to malloc.
void* operator new(std::size_t size)
{
    if (countingAllocations)
        ++allocationCount;
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept
{
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}

namespace
{

std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts)
{
    std::vector<uint8_t> bytes;
    for (const auto& part : parts)
        bytes.insert(bytes.end(), part.begin(), part.end());
    return bytes;
}

std::vector<uint8_t> ubxFrame(uint8_t classId, uint8_t msgId,
    const std::vector<uint8_t>& payload)
{
    const auto length = static_cast<uint16_t>(payload.size());
    std::vector<uint8_t> frame {
        0xB5, 0x62, classId, msgId,
        static_cast<uint8_t>(length), static_cast<uint8_t>(length >> 8)
    };
    frame.insert(frame.end(), payload.begin(), payload.end());
    UbxParser::addChecksum(frame);
    return frame;
}

// The UTC second tags each epoch, so a test can tell which PVT came last.
std::vector<uint8_t> navPvt(uint8_t second)
{
    std::vector<uint8_t> payload(92, 0);
    payload[10] = second;
    return ubxFrame(0x01, 0x07, payload);
}

std::vector<uint8_t> navDop()
{
    return ubxFrame(0x01, 0x04, std::vector<uint8_t>(18, 0));
}

std::vector<uint8_t> navSat(uint8_t numSvs)
{
    std::vector<uint8_t> payload(8 + 12 * numSvs, 0);
    payload[5] = numSvs;
    for (uint8_t i = 0; i < numSvs; i++)
        payload[8 + 12 * i + 1] = i + 1;  // svId
    return ubxFrame(0x01, 0x35, payload);
}

std::vector<uint8_t> monRf(uint8_t numberOfRfBlocks)
{
    std::vector<uint8_t> payload(4 + 24 * numberOfRfBlocks, 0);
    payload[1] = numberOfRfBlocks;
    return ubxFrame(0x0A, 0x38, payload);
}

std::vector<uint8_t> monSpan(uint8_t numberOfRfBlocks)
{
    std::vector<uint8_t> payload(4 + 272 * numberOfRfBlocks, 0);
    payload[1] = numberOfRfBlocks;
    return ubxFrame(0x0A, 0x31, payload);
}

std::vector<uint8_t> navPvtBurst(uint8_t firstSecond, uint8_t count)
{
    std::vector<uint8_t> bytes;
    for (uint8_t i = 0; i < count; i++)
    {
        const auto frame = navPvt(firstSecond + i);
        bytes.insert(bytes.end(), frame.begin(), frame.end());
    }
    return bytes;
}

std::vector<uint8_t> rtcm3Frame(uint16_t msgId, uint16_t dataLength)
{
    std::vector<uint8_t> frame {
        0xD3,
        static_cast<uint8_t>((dataLength >> 8) & 0x03),
        static_cast<uint8_t>(dataLength),
        static_cast<uint8_t>(msgId >> 4),
        static_cast<uint8_t>((msgId << 4) & 0xF0)
    };
    frame.resize(3 + dataLength, 0x55);
    const uint32_t crc = Rtcm3Parser::crc24q(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>(crc >> 16));
    frame.push_back(static_cast<uint8_t>(crc >> 8));
    frame.push_back(static_cast<uint8_t>(crc));
    return frame;
}

// Behaves like the module on SPI: hands out the queued bytes, then 0xFF
// idle padding once its output buffer is drained.
class FakeSpiDriver final : public ICommDriver
{
public:
    FakeSpiDriver()
    {
        queue_.reserve(64 * 1024);
    }

    void queue(std::span<const uint8_t> bytes)
    {
        queue_.insert(queue_.end(), bytes.begin(), bytes.end());
    }

    void transmitReceive(std::span<const uint8_t>,
        std::vector<uint8_t>&) override
    {
    }

    void getRxBuff(uint8_t* rxBuff, const uint32_t size) override
    {
        const auto count = std::min<std::size_t>(size, queue_.size() - read_);
        std::memcpy(rxBuff, queue_.data() + read_, count);
        std::memset(rxBuff + count, 0xFF, size - count);
        read_ += count;
        if (read_ == queue_.size())
        {
            queue_.clear();
            read_ = 0;
        }
    }

private:
    std::vector<uint8_t> queue_;
    std::size_t read_ = 0;
};

// What GnssHat::start() wires around an M9NRun, without the hardware.
struct M9NHarness
{
    GnssConfig config{};
    UbloxConfigRegistry configRegistry{config};
    NavigationStore navigationStore;
    Notifier timeMarkNotifier;
    Notifier txReadyNotifier;
    Notifier navigationNotifier;
    UbxParser parser{configRegistry, navigationStore, timeMarkNotifier};
    FakeSpiDriver driver;
    M9NRun run{driver, parser, txReadyNotifier, navigationNotifier};
    std::stop_source stop;

    // One TX-READY wakeup carrying `burst`, then `extraPasses` more passes
    // for a burst that overruns the receive buffer. Returns the allocations
    // the run loop made.
    std::size_t receive(std::span<const uint8_t> burst, int extraPasses = 0)
    {
        driver.queue(burst);
        txReadyNotifier.notify();
        const auto token = stop.get_token();

        AllocationCounter counter;
        run.execute(token);
        for (int pass = 0; pass < extraPasses; pass++)
            run.execute(token);
        return counter.stop();
    }
};

}  // namespace


// Every buffer is ready before the first epoch, so not even the first
// message of each type allocates.
TEST(RunAllocations, M9NDoesNotAllocateFromTheFirstEpoch)
{
    M9NHarness hat;
    const auto nav = concat({ navPvt(1), navDop(), navSat(40) });
    const auto mon = concat({ monRf(1), monSpan(1) });

    std::size_t allocations = 0;
    for (int epoch = 0; epoch < 20; epoch++)
    {
        allocations += hat.receive(nav);
        allocations += hat.receive(mon);
    }

    EXPECT_EQ(allocations, 0u);
    const auto navigation = hat.navigationStore.navigation();
    EXPECT_EQ(navigation.satellites.size(), 40u);
    EXPECT_EQ(navigation.rfBlocks.size(), 1u);
    EXPECT_EQ(navigation.rfBlocksSpectrumData.size(), 1u);
}

TEST(RunAllocations, M9NMoreSatellitesThanEverDoesNotAllocate)
{
    M9NHarness hat;
    hat.receive(concat({ navPvt(1), navSat(4) }));

    // 90 satellites make a NAV-SAT frame longer than one 1 KB SPI read.
    const auto crowdedSky = concat({ navPvt(2), navSat(90) });
    const auto allocations = hat.receive(crowdedSky);

    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(hat.navigationStore.navigation().satellites.size(), 90u);
}

TEST(RunAllocations, M9NFrameCutByFullBufferIsCarriedOver)
{
    M9NHarness hat;

    // 82 frames of 100 bytes overrun the 8 KB receive buffer: the first pass
    // stops with the last frame cut off, the second one completes it.
    const auto warmUp = navPvtBurst(1, 82);
    ASSERT_GT(warmUp.size(), 8192u);
    hat.receive(warmUp, 1);

    const auto allocations = hat.receive(navPvtBurst(101, 82), 1);

    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(hat.navigationStore.navigation().pvt.utc.ss, 182);
}

// F10TRun parses whatever the UART has delivered, so frames are routinely
// cut between reads, headers included.
TEST(RunAllocations, ParserCarriesFramesAcrossReadsWithoutAllocating)
{
    GnssConfig config{};
    UbloxConfigRegistry configRegistry(config);
    NavigationStore navigationStore;
    Notifier timeMarkNotifier;
    UbxParser parser(configRegistry, navigationStore, timeMarkNotifier);
    std::vector<uint8_t> buffer(8192);

    const auto feed = [&](const std::vector<uint8_t>& stream) {
        constexpr std::size_t readSize = 37;
        std::size_t offset = 0;
        for (std::size_t pos = 0; pos < stream.size(); pos += readSize)
        {
            const auto count = std::min(readSize, stream.size() - pos);
            std::memcpy(buffer.data() + offset, stream.data() + pos, count);
            offset = UbxParser::moveUnfinishedToFront(
                parser.parse({ buffer.data(), offset + count }), buffer
            );
        }
        return offset;
    };

    feed(concat({ navPvt(1), navDop(), navSat(30), monRf(2), monSpan(2) }));

    const auto stream =
        concat({ navPvt(2), navDop(), navSat(31), monRf(2), monSpan(2) });
    AllocationCounter counter;
    const auto leftover = feed(stream);
    const auto allocations = counter.stop();

    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(leftover, 0u);
    const auto navigation = navigationStore.navigation();
    EXPECT_EQ(navigation.pvt.utc.ss, 2);
    EXPECT_EQ(navigation.satellites.size(), 31u);
    EXPECT_EQ(navigation.rfBlocksSpectrumData.size(), 2u);
}

// As F9PRun::executeUartBase: UART reads into a 2 KB buffer, with frames of
// the same type growing between epochs as more satellites come into view.
TEST(RunAllocations, Rtcm3BaseStoresGrowingFramesInPlace)
{
    Rtcm3Store store;
    Rtcm3Parser parser(store);
    std::vector<uint8_t> unfinished;
    unfinished.reserve(Rtcm3Store::maxFrameSize);
    std::vector<uint8_t> buffer(2048);

    const auto feed = [&](const std::vector<uint8_t>& stream) {
        constexpr std::size_t readSize = 300;
        std::size_t offset = 0;
        for (std::size_t pos = 0; pos < stream.size(); pos += readSize)
        {
            const auto count = std::min(readSize, stream.size() - pos);
            std::memcpy(buffer.data() + offset, stream.data() + pos, count);
            parser.parse({ buffer.data(), offset + count }, unfinished);
            offset = unfinished.size();
            std::copy(unfinished.begin(), unfinished.end(), buffer.begin());
            unfinished.clear();
        }
    };
    const auto epoch = [](uint16_t extraLength) {
        return concat({
            rtcm3Frame(1005, 19),
            rtcm3Frame(1077, 200 + extraLength),
            rtcm3Frame(1087, 150 + extraLength)
        });
    };

    feed(epoch(0));

    const auto grownEpoch = epoch(800);
    AllocationCounter counter;
    feed(grownEpoch);
    const auto allocations = counter.stop();

    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(store.getFrame(1077), rtcm3Frame(1077, 1000));
    EXPECT_EQ(store.getFrame(1087), rtcm3Frame(1087, 950));
}

TEST(RunAllocations, Rtcm3RoverTakesBatchesWithoutCopying)
{
    Rtcm3Store store;
    std::stop_source stop;
    std::vector<std::vector<uint8_t>> frames;

    for (uint16_t extraLength : { 0, 100 })
    {
        const std::vector<std::vector<uint8_t>> corrections {
            rtcm3Frame(1005, 19), rtcm3Frame(1077, 300 + extraLength)
        };
        store.updateFramesAndNotify(corrections);

        AllocationCounter counter;
        const bool received = store.waitForFrames(stop.get_token(), frames);
        const auto allocations = counter.stop();

        EXPECT_TRUE(received);
        EXPECT_EQ(allocations, 0u);
        EXPECT_EQ(frames, corrections);
    }

    stop.request_stop();
    EXPECT_FALSE(store.waitForFrames(stop.get_token(), frames));
    EXPECT_TRUE(frames.empty());
}
