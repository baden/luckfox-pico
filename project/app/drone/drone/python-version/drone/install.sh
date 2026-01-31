#!/bin/bash

SERVER_URL="http://s.navi.cc/"
LOCAL_PROJECT_DIR="/root"
ARCHIVE_PATH="/tmp/drone.tar.gz"
echo "Завантаження $SERVER_URL/drone.tar.gz та розпакування $ARCHIVE_PATH..."
wget -qO- "$SERVER_URL/drone.tar.gz" | gunzip -c - | tar xf - -C "$LOCAL_PROJECT_DIR"
ln -sf "$LOCAL_PROJECT_DIR/etc/init.d/S99drone_autostart_script" /etc/init.d/S99drone_autostart_script
chmod +x "$LOCAL_PROJECT_DIR/run.sh"
chmod +x /etc/init.d/S99drone_autostart_script

echo "Для запуску, виконайте команду:"
echo "# /etc/init.d/S99drone_autostart_script start"
