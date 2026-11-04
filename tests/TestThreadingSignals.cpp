#include <gtest/gtest.h>

#include <pthread.h>
#include <signal.h>

#include <stop_token>
#include <thread>

#include "common/ThreadingSignals.hpp"

using namespace JimmyPaputto;


namespace
{

sigset_t currentMask()
{
    sigset_t mask;
    pthread_sigmask(SIG_BLOCK, nullptr, &mask);
    return mask;
}

void expectSameMask(const sigset_t& expected, const sigset_t& actual)
{
    for (int signal = 1; signal < NSIG; ++signal)
    {
        EXPECT_EQ(sigismember(&expected, signal), sigismember(&actual, signal))
            << "signal " << signal;
    }
}

void expectAsynchronousSignalsBlocked(const sigset_t& mask)
{
    EXPECT_TRUE(sigismember(&mask, SIGINT));
    EXPECT_TRUE(sigismember(&mask, SIGTERM));
    EXPECT_TRUE(sigismember(&mask, SIGUSR1));
    EXPECT_TRUE(sigismember(&mask, SIGHUP));
    EXPECT_FALSE(sigismember(&mask, SIGSEGV)) << "faults must reach crash handlers";
}

}  // namespace

// Runs in a thread of its own, so the test runner's mask stays untouched.
TEST(ThreadingSignals, SignalFreeThreadBlocksSignalsAndCallerMaskIsRestored)
{
    std::thread([] {
        // A signal the caller blocked itself must stay blocked afterwards,
        // and the ones it left open must stay open.
        sigset_t own;
        sigemptyset(&own);
        sigaddset(&own, SIGUSR2);
        pthread_sigmask(SIG_SETMASK, &own, nullptr);
        const sigset_t before = currentMask();

        sigset_t inJthread;
        createSignalFreeThread([&inJthread](std::stop_token) {
            inJthread = currentMask();
        }).join();
        sigset_t inThread;
        createSignalFreeThread<std::thread>([&inThread] {
            inThread = currentMask();
        }).join();

        expectAsynchronousSignalsBlocked(inJthread);
        expectAsynchronousSignalsBlocked(inThread);
        expectSameMask(before, currentMask());
        EXPECT_TRUE(sigismember(&before, SIGUSR2));
        EXPECT_FALSE(sigismember(&before, SIGINT));
    }).join();
}

TEST(ThreadingSignals, BlockAllSignalsRestoresMaskAtScopeEnd)
{
    std::thread([] {
        const sigset_t before = currentMask();
        {
            const BlockAllSignals blocked;
            expectAsynchronousSignalsBlocked(currentMask());
        }
        expectSameMask(before, currentMask());
    }).join();
}
