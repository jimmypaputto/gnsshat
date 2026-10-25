/*
 * Jimmy Paputto 2026
 */

#include "SignalStop.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <iterator>
#include <map>
#include <mutex>
#include <thread>

#include "BlockTerminationSignals.hpp"


namespace JimmyPaputto
{

namespace
{

constexpr int handledSignals[] = { SIGINT, SIGTERM };

std::atomic<int> signalPipeWrite{-1};
static_assert(std::atomic<int>::is_always_lock_free,
    "the signal handler needs a lock-free atomic");

void onSignal(int signal)
{
    // Only async-signal-safe calls in here.
    const int savedErrno = errno;
    ::signal(signal, SIG_DFL);
    const char byte = 1;
    [[maybe_unused]] const auto written =
        ::write(signalPipeWrite.load(), &byte, 1);
    errno = savedErrno;
}

struct SignalStopState
{
    std::mutex mtx;
    std::map<uint64_t, std::function<void()>> callbacks;
    uint64_t nextId{1};
    bool signalled{false};
    bool helperStarted{false};
    struct sigaction previous[std::size(handledSignals)]{};
};

// Never destroyed: a HAT destroyed during static destruction still needs it.
SignalStopState& state()
{
    static auto* instance = new SignalStopState;
    return *instance;
}

void runCallbacks()
{
    auto& s = state();
    std::lock_guard lock(s.mtx);
    if (s.signalled || s.callbacks.empty())
        return;

    s.signalled = true;
    for (auto& [id, callback] : s.callbacks)
        callback();
}

void helperLoop(int pipeRead)
{
    char byte;
    while (true)
    {
        const auto count = ::read(pipeRead, &byte, 1);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return;
        runCallbacks();
    }
}

// The pipe and its reader stay for the rest of the process once created:
// closing them could race with a handler that is already running.
bool startHelper(SignalStopState& s)
{
    if (s.helperStarted)
        return true;

    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0)
    {
        perror("[GNSS] Failed to create the signal pipe");
        return false;
    }
    signalPipeWrite.store(fds[1]);

    BlockTerminationSignals blocked;
    std::thread(helperLoop, fds[0]).detach();
    s.helperStarted = true;
    return true;
}

void installHandlers(SignalStopState& s)
{
    struct sigaction action{};
    action.sa_handler = onSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    for (size_t i = 0; i < std::size(handledSignals); ++i)
        sigaction(handledSignals[i], &action, &s.previous[i]);
}

void restoreHandlers(SignalStopState& s)
{
    for (size_t i = 0; i < std::size(handledSignals); ++i)
        sigaction(handledSignals[i], &s.previous[i], nullptr);
}

}  // namespace

std::unique_ptr<SignalStopRegistration> SignalStopRegistration::create(
    std::function<void()> onSignal)
{
    auto& s = state();
    std::lock_guard lock(s.mtx);
    if (!startHelper(s))
        return nullptr;

    if (s.callbacks.empty())
        installHandlers(s);
    if (s.signalled)
        onSignal();

    const auto id = s.nextId++;
    s.callbacks.emplace(id, std::move(onSignal));
    return std::unique_ptr<SignalStopRegistration>(
        new SignalStopRegistration(id));
}

SignalStopRegistration::SignalStopRegistration(uint64_t id)
:   id_(id)
{
}

SignalStopRegistration::~SignalStopRegistration()
{
    auto& s = state();
    std::lock_guard lock(s.mtx);
    s.callbacks.erase(id_);
    if (!s.callbacks.empty())
        return;

    restoreHandlers(s);
    s.signalled = false;
}

}  // JimmyPaputto
