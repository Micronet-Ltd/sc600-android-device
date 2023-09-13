#!/system/bin/sh
mode=`getprop vendor.can.set_op_mode`
if [ -n "$mode" ]; then
    /system/bin/ip link set can0 type can $mode
fi
