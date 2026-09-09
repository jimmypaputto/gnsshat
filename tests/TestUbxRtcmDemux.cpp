#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "ublox/Rtcm3Parser.hpp"
#include "ublox/UbxParser.hpp"
#include "ublox/UbxRtcmDemux.hpp"


using namespace JimmyPaputto;

namespace
{

std::vector<uint8_t> buildUbxFrame(uint8_t cls, uint8_t id,
    const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> frame = {
        0xB5, 0x62, cls, id,
        static_cast<uint8_t>(payload.size() & 0xFF),
        static_cast<uint8_t>((payload.size() >> 8) & 0xFF)
    };
    frame.insert(frame.end(), payload.begin(), payload.end());
    UbxParser::addChecksum(frame);
    return frame;
}

std::vector<uint8_t> buildRtcm3Frame(uint16_t payloadLength)
{
    std::vector<uint8_t> frame = {
        0xD3,
        static_cast<uint8_t>((payloadLength >> 8) & 0x03),
        static_cast<uint8_t>(payloadLength & 0xFF)
    };
    for (uint16_t i = 0; i < payloadLength; ++i)
        frame.push_back(static_cast<uint8_t>(i & 0xFF));

    const uint32_t crc = Rtcm3Parser::crc24q(frame.data(), frame.size());
    frame.push_back((crc >> 16) & 0xFF);
    frame.push_back((crc >> 8) & 0xFF);
    frame.push_back(crc & 0xFF);
    return frame;
}

void append(std::vector<uint8_t>& sink, const std::vector<uint8_t>& more)
{
    sink.insert(sink.end(), more.begin(), more.end());
}

}  // namespace


TEST(UbxRtcmDemux, EmptyBufferYieldsNothing)
{
    const auto result = UbxRtcmDemux::split({});

    EXPECT_TRUE(result.ubx.empty());
    EXPECT_TRUE(result.rtcm3.empty());
    EXPECT_TRUE(result.pending.empty());
}

TEST(UbxRtcmDemux, SingleUbxFrameGoesToUbx)
{
    const auto frame = buildUbxFrame(0x01, 0x07, {0x11, 0x22, 0x33});

    const auto result = UbxRtcmDemux::split(frame);

    EXPECT_EQ(result.ubx, frame);
    EXPECT_TRUE(result.rtcm3.empty());
    EXPECT_TRUE(result.pending.empty());
}

TEST(UbxRtcmDemux, SingleRtcm3FrameGoesToRtcm3)
{
    const auto frame = buildRtcm3Frame(8);

    const auto result = UbxRtcmDemux::split(frame);

    EXPECT_EQ(result.rtcm3, frame);
    EXPECT_TRUE(result.ubx.empty());
    EXPECT_TRUE(result.pending.empty());
}

TEST(UbxRtcmDemux, InterleavedStreamIsSeparated)
{
    const auto ubx1 = buildUbxFrame(0x01, 0x07, {0xAA, 0xBB});
    const auto rtcm1 = buildRtcm3Frame(6);
    const auto ubx2 = buildUbxFrame(0x0A, 0x04, {0x01});
    const auto rtcm2 = buildRtcm3Frame(10);

    std::vector<uint8_t> stream;
    append(stream, ubx1);
    append(stream, rtcm1);
    append(stream, ubx2);
    append(stream, rtcm2);

    const auto result = UbxRtcmDemux::split(stream);

    std::vector<uint8_t> expectedUbx;
    append(expectedUbx, ubx1);
    append(expectedUbx, ubx2);

    std::vector<uint8_t> expectedRtcm;
    append(expectedRtcm, rtcm1);
    append(expectedRtcm, rtcm2);

    EXPECT_EQ(result.ubx, expectedUbx);
    EXPECT_EQ(result.rtcm3, expectedRtcm);
    EXPECT_TRUE(result.pending.empty());
}

TEST(UbxRtcmDemux, TruncatedTrailingUbxFrameBecomesPending)
{
    const auto complete = buildUbxFrame(0x01, 0x07, {0x01, 0x02});
    auto truncated = buildUbxFrame(0x01, 0x35, {0x09, 0x08, 0x07, 0x06});
    truncated.resize(truncated.size() - 3);

    std::vector<uint8_t> stream;
    append(stream, complete);
    append(stream, truncated);

    const auto result = UbxRtcmDemux::split(stream);

    EXPECT_EQ(result.ubx, complete);
    EXPECT_EQ(result.pending, truncated);
}

TEST(UbxRtcmDemux, TruncatedTrailingRtcm3FrameBecomesPending)
{
    const auto complete = buildRtcm3Frame(4);
    auto truncated = buildRtcm3Frame(12);
    truncated.resize(truncated.size() - 5);

    std::vector<uint8_t> stream;
    append(stream, complete);
    append(stream, truncated);

    const auto result = UbxRtcmDemux::split(stream);

    EXPECT_EQ(result.rtcm3, complete);
    EXPECT_EQ(result.pending, truncated);
}

TEST(UbxRtcmDemux, LoneSyncByteAtEndBecomesPending)
{
    const auto complete = buildUbxFrame(0x01, 0x07, {0x42});

    std::vector<uint8_t> stream;
    append(stream, complete);
    stream.push_back(0xB5);

    const auto result = UbxRtcmDemux::split(stream);

    EXPECT_EQ(result.ubx, complete);
    ASSERT_EQ(result.pending.size(), 1u);
    EXPECT_EQ(result.pending[0], 0xB5);
}

// A UBX payload byte that happens to be 0xD3 must not open an RTCM3 frame and
// swallow the rest of the stream.
TEST(UbxRtcmDemux, Rtcm3PreambleInsideUbxPayloadIsNotMistakenForAFrame)
{
    const auto ubx = buildUbxFrame(0x01, 0x07, {0xD3, 0x00, 0x20, 0xFF, 0xFF});
    const auto rtcm = buildRtcm3Frame(6);

    std::vector<uint8_t> stream;
    append(stream, ubx);
    append(stream, rtcm);

    const auto result = UbxRtcmDemux::split(stream);

    EXPECT_EQ(result.ubx, ubx);
    EXPECT_EQ(result.rtcm3, rtcm);
    EXPECT_TRUE(result.pending.empty());
}

// The reserved bits above the RTCM3 length field are always zero, so a 0xD3
// followed by a non-zero reserved field is just noise to skip.
TEST(UbxRtcmDemux, Rtcm3PreambleWithNonZeroReservedBitsIsSkipped)
{
    const auto ubx = buildUbxFrame(0x01, 0x07, {0x01});

    std::vector<uint8_t> stream = {0xD3, 0xFC, 0x10};
    append(stream, ubx);

    const auto result = UbxRtcmDemux::split(stream);

    EXPECT_EQ(result.ubx, ubx);
    EXPECT_TRUE(result.rtcm3.empty());
    EXPECT_TRUE(result.pending.empty());
}

TEST(UbxRtcmDemux, GarbageBetweenFramesIsDropped)
{
    const auto ubx = buildUbxFrame(0x01, 0x07, {0x05});
    const auto rtcm = buildRtcm3Frame(4);

    std::vector<uint8_t> stream = {0x00, 0x13, 0x7E};
    append(stream, ubx);
    stream.push_back(0x41);
    stream.push_back(0x42);
    append(stream, rtcm);

    const auto result = UbxRtcmDemux::split(stream);

    EXPECT_EQ(result.ubx, ubx);
    EXPECT_EQ(result.rtcm3, rtcm);
    EXPECT_TRUE(result.pending.empty());
}

// Feeding the stream in arbitrary chunks must produce the same frames as one
// pass, which is what the run loop relies on when epoll splits a burst.
TEST(UbxRtcmDemux, ChunkedFeedReassemblesSameFrames)
{
    const auto ubx1 = buildUbxFrame(0x01, 0x07, {0x01, 0x02, 0x03});
    const auto rtcm1 = buildRtcm3Frame(9);
    const auto ubx2 = buildUbxFrame(0x01, 0x35, {0x04});

    std::vector<uint8_t> stream;
    append(stream, ubx1);
    append(stream, rtcm1);
    append(stream, ubx2);

    std::vector<uint8_t> collectedUbx;
    std::vector<uint8_t> collectedRtcm;
    std::vector<uint8_t> carry;

    constexpr size_t chunkSize = 7;
    for (size_t offset = 0; offset < stream.size(); offset += chunkSize)
    {
        const size_t take = std::min(chunkSize, stream.size() - offset);
        carry.insert(carry.end(), stream.begin() + offset,
            stream.begin() + offset + take);

        auto result = UbxRtcmDemux::split(carry);
        append(collectedUbx, result.ubx);
        append(collectedRtcm, result.rtcm3);
        carry = std::move(result.pending);
    }

    std::vector<uint8_t> expectedUbx;
    append(expectedUbx, ubx1);
    append(expectedUbx, ubx2);

    EXPECT_EQ(collectedUbx, expectedUbx);
    EXPECT_EQ(collectedRtcm, rtcm1);
    EXPECT_TRUE(carry.empty());
}
