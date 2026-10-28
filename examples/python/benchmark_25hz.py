# Jimmy Paputto 2026
# Benchmark: measures frames/s received via wait_and_get_fresh_navigation() at 25Hz.
# Output: one ISO 8601 UTC time per line
# Usage: sudo python3 benchmark_25hz.py > /tmp/bench_py.csv

import signal
import sys
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

    thread = threading.Thread(target=wait_for_signal)
    thread.start()
    return thread


def create_benchmark_config() -> dict:
    return {
        'measurement_rate_hz': 25,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
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
is_startup_done = hat.start(create_benchmark_config())
if not is_startup_done:
    print("Failed to start GNSS HAT, exiting...", flush=True)
    exit(1)

print("Benchmark started at 25Hz. Ctrl+C to stop.", file=sys.stderr, flush=True)

signals_interrupt = signals_handler(hat)
try:
    while (nav := hat.wait_and_get_fresh_navigation()) is not None:
        print(gnsshat.utc_time_iso8601(nav.pvt), flush=True)
finally:
    # Wakes sigwait if the loop ends without a signal (e.g. a break added
    # to the loop or an exception).
    if signals_interrupt.is_alive():
        signal.pthread_kill(signals_interrupt.ident, signal.SIGINT)
    signals_interrupt.join()
print("Stopped.", file=sys.stderr, flush=True)
