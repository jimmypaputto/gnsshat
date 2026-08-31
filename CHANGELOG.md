# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed
- SPI receive loop no longer stalls and discards data. `M9NRun::execute()` ended
  the drain on a latched TX-READY falling edge; an edge arriving while the
  thread was parsing got overwritten and lost, after which the loop spun for a
  full epoch (~590 kB of SPI traffic) and wrapped the 8 kB buffer ~72 times,
  silently dropping it on every wrap. Over half the navigation epochs never
  reached the parser while in that state. The loop now ends on the module's
  0xFF idle padding and never discards buffered bytes
- NMEA forwarding for gpsd no longer drifts against the module's navigation
  epoch. The forwarding thread used a free-running `sleep_for(1000ms)` loop, so
  its true period was 1 s plus the loop body; the sampling phase crept across
  the 1 Hz boundary and every ~30 minutes a second was emitted twice while
  another was skipped. gpsd then paired sentences with the wrong PPS edge and
  chrony stepped by exactly -1 s ([#38](https://github.com/jimmypaputto/gnsshat/issues/38))

## [1.1.0] - 2026-05-06

### Added
- Navigation input filters (experimental)
- NTRIP caster, client, and server with optional TLS
- CLI tools: `gnsshat-info`, `gnsshat-probe`
- `gnsshat-rtk-base` - RTK base station tool with TOML config and a systemd service unit
- Helpers for reading the HAT EEPROM device-tree entries without instantiating `IGnssHat` - JimmyPaputto::Hat namespace
- Many CMake fixes including Version.hpp generation for single version source
- CI workflows for self hosted services including RPI4, RPI5 and x64 machine for more frequent checks

### Changed
- Tests link against shared library instead of recompiling sources
- Visualization app: new **Altitude** tab - one-axis tape altimeter
- Visualization app: Configuration tab gained a **NavigationFilters** section with an Elevation Mask slider (0–60°)
- Visualization app: per-chart light / dark theme toggle
- Visualization app: RF Analyzer no longer flickers "No RF data" when only one of `spectrum` / `rf_blocks` is present in a frame; spectrum x-axis tick density is now width-aware
- Visualization app: Relative Map and Altitude charts support mouse wheel zoom on hover plus +/- zoom buttons next to the range slider; 
- Visualization app: Grid ladder extended down to 1 cm for RTK-grade ranges
- Visualization app: Sky View and RF Analyzer theme toggles moved intoa proper `.map-info` toolbar matching the other tabs

## [1.0.0] - 2026-04-06

Initial release — C++, C, and Python driver library for NEO-M9N, NEO-F10T,
and NEO-F9P GNSS HATs. Includes UBX protocol support, navigation, RTK,
geofencing, time base/mark, timepulse, gpsd forwarding, examples, and a
Flask visualization app.
