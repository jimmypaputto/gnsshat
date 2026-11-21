# Jimmy Paputto 2026

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


def main():
    try:
        with gnsshat.GnssHat() as hat:
            hat.soft_reset_hot_start()

            if not hat.start(create_config()):
                print("Startup failed, exit")
                return -1

            print("Startup done, ublox configured")
            print("Waiting for edges on the HAT's external EXTINT input\n")

            while (tm := hat.wait_and_get_fresh_time_mark()) is not None:
                print(tm)
    except KeyboardInterrupt:
        print("\nStopping time mark monitoring...")

    print("Exiting...")
    return 0


if __name__ == "__main__":
    exit(main())
