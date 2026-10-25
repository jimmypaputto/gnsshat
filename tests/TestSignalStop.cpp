#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "common/SignalStop.hpp"

using namespace JimmyPaputto;
using namespace std::chrono_literals;


namespace
{

void markerHandler(int)
{
}

void (*currentHandler(int signal))(int)
{
    struct sigaction action{};
    sigaction(signal, nullptr, &action);
    return action.sa_handler;
}

bool waitForCalls(const std::atomic<int>& calls, int expected)
{
    for (int i = 0; i < 200 && calls.load() != expected; ++i)
        std::this_thread::sleep_for(10ms);
    return calls.load() == expected;
}

// A handler of the test's own stands in for the application's, so that a
// signal raised by mistake after a registration is gone cannot kill the test.
class SignalStopTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        struct sigaction marker{};
        marker.sa_handler = markerHandler;
        sigemptyset(&marker.sa_mask);
        sigaction(SIGINT, &marker, &previousInt_);
        sigaction(SIGTERM, &marker, &previousTerm_);
    }

    void TearDown() override
    {
        sigaction(SIGINT, &previousInt_, nullptr);
        sigaction(SIGTERM, &previousTerm_, nullptr);
    }

private:
    struct sigaction previousInt_{};
    struct sigaction previousTerm_{};
};

}  // namespace

TEST_F(SignalStopTest, SignalRunsCallbacksOnceAndRestoresHandlers)
{
    std::atomic<int> calls{0};
    {
        auto first = SignalStopRegistration::create([&] { ++calls; });
        auto second = SignalStopRegistration::create([&] { ++calls; });
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        EXPECT_NE(currentHandler(SIGINT), markerHandler);

        std::raise(SIGINT);
        EXPECT_TRUE(waitForCalls(calls, 2));
        EXPECT_EQ(currentHandler(SIGINT), SIG_DFL)
            << "a second SIGINT must terminate the process";

        std::raise(SIGTERM);
        std::this_thread::sleep_for(100ms);
        EXPECT_EQ(calls.load(), 2) << "callbacks run only once";
    }
    EXPECT_EQ(currentHandler(SIGINT), markerHandler);
    EXPECT_EQ(currentHandler(SIGTERM), markerHandler);
}

TEST_F(SignalStopTest, RegistrationAfterSignalRunsAtOnce)
{
    std::atomic<int> first{0};
    auto early = SignalStopRegistration::create([&] { ++first; });
    std::raise(SIGTERM);
    ASSERT_TRUE(waitForCalls(first, 1));

    std::atomic<int> second{0};
    auto late = SignalStopRegistration::create([&] { ++second; });
    EXPECT_EQ(second.load(), 1);
}

TEST_F(SignalStopTest, SignalIsForgottenOnceAllRegistrationsAreGone)
{
    {
        std::atomic<int> calls{0};
        auto registration =
            SignalStopRegistration::create([&] { ++calls; });
        std::raise(SIGINT);
        ASSERT_TRUE(waitForCalls(calls, 1));
    }

    std::atomic<int> calls{0};
    auto registration = SignalStopRegistration::create([&] { ++calls; });
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(calls.load(), 0);
    EXPECT_NE(currentHandler(SIGINT), SIG_DFL);
}
