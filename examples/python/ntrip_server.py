# Jimmy Paputto 2025

#
# NTRIP Server example (Python)
#
# Demonstrates pushing RTCM3 correction data from a local GNSS base
# station to a remote NTRIP caster.  This is useful when the base
# station is behind NAT / a firewall and cannot accept incoming
# connections directly.
#
# Usage:
#   python ntrip_server.py --caster-host <HOST> --caster-port 2101 \
#       --mountpoint <MOUNT> --password <PWD>
#

import argparse
import signal
import sys

from jimmypaputto import gnsshat


running = True


def signal_handler(sig, frame):
    global running
    running = False


def log_callback(level, message):
    labels = {0: 'ERR', 1: 'WRN', 2: 'INF', 3: 'DBG'}
    print(f'[NtripServer][{labels.get(level, "?")}] {message}')


def main():
    parser = argparse.ArgumentParser(
        description='Push RTCM3 corrections to a remote NTRIP caster')
    parser.add_argument('--caster-host', required=True,
                        help='Remote caster hostname or IP')
    parser.add_argument('--caster-port', type=int, default=2101,
                        help='Remote caster port (default: 2101)')
    parser.add_argument('--mountpoint', required=True,
                        help='Mountpoint name on the remote caster')
    parser.add_argument('--username', default='',
                        help='Source / upload username')
    parser.add_argument('--password', default='',
                        help='Source / upload password')
    args = parser.parse_args()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # ── Initialise the GNSS HAT as Base ─────────────────────────────
    config = {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {'frequency': 1, 'pulse_width': 0.1},
            'pulse_when_no_fix': None,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE,
        },
        'geofencing': None,
        'rtk': {
            'mode': gnsshat.RtkMode.BASE,
            'base': {
                'base_mode': gnsshat.BaseMode.SURVEY_IN,
                'survey_in': {
                    'minimum_observation_time_s': 120,
                    'required_position_accuracy_m': 50.0,
                },
            },
        },
    }

    hat = gnsshat.GnssHat()
    hat.hard_reset_cold_start()
    if not hat.start(config):
        print('Failed to start GNSS', file=sys.stderr)
        return 1

    print('GNSS started as RTK Base (Survey-In mode).')

    # ── Connect NtripServer to remote caster ────────────────────────
    server = gnsshat.NtripServer(
        host=args.caster_host,
        port=args.caster_port,
        mountpoint=args.mountpoint,
        username=args.username,
        password=args.password,
    )
    server.set_log_callback(log_callback)
    server.set_log_level(2)  # Info
    server.set_auto_reconnect(True, 1000, 30000)

    # With auto-reconnect enabled, connect() only raises on setup errors;
    # a caster that is down at startup is retried in the background.
    try:
        server.connect()
    except RuntimeError as e:
        print(f'Failed to connect to remote caster: {e}', file=sys.stderr)
        return 1

    print(f'Pushing corrections to {args.caster_host}:{args.caster_port}'
          f'/{args.mountpoint}')

    # ── Main loop: forward corrections ──────────────────────────────
    while running:
        nav = hat.wait_and_get_fresh_navigation()
        fix_type = gnsshat.FixType(nav.pvt.fix_type)
        if fix_type != gnsshat.FixType.TIME_ONLY_FIX:
            print(f'Fix type: {fix_type.name}, '
                  f'waiting for TIME_ONLY_FIX for RTK Base')
            continue

        try:
            corrections = hat.rtk_get_full_corrections()
        except RuntimeError as e:
            print(f'RTK corrections not available: {e}', file=sys.stderr)
            continue

        server.feed(corrections)

    # ── Cleanup ─────────────────────────────────────────────────────
    stats = server.get_stats()
    print(f'\nSession stats: {stats["bytes_tx"]} bytes TX, '
          f'{stats["frames_tx"]} frames TX, '
          f'{stats["uptime_ms"] / 1000:.1f}s uptime')

    server.disconnect()
    print('Disconnected.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
