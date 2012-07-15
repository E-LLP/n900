#!/bin/bash

while : 
do
bg=$( cat /sys/devices/platform/i2c_omap.2/i2c-2/2-0029/device0/illuminance0_input)
if [ $bg -lt 8 ];  then

#echo "10" > /sys/class/backlight/acx565akm/brightness

echo "120" > /sys/class/leds/lp5523:channel0/brightness
echo "120" > /sys/class/leds/lp5523:channel1/brightness
echo "120" > /sys/class/leds/lp5523:channel2/brightness
echo "120" > /sys/class/leds/lp5523:channel3/brightness
echo "120" > /sys/class/leds/lp5523:channel7/brightness
echo "120" > /sys/class/leds/lp5523:channel8/brightness


fi

if [ $bg -ge 12 ];  then

echo "0" > /sys/class/leds/lp5523:channel0/brightness
echo "0" > /sys/class/leds/lp5523:channel1/brightness
echo "0" > /sys/class/leds/lp5523:channel2/brightness
echo "0" > /sys/class/leds/lp5523:channel3/brightness
echo "0" > /sys/class/leds/lp5523:channel7/brightness
echo "0" > /sys/class/leds/lp5523:channel8/brightness
fi
sleep 1.5
done
