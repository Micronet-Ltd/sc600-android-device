#!/bin/sh
#runs on boot_completed and on persist.vendor.ota.status=logCopy 

writeLog()
{
	echo $(date "+%Y-%m-%d %H:%M:%S") $1 >> /sdcard/Download/updater.log
	return
}

updateStatus()
{
writeLog "$1"
for i in 1 2 4 8 16 999
	do
		curl -f -k -X 'PUT' "https://$baseUrl/api/campaigns/result" -H 'accept: */*' -H 'Content-Type: application/json' -H "$token" -d "$1"
		uploadCode=$?
		if [[ i -le 999 ]] && [[ $uploadCode == 0 ]]
			then
				writeLog "Update server done"
				break
		elif [[ i -eq 999 ]] && [[ $uploadCode != 0 ]]
			then
				writeLog "Update server Failed: $uploadCode"
				break
		fi
		writeLog "Update server Failed: $uploadCode, waiting $i minutes"
		am broadcast -a com.micronet.ota.status --es status "Update server Failed: $exitCode, waiting $i minutes" --ez chk false --ez dnl false --ez inst false > /dev/null
		sleep $(($i*60))s
	done
return
}

specialCommand=$(sed -n 's/Command://p' /sdcard/Download/manifest.txt)
imeiNum=$(cat /sdcard/Download/imei.txt)
CampaignID=$(sed -n 's/CampaignID://p' /sdcard/Download/manifest.txt)

if [[ -f /sdcard/Download/settings.cfg ]]
	then
		interval=$(sed -n 's/Interval://p' /sdcard/Download/settings.cfg)
		baseUrl=$(sed -n 's/BaseUrl://p' /sdcard/Download/settings.cfg)
		token=$(sed -n 's/Token://p' /sdcard/Download/settings.cfg)
	else
		interval=30m
		baseUrl=devices.micronet-inc.com
		token="x-api-key: b3479f8d-fc96-414a-99bf-1e883e052592"
fi

writeLog "Start Log operation: $specialCommand"
am broadcast -a com.micronet.ota.status --es status "Start Log Operation: $specialCommand, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null

if [ "$specialCommand" == "Off" ]
	then
		setprop persist.logd.logpersistd stop
		setprop logd.logpersistd stop
		sleep 2s
		tar -cz /data/misc/logd/* -f /sdcard/Download/${imeiNum}_${CampaignID}_Log.tar
		
		for i in 1 2 4 8 16 999
			do
				curl -f -k --location --request PUT "https://$baseUrl/api/campaigns/$CampaignID/file-for-device/$imeiNum" --header "$token" --form "file=@/sdcard/Download/${imeiNum}_${CampaignID}_Log.tar"
				uploadCode=$?
				if [[ i -le 999 ]] && [[ $uploadCode == 0 ]]
					then
						writeLog "Upload done"
						break
				elif [[ i == 999 ]] && [[ $uploadCode != 0 ]]
					then
						writeLog "Upload Failed: $uploadCode"
						break
				fi
				writeLog "Upload Failed: $uploadCode, waiting $i minutes"
				am broadcast -a com.micronet.ota.status --es status "Upload Failed: $exitCode, waiting $i minutes" --ez chk false --ez dnl false --ez inst false > /dev/null
				sleep $(($i*60))s
			done
		rm /sdcard/Download/${imeiNum}_${CampaignID}_Log.tar

elif [ "$specialCommand" == "Kernel" ]
	then
		setprop persist.logd.logpersistd.buffer kernel
		sleep 2s
		setprop logd.logpersistd.buffer kernel
		sleep 2s
		setprop persist.logd.logpersistd logcatd
		sleep 2s
		setprop logd.logpersistd logcatd
		sleep 2s
		updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 5,\"state\": 1}"
elif [ "$specialCommand" == "All" ]
	then
		setprop persist.logd.logpersistd.buffer default,security,kernel,radio
		sleep 2s
		setprop logd.logpersistd.buffer default,security,kernel,radio
		sleep 2s
		setprop persist.logd.logpersistd logcatd
		sleep 2s
		setprop logd.logpersistd logcatd
		sleep 2s
		
		updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 5,\"state\": 1}"
fi

writeLog "Log Operation done."
am broadcast -a com.micronet.ota.status --es status "Log Operation Done!" --ez chk true --ez dnl false --ez inst false > /dev/null


status=$(getprop persist.vendor.ota.status)
if [ "$status" == "logCopy" ]
	then
		setprop persist.vendor.ota.status init
	else
		setprop persist.vendor.ota.status manualControl
fi

