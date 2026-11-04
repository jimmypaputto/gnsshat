# Jimmy Paputto 2025

from jimmypaputto import gnsshat


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

try:
    with gnsshat.GnssHat() as hat:
        hat.soft_reset_hot_start()
        if not hat.start(create_config()):
            print("Failed to start GNSS HAT, exiting...")
            exit(1)
        print("GNSS HAT started successfully!")

        while (nav := hat.wait_and_get_fresh_navigation()) is not None:
            print("\033[2J\033[H")  # clear terminal
            print(nav.pvt)
except KeyboardInterrupt:
    print("Exiting...")
