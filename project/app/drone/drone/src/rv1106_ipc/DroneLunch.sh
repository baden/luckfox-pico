#!/bin/sh

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

check_linker() {
	[ ! -L "$2" ] && ln -sf $1 $2
}

network_init() {
	# TODO: Start VPN service if needed (wireguard)
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
	ifconfig eth0 up && udhcpc -i eth0 >/dev/null 2>&1
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

	drone &
}

rcS

ulimit -c unlimited
echo "/data/core-%p-%e" >/proc/sys/kernel/core_pattern
# echo 0 > /sys/devices/platform/rkcif-mipi-lvds/is_use_dummybuf

echo 1 >/proc/sys/vm/overcommit_memory

post_chk &
