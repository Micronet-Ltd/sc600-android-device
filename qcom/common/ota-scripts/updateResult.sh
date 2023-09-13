#!/bin/sh
#runs on property persist.vendor.ota.status = updateResult

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

writeLog "start updateResult"
if [[ -f /cache/result ]]
	then
		exitCode=$(cat /cache/result)
		sleep 15s
		CampaignID=$(sed -n 's/CampaignID://p' /sdcard/Download/manifest.txt)
		imeiNum=$(cat /sdcard/Download/imei.txt)
		
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
		
		writeLog "Found result file. Ended with result: $exitCode"
		if [ $exitCode == 0 ]
			then
				am broadcast -a com.micronet.ota.status --es status "OS Update Successful!" --ez chk false --ez dnl false --ez inst false > /dev/null
				updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 1}"
			else
				am broadcast -a com.micronet.ota.status --es status "OS Update Failed!" --ez chk false --ez dnl false --ez inst false > /dev/null
				updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Update result code: $exitCode\"}"
		fi
	else
		sleep 15s
		CampaignID=$(sed -n 's/CampaignID://p' /sdcard/Download/manifest.txt)
		imeiNum=$(cat /sdcard/Download/imei.txt)
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
		writeLog "Result file not found. Error?"
		am broadcast -a com.micronet.ota.status --es status "OS Update Failed!" --ez chk false --ez dnl false --ez inst false > /dev/null
		updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Update result file not found. Error?\"}"
fi

sdcardName=$(ls /storage/ | grep -)
sdcardPath=/storage/$sdcardName
rm /cache/result
rm /cache/msm8937_32-incremental-ota.zip
if [[ -n $sdcardName ]]
	then
		rm $sdcardPath/msm8937_32-incremental-ota.zip
fi

status=$(getprop persist.vendor.ota.status)
if [ $status == manualUpdateResult ]
	then
		setprop persist.vendor.ota.status manualControl
	else
		setprop persist.vendor.ota.status init
fi

