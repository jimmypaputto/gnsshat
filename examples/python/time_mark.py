# Jimmy Paputto 2026

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


def main():
    try:
        with gnsshat.GnssHat() as hat:
            hat.soft_reset_hot_start()

            if not hat.start(create_config()):
                print("Startup failed, exit")
                return -1

            print("Startup done, ublox configured")

            hat.enable_time_mark_trigger()
            print("TimeMark trigger enabled, toggling EXTINT every 5s\n")

            toggle = threading.Thread(target=toggle_thread_func, args=(hat,))
            toggle.start()
            try:
                while (tm := hat.wait_and_get_fresh_time_mark()) is not None:
                    print(tm)
            finally:
                # Ends the wait in the toggle thread.
                hat.shutdown()
                toggle.join()
                hat.disable_time_mark_trigger()
    except KeyboardInterrupt:
        print("\nStopping time mark monitoring...")

    print("Exiting...")
    return 0


if __name__ == "__main__":
    exit(main())
