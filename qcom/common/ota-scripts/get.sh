#!/bin/sh

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

writeLog "Start Get operation: $specialCommand"
am broadcast -a com.micronet.ota.status --es status "Start Get Operation: $specialCommand, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null

tar -cz $specialCommand -f /sdcard/Download/${imeiNum}_${CampaignID}_Get.tar
exitCode=$?
writeLog "tar code - $exitCode"
if [ $exitCode == 0 ]
	then
		am broadcast -a com.micronet.ota.status --es status "Uploading..." --ez chk false --ez dnl false --ez inst false > /dev/null
		writeLog "Uploading..."
		for i in 1 2 4 8 16 999
			do
				curl -f -k --location --request PUT "https://$baseUrl/api/campaigns/$CampaignID/file-for-device/$imeiNum" --header "$token" --form "file=@/sdcard/Download/${imeiNum}_${CampaignID}_Get.tar"
				uploadCode=$?
				if [[ i -le 999 ]] && [[ $uploadCode == 0 ]]
					then
						writeLog "Upload done"
						am broadcast -a com.micronet.ota.status --es status "Upload done" --ez chk true --ez dnl false --ez inst false > /dev/null
						break
				elif [[ i == 999 ]] && [[ $uploadCode != 0 ]]
					then
						writeLog "Upload Failed: $uploadCode"
						am broadcast -a com.micronet.ota.status --es status "Upload Failed: $uploadCode" --ez chk true --ez dnl false --ez inst false > /dev/null
						sleep $interval
						break
				fi
				writeLog "Upload Failed: $uploadCode, waiting $i minutes"
				am broadcast -a com.micronet.ota.status --es status "Uploading Failed: $exitCode, waiting $i minutes" --ez chk false --ez dnl false --ez inst false > /dev/null
				sleep $(($i*60))s
			done
	else
		am broadcast -a com.micronet.ota.status --es status "TAR error code: $exitCode, updating server..." --ez chk true --ez dnl false --ez inst false > /dev/null
		updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 7,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"TAR error code $exitCode\"}"
		if [ "$status" == "get" ]
			then
				sleep $interval
		fi
fi

rm -f /sdcard/Download/${imeiNum}_${CampaignID}_Get.tar

status=$(getprop persist.vendor.ota.status)
if [ "$status" == "get" ]
	then
		setprop persist.vendor.ota.status init
	else
		setprop persist.vendor.ota.status manualControl
fi