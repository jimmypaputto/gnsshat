# Jimmy Paputto 2025

import signal
import threading

from jimmypaputto import gnsshat


PULSE_RATE_HZ = 5


def create_default_config() -> dict:
    """Create configuration with timepulse enabled at PULSE_RATE_HZ"""
    pulse = {
        'frequency': PULSE_RATE_HZ,
        'pulse_width': 0.1
    }
    return {
        'measurement_rate_hz': PULSE_RATE_HZ,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': pulse,
            'pulse_when_no_fix': pulse,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None
    }


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


def main():
    hat = gnsshat.GnssHat()
    hat.hard_reset_cold_start()

    is_startup_done = hat.start(create_default_config())
    if not is_startup_done:
        print("Startup failed, exit")
        return -1

    print("Startup done, ublox configured")

    # Raises RuntimeError while the kernel PPS driver holds GPIO 5
    # (dtoverlay=pps-gpio).
    hat.enable_timepulse()

    signals_interrupt = signals_handler(hat)
    counter = 0
    while hat.timepulse():
        nav = hat.get_navigation()
        utc = nav.pvt.utc_time
        date = nav.pvt.date
        time_valid = "VALID" if utc.valid else "INVALID"
        date_valid = "VALID" if date.valid else "INVALID"
        print(f"[{gnsshat.utc_time_iso8601(nav.pvt)}] Timepulse: {counter} | time:{time_valid} date:{date_valid} acc:{utc.accuracy}ns")
        counter += 1

    signals_interrupt.join()

    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
