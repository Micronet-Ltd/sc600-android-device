#!/system/bin/sh
bitrate=`getprop vendor.can.set_bitrate`
if [ -n "$bitrate" ]; then
    /system/bin/ip link set can0 type can bitrate $bitrate
fi
