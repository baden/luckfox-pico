#!/bin/sh

export WG_PRIVATE_KEY="/etc/wireguard/privatekey"
export WG_PUBLIC_KEY="/etc/wireguard/publickey"

# TODO: New luckdrone binary for better performance and stability. For now, we keep the old one for compatibility.
#DRONE_PROCESS_NAME="drone"
#DRONE_BIN="/oem/usr/bin/drone"
#UPDATE_FILE="/oem/usr/bin/drone.update"
#OLD_FILE="/oem/usr/bin/drone.old"

DRONE_PROCESS_NAME="luckdrone"
DRONE_PATH="/root"
DRONE_BIN="/root/luckdrone"
UPDATE_FILE="/root/luckdrone.update"
OLD_FILE="/root/luckdrone.old"

if [ -f /oem/usr/share/drone-env.sh ]; then
	echo "Loading environment variables from /oem/usr/share/drone-env.sh"
	. /oem/usr/share/drone-env.sh
else
	echo "Error: /oem/usr/share/drone-env.sh not found."
	exit 1
fi


# Function to check if Wireguard interface exists
interface_exists() {
    ip link show wg0 >/dev/null 2>&1
}

# Function to check if Wireguard interface is up
# Це не працює для Wireguard бо він не має стану "up" як звичайні інтерфейси, тому поки що не використовуємо цю функцію.
interface_is_up() {
    [ -d /sys/class/net/wg0 ] && [ "$(cat /sys/class/net/wg0/operstate)" = "up" ]
}

check_linker() {
	[ ! -L "$2" ] && ln -sf $1 $2
}

wg_help() {
	echo "Error: WireGuard keys not found."
	# Generate keys if they don't exist
	# cd /etc/wireguard
	umask 077
	wg genkey | tee $WG_PRIVATE_KEY | wg pubkey > $WG_PUBLIC_KEY
	echo "======================================================================"
	echo "Generated WireGuard keys. Please configure the VPN settings on server."
	#echo "Drone public key:"
	#cat $WG_PUBLIC_KEY
	echo "Go to server and add a new peer with the following configuration."
	echo " vi /etc/wireguard/wg0.conf"
	echo "Edit/Add the following peer configuration:"
	echo ""
	echo "[Peer]"
	echo "# Drone (Luckfox)"
	echo "PublicKey = $(cat $WG_PUBLIC_KEY)"
	echo "AllowedIPs = $WG_ADDRESS/32, $WG_SUBNET/24"
	echo ""
	echo "Execute on server and copy IP and key from output:"
	echo "  cat /etc/wireguard/publickey"
	echo "  ip addr|grep eth0"
	echo "  systemctl restart wg-quick@wg0.service"
	echo ""
	echo "Then update the WG_SERVER_IP and WG_SERVER_PUBLIC_KEY variables in this script."
	echo "  vi /userdata/drone-env.sh"
	echo "Then restart the drone or execute the script again."
	echo "  /etc/init.d/S21appinit start"
	echo "======================================================================"
}

setup_eth0() {
	# ifconfig eth0 up && udhcpc -i eth0 >/dev/null 2>&1

	# TODO: What is this for?
	ethaddr1=$(ifconfig -a | grep "eth.*HWaddr" | awk '{print $5}')

	if [ -f /data/ethaddr.txt ]; then
		ethaddr2=$(cat /data/ethaddr.txt)
		if [ $ethaddr1 == $ethaddr2 ]; then
			echo "eth HWaddr cfg ok"
		else
			ifconfig eth0 down
			ifconfig eth0 hw ether $ethaddr2
		fi
	else
		echo $ethaddr1 >/data/ethaddr.txt
	fi

	#ifconfig eth0 up && udhcpc -i eth0 >/dev/null 2>&1

	if [ "$WG_LOCAL" = "yes" ]; then
		ifconfig eth0 192.168.1.207 netmask 255.255.255.0
		route add default gw 192.168.1.1

		#ifconfig eth0:0 192.168.3.222 netmask 255.255.255.0
		#route add default gw 192.168.3.1
	else
		# Wireguard on router, just get IP for VPN
		ifconfig eth0 10.8.$DRONE_ID.2 netmask 255.255.0.0
		route add default gw 10.8.$DRONE_ID.1
	fi

	cat > /etc/resolv.conf <<EOF
nameserver 8.8.8.8
nameserver 8.8.4.4
EOF

}

setup_wireguard() {

	# Check if WireGuard interface already exists
	if interface_exists; then
		echo "WireGuard interface wg0 already exists."
		# Do nothing if it exists
		return
	fi

	# Check if private and public keys exist
	if [ ! -f "$WG_PRIVATE_KEY" ] || [ ! -f "$WG_PUBLIC_KEY" ]; then
		wg_help
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

	echo "Creating WireGuard interface wg0..."
	ip link add dev wg0 type wireguard

	# Set IP address
	echo "Setting IP address $WG_NET.$DRONE_ID.2..."
	ip address add dev wg0 $WG_NET.$DRONE_ID.2/16

	# Configure WireGuard with private key and peer
	wg set wg0 \
		listen-port 51820 \
		private-key $WG_PRIVATE_KEY \
		peer $WG_SERVER_PUBLIC_KEY \
		allowed-ips $WG_NET.0.0/16 \
		endpoint $WG_SERVER_IP:51820 \
		persistent-keepalive 25
	ip link set dev wg0 mtu $WG_MTU

	# Bring interface up
	echo "Bringing up interface wg0..."
	ip link set up dev wg0

	# Add 10.0.x.0/24 to eth0 for local network access to cameras
	if ! ip addr show eth0 | grep -q "10.0.$DRONE_ID.1"; then
		echo "Adding route to local network"
    	ip addr add 10.0.$DRONE_ID.1/24 dev eth0
	fi

	# Шоб мати доступ до камер з заводськими налаштуваннями (192.168.1.108/32)
	# ip route add 192.168.1.108 dev eth0
	#ip route replace 192.168.1.108 dev eth0

	# Enable IP forwarding
	echo "Enabling IP forwarding..."
	sysctl -w net.ipv4.ip_forward=1

	# Set up NAT (assuming eth0 is the outbound interface)
	echo "Setting up NAT..."
	# 1. Очищення (опціонально, залежить від вашої системи)
	# iptables -F FORWARD
	# iptables -F INPUT

	# 2. Дозволяємо вхідний трафік для go2rtc (на самому Luckfox) через VPN
	# Порти: 1984 (API/Web), 8554 (RTSP), 8555 (WebRTC UDP/TCP)
	iptables -I INPUT -i wg0 -p tcp -m multiport --dports 1984,8554,8555 -j ACCEPT
	iptables -I INPUT -i wg0 -p udp --dport 8555 -j ACCEPT

	# 3. NAT для виходу в інтернет через VPN (якщо потрібно для самого Luckfox)
	iptables -t nat -A POSTROUTING -o wg0 -j MASQUERADE
	iptables -t nat -A POSTROUTING -o eth0 -d 192.168.1.0/24 -j MASQUERADE
	iptables -t nat -A POSTROUTING -o eth0 -d 10.0.$DRONE_ID.0/24 -j MASQUERADE

	# 4. Forwarding: Дозволяємо клієнтам з VPN бачити камери
	iptables -A FORWARD -i wg0 -o eth0 -j ACCEPT
	# Дозволяємо відповіді від камер у VPN
	iptables -A FORWARD -i eth0 -o wg0 -m state --state RELATED,ESTABLISHED -j ACCEPT

	# Підрізаємо пакети TCP до максимальної величини, щоб уникнути фрагментації
 	iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

	# Tune network parameters for better performance
	# Set txqueuelen to 5000 for better performance
	tc qdisc add dev wg0 root fq_codel limit 1000 target 5ms interval 100ms
	ifconfig wg0 txqueuelen 5000
}

network_init() {
	echo "Initializing VPN network..."

	setup_eth0

	if [ "$WG_LOCAL" = "yes" ]; then
		setup_wireguard
	fi

	# Tune network parameters for better performance
	tc qdisc add dev eth0 root fq_codel
	ifconfig eth0 txqueuelen 5000
	sysctl -w net.core.rmem_max=2097152
	sysctl -w net.core.wmem_max=2097152

	echo "Done."
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

	# Disable fb for OLED on I2C3 if exists
	# echo 3-003c > /sys/bus/i2c/devices/3-003c/driver/unbind


	# Check if drone is already running
	pidof $DRONE_PROCESS_NAME >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "Drone application is already running."
	else

		# 1. Перевіряємо, чи існує файл оновлення
		if [ -f "$UPDATE_FILE" ]; then
			echo "Update for drone app found! Processing..."

			# 2. Якщо основний бінарник вже існує, перейменовуємо його в .old
			if [ -f "$DRONE_BIN" ]; then
				mv -f "$DRONE_BIN" "$OLD_FILE"
			fi

			# 3. Перейменовуємо оновлення в основний бінарник
			mv "$UPDATE_FILE" "$DRONE_BIN"
			
			chmod +x "$DRONE_BIN"
			
			echo "Update for drone app installed successfully."
		fi

		echo "Starting Drone application..."
		cd $DRONE_PATH
		rm -f /tmp/no_drone_reboot
		(
			$DRONE_BIN \
				-s $DR_UDP_HOST \
				-g \
				 2>&1 | logger -t drone_app
			
			if [ ! -f /tmp/no_drone_reboot ]; then
				echo "CRITICAL: Drone application terminated! Rebooting device in 3 seconds..." | logger -t drone_app
				sleep 3
				reboot
			else
				echo "Drone stopped intentionally. Skipping reboot." | logger -t drone_app
				rm -f /tmp/no_drone_reboot
			fi
		) &
	fi

	# pidof go2rtc >/dev/null 2>&1
	# if [ $? -eq 0 ]; then
	# 	echo "go2rtc is already running."
	# else
	# 	echo "Starting go2rtc..."
	# 	export GOGC=20
    #     export GOMEMLIMIT=50MiB
	# 	go2rtc -c /oem/usr/share/go2rtc.yaml 2>&1 | logger -t go2rtc &
	# fi
}

#rcS

ulimit -c unlimited
echo "/data/core-%p-%e" >/proc/sys/kernel/core_pattern
# echo 0 > /sys/devices/platform/rkcif-mipi-lvds/is_use_dummybuf

echo 1 >/proc/sys/vm/overcommit_memory

post_chk &
