# Jimmy Paputto 2025

import os
import signal
import sys
import threading
import time

from jimmypaputto import gnsshat


def create_default_config():
    """Create default configuration for the GNSS HAT."""
    return {
        'measurement_rate_hz': 10,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'pulse_when_no_fix': None
        },
        'geofencing': None
    }


def wait_for_fix(gnss_hat, timeout_seconds=600):
    """Wait for GPS fix to be acquired. False on timeout or once stopped."""
    start_time = time.time()

    while time.time() - start_time < timeout_seconds:
        nav = gnss_hat.wait_and_get_fresh_navigation()
        if nav is None:
            return False

        if gnsshat.FixStatus(nav.pvt.fix_status) == gnsshat.FixStatus.ACTIVE:
            return True

    return False


def wait_for_epochs(gnss_hat, count):
    """Wait for `count` navigation epochs. False once the waits are stopped."""
    for _ in range(count):
        if gnss_hat.wait_and_get_fresh_navigation() is None:
            return False
    return True


def measure_startups(hat, measurement_rate_hz):
    print("\n--- COLD START TEST ---")
    print("Performing hard reset (cold start)...")
    hat.hard_reset_cold_start()

    print("Waiting for GPS fix after cold start...")
    cold_start_time = time.time()

    if wait_for_fix(hat):
        cold_time_ms = int((time.time() - cold_start_time) * 1000)
        print(f"Cold start took {cold_time_ms} ms")
    else:
        print("Cold start stopped or timed out - no fix acquired")
        return 1

    print(
        "\nWaiting 40 seconds to collect satellite data for hot start..."
    )
    if not wait_for_epochs(hat, 40 * measurement_rate_hz):
        return 1

    print("\n--- HOT START TEST ---")
    print("Performing soft reset (hot start)...")
    hat.soft_reset_hot_start()

    print("Waiting for GPS fix after hot start...")
    hot_start_time = time.time()

    if wait_for_fix(hat):
        hot_time_ms = int((time.time() - hot_start_time) * 1000)
        print(f"Hot start took {hot_time_ms} ms")
    else:
        print("Hot start stopped or timed out - no fix acquired")
        return 1

    print(f"\n--- RESULTS ---")
    print(f"Cold start: {cold_time_ms} ms")
    print(f"Hot start:  {hot_time_ms} ms")

    if cold_time_ms > hot_time_ms:
        improvement = ((cold_time_ms - hot_time_ms) / cold_time_ms) * 100
        print(f"Hot start was {improvement:.1f}% faster!")
    else:
        print("Hot start was not faster")
    return 0


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
    print("This example compares cold start vs hot start performance")

    gnss_config = create_default_config()

    with gnsshat.GnssHat() as hat:
        print("Starting GNSS HAT...")
        
        if not hat.start(gnss_config):
            print("Failed to start GNSS HAT")
            return 1

        print("GNSS HAT started successfully!")

        signals_interrupt = signals_handler(hat)

        result = measure_startups(hat, gnss_config['measurement_rate_hz'])

        # Wakes sigwait when the measurement ends without a signal.
        if signals_interrupt.is_alive():
            signal.pthread_kill(signals_interrupt.ident, signal.SIGINT)
        signals_interrupt.join()

    if result == 0:
        print("\nExample completed successfully")
    return result


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)
