#!/bin/bash
# WWJAudio Driver Build Script
# Usage: ./build.sh [all|clean|install|uninstall|test|debug]

set -e

WWJAUDIO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
KERNELDIR=${KERNELDIR:-/lib/modules/$(uname -r)/build}

echo "========================================"
echo " WWJAudio Driver Build Script"
echo "========================================"
echo "WWJAUDIO_DIR: $WWJAUDIO_DIR"
echo "KERNELDIR: $KERNELDIR"
echo "========================================"

case "${1:-all}" in
    all)
        echo "[BUILD] Compiling wwjaudio driver..."
        make -C "$KERNELDIR" M="$WWJAUDIO_DIR" modules
        echo "[BUILD] Done! Module: $WWJAUDIO_DIR/wwjaudio.ko"
        ;;
    
    clean)
        echo "[CLEAN] Cleaning build artifacts..."
        make -C "$KERNELDIR" M="$WWJAUDIO_DIR" clean
        echo "[CLEAN] Done!"
        ;;
    
    install)
        echo "[INSTALL] Installing wwjaudio driver..."
        make -C "$KERNELDIR" M="$WWJAUDIO_DIR" modules
        sudo mkdir -p /lib/modules/$(uname -r)/extra/
        sudo cp "$WWJAUDIO_DIR/wwjaudio.ko" /lib/modules/$(uname -r)/extra/
        sudo depmod -a
        echo "[INSTALL] Done! Use: modprobe wwjaudio"
        ;;
    
    uninstall)
        echo "[UNINSTALL] Removing wwjaudio driver..."
        sudo rmmod wwjaudio 2>/dev/null || true
        sudo rm -f /lib/modules/$(uname -r)/extra/wwjaudio.ko
        sudo depmod -a
        echo "[UNINSTALL] Done!"
        ;;
    
    test)
        echo "[TEST] Building and testing wwjaudio driver..."
        make -C "$KERNELDIR" M="$WWJAUDIO_DIR" modules
        echo "[TEST] Loading module..."
        sudo insmod "$WWJAUDIO_DIR/wwjaudio.ko"
        sleep 1
        echo "[TEST] Kernel messages:"
        dmesg | tail -15
        echo ""
        echo "[TEST] Available audio devices:"
        aplay -l 2>/dev/null || echo "aplay not available"
        echo ""
        echo "[TEST] Testing aplay (1 second)..."
        aplay -D plughw:CARD=WWJAudio /dev/zero --duration=1 2>&1 || echo "Test skipped"
        echo "[TEST] Unloading module..."
        sudo rmmod wwjaudio
        echo "[TEST] Done!"
        ;;
    
    debug)
        echo "[DEBUG] Building with debug symbols..."
        make -C "$KERNELDIR" M="$WWJAUDIO_DIR" modules DEBUG=1
        echo "[DEBUG] Done! Module with debug symbols ready."
        ;;
    
    info)
        echo "[INFO] WWJAudio Driver Information"
        echo "--------------------------------"
        echo "Source file: $WWJAUDIO_DIR/wwjaudio_codec.c"
        echo "Module name: wwjaudio.ko"
        echo "Card name: WWJAudio"
        echo "No Device Tree required!"
        echo "--------------------------------"
        echo "Available targets:"
        echo "  all       - Build the driver"
        echo "  clean     - Clean build artifacts"
        echo "  install   - Build and install"
        echo "  uninstall - Remove driver"
        echo "  test      - Build, load, test with aplay, unload"
        echo "  debug     - Build with debug symbols"
        echo "  info      - Show this information"
        ;;
    
    *)
        echo "[ERROR] Invalid target: $1"
        echo "Usage: $0 [all|clean|install|uninstall|test|debug|info]"
        exit 1
        ;;
esac