#!/bin/sh

rcK()
{
	for i in $(ls /oem/usr/etc/init.d/S??*) ;do

		# Ignore dangling symlinks (if any).
		[ ! -f "$i" ] && continue

		case "$i" in
			*.sh)
				# Source shell script for speed.
				(
					trap - INT QUIT TSTP
					set stop
					. $i
				)
				;;
			*)
				# No sh extension, so fork subprocess.
				$i stop
				;;
		esac
	done
}

echo "Stop Application ..."
killall drone

while [ 1 ];
do
	sleep 1
	ps|grep drone|grep -v grep
	if [ $? -ne 0 ]; then
		echo "drone exit"
		break
	else
		echo "drone active"
	fi
done

rcK
