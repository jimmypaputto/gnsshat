/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_TIMEPULSE_HPP_
#define JIMMY_PAPUTTO_TIMEPULSE_HPP_

#include <cstdint>
#include <cstdio>
#include <thread>

#include "common/ThreadingSignals.hpp"
#include "common/GpioInterruptLine.hpp"
#include "common/Notifier.hpp"


#define TIMEPULSE_PIN 5

namespace JimmyPaputto
{

class Timepulse
{
public:
    explicit Timepulse(Notifier& notifier)
    :   notifier_(notifier),
        gpioLine_(TIMEPULSE_PIN,
            GpioInterruptLine::Edge::Rising, "ublox_timepulse")
    {
    }

    ~Timepulse()
    {
        interruptHandler_.request_stop();
        if (interruptHandler_.joinable())
            interruptHandler_.join();
    }

    void run()
    {
        interruptHandler_ = createSignalFreeThread([this](std::stop_token stoken) {
            interruptHandler(stoken);
        });
    }

private:
    // A timeout of 0 would return at once and spin a whole core. This one
    // bounds how long a stop waits for the loop to notice it.
    static constexpr int64_t waitTimeoutNs = 100'000'000;

    void interruptHandler(std::stop_token stoken)
    {
        while (!stoken.stop_requested())
        {
            auto event = gpioLine_.waitEvent(waitTimeoutNs);
            if (event == GpioInterruptLine::EventType::Rising)
            {
                notifier_.notify();
            }
            else if (event == GpioInterruptLine::EventType::Error)
            {
                fprintf(stderr, "[Timepulse] GPIO event error\r\n");
            }
        }
    }

    Notifier& notifier_;
    GpioInterruptLine gpioLine_;
    std::jthread interruptHandler_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_TIMEPULSE_HPP_
