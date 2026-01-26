# Drone Application Setup Guide

## Overview
This guide explains how the drone application is integrated into the Luckfox Pico build system and how to use it.

## Build Integration
The drone application is automatically built when you run:
```bash
./build.sh media
```

## File Locations After Build
The following files will be installed in the target filesystem:

### Application Binary
- `/usr/bin/drone` - Main application binary

### Service Files (BusyBox/SysV init)
- `/etc/init.d/S10wireguard` - WireGuard VPN service (starts first)
- `/etc/init.d/S99drone` - Drone application service (starts after VPN)

### Configuration Files
- `/etc/drone.conf` - Drone application configuration
- `/etc/wireguard/wg0.conf` - WireGuard VPN configuration

### Setup & Management Scripts
- `/usr/bin/setup_drone.sh` - Drone service activation script
- `/usr/bin/wireguard-setup.sh` - WireGuard VPN management script
- `/usr/bin/generate-wg-keys.sh` - WireGuard key generation utility

### Log Files (created at runtime)
- `/var/log/drone.log` - Drone application logs
- `/var/log/wireguard.log` - WireGuard VPN logs

### Note
- **SystemD is not supported** on Luckfox Pico (uses BusyBox)
- The `S10` prefix ensures WireGuard starts early (after network init)
- The `S99` prefix ensures Drone starts last (after VPN is ready)

## Installation on Target Device

### Method 1: Automatic Setup
After flashing the firmware, run:
```bash
setup_drone.sh
```

### Method 2: Manual Setup (SysV Init)
```bash
# The service should already be installed as S99drone
# Make it executable
chmod +x /etc/init.d/S99drone

# Create runlevel links if needed (S99 prefix means auto-start)
ln -sf /etc/init.d/S99drone /etc/rc2.d/S99drone
ln -sf /etc/init.d/S99drone /etc/rc3.d/S99drone
ln -sf /etc/init.d/S99drone /etc/rc4.d/S99drone
ln -sf /etc/init.d/S99drone /etc/rc5.d/S99drone

# Start immediately
/etc/init.d/S99drone start

# Check status
/etc/init.d/S99drone status
```



## Configuration
Edit `/etc/drone.conf` to customize the application:
```bash
vi /etc/drone.conf
```

Key settings:
- `STARTUP_DELAY` - Delay before starting (seconds)
- `LOG_LEVEL` - 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG
- `AUTO_RESTART` - Enable automatic restart on crash
- `CAMERA_*` - Camera configuration settings

## Usage

### Starting Services
```bash
# WireGuard VPN (starts first)
/etc/init.d/S10wireguard start

# Drone application (starts after VPN)
/etc/init.d/S99drone start

# Or use setup script (handles both)
setup_drone.sh
```

### Stopping Services
```bash
# WireGuard VPN
/etc/init.d/S10wireguard stop

# Drone application
/etc/init.d/S99drone stop
```

### Viewing Logs
```bash
# WireGuard logs
tail -f /var/log/wireguard.log

# Drone logs
tail -f /var/log/drone.log
```

### Checking Status
```bash
# WireGuard status
/etc/init.d/S10wireguard status

# Drone status
/etc/init.d/S99drone status
```

## Debugging

### Check if Service is Enabled
```bash
# SystemD
systemctl is-enabled drone

# Init.d
ls -la /etc/rc?.d/ | grep drone
```

### Manual Testing
```bash
# Run in foreground
/usr/bin/drone

# Run in background
/usr/bin/drone &

# Check if running
ps aux | grep drone
```

### Log Issues
If logs aren't appearing:
```bash
# Create log directory
mkdir -p /var/log

# Check permissions
ls -la /var/log/drone.log

# Test logging
echo "test" >> /var/log/drone.log
```

## Development

### Building Only the Drone App
```bash
cd media/drone
make clean && make
```

### Installing to Current System
```bash
# Copy binary
sudo cp out/bin/drone /usr/bin/

# Copy service files
sudo cp out/etc/systemd/system/drone.service /etc/systemd/system/
sudo cp out/etc/init.d/drone /etc/init.d/
sudo cp out/etc/drone.conf /etc/

# Copy setup script
sudo cp out/usr/bin/setup_drone.sh /usr/bin/

# Setup service
sudo /usr/bin/setup_drone.sh
```

## Troubleshooting

### Service Won't Start
1. Check if binary exists: `ls -la /usr/bin/drone`
2. Check permissions: `chmod +x /usr/bin/drone`
3. Check service config: `systemctl daemon-reload`
4. Check dependencies: `ldd /usr/bin/drone`

### Application Crashes
1. Check logs: `journalctl -u drone` or `/var/log/drone.log`
2. Run manually: `/usr/bin/drone`
3. Check dependencies: `ldd /usr/bin/drone`

### Autostart Not Working
1. Verify service is enabled: `systemctl is-enabled drone`
2. Check boot sequence: `systemctl list-dependencies multi-user`
3. Verify init script: `ls -la /etc/rc2.d/ | grep drone`

## File Structure After Installation
```
/usr/bin/drone                    # Main binary
/usr/bin/setup_drone.sh          # Setup script
/etc/systemd/system/drone.service # SystemD service
/etc/init.d/drone                 # Init script
/etc/drone.conf                   # Configuration
/var/log/drone.log               # Runtime logs
/var/run/drone.pid               # PID file
```