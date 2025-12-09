#!/bin/bash
# setup.sh - Flash MicroPython and upload code to ESP8266
# Usage: ./setup.sh /dev/ttyUSB0

set -e

PORT=${1:-/dev/ttyUSB0}

echo "========================================"
echo "  Pill Reminder - ESP8266 Setup Script"
echo "========================================"
echo ""
echo "Using port: $PORT"
echo ""

# Check for required tools
command -v pip >/dev/null 2>&1 || { echo "pip required but not found"; exit 1; }

# Install tools if needed
echo "Installing/updating tools..."
pip install --quiet esptool adafruit-ampy

# Check if MicroPython firmware exists
FIRMWARE=$(ls esp8266*.bin 2>/dev/null | head -1)

if [ -z "$FIRMWARE" ]; then
    echo ""
    echo "⚠️  No MicroPython firmware found!"
    echo ""
    echo "Download the latest ESP8266 firmware from:"
    echo "  https://micropython.org/download/esp8266/"
    echo ""
    echo "Save the .bin file in this directory and run again."
    echo ""
    exit 1
fi

echo "Found firmware: $FIRMWARE"
echo ""

# Confirm before flashing
read -p "This will ERASE the ESP8266 and flash MicroPython. Continue? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
fi

# Erase flash
echo ""
echo "Erasing flash..."
esptool.py --port $PORT erase_flash

# Flash MicroPython
echo ""
echo "Flashing MicroPython..."
esptool.py --port $PORT --baud 460800 write_flash --flash_size=detect 0 $FIRMWARE

# Wait for device to reboot
echo ""
echo "Waiting for device to reboot..."
sleep 3

# Upload files
echo ""
echo "Uploading boot.py..."
ampy --port $PORT put boot.py

echo "Uploading main.py..."
ampy --port $PORT put main.py

echo ""
echo "========================================"
echo "  Setup Complete!"
echo "========================================"
echo ""
echo "The device will now reboot and run your code."
echo ""
echo "To see serial output:"
echo "  screen $PORT 115200"
echo "  (Press Ctrl+A then K to exit screen)"
echo ""
echo "Don't forget to edit main.py with your WiFi credentials!"
echo ""
