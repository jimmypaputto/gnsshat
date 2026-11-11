# Jimmy Paputto 2025

from jimmypaputto import gnsshat


PULSE_RATE_HZ = 5
# Part of the pulse period the pulse lasts: 10% of 200 ms is 20 ms.
PULSE_WIDTH_RATIO = 0.1


def create_default_config() -> dict:
    """Create configuration with timepulse enabled at PULSE_RATE_HZ"""
    pulse = {
        'frequency': PULSE_RATE_HZ,
        'pulse_width': PULSE_WIDTH_RATIO
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


def main():
    try:
        with gnsshat.GnssHat() as hat:
            hat.hard_reset_cold_start()

            if not hat.start(create_default_config()):
                print("Startup failed, exit")
                return -1

            print("Startup done, ublox configured")

            # Raises RuntimeError while the kernel PPS driver holds GPIO 5
            # (dtoverlay=pps-gpio).
            hat.enable_timepulse()

            counter = 0
            while hat.timepulse():
                nav = hat.get_navigation()
                utc = nav.pvt.utc_time
                date = nav.pvt.date
                time_valid = "VALID" if utc.valid else "INVALID"
                date_valid = "VALID" if date.valid else "INVALID"
                print(f"[{gnsshat.utc_time_iso8601(nav.pvt)}] Timepulse: {counter} | time:{time_valid} date:{date_valid} acc:{utc.accuracy}ns")
                counter += 1
    except KeyboardInterrupt:
        print("\nStopping timepulse monitoring...")

    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
