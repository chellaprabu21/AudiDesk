#!/bin/bash

# Install AudiDeck Virtual Audio Driver

set -e

DRIVER_NAME="AudiDeckDriver"
BUILD_DIR="build"
DRIVER_BUNDLE="${BUILD_DIR}/${DRIVER_NAME}.driver"
INSTALL_DIR="/Library/Audio/Plug-Ins/HAL"

if [ ! -d "${DRIVER_BUNDLE}" ]; then
    echo "❌ Driver not built. Run ./build.sh first"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "❌ Run with sudo: sudo ./install.sh"
    exit 1
fi

echo "🔧 Installing ${DRIVER_NAME}..."

mkdir -p "${INSTALL_DIR}"

# Remove old version
rm -rf "${INSTALL_DIR}/${DRIVER_NAME}.driver"

# Install new version
cp -R "${DRIVER_BUNDLE}" "${INSTALL_DIR}/"
chown -R root:wheel "${INSTALL_DIR}/${DRIVER_NAME}.driver"
chmod -R 755 "${INSTALL_DIR}/${DRIVER_NAME}.driver"

echo "🔄 Restarting Core Audio..."
# Use kill instead of launchctl (works with SIP enabled)
sudo killall coreaudiod 2>/dev/null || true

sleep 3

echo ""
echo "✅ Done! 'AudiDeck Virtual Output' should now appear in Sound settings."
echo ""
echo "Verify with: system_profiler SPAudioDataType | grep -i audideck"
