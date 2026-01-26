#!/bin/sh

# Drone application setup script for Luckfox Pico (BusyBox/SysV init)
# This script sets up drone service to start automatically

echo "Setting up drone application..."

# Check if drone binary exists
if [ ! -f "/usr/bin/drone" ]; then
    echo "Error: drone binary not found in /usr/bin/"
    exit 1
fi

# Check if init script exists (should be S99drone)
if [ -f "/etc/init.d/S99drone" ]; then
    echo "Found S99drone init script, making it executable..."
    chmod +x /etc/init.d/S99drone
    
    # Create startup links for different runlevels (if needed)
    # Since it's already S99drone, it should start automatically
    echo "Drone service is set up for automatic startup at boot."
    echo "The S99 prefix ensures it starts last in the boot sequence."
    
    # Test if we can create symlinks (some BusyBox setups need them)
    if [ -d "/etc/rc2.d" ]; then
        echo "Creating runlevel links..."
        ln -sf /etc/init.d/S99drone /etc/rc2.d/S99drone 2>/dev/null
        ln -sf /etc/init.d/S99drone /etc/rc3.d/S99drone 2>/dev/null
        ln -sf /etc/init.d/S99drone /etc/rc4.d/S99drone 2>/dev/null
        ln -sf /etc/init.d/S99drone /etc/rc5.d/S99drone 2>/dev/null
    fi
    
else
    echo "Warning: S99drone init script not found in /etc/init.d/"
    echo "Manual setup required. Check if drone was installed properly."
    exit 1
fi

# Create necessary directories
mkdir -p /var/run /var/log

echo "Drone application setup complete!"
echo ""
echo "To start drone application manually:"
echo "  Init script: /etc/init.d/S99drone start"
echo "  Direct: /usr/bin/drone &"
echo ""
echo "To stop drone application manually:"
echo "  Init script: /etc/init.d/S99drone stop"
echo "  Kill: pkill drone"
echo ""
echo "To check status:"
echo "  Init script: /etc/init.d/S99drone status"
echo "  Process list: ps aux | grep drone"
echo ""
echo "Logs will be written to: /var/log/drone.log"