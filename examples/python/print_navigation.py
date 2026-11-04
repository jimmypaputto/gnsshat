# Jimmy Paputto 2025

import signal
import threading

from jimmypaputto import gnsshat


def signals_handler(hat):
    signals = {signal.SIGINT, signal.SIGTERM}
    # Blocks the signals in the calling thread (main); the new thread
    # inherits the mask. Library threads block them on their own.
    signal.pthread_sigmask(signal.SIG_BLOCK, signals)

    def wait_for_signal():
        signal.sigwait(signals)
        hat.shutdown()

    # A daemon: an exception in main must not leave the process waiting here.
    thread = threading.Thread(target=wait_for_signal, daemon=True)
    thread.start()
    return thread


def create_config() -> dict:
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.PORTABLE,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None
    }

hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
isStartupDone = hat.start(create_config())
if isStartupDone:
    print("GNSS HAT started successfully!")
else:
    print("Failed to start GNSS HAT, exiting...")
    exit(1)

signals_interrupt = signals_handler(hat)
while (nav := hat.wait_and_get_fresh_navigation()) is not None:
    print("\033[2J\033[H")  # clear terminal
    print(nav.pvt)

signals_interrupt.join()
print("Exiting...")
