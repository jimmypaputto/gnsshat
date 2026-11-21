/*
 * Jimmy Paputto 2026
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "common/Notifier.hpp"
#include "ublox/GnssConfig.hpp"
#include "ublox/ICommDriver.hpp"
#include "ublox/NavigationStore.hpp"
#include "ublox/Startup.hpp"
#include "ublox/UbloxConfigRegistry.hpp"
#include "ublox/UbxCfgKeys.hpp"
#include "ublox/UbxParser.hpp"
#include "ublox/ubxmsg/UBX_CFG_VALGET.hpp"


using namespace JimmyPaputto;
using Clock = std::chrono::steady_clock;

namespace
{

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

uint32_t readKey(std::span<const uint8_t> bytes, std::size_t offset)
{
    uint32_t key = 0;
    std::memcpy(&key, bytes.data() + offset, sizeof(key));
    return key;
}

// Answers CFG-VALGET and CFG-VALSET like the receiver does on SPI: the reply
// comes out on the reads that follow a command, not during the command.
class FakeReceiver final : public ICommDriver
{
public:
    struct Command
    {
        std::vector<uint8_t> frame;
        std::size_t transferSize;
        Clock::time_point sentAt;
        std::optional<Clock::time_point> firstReadAt;
    };

    std::map<uint32_t, std::vector<uint8_t>> values;
    bool rejectCommands = false;
    std::vector<Command> commands;

    void transmitReceive(std::span<const uint8_t> txBuff,
        std::vector<uint8_t>& rxBuff) override
    {
        commands.push_back({
            { txBuff.begin(), txBuff.end() }, rxBuff.size(), Clock::now(), {}
        });
        std::ranges::fill(rxBuff, 0xFF);
        answer(txBuff);
    }

    void getRxBuff(uint8_t* rxBuff, const uint32_t size) override
    {
        if (!commands.empty() && !commands.back().firstReadAt)
            commands.back().firstReadAt = Clock::now();

        const auto count = std::min<std::size_t>(size, pending_.size());
        std::memcpy(rxBuff, pending_.data(), count);
        std::memset(rxBuff + count, 0xFF, size - count);
        pending_.erase(pending_.begin(), pending_.begin() + count);
    }

private:
    void answer(std::span<const uint8_t> frame)
    {
        // Both messages carry 4 bytes ahead of their keys.
        constexpr std::size_t firstKey = 6 + 4;
        const std::size_t end = frame.size() - 2;
        const uint8_t classId = frame[2];
        const uint8_t msgId = frame[3];

        if (rejectCommands)
        {
            reply(ubxFrame(0x05, 0x00, { classId, msgId }));
            return;
        }

        if (classId == 0x06 && msgId == 0x8B)
        {
            std::vector<uint8_t> payload { 0x01, 0x00, 0x00, 0x00 };
            for (std::size_t offset = firstKey; offset + 4 <= end; offset += 4)
            {
                const auto& value = values[readKey(frame, offset)];
                payload.insert(payload.end(),
                    frame.begin() + offset, frame.begin() + offset + 4);
                payload.insert(payload.end(), value.begin(), value.end());
            }
            reply(ubxFrame(0x06, 0x8B, payload));
        }
        else if (classId == 0x06 && msgId == 0x8A)
        {
            for (std::size_t offset = firstKey; offset + 4 <= end; )
            {
                const auto key = readKey(frame, offset);
                const auto size = UbxCfgKeys::keySize(key);
                offset += 4;
                values[key].assign(
                    frame.begin() + offset, frame.begin() + offset + size);
                offset += size;
            }
        }
        reply(ubxFrame(0x05, 0x01, { classId, msgId }));
    }

    void reply(const std::vector<uint8_t>& frame)
    {
        pending_.insert(pending_.end(), frame.begin(), frame.end());
    }

    std::vector<uint8_t> pending_;
};

class TestableStartup final : public M9NStartup
{
public:
    using M9NStartup::M9NStartup;
    using StartupBase::awaitAck;
    using StartupBase::configure;
};

GnssConfig oneHertz()
{
    GnssConfig config{};
    config.measurementRate_Hz = 1;
    return config;
}

struct StartupHarness
{
    GnssConfig config = oneHertz();
    UbloxConfigRegistry configRegistry{config};
    NavigationStore navigationStore;
    Notifier timeMarkNotifier;
    UbxParser parser{configRegistry, navigationStore, timeMarkNotifier};
    FakeReceiver receiver;
    TestableStartup startup{receiver, configRegistry, parser};
};

}  // namespace


TEST(Startup, SendsACommandInATransferOfItsOwnSize)
{
    StartupHarness hat;
    hat.receiver.values[UbxCfgKeys::CFG_RATE_MEAS] = { 0xE8, 0x03 };
    const auto poll = ubxmsg::UBX_CFG_VALGET::poll(UbxCfgKeys::CFG_RATE_MEAS);

    EXPECT_TRUE(hat.startup.awaitAck(poll, EUbxMsg::UBX_CFG_VALGET));

    ASSERT_EQ(hat.receiver.commands.size(), 1u);
    EXPECT_EQ(hat.receiver.commands[0].transferSize, poll.size());
}

TEST(Startup, LeavesTheBusIdleWhileTheSpiPortIsReconfigured)
{
    StartupHarness hat;
    hat.receiver.values[UbxCfgKeys::CFG_SPI_MAXFF] = { 50 };
    const std::array keys { UbxCfgKeys::CFG_SPI_MAXFF };

    EXPECT_TRUE(hat.startup.configure(keys));

    ASSERT_EQ(hat.receiver.commands.size(), 3u);
    const auto& valset = hat.receiver.commands[1];
    EXPECT_EQ(valset.frame[3], 0x8A);
    ASSERT_TRUE(valset.firstReadAt.has_value());
    EXPECT_GE(*valset.firstReadAt - valset.sentAt,
        std::chrono::milliseconds(10));
    EXPECT_EQ(hat.receiver.values[UbxCfgKeys::CFG_SPI_MAXFF],
        std::vector<uint8_t>{ 63 });
}

TEST(Startup, StopsWaitingWhenTheReceiverRejectsTheCommand)
{
    StartupHarness hat;
    hat.receiver.rejectCommands = true;
    const auto poll = ubxmsg::UBX_CFG_VALGET::poll(UbxCfgKeys::CFG_RATE_MEAS);

    const auto start = Clock::now();
    EXPECT_FALSE(hat.startup.awaitAck(poll, EUbxMsg::UBX_CFG_VALGET));

    EXPECT_LT(Clock::now() - start, std::chrono::seconds(1));
}
