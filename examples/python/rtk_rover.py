# Jimmy Paputto 2026

# RTK Rover example with NTRIP Client (Python)
#
# Demonstrates configuring the GNSS module as an RTK Rover that
# receives RTCM3 correction data from an NTRIP caster and applies
# it to the receiver for centimeter-level positioning accuracy.
#
# Uses the native gnsshat.NtripClient (built into the library).
#
# Usage: python rtk_rover.py [--host HOST] [--port PORT]
#                             [--mountpoint MP] [--user U] [--password P]


import argparse
import signal
import threading

from jimmypaputto import gnsshat


def create_config() -> dict:
    """Create rover configuration"""
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'pulse_when_no_fix': None,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None,
        'rtk': {
            'mode': gnsshat.RtkMode.ROVER
        }
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


class NtripRover:
    """
    RTK Rover that connects to an NTRIP caster, receives RTCM3
    corrections, and applies them to the GNSS receiver.
    """

    def __init__(self, host: str, port: int, mountpoint: str,
                 username: str = "", password: str = ""):
        self._ntrip_client: gnsshat.NtripClient | None = None
        self._hat: gnsshat.GnssHat | None = None
        self._host = host
        self._port = port
        self._mountpoint = mountpoint
        self._username = username
        self._password = password

    def start_gnss(self) -> bool:
        """Initialize and start the GNSS receiver in rover mode"""
        self._hat = gnsshat.GnssHat()
        self._hat.soft_reset_hot_start()

        if not self._hat.start(create_config()):
            print("ERROR: Failed to start GNSS receiver")
            return False

        print("GNSS receiver started in RTK Rover mode.")
        return True

    def start_ntrip(self) -> bool:
        """Connect the native NTRIP client."""
        print(f"Connecting to NTRIP caster: "
              f"{self._host}:{self._port}/{self._mountpoint}")

        self._ntrip_client = gnsshat.NtripClient(
            self._host, self._port, self._mountpoint,
            self._username, self._password)

        try:
            self._ntrip_client.connect()
        except RuntimeError as e:
            print(f"ERROR: NTRIP connection failed: {e}")
            return False

        print("NTRIP client connected - streaming RTCM3 corrections.\n")
        return True

    def _apply_corrections(self):
        """
        Applies the RTCM3 frames received from the NTRIP client since the
        last epoch. Called once per navigation epoch, so no extra thread
        has to be stopped.
        """
        frames = self._ntrip_client.receive()
        if frames:
            self._hat.rtk_apply_corrections(frames)

    def run(self):
        """Main loop — prints navigation status at each epoch"""
        if not self.start_gnss():
            return -1
        if not self.start_ntrip():
            return -1

        print("Monitoring RTK fix quality (Ctrl+C to stop)...\n")

        signals_interrupt = signals_handler(self._hat)
        while (nav := self._hat.wait_and_get_fresh_navigation()) is not None:
            self._apply_corrections()
            fix_quality = gnsshat.FixQuality(nav.pvt.fix_quality)
            fix_type = gnsshat.FixType(nav.pvt.fix_type)

            print(f"[{gnsshat.utc_time_iso8601(nav.pvt)}] "
                  f"{fix_quality.name} ({fix_type.name})  "
                  f"{nav.pvt.latitude:.6f}, {nav.pvt.longitude:.6f}  "
                  f"alt={nav.pvt.altitude_msl:.1f}m  "
                  f"sats={nav.pvt.visible_satellites}")

        signals_interrupt.join()
        self.stop()

        print("Program finished.")
        return 0

    def stop(self):
        """Disconnect NTRIP client and clean up."""
        if self._ntrip_client is not None:
            self._ntrip_client.disconnect()


def main():
    parser = argparse.ArgumentParser(description="RTK Rover with NTRIP Client")
    parser.add_argument("--host", default="localhost",
                        help="NTRIP caster hostname or IP")
    parser.add_argument("--port", type=int, default=2101,
                        help="NTRIP caster port")
    parser.add_argument("--mountpoint", default="GNSS_HAT",
                        help="NTRIP mountpoint name")
    parser.add_argument("--user", default="",
                        help="NTRIP username")
    parser.add_argument("--password", default="",
                        help="NTRIP password")
    args = parser.parse_args()

    rover = NtripRover(args.host, args.port, args.mountpoint,
                       args.user, args.password)
    return rover.run()


if __name__ == "__main__":
    exit(main())
