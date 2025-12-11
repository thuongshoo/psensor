#!/bin/bash
set -e  # QUAN TRỌNG: -x để debug

echo "=== BUILD SCRIPT START ==="
echo "Args: $@"

if [ $# -lt 4 ]; then
    echo "❌ ERROR: Missing arguments"
    exit 1
fi

DISTRO="$1"
DISTRO_VERSION="$2"
ARCH="$3"
VERSION="$4"
OUTPUT_DIR="${5:-./artifacts}"

echo "Building: $DISTRO $DISTRO_VERSION ($ARCH) v$VERSION"
echo "Output dir: $OUTPUT_DIR"

# Tạo output directory
mkdir -p "$OUTPUT_DIR"

# Tạo docker script INLINE
DOCKER_SCRIPT=$(cat << 'DOCKER_EOF'
#!/bin/bash
set -ex

echo "=== INSIDE CONTAINER ==="
echo "Distro: $1"
echo "PWD: $(pwd)"
echo "Home: $HOME"

# 1. INSTALL
echo "--- Updating apt ---"
apt-get update

echo "--- Installing appindicator ---"
if [ "$1" = "ubuntu" ]; then
    apt-get install -y libappindicator3-dev
elif [ "$1" = "debian" ]; then
    apt-get install -y libayatana-appindicator3-dev
else
    echo "ERROR: Unknown distro"
    exit 1
fi

echo "--- Installing other deps ---"
apt-get install -y \
    devscripts \
    debhelper \
    build-essential \
    cmake \
    pkg-config \
    gettext \
    help2man \
    libatasmart-dev \
    libcairo2-dev \
    libcurl4-openssl-dev \
    libglib2.0-dev \
    libgtk-3-dev \
    libgtop2-dev \
    libjson-c-dev \
    libmicrohttpd-dev \
    libnotify-dev \
    libsensors-dev \
    libudisks2-dev \
    libx11-dev \
    libxnvctrl-dev \
    libunity-dev \
    asciidoc

# 2. BUILD
echo "--- Building package ---"

if [ -f ./debian ]; then
    rm -rf debian
fi
cp -rv debian_data debian

echo "Checking debian directory..."
ls -la debian/ 2>/dev/null || echo "No debian directory yet"

# # Remove compat file
# if [ -f debian/compat ]; then
#     echo "Removing debian/compat"
#     rm debian/compat
# fi

echo "Running dpkg-buildpackage..."
dpkg-buildpackage -us -uc -b --no-sign

# 3. FIND AND COPY PACKAGE
echo "--- Finding package ---"
PKG_PATH=$(find .. -maxdepth 1 -name "psensor-fork_*.deb" -type f | head -1)

if [ -z "$PKG_PATH" ]; then
    echo "ERROR: No package found!"
    echo "Searching everywhere..."
    find / -name "*.deb" -type f 2>/dev/null | head -10
    exit 1
fi

echo "Found package: $PKG_PATH"
cp "$PKG_PATH" /workspace/
echo "PACKAGE_READY:$(basename "$PKG_PATH")"

echo "=== CONTAINER DONE ==="
DOCKER_EOF
)

echo "=== CREATING DOCKER SCRIPT ==="
echo "$DOCKER_SCRIPT" > /tmp/docker-script.sh
chmod +x /tmp/docker-script.sh

# Xác định image
IMAGE="$DISTRO:$DISTRO_VERSION"
if [ "$DISTRO" = "debian" ]; then
    IMAGE="debian:$DISTRO_VERSION-slim"
fi

echo "=== RUNNING DOCKER ==="
echo "Image: $IMAGE"
echo "Platform: linux/$ARCH"

# Chạy docker với LOGGING
docker run --platform "linux/$ARCH" --rm \
    -v "$(pwd):/workspace" \
    -v "/tmp/docker-script.sh:/docker-build.sh" \
    -w "/workspace" \
    "$IMAGE" \
    /docker-build.sh "$DISTRO" 2>&1 | tee /tmp/docker-output.log

DOCKER_EXIT=$?
echo "Docker exit code: $DOCKER_EXIT"

if [ $DOCKER_EXIT -ne 0 ]; then
    echo "❌ Docker failed!"
    echo "Last 50 lines of output:"
    tail -50 /tmp/docker-output.log
    exit 1
fi

# Tìm package trong output
echo "=== EXTRACTING PACKAGE NAME ==="
PACKAGE_LINE=$(grep "PACKAGE_READY:" /tmp/docker-output.log | tail -1)

if [ -z "$PACKAGE_LINE" ]; then
    echo "❌ ERROR: No PACKAGE_READY line in output"
    echo "Full output:"
    cat /tmp/docker-output.log
    exit 1
fi

PACKAGE_NAME=$(echo "$PACKAGE_LINE" | cut -d: -f2- | tr -d '[:space:]')
echo "Package from container: '$PACKAGE_NAME'"

# Kiểm tra file
if [ ! -f "$PACKAGE_NAME" ]; then
    echo "❌ ERROR: Package file not found: $PACKAGE_NAME"
    echo "Files in workspace:"
    ls -la
    exit 1
fi

# Đổi tên
NEW_NAME="psensor-fork_${VERSION}_${DISTRO}${DISTRO_VERSION}_${ARCH}.deb"
mv "$PACKAGE_NAME" "$NEW_NAME"

# Di chuyển đến output directory
if [ "$OUTPUT_DIR" != "." ]; then
    mkdir -p "$OUTPUT_DIR"
    mv "$NEW_NAME" "$OUTPUT_DIR/"
    echo "Inside bash script $0"
    ls -lah "$OUTPUT_DIR/"
    FINAL_PATH="$OUTPUT_DIR/$NEW_NAME"
else
    FINAL_PATH="$NEW_NAME"
fi

echo "✅ FINAL PACKAGE: $FINAL_PATH"
echo "=== SCRIPT END ==="

# OUTPUT cho workflow
echo "$FINAL_PATH"