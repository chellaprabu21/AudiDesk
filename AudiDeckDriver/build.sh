#!/bin/bash

# Build script for AudiDeck Virtual Audio Driver

set -e

DRIVER_NAME="AudiDeckDriver"
BUILD_DIR="build"
DRIVER_BUNDLE="${BUILD_DIR}/${DRIVER_NAME}.driver"

echo "🔨 Building ${DRIVER_NAME}..."

# Clean previous build
rm -rf "${BUILD_DIR}"
mkdir -p "${DRIVER_BUNDLE}/Contents/MacOS"

# Use system Xcode clang
/usr/bin/clang++ -std=c++17 -O2 -fPIC -shared \
    -framework CoreFoundation \
    -framework CoreAudio \
    -framework AudioToolbox \
    -o "${DRIVER_BUNDLE}/Contents/MacOS/${DRIVER_NAME}" \
    AudiDeckDriver.cpp

# Copy Info.plist
cp Info.plist "${DRIVER_BUNDLE}/Contents/"

# Code sign the driver (required for modern macOS)
echo "🔏 Signing driver..."
codesign --force --deep --sign - "${DRIVER_BUNDLE}"

echo "✅ Build complete: ${DRIVER_BUNDLE}"
echo ""
echo "📦 To install, run: sudo ./install.sh"
