#!/bin/bash
c=1

echo "120" > /sys/class/leds/lp5523:channel4/brightness
sleep $c
echo "0" > /sys/class/leds/lp5523:channel4/brightness
sleep $c

echo "120" > /sys/class/leds/lp5523:channel5/brightness
sleep $c
echo "0" > /sys/class/leds/lp5523:channel5/brightness
sleep $c

echo "120" > /sys/class/leds/lp5523:channel6/brightness
sleep $c
echo "0" > /sys/class/leds/lp5523:channel6/brightness
sleep $c

#1
echo "120" > /sys/class/leds/lp5523:channel4/brightness | echo "120" > /sys/class/leds/lp5523:channel5/brightness
sleep $c
echo "0" > /sys/class/leds/lp5523:channel4/brightness | echo "0" > /sys/class/leds/lp5523:channel5/brightness
sleep $c

#2
echo "120" > /sys/class/leds/lp5523:channel4/brightness | echo "120" > /sys/class/leds/lp5523:channel6/brightness
sleep $c
echo "0" > /sys/class/leds/lp5523:channel4/brightness | echo "0" > /sys/class/leds/lp5523:channel6/brightness
sleep $c

#3
echo "120" > /sys/class/leds/lp5523:channel5/brightness | echo "120" > /sys/class/leds/lp5523:channel6/brightness
sleep $c
echo "0" > /sys/class/leds/lp5523:channel5/brightness | echo "0" > /sys/class/leds/lp5523:channel6/brightness
sleep $c

#4

#5

#6


