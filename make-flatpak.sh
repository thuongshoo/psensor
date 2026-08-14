#!/bin/bash
set -euo pipefail

# Kiểm tra và cài đặt các gói cần thiết cho Flatpak build
if [ -f /etc/debian_version ]; then
    echo "Checking for flatpak-builder dependencies..."
    
    # flatpak-builder thường nằm trong gói flatpak-builder
    if ! command -v flatpak-builder >/dev/null 2>&1; then
        echo "Installing flatpak-builder..."
        if command -v sudo >/dev/null 2>&1; then
            sudo apt-get update
            sudo apt-get install -y flatpak-builder flatpak
        else
            apt-get update
            apt-get install -y flatpak-builder flatpak
        fi
    fi
else
    echo "Warning: Not a Debian/Ubuntu system, please ensure flatpak-builder is installed manually."
    if ! command -v flatpak-builder >/dev/null 2>&1; then
        echo "Error: flatpak-builder not found."
        exit 1
    fi
fi

echo "Building flatpak..."
#rm -rf build-dir
flatpak-builder --keep-build-dirs --repo=repo build-dir com.github.thuongshoo.psensorfork.yml --force-clean --disable-updates

# --force-clean --disable-updates --stop-at=psensorfork 

echo "Creating .flatpak bundle..."
flatpak build-bundle repo psensorfork.flatpak com.github.thuongshoo.psensorfork

echo "Done: psensorfork.flatpak"