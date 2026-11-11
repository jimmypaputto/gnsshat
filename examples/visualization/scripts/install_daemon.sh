#!/bin/bash
#
# Jimmy Paputto GNSS Visualization Daemon Installation Script
#
# Copies the visualization app to /opt, builds its virtual environment there
# and installs it as a systemd service that starts on boot
#

set -e

DAEMON_NAME="gnsshat-visualization"
INSTALL_DIR="/opt/${DAEMON_NAME}"
PREVIOUS_DIR="${INSTALL_DIR}.previous"
SERVICE_PATH="/etc/systemd/system/${DAEMON_NAME}.service"
SOURCE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_FILE="${SOURCE_DIR}/scripts/${DAEMON_NAME}.service"
PORT=5000
STARTUP_TIMEOUT_S=30
WAS_ACTIVE=false
REPLACING=false

# A failed update must not leave the machine without a working daemon
restore_previous() {
    if [ "${REPLACING}" != true ]; then
        return
    fi
    echo "❌ ERROR: Installation failed"
    rm -rf "${INSTALL_DIR}"
    if [ -d "${PREVIOUS_DIR}" ]; then
        echo "↩️  Restoring the previous installation..."
        mv "${PREVIOUS_DIR}" "${INSTALL_DIR}"
        if [ "${WAS_ACTIVE}" = true ]; then
            systemctl start "${DAEMON_NAME}" || true
        fi
    fi
}
trap restore_previous EXIT

echo "=============================================="
echo "  Jimmy Paputto GNSS Visualization Installer  "
echo "=============================================="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ ERROR: This script must be run as root (use sudo)"
   echo "   Usage: sudo ./install_daemon.sh"
   exit 1
fi

# Check if service file exists
if [ ! -f "${SERVICE_FILE}" ]; then
    echo "❌ ERROR: Service file not found at ${SERVICE_FILE}"
    exit 1
fi

# The virtual environment reuses the system-wide GnssHat Python module
if ! python3 -c "from jimmypaputto import gnsshat" 2> /dev/null; then
    echo "❌ ERROR: GnssHat Python module not found"
    echo "   Please build and install the library with Python support first:"
    echo "   cmake .. -DBUILD_PYTHON=ON && make && sudo make install"
    exit 1
fi

echo "📦 Installing ${DAEMON_NAME} daemon..."

# Stop service if running
if systemctl is-active --quiet "${DAEMON_NAME}"; then
    echo "🛑 Stopping existing ${DAEMON_NAME} service..."
    WAS_ACTIVE=true
    systemctl stop "${DAEMON_NAME}"
fi

# Copy application
echo "📁 Installing application to ${INSTALL_DIR}..."
rm -rf "${PREVIOUS_DIR}"
REPLACING=true
if [ -d "${INSTALL_DIR}" ]; then
    mv "${INSTALL_DIR}" "${PREVIOUS_DIR}"
fi
mkdir -p "${INSTALL_DIR}"
cp -r "${SOURCE_DIR}/app.py" "${SOURCE_DIR}/requirements.txt" \
    "${SOURCE_DIR}/templates" "${SOURCE_DIR}/static" "${SOURCE_DIR}/res" \
    "${INSTALL_DIR}/"

# Create virtual environment
echo "🐍 Creating virtual environment..."
if ! python3 -m venv --system-site-packages "${INSTALL_DIR}/venv"; then
    echo "❌ ERROR: Could not create the virtual environment"
    echo "   Please install it first: sudo apt-get install python3-venv"
    exit 1
fi
"${INSTALL_DIR}/venv/bin/python" -m pip install --disable-pip-version-check \
    -r "${INSTALL_DIR}/requirements.txt"

REPLACING=false
rm -rf "${PREVIOUS_DIR}"

# Copy service file
echo "⚙️  Installing systemd service..."
cp "${SERVICE_FILE}" "${SERVICE_PATH}"

# Reload systemd
echo "🔄 Reloading systemd daemon..."
systemctl daemon-reload

# Enable and start service
echo "✅ Enabling and starting ${DAEMON_NAME} service..."
systemctl enable "${DAEMON_NAME}"
systemctl restart "${DAEMON_NAME}"

# The web server only comes up once the receiver has answered
echo "⏳ Waiting for the web server on port ${PORT}..."
STARTED=false
for _ in $(seq 1 "${STARTUP_TIMEOUT_S}"); do
    if (exec 3<> "/dev/tcp/127.0.0.1/${PORT}") 2> /dev/null; then
        STARTED=true
        break
    fi
    sleep 1
done

ADDRESS="$(hostname -I 2> /dev/null | awk '{print $1}')"

echo ""
if [ "${STARTED}" = true ]; then
    echo "🎉 Installation completed successfully!"
else
    echo "⚠️  WARNING: Installed, but the web server did not come up within ${STARTUP_TIMEOUT_S} seconds"
    echo "   The daemon keeps retrying; check that the HAT is attached and see the logs:"
    echo "   sudo journalctl -u ${DAEMON_NAME} -e"
fi
echo ""
echo "The daemon starts on boot and takes the GNSS receiver for itself."
echo "Access the visualization at:"
echo "  http://${ADDRESS:-<raspberry-pi-ip>}:${PORT}"
echo ""
echo "Usage commands:"
echo "  sudo systemctl start ${DAEMON_NAME}     # Start the daemon"
echo "  sudo systemctl stop ${DAEMON_NAME}      # Stop the daemon"
echo "  sudo systemctl status ${DAEMON_NAME}    # Check status"
echo "  sudo systemctl restart ${DAEMON_NAME}   # Restart the daemon"
echo "  sudo journalctl -u ${DAEMON_NAME} -f    # View logs"
echo ""
