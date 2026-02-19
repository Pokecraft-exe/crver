#!/bin/bash
# Exit on any error
set -e

########################################
# Root / sudo handling
########################################
if [ "$EUID" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

########################################
# Check for apt (Debian-based systems)
########################################
if ! command -v apt-get >/dev/null 2>&1; then
    echo "Error: 'apt-get' not found."
    echo "This script only supports Debian-based distributions (Debian, Ubuntu, etc.)."
    exit 1
fi

echo "Updating system and installing dependencies..."

$SUDO apt-get update
$SUDO apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    python3 \
    pkg-config \
    libssl-dev \
    liblttng-ust-dev \
    git

########################################
# Clone MSQuic
########################################
echo "Cloning and building MSQuic..."

if [ ! -d "msquic" ]; then
    git clone --recursive --depth=1 https://github.com/microsoft/msquic.git
fi

cd msquic

# Clean previous build if it exists
rm -rf build
mkdir build
cd build

cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUIC_BUILD_SHARED=ON \
    -DQUIC_BUILD_TESTS=OFF \
    -DQUIC_BUILD_PERF=OFF \
    -DQUIC_TLS_LIB=openssl \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++

echo "Building MSQuic..."
ninja

echo "Installing MSQuic..."
$SUDO ninja install
$SUDO ldconfig

cd ../..

########################################
# System setup
########################################
echo "Setting up system directories and users..."

$SUDO mkdir -p \
    /etc/crver/ \
    /var/log/crver/ \
    /var/cwww/build \
    /var/cwww/src

# Create system user only if it doesn't exist
if ! id -u crver >/dev/null 2>&1; then
    $SUDO adduser --system --shell /usr/sbin/nologin --group crver
fi

$SUDO chown -R crver:crver /var/cwww
$SUDO chown -R crver:crver /var/log/crver
$SUDO chown -R crver:crver /etc/crver

echo "Done."
