#!/bin/bash
set -euo pipefail

# Danh sách các gói cần thiết để build (tổng hợp từ .github/scripts/build-package.sh)
PACKAGES="devscripts debhelper build-essential cmake pkg-config gettext help2man libatasmart-dev libcairo2-dev libcurl4-openssl-dev libglib2.0-dev libgtk-3-dev libgtop2-dev libjson-c-dev libmicrohttpd-dev libnotify-dev libsensors-dev libudisks2-dev libx11-dev libxnvctrl-dev libunity-dev asciidoc bison flex libappindicator3-dev"

echo "Checking for required build packages..."

# Kiểm tra và cài đặt nếu thiếu (chỉ hỗ trợ trên Debian/Ubuntu)
if [ -f /etc/debian_version ]; then
    if command -v sudo >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y $PACKAGES
    else
        apt-get update
        apt-get install -y $PACKAGES
    fi
else
    echo "Warning: Not a Debian/Ubuntu system, skipping automatic package installation."
fi

# Create debian directory structure
if [ -f ./debian ]; then
    rm -rf debian
fi
cp -r debian_data/. debian/
# Build the package
dpkg-buildpackage -b -us -uc
