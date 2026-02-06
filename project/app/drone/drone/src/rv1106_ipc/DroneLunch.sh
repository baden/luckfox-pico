#!/bin/sh

export WG_INTERFACE="wg0"
export WG_PRIVATE_KEY="/etc/wireguard/privatekey"
export WG_PUBLIC_KEY="/etc/wireguard/publickey"

if [ -f /userdata/drone-env.sh ]; then
	echo "Loading environment variables from /userdata/drone-env.sh"
	. /userdata/drone-env.sh
else
		cat >/userdata/drone-env.sh <<EOF
export WG_SERVER_IP="SERVER_IP_HERE"
export WG_SERVER_PUBLIC_KEY="SERVER_PUBLIC_KEY_HERE"
export WG_ADDRESS="10.8.0.2"
export WG_PEER_ALLOWED_IPS="10.8.0.0"
export WG_SUBNET="10.0.1.0"
export WG_UPLINK_IP="192.168.1.201"
export WG_UPLINK_NET="192.168.1.0"
export WG_UPLINK_GATE="192.168.1.1"
export WG_MTU="1200"
EOF
	echo "Error: /userdata/drone-env.sh not found. Init default settings. Please edit the file to configure VPN and drone settings."
	exit 1
fi


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

	ifconfig eth0 $WG_UPLINK_IP netmask 255.255.255.0
	route add default gw $WG_UPLINK_GATE

	ifconfig eth0:0 192.168.3.222 netmask 255.255.255.0
	route add default gw 192.168.3.1

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
		echo "Go to server and add a new peer with the following configuration."
		echo " vi /etc/wireguard/wg0.conf"
		echo "Edit/Add the following peer configuration:"
		echo ""
		echo "[Peer]"
		echo "# Drone (Luckfox)"
		echo "PublicKey = $(cat $WG_PUBLIC_KEY)"
		echo "AllowedIPs = $(cat $WG_ADDRESS)/32, $(cat $WG_SUBNET)/24"
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
	ip address add dev $WG_INTERFACE $WG_ADDRESS/24

	# Configure WireGuard with private key and peer
	wg set $WG_INTERFACE listen-port 51820 private-key $WG_PRIVATE_KEY peer $WG_SERVER_PUBLIC_KEY allowed-ips $WG_PEER_ALLOWED_IPS/24 endpoint $WG_SERVER_IP:51820 persistent-keepalive 25
	ip link set dev $WG_INTERFACE mtu $WG_MTU

	# Bring interface up
	echo "Bringing up interface $WG_INTERFACE..."
	ip link set up dev $WG_INTERFACE

	# Add WG$WG_SUBNET/24 to eth0 for local network access
	if ! ip addr show eth0 | grep -q "$WG_SUBNET"; then
		echo "Adding route to local network"
    	ip addr add $WG_SUBNET/24 dev eth0
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
	iptables -I INPUT -i $WG_INTERFACE -p tcp -m multiport --dports 1984,8554,8555 -j ACCEPT
	iptables -I INPUT -i $WG_INTERFACE -p udp --dport 8555 -j ACCEPT
	# iptables -A INPUT -i $WG_INTERFACE -p tcp --dport 1984 -j ACCEPT
	# iptables -A INPUT -i $WG_INTERFACE -p tcp --dport 8554 -j ACCEPT
	# iptables -A INPUT -i $WG_INTERFACE -p tcp --dport 8555 -j ACCEPT
	# iptables -A INPUT -i $WG_INTERFACE -p udp --dport 8555 -j ACCEPT

	# 3. NAT для виходу в інтернет через VPN (якщо потрібно для самого Luckfox)
	iptables -t nat -A POSTROUTING -o $WG_INTERFACE -j MASQUERADE
	iptables -t nat -A POSTROUTING -o eth0 -d $WG_UPLINK_NET/24 -j MASQUERADE
	iptables -t nat -A POSTROUTING -o eth0 -d $WG_SUBNET/24 -j MASQUERADE

	# 4. Forwarding: Дозволяємо клієнтам з VPN бачити камери
	iptables -A FORWARD -i $WG_INTERFACE -o eth0 -j ACCEPT
	# Дозволяємо відповіді від камер у VPN
	iptables -A FORWARD -i eth0 -o $WG_INTERFACE -m state --state RELATED,ESTABLISHED -j ACCEPT

	# Підрізаємо пакети TCP до максимальної величини, щоб уникнути фрагментації
 	iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

	# 5. Forwarding: Дозволяємо відповіді від камер іти в VPN
	# Попрередньє правиль вже це дозволяє?
	# iptables -A FORWARD -i eth0 -o $WG_INTERFACE -m state --state RELATED,ESTABLISHED -j ACCEPT

	# iptables -A FORWARD -i eth0 -o $WG_INTERFACE -j ACCEPT
	# iptables -A FORWARD -i wg0 -m state --state RELATED,ESTABLISHED -j ACCEPT
	# чи це треба?
	# iptables -A FORWARD -i $WG_INTERFACE -o eth0 -m state --state RELATED,ESTABLISHED -j ACCEPT

	# 6. БЛОКУВАННЯ: камери НЕ можуть ходити в інтернет через Luckfox
	# (дозволяємо їм тільки спілкування з VPN мережею, все інше DROP)
	# Поки залишаємо, ще перевіримо роботу RTMP.
	# iptables -A FORWARD -i eth0 -s 10.0.0.0/24 ! -d 10.8.0.0/24 -j DROP

	# Якшо треба буде обмежити роботу тільки одним IP
	# iptables -t nat -A POSTROUTING -o eth0 -d 192.168.1.0/24 -j MASQUERADE
 	# iptables -A FORWARD -i wg0 -o eth0 -d 192.168.1.108 -j ACCEPT
 	# iptables -A FORWARD -i eth0 -o wg0 -m state --state RELATED,ESTABLISHED -j ACCEPT

	# Set txqueuelen to 5000 for better performance
	tc qdisc add dev eth0 root fq_codel
	tc qdisc add dev wg0 root fq_codel limit 1000 target 5ms interval 100ms
	ifconfig eth0 txqueuelen 5000
	ifconfig wg0 txqueuelen 5000
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
	pidof drone >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "Drone application is already running."
	else
		echo "Starting Drone application..."
		/oem/usr/bin/drone 2>&1 | logger -t drone_app &
	fi


	pidof go2rtc >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "go2rtc is already running."
	else
		echo "Starting go2rtc..."
		export GOGC=20
        export GOMEMLIMIT=50MiB
		go2rtc -c /oem/usr/share/go2rtc.yaml 2>&1 | logger -t go2rtc &
	fi
}

rcS

ulimit -c unlimited
echo "/data/core-%p-%e" >/proc/sys/kernel/core_pattern
# echo 0 > /sys/devices/platform/rkcif-mipi-lvds/is_use_dummybuf

echo 1 >/proc/sys/vm/overcommit_memory

post_chk &
