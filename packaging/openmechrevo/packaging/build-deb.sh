#!/usr/bin/env bash
# OpenMechrevo mechrevo-drivers-dkms Debian/Ubuntu package builder
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VERSION="4.22.3"
BUILD_DIR="$SCRIPT_DIR/build-deb"

echo "==> Preparing Debian package build directory for mechrevo-drivers-dkms v$VERSION..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

TAR_URL="https://gitlab.com/tuxedocomputers/development/packages/tuxedo-drivers/-/archive/v${VERSION}/tuxedo-drivers-v${VERSION}.tar.gz"
TAR_FILE="tuxedo-drivers-v${VERSION}.tar.gz"

if [ ! -f "$TAR_FILE" ]; then
    echo "==> Downloading upstream tuxedo-drivers v$VERSION..."
    curl -sSL "$TAR_URL" -o "$TAR_FILE"
fi

echo "==> Extracting source..."
tar -xzf "$TAR_FILE"
SRC_DIR="tuxedo-drivers-v${VERSION}"
cd "$SRC_DIR"

echo "==> Applying OpenMechrevo kernel patches..."
for patch_file in "$KERNEL_DIR"/patches/*.patch; do
    if [ -f "$patch_file" ]; then
        echo "  -> Applying $(basename "$patch_file")..."
        patch -Np1 -i "$patch_file"
    fi
done

echo "==> Copying debian packaging files..."
cp -r "$SCRIPT_DIR/debian" .
cp "$KERNEL_DIR/dkms.conf" .
cp "$KERNEL_DIR/60-mechrevo.rules" .

echo "==> Building Debian binary package..."
dpkg-buildpackage -us -uc -b

echo "==> Package build completed. Output .deb is in $BUILD_DIR"
