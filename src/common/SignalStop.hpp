/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_SIGNAL_STOP_HPP_
#define JIMMY_PAPUTTO_SIGNAL_STOP_HPP_

#include <cstdint>
#include <functional>
#include <memory>


namespace JimmyPaputto
{

// Runs a function once SIGINT or SIGTERM arrives. While any registration
// exists the process' handlers of both signals are replaced; the last one to
// go restores the previous handlers. The first signal resets its disposition
// to the default, so a second one terminates the process at once.
class SignalStopRegistration
{
public:
    // `onSignal` runs on a helper thread, because a signal handler may not
    // take locks; it runs at once if a signal already arrived. It must not
    // create or destroy registrations. Returns nullptr if the handlers could
    // not be installed.
    static std::unique_ptr<SignalStopRegistration> create(
        std::function<void()> onSignal);

    // Waits for a running `onSignal` to return.
    ~SignalStopRegistration();

    SignalStopRegistration(const SignalStopRegistration&) = delete;
    SignalStopRegistration& operator=(const SignalStopRegistration&) = delete;

private:
    explicit SignalStopRegistration(uint64_t id);

    uint64_t id_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_SIGNAL_STOP_HPP_
