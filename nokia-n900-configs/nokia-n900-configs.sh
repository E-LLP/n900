#!/bin/bash

# Set up GPIO lines for N900 modem and other N900 setup items
# Authors: Kai Vehmanen, Marko Saukko, Kalle Jokiniemi.

function setup_gpio
{
    # set up the GPIO's for N900 modem:
    echo 70 >/sys/class/gpio/export
    echo low >/sys/class/gpio/gpio70/direction
    echo 0 >/sys/class/gpio/gpio70/value
    echo 73 >/sys/class/gpio/export
    echo high >/sys/class/gpio/gpio73/direction
    echo 0 >/sys/class/gpio/gpio73/value
    echo 74 >/sys/class/gpio/export
    echo low >/sys/class/gpio/gpio74/direction
    echo 75 >/sys/class/gpio/export
    echo low >/sys/class/gpio/gpio75/direction
    echo 157 >/sys/class/gpio/export
    echo low >/sys/class/gpio/gpio157/direction
    echo 0 >/sys/class/gpio/gpio157/value
    # create symlinks for ofono N900 plugin
    mkdir -p /dev/cmt
    ln -nsf /sys/class/gpio/gpio70 /dev/cmt/cmt_apeslpx
    ln -nsf /sys/class/gpio/gpio74 /dev/cmt/cmt_en
    ln -nsf /sys/class/gpio/gpio73 /dev/cmt/cmt_rst_rq
    ln -nsf /sys/class/gpio/gpio75 /dev/cmt/cmt_rst
    ln -nsf /sys/class/gpio/gpio157 /dev/cmt/cmt_bsi
}

function setup_keymap
{
    /bin/loadkeys /lib/kbd/keymaps/arm/qwerty/nokia-n900.map
}

function setup_pm
{
       if [ $1 -ne 0 ]; then
               UART_SLEEP=15
       else
               UART_SLEEP=0
       fi

       echo $UART_SLEEP > /sys/class/tty/ttyO0/device/sleep_timeout
       echo $UART_SLEEP > /sys/class/tty/ttyO1/device/sleep_timeout
       echo $UART_SLEEP > /sys/class/tty/ttyO2/device/sleep_timeout

       echo $1 > /sys/kernel/debug/pm_debug/sleep_while_idle
}


case "$1" in
  start)
        echo "Setting up GPIO lines for N900 modem"
        setup_gpio
        echo "Setting up PM"
        setup_pm 1
        echo "Setting up keymap"
        setup_keymap || echo ".. failed"
        ;;
  stop)
        echo "Stopping PM"
        setup_pm 0
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
