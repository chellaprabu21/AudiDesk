#!/bin/bash

# Uninstall AudiDeck Virtual Audio Driver

DRIVER_NAME="AudiDeckDriver"
INSTALL_DIR="/Library/Audio/Plug-Ins/HAL"

if [ "$EUID" -ne 0 ]; then
    echo "❌ Run with sudo: sudo ./uninstall.sh"
    exit 1
fi

echo "🗑️  Removing ${DRIVER_NAME}..."

rm -rf "${INSTALL_DIR}/${DRIVER_NAME}.driver"

echo "🔄 Restarting Core Audio..."
sudo killall coreaudiod 2>/dev/null || true

sleep 2

echo "✅ Uninstalled successfully"
