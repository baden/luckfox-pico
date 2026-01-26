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

### Binary
- `/usr/bin/drone` - Main application binary

### Service File (BusyBox/SysV init)
- `/etc/init.d/S99drone` - System V init script (auto-starts at boot)

### Configuration
- `/etc/drone.conf` - Default configuration file

### Setup Script
- `/usr/bin/setup_drone.sh` - Service activation script

### Note
- **SystemD is not supported** on Luckfox Pico (uses BusyBox)
- The `S99` prefix ensures the service starts last during boot sequence

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

### Starting the Application
```bash
# As a service
systemctl start drone
# or
/etc/init.d/drone start

# Directly (for testing)
/usr/bin/drone
```

### Stopping the Application
```bash
# As a service
systemctl stop drone
# or
/etc/init.d/drone stop

# Directly (if running directly)
pkill drone
```

### Viewing Logs
```bash
# SystemD journal
journalctl -u drone -f

# Custom log file
tail -f /var/log/drone.log
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