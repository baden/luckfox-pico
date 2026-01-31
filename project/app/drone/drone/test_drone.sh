#!/bin/bash

# Test script for the drone C application

echo "=== Drone C Application Test ==="

# Build directory
BUILD_DIR="./build"
EXECUTABLE="$BUILD_DIR/src/rv1106_ipc/drone"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable not found at $EXECUTABLE"
    echo "Please run: mkdir -p build && cd build && cmake -DCOMPILE_FOR_RV1106_IPC=ON .. && make"
    exit 1
fi

echo "✓ Executable found at $EXECUTABLE"

# Check if we have the required devices
echo "=== Checking required devices ==="

# Check UART device
if [ -e "/dev/ttyS3" ]; then
    echo "✓ /dev/ttyS3 exists"
else
    echo "⚠ /dev/ttyS3 not found (CRSF will not work)"
fi

# Check GPIO sysfs
if [ -d "/sys/class/gpio" ]; then
    echo "✓ GPIO sysfs available"
else
    echo "⚠ GPIO sysfs not available"
fi

# Check PWM sysfs
if [ -d "/sys/class/pwm" ]; then
    echo "✓ PWM sysfs available"
    # Check for specific PWM chips
    if [ -d "/sys/class/pwm/pwmchip5" ]; then
        echo "✓ PWM chip 5 available (servo1)"
    else
        echo "⚠ PWM chip 5 not found"
    fi
    if [ -d "/sys/class/pwm/pwmchip6" ]; then
        echo "✓ PWM chip 6 available (servo2)"
    else
        echo "⚠ PWM chip 6 not found"
    fi
else
    echo "⚠ PWM sysfs not available"
fi

echo ""
echo "=== Feature Summary ==="
echo "✓ CRSF protocol support - UART /dev/ttyS3 at 420000 baud"
echo "✓ UDP client support - connects to s.navi.cc:8766"
echo "✓ GPIO control - Lebidka, Aktuator, Buzzer (via sysfs)"
echo "✓ PWM servo control - 2 servos via /sys/class/pwm/"
echo "✓ Multi-threaded architecture with pthread"
echo "✓ Automatic reconnection and error recovery"
echo "✓ Priority system - CRSF > UDP > Watchdog timeout"
echo ""
echo "=== Starting Application ==="
echo "The application will now start. Press Ctrl+C to exit."
echo "The app will attempt to:"
echo "1. Initialize all modules"
echo "2. Start 4 threads (CRSF, UDP, Control, Watchdog)"
echo "3. Listen for CRSF packets on /dev/ttyS3"
echo "4. Connect to UDP server for remote control"
echo "5. Update servos at 20ms intervals"
echo ""

# Run the application
sudo "$EXECUTABLE"