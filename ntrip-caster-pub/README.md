# ntrip-caster-pub

Standalone, single-mountpoint **NTRIP v2.0 caster** designed to run without gnsshat library.
Acts as a relay: one base station POSTs RTCM3 corrections to the configured
mountpoint, and every connected rover client receives the stream over plain TCP
or TLS.

Extracted and stripped down from the [GnssHat](../README.md) library — **no
hardware HAT dependencies**. Builds and runs anywhere with a C++20 compiler,
`pthreads`, and (optionally) OpenSSL.

Note: this project is not yet ready for production use on the public internet. It is still a work in progress and requires further testing and bug fixes. It currently serves as an auxiliary server for RTK testing.

## Features

- NTRIP v2.0 caster — `GET /mount` for rovers, `POST /mount` for the source
- **Dynamic mountpoint**: name is claimed by the first source that POSTs;
  released when it disconnects
- **Auto-position**: lat/lon are decoded from RTCM 1005/1006 (Stationary RTK
  Reference Station ARP) frames in the source stream and advertised in the
  sourcetable
- HTTP Basic auth (optional)
- Optional TLS (server cert + key, PEM)
- Live stats (bytes, frames, last-frame age, uptime, clients, mount)
- Optional HTTP status page (web UI + JSON API under `/api/`) with its own
  Basic auth
- Graceful shutdown on `SIGINT` / `SIGTERM`
- Runs from command-line flags alone; an optional TOML config file
  (`--config`) is what the systemd unit uses
- Fully static binary on request (`NTRIP_CASTER_STATIC`)

## Build

```bash
cd ntrip-caster-pub
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

CMake options:

| Option | Default | Description |
|---|---|---|
| `NTRIP_CASTER_TLS` | `OFF` | Link against OpenSSL and enable TLS |
| `NTRIP_CASTER_STATIC` | `OFF` | Produce a fully static binary |
| `NTRIP_CASTER_TESTS` | `OFF` | Build smoke tests |

Install:

```bash
sudo cmake --install build --prefix /usr/local
```

This installs:

| Path | Contents |
|---|---|
| `${prefix}/bin/ntrip-caster` | the caster |
| `${prefix}/share/ntrip-caster/web/` | HTTP status page assets |
| `/etc/ntrip-caster/ntrip-caster.toml.example` | annotated example config |
| `/etc/ntrip-caster/ntrip-caster.toml` | live config, copied from the example only if it does not exist yet (your edits are never overwritten) |
| `${prefix}/lib/systemd/system/ntrip-caster.service` | systemd unit |

The config directory does not follow `--prefix`; change it with
`-DNTRIP_CASTER_CONF_DIR=<path>` at configure time. The unit starts
`/usr/local/bin/ntrip-caster`, so keep `--prefix /usr/local` if you use it.

## Usage

```text
ntrip-caster [options]

  --config <path>       Load TOML config file (CLI flags override)
  --host <addr>         Bind address           (default 0.0.0.0)
  --port <n>            Listen port            (default 2101)
  --max-clients <n>     Max concurrent clients (default 64)
  --user <name>         Basic-auth username    (default: open access)
  --pass <pwd>          Basic-auth password
  --tls-cert <path>     PEM certificate (enables TLS)
  --tls-key  <path>     PEM private key
  --log-level <lvl>     error|warning|info|debug (default info)
  --stats-interval <s>  Print stats every N seconds (0=off, default 30)
  --http                Enable HTTP status page (default off)
  --no-http             Disable HTTP status page (overrides config)
  --http-port <n>       HTTP status page port  (default 8080)
  --http-user <name>    HTTP Basic-auth user   (default rtk)
  --http-pass <pwd>     HTTP Basic-auth pass   (default rtkpassword)
  --http-web-root <p>   Override static-asset directory
  -h, --help            Show this help
```

The mountpoint name is whatever the first source POSTs to (e.g. POSTing
to `/BASE1` makes `BASE1` the active mountpoint). Lat/lon are auto-decoded
from RTCM 1005/1006 ARP messages within the source stream.

### Config file

`--config <path>` loads a TOML file with an `[ntrip]` and an `[http]` table;
see [ntrip-caster.toml.example](ntrip-caster.toml.example) for every key.
Missing keys keep the defaults listed above, and command-line flags override
the file. A config file that is missing or does not parse makes the caster exit
with code 2.

Without a config file the HTTP status page is off. **The example config — and
therefore the `ntrip-caster.toml` that `cmake --install` creates from it —
turns the page on**, on `0.0.0.0:8080` with the default credentials `rtk` /
`rtkpassword`. Change `[http] user` and `pass` (or set `enabled = false`)
before exposing the host. The page is plain HTTP, so its password travels
unencrypted: keep it on a trusted network or behind a TLS reverse proxy.

The page assets come from `--http-web-root` / `web_root`, else from
`$NTRIP_CASTER_WEB_ROOT`, `/usr/local/share/ntrip-caster/web`,
`/usr/share/ntrip-caster/web`, or `web/` in the working directory or up to two
levels above it. Without them only the JSON API is served.

### Example

Plain TCP, open access:

```bash
ntrip-caster --port 2101
```

With auth + TLS:

```bash
ntrip-caster \
    --port 2102 \
    --user rover --pass s3cret \
    --tls-cert /etc/letsencrypt/live/example.com/fullchain.pem \
    --tls-key  /etc/letsencrypt/live/example.com/privkey.pem
```

### Pushing data from a base

Any tool that speaks the **NTRIP source protocol** (e.g. `str2str` from
RTKLIB, an `ntripserver`, or the `gnsshat-rtk-base` tool from GnssHat) can
feed this caster:

```bash
str2str -in serial://ttyACM0:115200 \
        -out ntrips://user:pass@your-vps:2101/BASE1
```

### Connecting a rover

Any standard NTRIP client works:

```bash
str2str -in ntrip://rover:s3cret@your-vps:2101/BASE1 \
        -out tcpsvr://:5555
```

## systemd

The unit [systemd/ntrip-caster.service](systemd/ntrip-caster.service) runs
`/usr/local/bin/ntrip-caster --config /etc/ntrip-caster/ntrip-caster.toml` as
a sandboxed dynamic user. `cmake --install` puts the binary, the web assets,
the config and the unit in place, so review the config (see
[Config file](#config-file)) and enable the service:

```bash
sudo cmake --install build --prefix /usr/local
sudoedit /etc/ntrip-caster/ntrip-caster.toml   # at least the [http] password
sudo systemctl daemon-reload
sudo systemctl enable --now ntrip-caster
```

When deploying by hand, copy the config as well: without
`/etc/ntrip-caster/ntrip-caster.toml` the service fails with exit code 2.

```bash
sudo cp build/ntrip-caster /usr/local/bin/
sudo install -D -m 644 ntrip-caster.toml.example /etc/ntrip-caster/ntrip-caster.toml
sudo mkdir -p /usr/local/share/ntrip-caster
sudo cp -r web /usr/local/share/ntrip-caster/
sudo cp systemd/ntrip-caster.service /etc/systemd/system/
```

## Project layout

```
ntrip-caster-pub/
├── CMakeLists.txt
├── README.md
├── ntrip-caster.toml.example  # annotated config, installed to /etc/ntrip-caster/
├── app/
│   └── ntrip-caster.cpp     # CLI entry point
├── src/
│   ├── Base64.hpp           # base64 decoder for Basic auth
│   ├── CasterConfig.hpp     # TOML config + defaults
│   ├── HttpStatusServer.hpp # HTTP status page and JSON API
│   ├── NtripCaster.hpp/.cpp # caster core
│   ├── NtripLog.hpp         # log mixin
│   ├── NtripStats.hpp       # stats mixin
│   ├── NtripTls.hpp         # OpenSSL wrapper (optional)
│   ├── RtcmAnalyzer.hpp     # RTCM3 stream analysis for the status page
│   ├── RtcmArp.hpp          # RTCM 1005/1006 ARP → lat/lon decoder
│   ├── RtcmEphemeris.hpp    # RTCM3 ephemeris decoders (sky plot)
│   └── SatPos.hpp           # satellite azimuth/elevation from ephemerides
├── third_party/             # vendored cpp-httplib and toml++
├── web/                     # status page assets
├── tests/                   # GTest tests (NTRIP_CASTER_TESTS)
└── systemd/
    └── ntrip-caster.service
```

## License

MIT — same as the parent GnssHat project.
