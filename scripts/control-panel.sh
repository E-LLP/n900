#!/bin/bash

clear

a="| A little control panel for our lovely N900! |"
b="|---------------------------------------------|"
c="Battery:"
d="%    "
e="mA    "
f="mV"
g="|"
h="Uptime:"
i="       Load:"
j="Light:"
k="lux"
l="  Accelerometer:"
m="Jack:"

while :
do

#Data
capacity=$( cat /sys/class/power_supply/bq27200-0/capacity)
current_now=$( cat /sys/class/power_supply/bq27200-0/current_now)
voltage_now=$( cat /sys/class/power_supply/bq27200-0/voltage_now)
uptime=$( uptime | awk '{print $3}')
load_1=$( uptime | awk '{print $8}')
load_2=$( uptime | awk '{print $9}')
load_3=$( uptime | awk '{print $10}')
ambient=$( cat /sys/devices/platform/i2c_omap.2/i2c-2/2-0029/device0/illuminance0_input)
accel=$( cat /sys/devices/platform/lis3lv02d/input/input6/device/position)
jack=$( cat /sys/class/gpio/gpio177/value)

if [ $jack -eq 1 ]; then
jack_state="Disconnected"
fi

if [ $jack -eq 0 ]; then
jack_state="Connected"
fi


echo "$b"
#First line
echo "$a"

#Second line
echo "$b"

#Third line
echo "$g $c $capacity $d $current_now $e $voltage_now $f $g"

#Fourth line
echo "$b"

#Fifth line
echo "$g $h $uptime $i $load_1 $load_2 $load_3 $g"

#Sixth line
echo "$b"

#Seventh line
echo "$g $j $ambient $k $l $accel $g"

#Eight line
echo "$b"

#Ninth line
echo "$g $m $jack_state"

echo "$b"


Keypress=$(dd bs=1 count=1 2> /dev/null)

clear
done
exit 0
