#!/bin/bash
# Exit on any error
set -e

echo "Updating system and installing dependencies..."
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    python3 \
    pkg-config \
    libssl-dev \
    liblttng-ust-dev \
    git

echo "Cloning and building MSQuic with Ninja..."
if [ ! -d "msquic" ]; then
    git clone --recursive https://github.com/microsoft/msquic.git
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

echo "Installing MSQuic..."
ninja
sudo ninja install
sudo ldconfig

cd ../..

echo "Setting up system directories and users..."
sudo mkdir -p \
    /etc/crver/ \
    /var/log/crver/ \
    /var/cwww/build \
    /var/cwww/src

sudo adduser --system --shell /usr/sbin/nologin --group crver || true

sudo chown -R crver:crver /var/cwww
sudo chown -R crver:crver /var/log/crver
sudo chown -R crver:crver /etc/crver

echo "Done."
