#! /vendor/bin/sh
setprop hw.board.id $(cat /proc/board_id)
setprop vib.get_profile $(cat /proc/vib_profile)
#setprop vendor.serialno $(getprop ro.serialno)