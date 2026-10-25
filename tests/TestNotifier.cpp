#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
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
    Notifier notifier;
    notifier.notify();

    std::stop_source stopped;
    stopped.request_stop();
    EXPECT_FALSE(notifier.wait({}, stopped.get_token()));

    // Would block for good if the stopped wait had consumed it.
    EXPECT_TRUE(notifier.wait({}, {}));
}
