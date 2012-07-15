#!/bin/bash
clear
while :
do
echo "This script shows some data about the battery."
        more /sys/class/power_supply/bq27200-0/capacity
echo "%"
echo "-----------"
more /sys/class/power_supply/bq27200-0/current_now
echo "mA"
echo "-----------"
more /sys/class/power_supply/bq27200-0/voltage_now
echo "mV"
echo "-----------"
sleep 10
        clear
done
