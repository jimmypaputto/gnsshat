# Jimmy Paputto 2026

import signal
import threading

from jimmypaputto import gnsshat


def create_config() -> dict:
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'timing': {
            'enable_time_mark': True
        }
    }


def toggle_thread_func(hat):
    # Toggles EXTINT every 5 navigation epochs (5 s at 1 Hz) and ends when
    # shutdown() stops its wait.
    epoch = 0
    while hat.wait_and_get_fresh_navigation() is not None:
        if epoch % 5 == 0:
            hat.trigger_time_mark(gnsshat.TimeMarkTriggerEdge.TOGGLE)
            print("[EXTINT] toggled")
        epoch += 1


def signals_handler(hat):
    signals = {signal.SIGINT, signal.SIGTERM}
    # Blocks the signals in the calling thread (main); the new thread
    # inherits the mask. Library threads block them on their own.
    signal.pthread_sigmask(signal.SIG_BLOCK, signals)

    def wait_for_signal():
        signal.sigwait(signals)
        hat.shutdown()

    thread = threading.Thread(target=wait_for_signal)
    thread.start()
    return thread


def main():
    with gnsshat.GnssHat() as hat:
        hat.soft_reset_hot_start()

        if not hat.start(create_config()):
            print("Startup failed, exit")
            return -1

        print("Startup done, ublox configured")

        hat.enable_time_mark_trigger()
        print("TimeMark trigger enabled, toggling EXTINT every 5s\n")

        signals_interrupt = signals_handler(hat)
        # After signals_handler(): the toggle thread inherits the blocked
        # signals.
        toggle = threading.Thread(target=toggle_thread_func, args=(hat,))
        toggle.start()
        try:
            while (tm := hat.wait_and_get_fresh_time_mark()) is not None:
                print(tm)
        finally:
            # Wakes sigwait if the loop ends without a signal (e.g. a break added
            # to the loop or an exception).
            if signals_interrupt.is_alive():
                signal.pthread_kill(signals_interrupt.ident, signal.SIGINT)
            signals_interrupt.join()
            toggle.join()
            hat.disable_time_mark_trigger()

    print("Exiting...")
    return 0


if __name__ == "__main__":
    exit(main())
