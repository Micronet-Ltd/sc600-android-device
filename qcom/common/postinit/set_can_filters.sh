#!/system/bin/sh
filters=`getprop vendor.can.filters`
if [ -n "$filters" ]; then
    echo $filters > /sys/class/net/can0/filters
fi
