#!/bin/sh
#runs on property persist.vendor.ota.status = manualInstall

updateType=$(sed -n 's/UpdateType://p' /sdcard/Download/manifest.txt)

am broadcast -a com.micronet.ota.status --es status "Please wait: $updateType" --ez chk false --ez dnl false --ez inst false > /dev/null
echo $(date "+%Y-%m-%d %H:%M:%S") "Update type $updateType" >> /sdcard/Download/updater.log

if [ $updateType == 2 ]
	then 
		setprop persist.vendor.ota.status manualCopyApp
elif [ $updateType == 1 ]
	then
		setprop persist.vendor.ota.status manualUpdate
else
	am broadcast -a com.micronet.ota.status --es status "Nothing to do." --ez chk true --ez dnl false --ez inst false > /dev/null
fi
