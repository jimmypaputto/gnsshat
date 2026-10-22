#include <gtest/gtest.h>

#include <string>
#include <unistd.h>

#include "ublox/NmeaForwarder.hpp"

using namespace JimmyPaputto;


namespace
{

std::string symlinkTarget(const std::string& path)
{
    char target[256];
    const ssize_t length = readlink(path.c_str(), target, sizeof(target) - 1);
    return length > 0 ? std::string(target, length) : std::string{};
}

}  // namespace

// /dev/jimmypaputto is only writable by root.
TEST(NmeaForwarder, StoppingOldForwarderKeepsNewerDevicePath)
{
    if (geteuid() != 0)
        GTEST_SKIP() << "needs root to create /dev/jimmypaputto/gnss";

    NmeaForwarder older;
    ASSERT_TRUE(older.createVirtualTty());
    const std::string olderPty = symlinkTarget(older.getDevicePath());

    NmeaForwarder newer;
    ASSERT_TRUE(newer.createVirtualTty());
    const std::string newerPty = symlinkTarget(newer.getDevicePath());
    ASSERT_NE(olderPty, newerPty);

    older.stopForwarding();
    EXPECT_EQ(symlinkTarget(newer.getDevicePath()), newerPty);

    newer.stopForwarding();
    EXPECT_TRUE(symlinkTarget(newer.getDevicePath()).empty());
}
