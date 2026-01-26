#!/bin/sh

# WireGuard VPN Setup Script for Luckfox Pico
# This script sets up WireGuard interface without wg-quick

WG_INTERFACE="wg0"
WG_CONFIG="/etc/wireguard/wg0.conf"
WG_PRIVATE_KEY="/etc/wireguard/privatekey"
WG_PIDFILE="/var/run/wg0.pid"
WG_LOGFILE="/var/log/wireguard.log"

# Function to log messages
log_msg() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> $WG_LOGFILE
    echo "$1"
}

# Function to check if interface exists
interface_exists() {
    ip link show $WG_INTERFACE >/dev/null 2>&1
}

# Function to check if interface is up
interface_is_up() {
    [ -d /sys/class/net/$WG_INTERFACE ] && [ "$(cat /sys/class/net/$WG_INTERFACE/operstate)" = "up" ]
}

# Function to read config file
read_config() {
    if [ ! -f "$WG_CONFIG" ]; then
        log_msg "Error: WireGuard config file not found: $WG_CONFIG"
        return 1
    fi
    
    # Simple config parsing (INI-like format)
    # We'll source the config file with proper parsing
    WG_ADDRESS=$(grep '^Address=' "$WG_CONFIG" | cut -d'=' -f2 | tr -d ' ')
    WG_PRIVATE_KEY=$(grep '^PrivateKey=' "$WG_CONFIG" | cut -d'=' -f2 | tr -d ' ')
    WG_PEER_PUBLIC_KEY=$(grep '^PublicKey=' "$WG_CONFIG" | cut -d'=' -f2 | tr -d ' ')
    WG_PEER_ENDPOINT=$(grep '^Endpoint=' "$WG_CONFIG" | cut -d'=' -f2 | tr -d ' ')
    WG_PEER_ALLOWED_IPS=$(grep '^AllowedIPs=' "$WG_CONFIG" | cut -d'=' -f2 | tr -d ' ')
    WG_LISTEN_PORT=$(grep '^ListenPort=' "$WG_CONFIG" | cut -d'=' -f2 | tr -d ' ')
    
    # Set defaults if not specified
    WG_ADDRESS=${WG_ADDRESS:-"10.8.0.2/24"}
    WG_LISTEN_PORT=${WG_LISTEN_PORT:-"51820"}
    WG_PEER_ALLOWED_IPS=${WG_PEER_ALLOWED_IPS:-"10.8.0.0/24"}
    
    if [ -z "$WG_PRIVATE_KEY" ] || [ -z "$WG_PEER_PUBLIC_KEY" ] || [ -z "$WG_PEER_ENDPOINT" ]; then
        log_msg "Error: Required WireGuard parameters missing in config"
        return 1
    fi
    
    log_msg "Configuration loaded successfully"
    return 0
}

# Function to setup WireGuard interface
setup_interface() {
    log_msg "Setting up WireGuard interface $WG_INTERFACE..."
    
    # Remove existing interface if present
    if interface_exists; then
        log_msg "Removing existing interface $WG_INTERFACE"
        ip link del dev $WG_INTERFACE 2>/dev/null
    fi
    
    # Create new WireGuard interface
    log_msg "Creating WireGuard interface $WG_INTERFACE"
    if ! ip link add dev $WG_INTERFACE type wireguard; then
        log_msg "Error: Failed to create WireGuard interface"
        return 1
    fi
    
    # Set IP address
    log_msg "Setting IP address: $WG_ADDRESS"
    if ! ip address add dev $WG_INTERFACE $WG_ADDRESS; then
        log_msg "Error: Failed to set IP address"
        return 1
    fi
    
    # Configure WireGuard with private key and peer
    log_msg "Configuring WireGuard with peer $WG_PEER_ENDPOINT"
    if ! wg set $WG_INTERFACE listen-port $WG_LISTEN_PORT private-key $WG_PRIVATE_KEY peer $WG_PEER_PUBLIC_KEY allowed-ips $WG_PEER_ALLOWED_IPS endpoint $WG_PEER_ENDPOINT; then
        log_msg "Error: Failed to configure WireGuard"
        return 1
    fi
    
    # Bring interface up
    log_msg "Bringing interface $WG_INTERFACE up"
    if ! ip link set up dev $WG_INTERFACE; then
        log_msg "Error: Failed to bring interface up"
        return 1
    fi
    
    # Save PID for tracking
    echo $$ > $WG_PIDFILE
    
    log_msg "WireGuard interface $WG_INTERFACE is now up and configured"
    return 0
}

# Function to bring down interface
teardown_interface() {
    log_msg "Tearing down WireGuard interface $WG_INTERFACE"
    
    if interface_exists; then
        ip link del dev $WG_INTERFACE
        log_msg "WireGuard interface $WG_INTERFACE removed"
    fi
    
    rm -f $WG_PIDFILE
    log_msg "WireGuard interface $WG_INTERFACE is down"
}

# Function to check connection status
check_status() {
    if interface_is_up; then
        log_msg "WireGuard interface $WG_INTERFACE is UP"
        
        # Show interface info
        echo "=== Interface Status ==="
        ip addr show $WG_INTERFACE
        
        echo "=== WireGuard Status ==="
        wg show $WG_INTERFACE
        
        # Test connectivity to peer
        if [ -n "$WG_PEER_ENDPOINT" ]; then
            PEER_IP=$(echo $WG_PEER_ENDPOINT | cut -d':' -f1)
            echo "=== Connectivity Test ==="
            if ping -c 3 -W 2 $PEER_IP >/dev/null 2>&1; then
                echo "✓ Connectivity to peer $PEER_IP: OK"
            else
                echo "✗ Connectivity to peer $PEER_IP: FAILED"
            fi
        fi
    else
        log_msg "WireGuard interface $WG_INTERFACE is DOWN"
        echo "WireGuard interface $WG_INTERFACE is not running"
    fi
}

# Function to monitor and auto-reconnect
monitor_interface() {
    log_msg "Starting WireGuard monitoring (PID: $$)"
    
    while [ -f $WG_PIDFILE ]; do
        if ! interface_is_up; then
            log_msg "Interface is down, attempting to reconnect..."
            teardown_interface
            sleep 5
            read_config && setup_interface
        fi
        
        # Check every 30 seconds
        sleep 30
    done
    
    log_msg "WireGuard monitoring stopped"
}

# Main execution logic
case "$1" in
    start)
        log_msg "Starting WireGuard VPN setup..."
        
        # Create log directory
        mkdir -p /var/log /var/run
        
        # Check if already running
        if [ -f $WG_PIDFILE ] && kill -0 $(cat $WG_PIDFILE) 2>/dev/null; then
            log_msg "WireGuard is already running with PID $(cat $WG_PIDFILE)"
            exit 1
        fi
        
        # Read configuration
        if ! read_config; then
            exit 1
        fi
        
        # Setup interface
        if ! setup_interface; then
            exit 1
        fi
        
        # Start monitoring in background
        monitor_interface &
        MONITOR_PID=$!
        echo $MONITOR_PID > /var/run/wg0_monitor.pid
        
        log_msg "WireGuard VPN started successfully"
        ;;
        
    stop)
        log_msg "Stopping WireGuard VPN..."
        
        # Stop monitor
        if [ -f /var/run/wg0_monitor.pid ]; then
            MONITOR_PID=$(cat /var/run/wg0_monitor.pid)
            kill $MONITOR_PID 2>/dev/null
            rm -f /var/run/wg0_monitor.pid
        fi
        
        # Teardown interface
        teardown_interface
        ;;
        
    restart)
        log_msg "Restarting WireGuard VPN..."
        $0 stop
        sleep 2
        $0 start
        ;;
        
    status)
        check_status
        ;;
        
    monitor)
        read_config && monitor_interface
        ;;
        
    *)
        echo "Usage: $0 {start|stop|restart|status|monitor}"
        echo ""
        echo "Commands:"
        echo "  start   - Setup and start WireGuard interface"
        echo "  stop    - Teardown WireGuard interface"
        echo "  restart - Restart WireGuard interface"
        echo "  status  - Show interface status"
        echo "  monitor - Monitor and auto-reconnect (internal use)"
        exit 1
        ;;
esac

exit $?