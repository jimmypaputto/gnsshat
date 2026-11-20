# GNSS HAT - Python Bindings

CPython extension module for the Jimmy Paputto GNSS HAT library. Wraps the C API (`libgnsshat.so`) as a native Python module.

```python
from jimmypaputto import gnsshat

hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
hat.start({
    'measurement_rate_hz': 1,
    'dynamic_model': gnsshat.DynamicModel.PORTABLE,
    'timepulse_pin_config': {
        'active': True,
        'fixed_pulse': {'frequency': 1, 'pulse_width': 0.1},
        'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
    }
})

nav = hat.wait_and_get_fresh_navigation()
print(f"{nav.pvt.latitude:.6f}, {nav.pvt.longitude:.6f}")
```

## Installation

```bash
sudo apt install build-essential cmake libgpiod-dev python3-dev

cd gnsshat
mkdir -p build && cd build
cmake .. -DBUILD_PYTHON=ON
make -j$(nproc)
sudo make install && sudo ldconfig
```

The Python module can also be built standalone (requires the C++ library installed first):

```bash
cd gnsshat/python
mkdir -p build && cd build
cmake .. && make -j$(nproc)
sudo make install
```

Both builds install the `jimmypaputto` package system-wide, into the `platlib`
directory of the `python3` found by CMake for the `/usr` prefix, regardless of
`CMAKE_INSTALL_PREFIX`. On Raspberry Pi OS and Debian that is
`/usr/local/lib/python3.X/dist-packages/jimmypaputto/`. CMake prints the path at
configure time (`Will install to: ...`), and you can query it with:

```bash
python3 -c "import sysconfig; print(sysconfig.get_path('platlib', vars={'base': '/usr', 'platbase': '/usr'}))"
```

A virtual environment sees the module only if it was created with
`--system-site-packages`.

## Configuration

The `start()` method accepts a Python dict. All fields are optional: `measurement_rate_hz` defaults to 1 Hz and `dynamic_model` to `PORTABLE`. Timepulse stays disabled unless `timepulse_pin_config` is given with `'active': True`.

```python
config = {
    'measurement_rate_hz': 10,                    # 1-25 Hz
    'dynamic_model': gnsshat.DynamicModel.AUTOMOTIVE,
    # Timepulse — None or omit to disable
    'timepulse_pin_config': {
        'active': True,
        'fixed_pulse': {'frequency': 1, 'pulse_width': 0.1},
        'pulse_when_no_fix': {'frequency': 1, 'pulse_width': 0.5},  # optional
        'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
    },

    # Geofencing (L1 HAT, RTK HAT only) — None or omit to disable
    'geofencing': {
        'confidence_level': 3,          # 0-5
        'pin_polarity': gnsshat.PioPinPolarity.LOW_MEANS_INSIDE,  # optional
        'geofences': [
            {'lat': 52.2297, 'lon': 21.0122, 'radius': 100.0},   # max 4
        ]
    },

    # RTK (RTK HAT only) — None or omit to disable
    'rtk': {
        'mode': gnsshat.RtkMode.BASE,
        'base': {
            'base_mode': gnsshat.BaseMode.SURVEY_IN,
            'survey_in': {
                'minimum_observation_time_s': 60,
                'required_position_accuracy_m': 2.0
            }
            # OR for fixed position:
            # 'base_mode': gnsshat.BaseMode.FIXED_POSITION,
            # 'fixed_position': {
            #     'position_type': gnsshat.FixedPositionType.LLA,
            #     'lla': {'latitude_deg': 52.2, 'longitude_deg': 21.0, 'height_m': 120.0},
            #     'position_accuracy_m': 0.01
            # }
        }
    },

    # Timing (TIME HAT only) — None or omit to disable
    'timing': {
        'enable_time_mark': False,           # Enable UBX-TIM-TM2 time mark events
        'time_base': {                       # Optional — Survey-In or Fixed Position for improved time accuracy
            'base_mode': gnsshat.BaseMode.SURVEY_IN,
            'survey_in': {
                'minimum_observation_time_s': 60,
                'required_position_accuracy_m': 5.0
            }
        }
    },

    # Navigation filters (CFG-NAVSPG) — None or omit to keep the receiver's
    # current values. Every key is optional; only the given ones are set.
    'navigation_filters': {
        'min_svs': 4,             # 3-32
        'max_svs': 32,            # 3-32, >= min_svs
        'min_cno_dbhz': 6,        # 0-63
        'min_elev_deg': 10,       # -90 to 90, elevation mask
        'n_cno_thrs': 0,          # satellites needed above cno_thrs_dbhz
        'cno_thrs_dbhz': 0,       # 0-63
        'fix_mode': 3,            # 1 = 2D only, 2 = 3D only, 3 = auto
        'pdop_mask_x10': 250,     # PDOP mask in 0.1 units (250 = 25.0), 0 disables
        'tdop_mask_x10': 250,     # TDOP mask in 0.1 units, 0 disables
        'p_acc_mask_m': 100,      # position accuracy mask (m), 0 disables
        't_acc_mask_m': 350       # time accuracy mask (m), 0 disables
    },

    # Save the configuration to the receiver's flash when start() had to
    # change it, so it survives a power cycle. Default False.
    'save_to_flash': False
}
```

## GnssHat Methods

| Method | Description |
|--------|-------------|
| `start(config)` | Configure and start the GNSS module. Returns `True` on success, `False` if the config is rejected or startup fails. Raises `TypeError` / `ValueError` for a malformed config dict. |
| `get_navigation()` | Get current navigation data (non-blocking). |
| `wait_and_get_fresh_navigation()` | Block until new navigation data arrives. Releases GIL; Ctrl+C raises `KeyboardInterrupt`. Returns `None` once the waits are stopped by `shutdown()` or `close()`. |
| `name()` | Get detected HAT name (e.g. `'L1/L5 GNSS TIME HAT'`). |
| `get_system_health()` | Receiver health from UBX-MON-SYS (non-blocking). Returns `SystemHealth`; its `valid` is `False` until the first MON-SYS frame arrives. |
| `get_mon_ver()` | Receiver firmware and hardware versions from UBX-MON-VER, polled at the end of a successful `start()`. Returns `MonVer`; its `valid` is `False` before that. |
| `soft_reset_hot_start()` | Software reset — preserves almanac/ephemeris for fast TTFF. |
| `hard_reset_cold_start()` | Hardware reset — full cold start. |
| `enable_timepulse()` | Enable 1PPS output on GPIO 5. Raises `RuntimeError` if the line is taken, e.g. by the kernel PPS driver. |
| `disable_timepulse()` | Disable 1PPS output. |
| `timepulse()` | Block until next 1PPS interrupt. Releases GIL; Ctrl+C raises `KeyboardInterrupt`. Returns `True`, or `False` when the timepulse is not enabled or the waits are stopped. |
| `start_forward_for_gpsd()` | Start NMEA forwarding to a virtual TTY for gpsd. |
| `stop_forward_for_gpsd()` | Stop NMEA forwarding. |
| `join_forward_for_gpsd()` | Block until forwarding is stopped by `stop_forward_for_gpsd()`, `shutdown()`, or `close()`. Releases GIL; Ctrl+C raises `KeyboardInterrupt`. |
| `get_gpsd_device_path()` | Get the virtual TTY path (e.g. `/dev/pts/3`). Returns `None` if not active. |
| `rtk_get_full_corrections()` | Get full RTCM3 correction frames from RTK base. Returns `list[bytes]`, empty until the base has data. Raises `RuntimeError` if the HAT is not an RTK base. |
| `rtk_get_tiny_corrections()` | Get compact RTCM3 correction frames from RTK base. Returns `list[bytes]`, empty until the base has data. Raises `RuntimeError` if the HAT is not an RTK base. |
| `rtk_get_rtcm3_frame(msg_id)` | Get a specific RTCM3 frame by message ID. Returns `bytes`. |
| `rtk_apply_corrections(frames)` | Send RTCM3 correction frames to RTK rover. Takes `list[bytes]`. |
| `get_time_mark()` | Get current time mark data (non-blocking). Returns `TimeMark` or `None`. |
| `wait_and_get_fresh_time_mark()` | Block until new time mark event arrives. Releases GIL; Ctrl+C raises `KeyboardInterrupt`. Returns `None` when the HAT has no time mark input or the waits are stopped. |
| `shutdown()` | Stop all waits and gpsd forwarding for good: the waits return `None` / `False` and `start()` returns `False` from then on. Releases GIL. Idempotent; safe from another thread and from a Python signal handler. Does not close the HAT: join the threads that use it, then `close()`. |
| `close()` | Stop the waits and gpsd forwarding and release the HAT. Threads blocked in `wait_and_get_fresh_*()` or `timepulse()` get `None` / `False`. Idempotent. |
| `closed` | `True` once `close()` has been called. Any other method then raises `RuntimeError`. |

Supports context manager. Leaving the `with` block calls `close()`, like a file:
the object cannot be reused afterwards, so create a new `GnssHat()` instead.

```python
with gnsshat.GnssHat() as hat:
    hat.start(config)
    while (nav := hat.wait_and_get_fresh_navigation()) is not None:
        print(nav.pvt.latitude, nav.pvt.longitude)
# close() stopped GPSD forwarding, disabled timepulse and released the HAT
```

`close()` may be called while other threads use the HAT: it wakes the ones
blocked in `wait_and_get_fresh_*()` or `timepulse()` (they get `None` /
`False`) and waits for in-progress calls before releasing the handle.

Only one thread may call `join_forward_for_gpsd()` at a time per instance.
Another thread may stop forwarding, read its device path or close the HAT
while that call waits. The join does not request a stop; repeated joins on an
open HAT without restarting forwarding return immediately.

## Ctrl+C and signals

The blocking calls (`wait_and_get_fresh_*()`, `timepulse()`,
`join_forward_for_gpsd()`) behave like `time.sleep()`: they release the GIL and
Python signal handlers run while they wait, at most 100 ms after the signal.
Ctrl+C therefore raises `KeyboardInterrupt`, and leaving the `with` block
releases the HAT, even when no data is arriving:

```python
try:
    with gnsshat.GnssHat() as hat:
        hat.start(config)
        while (nav := hat.wait_and_get_fresh_navigation()) is not None:
            print(nav.pvt.latitude, nav.pvt.longitude)
except KeyboardInterrupt:
    print("Stopped")
```

The module installs no signal handlers. By default Python dies on SIGTERM
without cleaning up, so a service should make it behave like Ctrl+C:

```python
signal.signal(signal.SIGTERM, signal.default_int_handler)
```

A handler runs as ordinary Python code in the main thread, so it may call
`hat.shutdown()` or `hat.close()`. Only the main thread gets the exception: end
the waits of worker threads with `shutdown()` and join them before leaving the
`with` block.

Every other call that can block (`start()`, the resets, enabling and disabling)
also releases the GIL, so other Python threads keep running. Calls that must not
overlap wait for each other, and Ctrl+C takes effect when the call returns.

All examples in [examples/python](../examples/python) follow this pattern.

## Module Functions and Constants

```python
gnsshat.version()              # Returns '1.2.0'
gnsshat.utc_time_iso8601(nav)  # Convert Navigation or PVT to ISO 8601 string
gnsshat.fetch_sourcetable(host, port=2101, username='', password='',
                          timeout_ms=5000, use_tls=False, verify_peer=True)
                               # NTRIP sourcetable, see NTRIP below

gnsshat.MAX_GEOFENCES   # 4
gnsshat.MAX_RF_BLOCKS    # 2
gnsshat.MAX_SATELLITES   # 64

import jimmypaputto
jimmypaputto.__version__       # Version of the installed Python package
```

## Navigation Data

`wait_and_get_fresh_navigation()` and `get_navigation()` return a `Navigation` object:

| Field | Type | Description |
|-------|------|-------------|
| `pvt` | `PositionVelocityTime` | Position, velocity, time, fix info |
| `dop` | `DilutionOverPrecision` | Dilution of precision values |
| `geofencing` | `Geofencing` | Geofencing config and status |
| `rf_blocks` | `list[RfBlock]` | RF band info (up to 2) |
| `rf_blocks_spectrum` | `list[RfBlockSpectrumData]` | RF spectrum per RF block from UBX-MON-SPAN (up to 2) |
| `satellites` | `list[SatelliteInfo]` | Tracked satellites (up to 64) |

`Navigation`, `PositionVelocityTime`, `DilutionOverPrecision`, `RfBlock`,
`RfBlockSpectrumData`, `SatelliteInfo`, `UtcTime`, `Date`, `TimeMark` and
`SystemHealth` have `__str__` and `__repr__` for easy printing. The geofencing
types (`Geofencing`, `GeofencingCfg`, `GeofencingNav`, `Geofence`), `Pulse`,
`TimepulsePinConfig` and `MonVer` print as plain Python objects; read their
fields instead.

### PositionVelocityTime

| Field | Type | Description |
|-------|------|-------------|
| `latitude` | `float` | Degrees |
| `longitude` | `float` | Degrees |
| `altitude` | `float` | Above WGS84 ellipsoid (m) |
| `altitude_msl` | `float` | Above mean sea level (m) |
| `speed_over_ground` | `float` | m/s |
| `speed_accuracy` | `float` | m/s |
| `heading` | `float` | Degrees |
| `heading_accuracy` | `float` | Degrees |
| `horizontal_accuracy` | `float` | Meters |
| `vertical_accuracy` | `float` | Meters |
| `visible_satellites` | `int` | Number of satellites used |
| `fix_quality` | `int` | `FixQuality` enum value |
| `fix_status` | `int` | `FixStatus` enum value |
| `fix_type` | `int` | `FixType` enum value |
| `utc_time` | `UtcTime` | `.hours`, `.minutes`, `.seconds`, `.valid`, `.accuracy` (ns) |
| `date` | `Date` | `.day`, `.month`, `.year`, `.valid` |

### DilutionOverPrecision

| Field | Type | Description |
|-------|------|-------------|
| `geometric` | `float` | GDOP — overall geometric quality |
| `position` | `float` | PDOP — 3D position |
| `time` | `float` | TDOP — time |
| `vertical` | `float` | VDOP — vertical position |
| `horizontal` | `float` | HDOP — horizontal position |
| `northing` | `float` | NDOP — northing |
| `easting` | `float` | EDOP — easting |

### RfBlock

| Field | Type | Description |
|-------|------|-------------|
| `id` | `int` | Index of the RF block in UBX-MON-RF (0 or 1), not the band |
| `gnss_band` | `int` | `RfBand` enum value — the band this RF block receives |
| `jamming_state` | `int` | `JammingState` enum value |
| `antenna_status` | `int` | `AntennaStatus` enum value |
| `antenna_power` | `int` | `AntennaPower` enum value |
| `post_status` | `int` | POST status word |
| `noise_per_ms` | `int` | Noise level |
| `agc_monitor` | `float` | AGC monitor percentage |
| `cw_interference_suppression_level` | `float` | CW suppression level |
| `ofs_i`, `mag_i` | `int` | I-channel offset and magnitude (I/Q imbalance) |
| `ofs_q`, `mag_q` | `int` | Q-channel offset and magnitude (I/Q imbalance) |

Use `gnsshat.RfBand(rf.gnss_band)` for the band; `RfBand(rf.id)` gives a wrong
result.

### RfBlockSpectrumData

Spectrum of one RF block from UBX-MON-SPAN, in `Navigation.rf_blocks_spectrum`.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `int` | Index of the RF block in UBX-MON-SPAN (0 or 1) |
| `spectrum_data` | `list[int]` | 256 FFT amplitude bins (0-255) |
| `span` | `int` | Frequency span (Hz) |
| `resolution` | `int` | Frequency resolution per bin (Hz) |
| `center_freq` | `int` | Center frequency (Hz) |
| `gain` | `int` | Receiver gain setting |

### SatelliteInfo

| Field | Type | Description |
|-------|------|-------------|
| `gnss_id` | `int` | `GnssId` enum value |
| `sv_id` | `int` | Satellite vehicle ID |
| `cno` | `int` | Carrier-to-noise ratio (dBHz) |
| `elevation` | `int` | Degrees (-90 to 90) |
| `azimuth` | `int` | Degrees (0 to 360) |
| `quality` | `int` | `SvQuality` enum value |
| `used_in_fix` | `bool` | Whether satellite is used in fix |
| `healthy` | `bool` | Satellite health flag |

### Geofencing

| Field | Type | Description |
|-------|------|-------------|
| `cfg` | `GeofencingCfg` | `.confidence_level`, `.geofences` (list of `Geofence`) |
| `nav` | `GeofencingNav` | `.status`, `.number_of_geofences`, `.combined_state`, `.geofences` (list of status ints) |

### TimeMark

Returned by `get_time_mark()` and `wait_and_get_fresh_time_mark()`.

| Field | Type | Description |
|-------|------|-------------|
| `channel` | `int` | Time mark channel |
| `mode` | `int` | `TimeMarkMode` enum value |
| `run` | `int` | `TimeMarkRun` enum value |
| `time_base` | `int` | `TimeMarkTimeBase` enum value |
| `new_rising_edge` | `bool` | New rising edge detected |
| `new_falling_edge` | `bool` | New falling edge detected |
| `utc_available` | `bool` | UTC available |
| `time_valid` | `bool` | Time valid |
| `count` | `int` | Time mark event count |
| `week_number_rising` | `int` | GPS week number of rising edge |
| `week_number_falling` | `int` | GPS week number of falling edge |
| `tow_rising_ms` | `int` | TOW of rising edge (ms) |
| `tow_sub_rising_ns` | `int` | Sub-millisecond TOW of rising edge (ns) |
| `tow_falling_ms` | `int` | TOW of falling edge (ms) |
| `tow_sub_falling_ns` | `int` | Sub-millisecond TOW of falling edge (ns) |
| `accuracy_estimate_ns` | `int` | Accuracy estimate (ns) |

### SystemHealth

Returned by `get_system_health()`, from UBX-MON-SYS.

| Field | Type | Description |
|-------|------|-------------|
| `valid` | `bool` | `True` once a MON-SYS frame has been received |
| `msg_version` | `int` | Message version |
| `boot_type` | `int` | `BootType` enum value — cause of the last receiver start |
| `cpu_load`, `cpu_load_max` | `int` | Current and peak CPU load (%) |
| `mem_usage`, `mem_usage_max` | `int` | Current and peak memory usage (%) |
| `io_usage`, `io_usage_max` | `int` | Current and peak I/O usage (%) |
| `run_time_s` | `int` | Time since the last receiver restart (s) |
| `notice_count`, `warn_count`, `error_count` | `int` | Notice, warning and error counters |
| `temperature_c` | `int` | Internal temperature (°C) |

### MonVer

Returned by `get_mon_ver()`, from UBX-MON-VER.

| Field | Type | Description |
|-------|------|-------------|
| `valid` | `bool` | `True` once the MON-VER reply has been received |
| `sw_version` | `str` | Firmware version |
| `hw_version` | `str` | Hardware revision |
| `extensions` | `list[str]` | Extension strings (firmware, protocol version, module name, ...) |

## NTRIP

The module has its own NTRIP v2.0 implementation; these classes do not need a
HAT. RTCM3 frames are passed as `list[bytes]`, the same type that
`rtk_get_*_corrections()` returns and `rtk_apply_corrections()` takes. All
three classes support `with`: leaving the block stops or disconnects and
destroys the object, after which its methods raise `RuntimeError`. See
[examples/python](../examples/python) for complete base, rover and server
setups.

| Class | Role |
|-------|------|
| `NtripCaster(host='0.0.0.0', port=2101, mountpoint='GNSS_HAT', max_clients=10)` | Local single-mountpoint caster: rovers connect to it directly |
| `NtripServer(host='localhost', port=2101, mountpoint='GNSS_HAT', username='', password='')` | NTRIP source: pushes corrections to a remote caster |
| `NtripClient(host='localhost', port=2101, mountpoint='GNSS_HAT', username='', password='')` | Rover side: receives corrections from a caster |

| Method | Classes | Description |
|--------|---------|-------------|
| `start()` / `stop()` | Caster | Start / stop listening. `start()` raises `RuntimeError` on failure. |
| `connect()` / `disconnect()` | Server, Client | Connect to / disconnect from the caster. `connect()` raises `RuntimeError` on failure. |
| `is_connected()` | Server, Client | `True` while connected. |
| `feed(frames)` | Caster, Server | Send RTCM3 frames: to all connected rovers (caster) or to the remote caster (server). |
| `receive()` | Client | RTCM3 frames received since the last call, as `list[bytes]` (non-blocking). |
| `client_count()` | Caster | Number of connected rovers. |
| `update_position(lat, lon)` | Caster | Base position advertised in the sourcetable (degrees). |
| `set_credentials(username, password)` | Caster | HTTP Basic auth for rovers; empty strings disable it. |
| `send_position(lat, lon, alt=0.0)` | Client | Send one GGA position to the caster (for VRS / nearest-base casters). |
| `update_position(lat, lon, alt=0.0)` + `set_auto_gga(interval_ms)` | Client | Send the stored position as GGA periodically; `interval_ms=0` disables. |
| `set_auto_reconnect(enable, initial_ms=1000, max_ms=30000)` | Server, Client | Reconnect automatically with exponential backoff. |
| `reconnect_count()` | Server, Client | Reconnect attempts since the last `connect()`. |
| `set_tls(cert_file, key_file)` | Caster | Serve TLS with PEM certificate and key. Call before `start()`; returns `bool`. |
| `set_tls(enable, verify_peer=True)` | Server, Client | Connect over TLS. Call before `connect()`. |
| `is_tls_available()` | all | `True` if the library was built with `-DNTRIP_TLS_SUPPORT=ON`. |
| `set_log_level(level)` | all | Minimum log level, an `NtripLogLevel` value (default `INFO`). |
| `set_log_callback(callback)` | all | `callback(level: int, message: str)` for log messages; `None` removes it. Runs in a library thread; exceptions it raises are ignored. |
| `get_stats()` | all | Statistics `dict`: `bytes_tx`, `bytes_rx`, `frames_tx`, `frames_rx`, `uptime_ms`, `last_frame_age_ms`, `avg_inter_frame_ms`, `max_inter_frame_ms`, `message_types` (`{rtcm_message_id: count}`). |

`fetch_sourcetable(host, port=2101, username='', password='', timeout_ms=5000,
use_tls=False, verify_peer=True)` downloads a caster's sourcetable and returns
a `list[dict]` with the keys `mountpoint`, `identifier`, `format`,
`format_details`, `carrier`, `nav_system`, `latitude` and `longitude`. It
returns an empty list when the caster cannot be reached.

```python
with (
    gnsshat.GnssHat() as hat,
    gnsshat.NtripClient('caster.example.com', 2101, 'MOUNT', 'user', 'pass') as client,
):
    if not hat.start({'rtk': {'mode': gnsshat.RtkMode.ROVER}}):
        raise SystemExit("Failed to start GNSS")
    client.connect()
    while (nav := hat.wait_and_get_fresh_navigation()) is not None:
        if frames := client.receive():
            hat.rtk_apply_corrections(frames)
```

## Enums

All enums are `IntEnum` types accessed as `gnsshat.<EnumName>.<MEMBER>`.

| Enum | Members |
|------|---------|
| `DynamicModel` | `PORTABLE`, `STATIONARY`, `PEDESTRIAN`, `AUTOMOTIVE`, `SEA`, `AIRBORNE_1G`, `AIRBORNE_2G`, `AIRBORNE_4G`, `WRIST`, `BIKE`, `MOWER`, `ESCOOTER` |
| `TimepulsePolarity` | `FALLING_EDGE`, `RISING_EDGE` |
| `PioPinPolarity` | `LOW_MEANS_INSIDE`, `LOW_MEANS_OUTSIDE` |
| `FixQuality` | `INVALID`, `GPS_FIX_2D_3D`, `DGNSS`, `PPS_FIX`, `FIXED_RTK`, `FLOAT_RTK`, `DEAD_RECKONING` |
| `FixStatus` | `VOID`, `ACTIVE` |
| `FixType` | `NO_FIX`, `DEAD_RECKONING_ONLY`, `FIX_2D`, `FIX_3D`, `GNSS_WITH_DEAD_RECKONING`, `TIME_ONLY_FIX` |
| `GeofenceStatus` | `UNKNOWN`, `INSIDE`, `OUTSIDE` |
| `GeofencingStatus` | `NOT_AVAILABLE`, `ACTIVE` |
| `RfBand` | `UNKNOWN`, `L1`, `L2`, `L3`, `L5`, `L2_OR_L5` |
| `JammingState` | `UNKNOWN`, `OK_NO_SIGNIFICANT_JAMMING`, `WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK`, `CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX` |
| `AntennaStatus` | `INIT`, `DONT_KNOW`, `OK`, `SHORT`, `OPEN` |
| `AntennaPower` | `OFF`, `ON`, `DONT_KNOW` |
| `RtkMode` | `BASE`, `ROVER` |
| `BaseMode` | `SURVEY_IN`, `FIXED_POSITION` |
| `FixedPositionType` | `ECEF`, `LLA` |
| `GnssId` | `GPS`, `SBAS`, `GALILEO`, `BEIDOU`, `IMES`, `QZSS`, `GLONASS` |
| `SvQuality` | `NO_SIGNAL`, `SEARCHING`, `SIGNAL_ACQUIRED`, `SIGNAL_DETECTED_BUT_UNUSABLE`, `CODE_LOCKED_AND_TIME_SYNCHRONIZED`, `CODE_AND_CARRIER_LOCKED_1`, `CODE_AND_CARRIER_LOCKED_2`, `CODE_AND_CARRIER_LOCKED_3` |
| `TimeMarkMode` | `SINGLE`, `RUNNING` |
| `TimeMarkRun` | `ARMED`, `STOPPED` |
| `TimeMarkTimeBase` | `RECEIVER`, `GNSS`, `UTC` |
| `BootType` | `UNKNOWN`, `COLD_START`, `WATCHDOG`, `HARDWARE_RESET`, `HARDWARE_BACKUP`, `SOFTWARE_BACKUP`, `SOFTWARE_RESET`, `VIO_FAIL`, `VDD_X_FAIL`, `VDD_RF_FAIL`, `V_CORE_HIGH_FAIL`, `SYSTEM_RESET` |
| `NtripLogLevel` | `ERROR`, `WARNING`, `INFO`, `DEBUG` |
