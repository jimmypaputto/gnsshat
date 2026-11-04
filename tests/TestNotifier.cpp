#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <latch>
#include <stop_token>
#include <thread>

#include "common/Notifier.hpp"

using namespace JimmyPaputto;
using namespace std::chrono_literals;


TEST(Notifier, NotifyWakesTwoTokenWait)
{
    Notifier notifier;
    std::stop_source own, other;
    std::atomic<int> result{-1};
    std::thread waiter([&] {
        result = notifier.wait(own.get_token(), other.get_token());
    });

    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(result.load(), -1);
    notifier.notify();
    waiter.join();
    EXPECT_EQ(result.load(), 1);
}

TEST(Notifier, EitherTokenEndsTheWait)
{
    for (const bool stopOwn : { true, false })
    {
        Notifier notifier;
        std::stop_source own, other;
        std::atomic<int> result{-1};
        std::thread waiter([&] {
            result = notifier.wait(own.get_token(), other.get_token());
        });

        std::this_thread::sleep_for(100ms);
        EXPECT_EQ(result.load(), -1);
        (stopOwn ? own : other).request_stop();
        waiter.join();
        EXPECT_EQ(result.load(), 0) << (stopOwn ? "own" : "other");
    }
}

TEST(Notifier, StoppedTokenKeepsPendingNotification)
{
    for (const bool stopOwn : { true, false })
    {
        SCOPED_TRACE(stopOwn ? "instance token" : "caller token");
        Notifier notifier;
        notifier.notify();

        std::stop_source own, other;
        (stopOwn ? own : other).request_stop();
        const bool consumed = notifier.wait(own.get_token(), other.get_token());
        EXPECT_FALSE(consumed);

        if (!consumed)
        {
            EXPECT_TRUE(notifier.wait({}, {}));
        }
    }
}

TEST(Notifier, StoppedTokensRejectLaterNotifications)
{
    for (const bool stopOwn : { true, false })
    {
        SCOPED_TRACE(stopOwn ? "instance token" : "caller token");
        Notifier notifier;
        std::stop_source own, other;
        (stopOwn ? own : other).request_stop();
        for (int i = 0; i < 20; ++i)
        {
            notifier.notify();
            EXPECT_FALSE(notifier.wait(own.get_token(), other.get_token()));
        }
    }
}

TEST(Notifier, CancellingOneCallerLeavesAnotherWaitUsable)
{
    Notifier notifier;
    std::stop_source own;
    std::latch entered(2);
    std::promise<bool> firstResult, secondResult;
    auto firstFuture = firstResult.get_future();
    auto secondFuture = secondResult.get_future();
    std::jthread first([&](std::stop_token stop) {
        entered.count_down();
        firstResult.set_value(notifier.wait(own.get_token(), stop));
    });
    std::jthread second([&](std::stop_token stop) {
        entered.count_down();
        secondResult.set_value(notifier.wait(own.get_token(), stop));
    });

    entered.wait();
    first.request_stop();
    first.join();
    EXPECT_FALSE(firstFuture.get());
    EXPECT_EQ(secondFuture.wait_for(0ms), std::future_status::timeout);

    notifier.notify();
    second.join();
    EXPECT_TRUE(secondFuture.get());
}

TEST(Notifier, StoppingInstanceEndsAllWaits)
{
    Notifier notifier;
    std::stop_source own;
    std::latch entered(4);
    std::array<bool, 4> results{true, true, true, true};
    std::array<std::jthread, 4> waiters;
    for (size_t i = 0; i < waiters.size(); ++i)
    {
        waiters[i] = std::jthread([&, i](std::stop_token stop) {
            entered.count_down();
            results[i] = notifier.wait(own.get_token(), stop);
        });
    }

    entered.wait();
    own.request_stop();
    for (auto& waiter : waiters)
        waiter.join();
    for (const bool result : results)
        EXPECT_FALSE(result);
}

TEST(Notifier, WaitForReportsNotifiedAndTimedOut)
{
    Notifier notifier;
    notifier.notify();
    EXPECT_EQ(notifier.waitFor({}, 10ms), Notifier::EWaitFor::Notified);
    EXPECT_EQ(notifier.waitFor({}, 10ms), Notifier::EWaitFor::TimedOut);
}

TEST(Notifier, WaitForStopKeepsPendingNotification)
{
    Notifier notifier;
    notifier.notify();
    std::stop_source stopped;
    stopped.request_stop();
    EXPECT_EQ(notifier.waitFor(stopped.get_token(), 10ms),
        Notifier::EWaitFor::Stopped);
    EXPECT_EQ(notifier.waitFor({}, 10ms), Notifier::EWaitFor::Notified);
}

TEST(Notifier, StopEndsWaitForBeforeTheTimeout)
{
    Notifier notifier;
    std::stop_source stop;
    std::promise<Notifier::EWaitFor> result;
    auto future = result.get_future();
    std::thread waiter([&] {
        result.set_value(notifier.waitFor(stop.get_token(), 30s));
    });

    std::this_thread::sleep_for(50ms);
    stop.request_stop();
    ASSERT_EQ(future.wait_for(5s), std::future_status::ready);
    EXPECT_EQ(future.get(), Notifier::EWaitFor::Stopped);
    waiter.join();
}
