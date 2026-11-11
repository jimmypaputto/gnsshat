# GNSS Real-Time Visualization

A web application for real-time GNSS data visualization from the u-blox NEO-M9N module (Jimmy Paputto GNSS HAT).  
A Flask + Socket.IO server streams navigation data to the browser — runs on Raspberry Pi and is accessible from any device on the local network.

![Stack](res/NazwaStos.svg)

---

## Operating Modes

The application supports **three data source modes**:

| Mode | Description | Requirements |
|------|-------------|--------------|
| `native` | Direct module communication via the GnssHat library (SPI) | Installed `jimmypaputto` (GnssHat) library |
| `external_tty` | NMEA sentence reading from a serial port (`/dev/jimmypaputto/gnss`) | `pyserial`, `pynmea2`, connected UART port |
| `ros2` | ROS 2 topic subscription to `/gnss/navigation` | ROS 2 environment, `jp_gnss_hat` package |

---

## Getting Started

### Install Dependencies

```bash
cd examples/visualization
python3 -m venv --system-site-packages venv
source venv/bin/activate
pip install -r requirements.txt
```

### Native Mode (default)

Direct communication with the GNSS module through the GnssHat library. Requires root privileges (SPI).

```bash
python3 app.py
# or explicitly:
python3 app.py native
```

### External TTY Mode

Reads raw NMEA sentences from a serial port. Useful when the module is exposed as a TTY device.

```bash
python3 app.py external_tty
```

Default port: `/dev/jimmypaputto/gnss` @ 9600 baud (configurable via `SERIAL_PORT` / `BAUD_RATE` constants in `app.py`).

### ROS 2 Mode

Subscribes to the navigation topic from the `jp_gnss_hat` ROS 2 node.

```bash
# Without node name — topic: /gnss/navigation
python3 app.py ros2

# With node name (e.g. rover) — topic: /gnss/rover/navigation
python3 app.py ros2 rover

# With node name (e.g. base) — topic: /gnss/base/navigation
python3 app.py ros2 base
```

> **Note:** Before launching, source the ROS 2 workspace:
> ```bash
> source /opt/ros/<distro>/setup.bash
> source ~/ros-ws/install/setup.bash
> python3 app.py ros2 [node_name]
> ```

#### Topic and Service Names in ROS 2 Mode

The optional `node_name` argument affects the topic and configuration service prefixes:

| Command | Navigation Topic | get_config Service | set_config Service |
|---------|-----------------|-------------------|-------------------|
| `app.py ros2` | `/gnss/navigation` | `/gnss/get_config` | `/gnss/set_config` |
| `app.py ros2 rover` | `/gnss/rover/navigation` | `/gnss/rover/get_config` | `/gnss/rover/set_config` |
| `app.py ros2 base` | `/gnss/base/navigation` | `/gnss/base/get_config` | `/gnss/base/set_config` |

This allows simultaneous monitoring of multiple nodes (e.g. base and rover in an RTK setup) — each in its own application instance.

---

## Accessing the Interface

After startup, the server listens on port **5000**:

- From the Raspberry Pi: **http://localhost:5000**
- From the local network: **http://\<raspberry-pi-ip\>:5000**

---

## Running as a Daemon

The app can run in native mode as a systemd service that starts on boot:

```bash
cd examples/visualization/scripts
sudo ./install_daemon.sh
```

The installer copies the app to `/opt/gnsshat-visualization`, creates a virtual environment
there (`--system-site-packages`, so the GnssHat Python module installed by `sudo make install`
is reused), installs the dependencies and enables `gnsshat-visualization.service`. Run it again
to update an existing installation.

```bash
sudo systemctl status gnsshat-visualization    # Check status
sudo journalctl -u gnsshat-visualization -f    # View logs
sudo ./uninstall_daemon.sh                     # Remove the service and /opt/gnsshat-visualization
```

> **Note:** The service runs as root, uses the `eventlet` runtime and holds the GNSS receiver
> for as long as it runs — stop it before starting anything else that opens the receiver.
> It restarts until the receiver answers, so the page becomes reachable only once a HAT with
> a programmed EEPROM is attached.

---

## Interface Features

### Maps (tabs)

| Tab | Description |
|-----|-------------|
| **Relative Map** | Canvas map — current position relative to a reference point (offset in meters N/E). Adjustable range (0.05 m – 30 m) with mouse-wheel zoom on hover and +/- buttons; grid auto-adapts from 1 cm (RTK fixed) up to 5 m. |
| **Altitude** | One-axis vertical tape altimeter showing Δ altitude relative to a reference. MSL / WGS84 source toggle (origin is preserved across conversions). Same zoom controls and 1 cm grid as the Relative Map, up to ±100 m. |
| **Terrain Map** | OpenStreetMap (Leaflet) — real-time position on a terrain map. |
| **Sky View** | Polar sky plot with satellites (elevation/azimuth), color-coded by constellation (GPS, Galileo, GLONASS, BeiDou, SBAS, QZSS). Includes a table of tracked satellites. Available in `native` and `ros2` modes. |
| **RF Analyzer** | Live spectrum (MON-SPAN) plus RF status (MON-RF): jamming, AGC, noise per band. Available in `native` and `ros2` modes. |

Every chart has a light / dark theme toggle (🌙 / ☀️) in its toolbar; the
choice is persisted per-chart in `localStorage`. Dark is the default.

### Navigation Data Panel

- **Time & Date** — UTC time and date from the receiver
- **Fix Information** — fix quality (2D/3D/RTK Fixed/RTK Float/DGPS), status, type, satellite count
- **Position** — latitude/longitude, MSL and WGS84 altitude
- **Velocity** — ground speed, heading
- **Accuracy** — horizontal, vertical, speed, and heading accuracy *(native/ros2)*
- **Dilution of Precision** — GDOP, PDOP, HDOP, VDOP, TDOP, NDOP, EDOP *(native/ros2)*
- **Geofencing** — status and number of active geofences *(native/ros2)*
- **RF / Antenna** — band (L1/L2), jamming state, antenna status, AGC, noise *(native/ros2)*

### Module Configuration (native / ros2)

The **Configuration** tab allows reading and changing the GNSS module configuration live:

- **Measurement Rate** — measurement frequency (1–25 Hz)
- **Dynamic Model** — dynamic model (Stationary, Pedestrian, Automotive, Sea, Airborne, Bike, E-Scooter, …)
- **Timepulse Pin** — PPS pulse configuration (frequency, pulse width, polarity, no-fix mode)
- **Geofencing** — define geofence zones (lat/lon/radius, confidence level)
- **RTK** — RTK mode (Base/Rover), base configuration (Survey-In or Fixed Position ECEF/LLA)
- **ROS 2** — ROS-specific options: publish standard topics, use NTRIP RTCM *(ros2 only)*

Buttons:
- **📥 Load Config** — reads the current configuration from the module/node
- **🚀 Send Config** — sends a new configuration (with a progress bar)

### Hardware Test (native, L1 GNSS HAT and L1/L5 GNSS RTK HAT)

The **Hardware Test** tab is a three-step check for testing boards in series:

1. **Receiver Link** — turns OK as soon as navigation data arrives from the module
2. **GNSS Fix** — waits until the receiver reports a valid fix
3. **Geofence Relay** — **▶ Run Relay Test** takes the position at the moment of the click and
   sends a configuration with a 300 m geofence around it and the PIO output enabled
   (Low = Inside). Once the receiver reports *Inside*, the GEOFENCE LED must be lit and the
   relay closed — check both on the board

The test geofence lives in the module's RAM only and is gone after a power cycle. The tab
resets itself whenever the connection to the server is lost, and the page reopens on the tab
that was active before a refresh, so the next board needs no clicking around.

### Other

- **Reset Origin** — resets the reference point to the current position (button in the status bar)
- Real-time updates via **WebSocket** (Socket.IO)

---

## File Structure

```
Visualization/
├── app.py                 # Flask server + data source logic
├── requirements.txt       # Python dependencies
├── res/
│   └── NazwaStos.svg      # Logo
├── scripts/
│   ├── gnsshat-visualization.service  # systemd unit
│   ├── install_daemon.sh  # Installs the app as a systemd service
│   └── uninstall_daemon.sh
├── static/
│   ├── css/
│   │   └── style.css      # UI styles
│   └── js/
│       ├── map.js         # Relative map, sky view, RF analyzer, app wiring
│       ├── altitude.js    # Altitude tape (vertical one-axis altimeter)
│       └── hwtest.js      # Hardware Test tab
└── templates/
    └── index.html          # HTML template (Jinja2, conditional rendering by mode)
```

---

## System Requirements

- Python 3.10+
- Raspberry Pi with GNSS HAT module (native mode) or any machine with serial port access / ROS 2
- Browser with WebSocket support (any modern browser)
