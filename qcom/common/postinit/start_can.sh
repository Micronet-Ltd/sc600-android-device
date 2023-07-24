#!/system/bin/sh
linkop=`getprop vendor.can`
if [ -n "$linkop" ]; then
    ifconfig can0 $linkop
fi
