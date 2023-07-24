#!/system/bin/sh
masks=`getprop vendor.can.masks`
if [ -n "$masks" ]; then
    echo $masks > /sys/class/net/can0/masks
fi
