#!/bin/sh

BRIGHTNESS_SYSFS_PATH="/sys/class/backlight/acx565akm/brightness"

if [ $# -ne 1 ]; then
	echo "Usage: $0 <brightness>"
	exit 1
fi

echo $1 > $BRIGHTNESS_SYSFS_PATH

exit $?
