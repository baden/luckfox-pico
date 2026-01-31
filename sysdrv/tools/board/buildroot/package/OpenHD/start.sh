#!/bin/sh
[ -f /etc/profile.d/RkEnv.sh ] && source /etc/profile.d/RkEnv.sh

LOG_FILE="/var/log/openhd.log"
COMMAND="openhd"
PID_FILE="/var/run/openhd.pid"

/bin/sh /oem/usr/ko/insmod_ko.sh

network_init() {
    LOCK=/run/network_init.lock

    # if lock exists and process is alive, exit
    if [ -e "$LOCK" ] && kill -0 "$(cat "$LOCK")" 2>/dev/null; then
        echo "network_init already running"
        return
    fi

    echo $$ > "$LOCK"
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

start_openhd() {
network_init&
    echo "Starting OpenHD..." | tee -a "$LOG_FILE"
    if [ -f "/config/mis5001_CMK-OT2115-PC1_30IRC-F16.json" ]; then
        cp /config/mis5001_CMK-OT2115-PC1_30IRC-F16.json /oem/usr/share/iqfiles/mis5001_CMK-OT2115-PC1_30IRC-F16.json
    fi
    sleep 5
    if [[ $(id -u) -ne 0 ]]; then
        echo "Restarting script with root privileges..." | tee -a "$LOG_FILE"
        exec sudo "$0" start
        exit 0
    fi

    stop_openhd  # Ensure no previous instance is running

    # Run OpenHD in the background
    nohup $COMMAND >> "$LOG_FILE" 2>&1 &
    echo $! > "$PID_FILE"

    echo "OpenHD started successfully with PID $(cat $PID_FILE)!" | tee -a "$LOG_FILE"
}

stop_openhd() {
    echo "Stopping OpenHD..." | tee -a "$LOG_FILE"

    if [[ -f "$PID_FILE" ]]; then
        PID=$(cat "$PID_FILE")
        if kill "$PID" 2>/dev/null; then
            echo "OpenHD stopped (PID: $PID)." | tee -a "$LOG_FILE"
        else
            echo "Failed to stop OpenHD. Process might not exist." | tee -a "$LOG_FILE"
        fi
        rm -f "$PID_FILE"
    else
        echo "OpenHD is not running." | tee -a "$LOG_FILE"
    fi
}

restart_openhd() {
    stop_openhd
    start_openhd
}

status_openhd() {
    if [[ -f "$PID_FILE" ]]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "OpenHD is running (PID: $PID)." | tee -a "$LOG_FILE"
            exit 0
        else
            echo "OpenHD is not running, but PID file exists." | tee -a "$LOG_FILE"
            exit 1
        fi
    else
        echo "OpenHD is not running." | tee -a "$LOG_FILE"
        exit 1
    fi
}

case "$1" in
    start)
        start_openhd
        ;;
    stop)
        stop_openhd
        ;;
    restart)
        restart_openhd
        ;;
    status)
        status_openhd
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}" | tee -a "$LOG_FILE"
        exit 1
        ;;
esac

exit 0
