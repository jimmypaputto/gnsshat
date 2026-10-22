# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- `start()` on the RTK HAT now fails when `rtk.mode` is `Rover` and
  `rtk.base` is set, because the base config was silently ignored. Leave
  `rtk.base` unset for a rover

### Fixed
- `GnssConfig::rtk` was never validated: `Base` mode without `rtk.base`
  configured the module as a rover, invalid base parameters were accepted and
  `rtk` on the L1 GNSS HAT was ignored. `start()` now rejects these configs
  with a message on stderr and leaves `rtk()` null after a failed start
- Projects using gnsshat through `add_subdirectory()` or `FetchContent` could
  not find `<jimmypaputto/...>` headers, or silently took them from a
  system-wide install, unless `BUILD_EXAMPLES` was on. The build tree now
  always provides the installed `include/jimmypaputto` layout

## [1.2.0] - 2026-10-16

### Added
- gnsshat can be pulled into another CMake project with `add_subdirectory()`
  or `FetchContent`. Paths no longer assume a top-level build, and the install,
  CMake package, pkg-config, ldconfig and `uninstall` rules are skipped then

### Changed
- The CMake package is now `gnsshat` with the imported target
  `gnsshat::gnsshat`, replacing `GnssHat` / `GnssHat::GnssHat`. Breaking for
  CMake consumers only: the library file and the pkg-config name stay `gnsshat`
- `BUILD_TOOLS` now defaults to OFF, because the quick-start build failed
  without `toml11`. Enable the CLI tools with `-DBUILD_TOOLS=ON`

### Fixed
- Navigation notifications fired twice per epoch, because every SPI drain pass
  notified and the module sends its output in two bursts. They now fire only
  after a buffer carrying NAV-PVT is parsed
  ([#42](https://github.com/jimmypaputto/gnsshat/pull/42))
- gpsd bridge forwarded empty NMEA after boot when the GNSS engine came up
  stopped. Startup now ends with a controlled GNSS start (`UBX-CFG-RST` 0x09)
  and the hot-start reset waits 1 s for the receiver
  ([#40](https://github.com/jimmypaputto/gnsshat/issues/40))
- SPI receive loop could stall for a whole epoch and drop buffered data when a
  TX-READY edge was missed during parsing. The drain now ends on the module's
  0xFF idle padding instead of the edge, and resumes at the buffer limit
  instead of discarding the buffer
- NMEA forwarding for gpsd ran on a free-running 1 s timer and drifted against
  the navigation epoch, causing a -1 s chrony step every ~30 minutes. The
  forwarder now waits on the navigation notifier and emits one deduplicated
  sentence set per epoch
  ([#38](https://github.com/jimmypaputto/gnsshat/issues/38))
- `find_package(gnsshat)` failed for TLS builds, because the exported target
  links `OpenSSL::SSL` and `OpenSSL::Crypto`. The package config now calls
  `find_dependency(OpenSSL)`
- A new `IGnssHat` object inherited the previous object's data (geofences,
  MON-VER firmware version, last position, time mark), because receiver data
  lived in a process-wide singleton. Each `IGnssHat` now owns its own data,
  so destroying the object and creating a new one starts clean
  ([#44](https://github.com/jimmypaputto/gnsshat/pull/44))
- The exported include directory was `<includedir>/jimmypaputto`, one level too
  deep for `#include <jimmypaputto/GnssHat.hpp>`. The CMake target and
  `gnsshat.pc` now export `<includedir>`
  ([#33](https://github.com/jimmypaputto/gnsshat/issues/33))
- gpsd could stay inactive after boot, because `jpgnss2gpsd-bridge.service`
  reported itself started before the PTY existed. The unit now stays
  activating until `/dev/jimmypaputto/gnss` is created
  ([#33](https://github.com/jimmypaputto/gnsshat/issues/33))
- gpsd bridge startup messages reached journald only when the process exited,
  with misleading timestamps. The bridge now line-buffers stdout

### Removed
- `GenericSingleton`. Receiver data, decoded UBX messages and startup state
  are now owned by each `IGnssHat` instance
  ([#44](https://github.com/jimmypaputto/gnsshat/pull/44))

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
