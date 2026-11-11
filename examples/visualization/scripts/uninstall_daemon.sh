#!/bin/bash
#
# Jimmy Paputto GNSS Visualization Daemon Uninstallation Script
#

set -e

DAEMON_NAME="gnsshat-visualization"
INSTALL_DIR="/opt/${DAEMON_NAME}"
SERVICE_PATH="/etc/systemd/system/${DAEMON_NAME}.service"

echo "=============================================="
echo " Jimmy Paputto GNSS Visualization Uninstaller "
echo "=============================================="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ ERROR: This script must be run as root (use sudo)"
   echo "   Usage: sudo ./uninstall_daemon.sh"
   exit 1
fi

echo "🗑️  Uninstalling ${DAEMON_NAME} daemon..."

# Stop and disable service
if [ -f "${SERVICE_PATH}" ]; then
    echo "🛑 Stopping and disabling ${DAEMON_NAME} service..."
    systemctl stop "${DAEMON_NAME}" || true
    systemctl disable "${DAEMON_NAME}" || true
    echo "🗑️  Removing service file..."
    rm -f "${SERVICE_PATH}"
fi

# Reload systemd
echo "🔄 Reloading systemd daemon..."
systemctl daemon-reload

# Remove application and its virtual environment
if [ -d "${INSTALL_DIR}" ]; then
    echo "🗑️  Removing ${INSTALL_DIR}..."
    rm -rf "${INSTALL_DIR}"
fi

echo ""
echo "🎉 Uninstallation completed successfully!"
echo "   ${DAEMON_NAME} has been completely removed from the system."
echo ""
