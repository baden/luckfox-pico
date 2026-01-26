#!/bin/sh

# WireGuard Key Generation Script for Luckfox Pico
# Generates private and public keys for WireGuard

WG_DIR="/etc/wireguard"
PRIVATE_KEY_FILE="$WG_DIR/privatekey"
PUBLIC_KEY_FILE="$WG_DIR/publickey"
CONFIG_FILE="$WG_DIR/wg0.conf"

echo "WireGuard Key Generation Script for Luckfox Pico"
echo "=========================================="

# Create wireguard directory
mkdir -p $WG_DIR

# Generate private key
echo "Generating private key..."
wg genkey > $PRIVATE_KEY_FILE
chmod 600 $PRIVATE_KEY_FILE

# Generate public key from private key
echo "Generating public key..."
wg pubkey < $PRIVATE_KEY_FILE > $PUBLIC_KEY_FILE
chmod 644 $PUBLIC_KEY_FILE

# Display keys
echo ""
echo "Keys generated successfully:"
echo "========================="

echo "Private Key (keep secret!):"
cat $PRIVATE_KEY_FILE
echo ""

echo "Public Key (share with server/peers):"
cat $PUBLIC_KEY_FILE
echo ""

# Create a basic configuration template
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Creating basic configuration template..."
    
    PRIVATE_KEY_CONTENT=$(cat $PRIVATE_KEY_FILE)
    
    cat > "$CONFIG_FILE" << EOF
# WireGuard Configuration File for Luckfox Pico
# Generated on $(date)

[Interface]
Address = 10.8.0.2/24
PrivateKey = $PRIVATE_KEY_CONTENT
ListenPort = 51820

[Peer]
# Server configuration - fill in the details below
PublicKey = SERVER_PUBLIC_KEY_HERE
AllowedIPs = 10.8.0.0/24
Endpoint = SERVER_IP_ADDRESS:51820
PersistentKeepalive = 25
EOF
    
    echo "Configuration template created at: $CONFIG_FILE"
    echo "Please edit this file with your server details."
else
    echo "Configuration file already exists: $CONFIG_FILE"
fi

echo ""
echo "Files created:"
echo "- Private key: $PRIVATE_KEY_FILE"
echo "- Public key:  $PUBLIC_KEY_FILE"
echo "- Config file: $CONFIG_FILE"
echo ""
echo "Next steps:"
echo "1. Share the public key with your WireGuard server admin"
echo "2. Get the server's public key and endpoint information"
echo "3. Edit $CONFIG_FILE with server details"
echo "4. Run: /usr/bin/wireguard-setup.sh start"
echo ""
echo "Security note: Keep your private key secure!"