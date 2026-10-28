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
        'geofencing': None
    }


def gnss_id_name(gnss_id: int) -> str:
    names = {
        gnsshat.GnssId.GPS:     "GPS",
        gnsshat.GnssId.SBAS:    "SBAS",
        gnsshat.GnssId.GALILEO: "Galileo",
        gnsshat.GnssId.BEIDOU:  "BeiDou",
        gnsshat.GnssId.IMES:    "IMES",
        gnsshat.GnssId.QZSS:    "QZSS",
        gnsshat.GnssId.GLONASS: "GLONASS",
    }
    return names.get(gnss_id, "Unknown")


def sv_quality_name(quality: int) -> str:
    names = {
        gnsshat.SvQuality.NO_SIGNAL:                        "No Signal",
        gnsshat.SvQuality.SEARCHING:                        "Searching",
        gnsshat.SvQuality.SIGNAL_ACQUIRED:                  "Acquired",
        gnsshat.SvQuality.SIGNAL_DETECTED_BUT_UNUSABLE:     "Unusable",
        gnsshat.SvQuality.CODE_LOCKED_AND_TIME_SYNCHRONIZED: "Code Locked",
        gnsshat.SvQuality.CODE_AND_CARRIER_LOCKED_1:        "Carrier Lock 1",
        gnsshat.SvQuality.CODE_AND_CARRIER_LOCKED_2:        "Carrier Lock 2",
        gnsshat.SvQuality.CODE_AND_CARRIER_LOCKED_3:        "Carrier Lock 3",
    }
    return names.get(quality, "Unknown")


def print_satellites(nav):
    satellites = nav.satellites

    print("=== Satellite Information ===")
    print(f"Total satellites: {len(satellites)}")
    print(f"{'System':<10} {'SV ID':<6} {'C/N0':<8} {'Elev':<8} "
          f"{'Azim':<8} {'Quality':<14} {'Used':<6} {'Health':<6}")
    print("-" * 78)

    used_count = 0
    eph_count = 0
    alm_count = 0
    diff_count = 0

    for sat in satellites:
        if sat.used_in_fix:
            used_count += 1
        if sat.eph_avail:
            eph_count += 1
        if sat.alm_avail:
            alm_count += 1
        if sat.diff_corr:
            diff_count += 1

        print(f"{gnss_id_name(sat.gnss_id):<10} "
              f"{sat.sv_id:<6} "
              f"{sat.cno:<5} dB "
              f"{sat.elevation:>4}°   "
              f"{sat.azimuth:>4}°   "
              f"{sv_quality_name(sat.quality):<14} "
              f"{'Yes' if sat.used_in_fix else 'No':<6} "
              f"{'OK' if sat.healthy else 'Bad':<6}")

    print("-" * 78)
    print(f"Satellites used in fix: {used_count} / {len(satellites)}")
    print(f"Ephemeris: {eph_count}  Almanac: {alm_count}  DGPS: {diff_count}")
    print()


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


hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
is_startup_done = hat.start(create_config())
if is_startup_done:
    print("GNSS HAT started successfully!")
else:
    print("Failed to start GNSS HAT, exiting...")
    exit(1)

signals_interrupt = signals_handler(hat)
try:
    while (nav := hat.wait_and_get_fresh_navigation()) is not None:
        print("\033[2J\033[H", end="")  # clear terminal
        print(f"Time: {gnsshat.utc_time_iso8601(nav)}")
        print()
        print_satellites(nav)
finally:
    # Wakes sigwait if the loop ends without a signal (e.g. a break added
    # to the loop or an exception).
    if signals_interrupt.is_alive():
        signal.pthread_kill(signals_interrupt.ident, signal.SIGINT)
    signals_interrupt.join()
print("Exiting...")
