#!/bin/sh

WG_INTERFACE="wg0"
WG_PRIVATE_KEY="/etc/wireguard/privatekey"
WG_PUBLIC_KEY="/etc/wireguard/publickey"
WG_SERVER_IP="SERVER_IP_HERE"
WG_SERVER_PUBLIC_KEY="SERVER_PUBLIC_KEY_HERE"
WG_ADDRESS="10.8.0.2/24"
WG_PEER_ALLOWED_IPS="10.8.0.0/24"

rcS() {
	for i in /oem/usr/etc/init.d/S??*; do

		# Ignore dangling symlinks (if any).
		[ ! -f "$i" ] && continue

		case "$i" in
		*.sh)
			# Source shell script for speed.
			(
				trap - INT QUIT TSTP
				set start
				. $i
			)
			;;
		*)
			# No sh extension, so fork subprocess.
			$i start
			;;
		esac
	done
}

# Function to check if interface exists
interface_exists() {
    ip link show $WG_INTERFACE >/dev/null 2>&1
}

# Function to check if interface is up
interface_is_up() {
    [ -d /sys/class/net/$WG_INTERFACE ] && [ "$(cat /sys/class/net/$WG_INTERFACE/operstate)" = "up" ]
}

check_linker() {
	[ ! -L "$2" ] && ln -sf $1 $2
}

network_init() {
	echo "Initializing VPN network..."

	# Check if WireGuard interface already exists
	if interface_exists; then
		echo "WireGuard interface $WG_INTERFACE already exists."
		# Do nothing if it exists
		return
	fi

	# Check if private and public keys exist
	if [ ! -f "$WG_PRIVATE_KEY" ] || [ ! -f "$WG_PUBLIC_KEY" ]; then
		echo "Error: WireGuard keys not found."

		# Generate keys if they don't exist
		# cd /etc/wireguard
		umask 077
		wg genkey | tee $WG_PRIVATE_KEY | wg pubkey > $WG_PUBLIC_KEY
		echo "======================================================================"
		echo "Generated WireGuard keys. Please configure the VPN settings on server."
		#echo "Drone public key:"
		#cat $WG_PUBLIC_KEY
		echo "Go to server and add a new peer with the following configuration (/etc/wireguard/wg0.conf):"
		echo "[Peer]"
		echo "# Drone (Luckfox)"
		echo "PublicKey = $(cat $WG_PUBLIC_KEY)"
		echo "AllowedIPs = 10.8.0.2/32, 10.0.0.0/24"
		echo ""
		echo "Execute on server and copy IP and key from output:"
		echo "  cat /etc/wireguard/publickey"
		echo "  ip addr|grep eth0"
		echo "  systemctl restart wg-quick@wg0.service"
		echo ""
		echo "Then update the WG_SERVER_IP and WG_SERVER_PUBLIC_KEY variables in this script."
		echo "  vi /oem/usr/bin/DroneLunch.sh"
		echo "Then restart the drone or execute the script again."
		echo "  /etc/init.d/S21appinit start"
		echo "======================================================================"
		return 1
	fi

	# Check is server IP is set
	if [ "$WG_SERVER_IP" = "SERVER_IP_HERE" ]; then
		echo "Error: Please set the server IP in the script."
		return 1
	fi

	# Check is server public key is set
	if [ "$WG_SERVER_PUBLIC_KEY" = "SERVER_PUBLIC_KEY_HERE" ]; then
		echo "Error: Please set the server public key in the script."
		return 1
	fi

	echo "Creating WireGuard interface $WG_INTERFACE..."
	ip link add dev $WG_INTERFACE type wireguard

	# Set IP address
	echo "Setting IP address $WG_ADDRESS..."
	ip address add dev $WG_INTERFACE $WG_ADDRESS

	# Configure WireGuard with private key and peer
	wg set $WG_INTERFACE listen-port 51820 private-key $WG_PRIVATE_KEY peer $WG_SERVER_PUBLIC_KEY allowed-ips $WG_PEER_ALLOWED_IPS endpoint $WG_SERVER_IP:51820 persistent-keepalive 25

	# Bring interface up
	echo "Bringing up interface $WG_INTERFACE..."
	ip link set up dev $WG_INTERFACE


	# Add 10.0.0.0/24 to eth0 for local network access
	echo "Adding route to local network"
	ip addr add 10.0.0.1/24 dev eth0

	# Enable IP forwarding
	echo "Enabling IP forwarding..."
	sysctl -w net.ipv4.ip_forward=1

	# Set up NAT (assuming eth0 is the outbound interface)
	echo "Setting up NAT..."
	iptables -t nat -A POSTROUTING -o $WG_INTERFACE -j MASQUERADE
	iptables -A FORWARD -i eth0 -o $WG_INTERFACE -j ACCEPT
	# iptables -A FORWARD -i wg0 -m state --state RELATED,ESTABLISHED -j ACCEPT
	iptables -A FORWARD -i $WG_INTERFACE -o eth0 -m state --state RELATED,ESTABLISHED -j ACCEPT


	echo "Done."

	# TODO: Start VPN service if needed (wireguard)
	# ethaddr1=$(ifconfig -a | grep "eth.*HWaddr" | awk '{print $5}')

	# if [ -f /data/ethaddr.txt ]; then
	# 	ethaddr2=$(cat /data/ethaddr.txt)
	# 	if [ $ethaddr1 == $ethaddr2 ]; then
	# 		echo "eth HWaddr cfg ok"
	# 	else
	# 		ifconfig eth0 down
	# 		ifconfig eth0 hw ether $ethaddr2
	# 	fi
	# else
	# 	echo $ethaddr1 >/data/ethaddr.txt
	# fi
	# ifconfig eth0 up && udhcpc -i eth0 >/dev/null 2>&1
}

post_chk() {
	#TODO: ensure /userdata mount done
	# cnt=0
	# while [ $cnt -lt 30 ]; do
	# 	cnt=$((cnt + 1))
	# 	if mount | grep -w userdata; then
	# 		break
	# 	fi
	# 	sleep .1
	# done

	network_init &
	# check_linker /userdata /oem/usr/www/userdata
	# check_linker /media/usb0 /oem/usr/www/usb0
	# check_linker /mnt/sdcard /oem/usr/www/sdcard
	# if /data/rkipc not exist, cp /usr/share
	drone_ini=/userdata/drone.ini
	# default_rkipc_ini=/tmp/rkipc-factory-config.ini

	# if [ ! -f "/oem/usr/share/rkipc.ini" ]; then
	# fi

	# if [ ! -f "$default_rkipc_ini" ]; then
	# 	echo "Error: not found rkipc.ini !!!"
	# 	exit -1
	# fi

	# drone > /var/log/drone.log 2> /var/log/drone-err.log &
	# Look logs via:
	# cat /var/log/messages | grep drone_app
	# or
	# tail -f /var/log/messages | awk '/drone_app/ {print $0; fflush()}'

	# Check if drone is already running
	pidof drone >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "Drone application is already running."
		return
	fi

	/oem/usr/bin/drone 2>&1 | logger -t drone_app &
}

rcS

ulimit -c unlimited
echo "/data/core-%p-%e" >/proc/sys/kernel/core_pattern
# echo 0 > /sys/devices/platform/rkcif-mipi-lvds/is_use_dummybuf

echo 1 >/proc/sys/vm/overcommit_memory

post_chk &
