# WireGuard VPN Integration for Luckfox Pico

## Overview
This guide explains how to set up WireGuard VPN on Luckfox Pico without using wg-quick, using direct ip commands instead.

## Files Included

### Scripts
- `/etc/init.d/S10wireguard` - VPN service (starts at boot priority 10)
- `/usr/bin/wireguard-setup.sh` - Main WireGuard setup script
- `/usr/bin/generate-wg-keys.sh` - Key generation utility

### Configuration
- `/etc/wireguard/wg0.conf` - WireGuard configuration template
- `/etc/wireguard/privatekey` - Private key (generated)
- `/etc/wireguard/publickey` - Public key (generated)

### Service Integration
- `S10wireguard` starts BEFORE drone (`S99drone`)
- Automatic retry and monitoring
- Network readiness detection

## Quick Setup

### 1. Generate Keys (on device)
```bash
/usr/bin/generate-wg-keys.sh
```

This will:
- Generate private/public key pair
- Create basic config template
- Show the public key to share with server

### 2. Configure Server Connection
Edit `/etc/wireguard/wg0.conf`:

```ini
[Interface]
Address = 10.8.0.2/24
PrivateKey = YOUR_GENERATED_PRIVATE_KEY
ListenPort = 51820

[Peer]
PublicKey = SERVER_PUBLIC_KEY_HERE
AllowedIPs = 10.8.0.0/24
Endpoint = SERVER_IP:51820
PersistentKeepalive = 25
```

### 3. Start VPN
```bash
# Start service
/etc/init.d/S10wireguard start

# Check status
/etc/init.d/S10wireguard status

# Monitor logs
tail -f /var/log/wireguard.log
```

## Manual Commands (equivalent to wg-quick)

The WireGuard interface is configured using these commands:

```bash
# Create interface
ip link add dev wg0 type wireguard

# Set IP address
ip address add dev wg0 10.8.0.2/24

# Configure WireGuard settings
wg set wg0 listen-port 51820 private-key /etc/wireguard/privatekey peer <SERVER_PUBKEY> allowed-ips 10.8.0.0/24 endpoint <SERVER_IP>:51820

# Bring interface up
ip link set up dev wg0

# Remove interface
ip link del dev wg0
```

## Service Management

### Start/Stop Services
```bash
# WireGuard VPN (starts first)
/etc/init.d/S10wireguard start
/etc/init.d/S10wireguard stop
/etc/init.d/S10wireguard status

# Drone application (starts after VPN)
/etc/init.d/S99drone start
/etc/init.d/S99drone stop
/etc/init.d/S99drone status
```

### Autostart Order
- `S10wireguard` - VPN setup (early boot)
- `S99drone` - Drone app (late boot)

## Configuration Options

### Interface Settings
```ini
[Interface]
Address = 10.8.0.2/24        # Client IP
PrivateKey = <PRIVATE_KEY>      # Client private key
ListenPort = 51820             # Client listen port
DNS = 1.1.1.1, 8.8.8.8      # Optional DNS servers
```

### Peer Settings
```ini
[Peer]
PublicKey = <SERVER_PUBKEY>     # Server public key
AllowedIPs = 10.8.0.0/24     # Routes through VPN
Endpoint = server.com:51820     # Server address
PersistentKeepalive = 25        # NAT keepalive
```

## Advanced Usage

### Multiple Peers
```ini
[Interface]
Address = 10.8.0.2/24
PrivateKey = <PRIVATE_KEY>

[Peer]  # Main server
PublicKey = <SERVER1_PUBKEY>
AllowedIPs = 10.8.0.0/24
Endpoint = server1.com:51820

[Peer]  # Site-to-site
PublicKey = <SERVER2_PUBKEY>
AllowedIPs = 192.168.1.0/24
Endpoint = server2.com:51820
```

### Network Detection
The service waits for network before starting:
- Checks for default route
- Pings gateway to verify connectivity
- 30-second timeout

### Auto-reconnect
The WireGuard monitor automatically:
- Detects interface failures
- Attempts reconnection
- Logs all activities to `/var/log/wireguard.log`

## Troubleshooting

### Check Interface Status
```bash
# Interface info
ip addr show wg0

# WireGuard status
wg show wg0

# Routing
ip route | grep wg0

# Connection test
ping -c 3 10.8.0.1
```

### Common Issues

#### "wg command not found"
```bash
# Install WireGuard tools (if available)
opkg update
opkg install wireguard-tools
```

#### "Network not ready"
```bash
# Check network status
ip addr show
ip route show

# Test connectivity
ping -c 3 8.8.8.8
```

#### "Interface creation failed"
```bash
# Check kernel module
lsmod | grep wireguard

# Load module manually
modprobe wireguard
```

#### "Permission denied"
```bash
# Check file permissions
ls -la /etc/wireguard/privatekey
chmod 600 /etc/wireguard/privatekey

# Run as root
sudo /etc/init.d/S10wireguard start
```

### Debug Mode
Enable debug logging:
```bash
# Enable verbose logging
export WG_DEBUG=1

# Manually run setup script
/usr/bin/wireguard-setup.sh start

# Monitor kernel messages
dmesg | grep wireguard
```

## Security Notes

### Key Management
- Private keys are stored with 600 permissions
- Never share private keys
- Generate new keys if compromise suspected

### Firewall Considerations
```bash
# Allow WireGuard traffic (if using iptables)
iptables -A INPUT -p udp --dport 51820 -j ACCEPT
iptables -A FORWARD -i wg0 -j ACCEPT
iptables -A FORWARD -o wg0 -j ACCEPT
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

## Integration with Drone

The Drone application will automatically:
1. Wait for WireGuard to be established
2. Use VPN tunnel for network communications
3. Have access to VPN network resources

Applications can check if VPN is active:
```bash
# In your application code
if ip link show wg0 >/dev/null 2>&1; then
    echo "VPN is active, using tunnel"
    # Use VPN routes
else
    echo "VPN not available, using direct connection"
    # Fallback behavior
fi
```

## Log Files

### WireGuard Logs
- `/var/log/wireguard.log` - Main service log
- Kernel messages via `dmesg | grep wireguard`

### PID Files
- `/var/run/wg0.pid` - Main interface PID
- `/var/run/wg0_monitor.pid` - Monitor process PID

This setup provides robust VPN connectivity with automatic recovery, proper service ordering, and comprehensive logging suitable for embedded IoT deployments.