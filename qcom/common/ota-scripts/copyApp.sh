#!/bin/sh
#runs on property persist.vendor.ota.status = copyApp

writeLog()
{
	echo $(date "+%Y-%m-%d %H:%M:%S") $1 >> /sdcard/Download/updater.log
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
# start script

status=$(getprop persist.vendor.ota.status)
CampaignID=$(sed -n 's/CampaignID://p' /sdcard/Download/manifest.txt)
imeiNum=$(cat /sdcard/Download/imei.txt)
specialCommand=$(sed -n 's/Command://p' /sdcard/Download/manifest.txt)
packageName=$(sed -n 's/PackageNameDeltaSourceVersion://p' /sdcard/Download/manifest.txt)

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

am broadcast -a com.micronet.ota.status --es status "Moving APK, please wait..." > /dev/null
mv -f /sdcard/Download/downloadedApplication.apk /data/local/tmp/
exitCode=$?
writeLog "Move Code: $exitCode"
if [ $exitCode == 0 ]
	then
		writeLog "Installing"
		am broadcast -a com.micronet.ota.status --es status "Installing APK, please wait..." > /dev/null
		pm install -r /data/local/tmp/downloadedApplication.apk
		exitCode=$?
		writeLog "Installation Exit Code: $exitCode"
		if [ $exitCode == 0 ]
			then
				if [[ "$specialCommand" == None ]]
					then
						am broadcast -a com.micronet.ota.status --es status "Installation completed successfully, Updating server..." --ez chk false --ez dnl false --ez inst false > /dev/null
						updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 1}"
						am broadcast -a com.micronet.ota.status --es status "Installation completed successfully!" --ez chk true --ez dnl false --ez inst false > /dev/null
						rm /data/local/tmp/downloadedApplication.apk
				elif [[ "$specialCommand" == Invoke ]]
					then
						writeLog "Invoking application"
						monkey -p $packageName 1
						exitCode=$?
						if [ $exitCode == 0 ]
							then
								writeLog "Installation and Invokation completed successfully!"
								am broadcast -a com.micronet.ota.status --es status "Installation and Invokation completed successfully, Updating server..." --ez chk false --ez dnl false --ez inst false > /dev/null
								updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 1}"
								am broadcast -a com.micronet.ota.status --es status "Installation and Invokation completed successfully!" --ez chk true --ez dnl false --ez inst false
								rm /data/local/tmp/downloadedApplication.apk
							else
								writeLog "Installation successful, but and Invokation error $exitCode"
								am broadcast -a com.micronet.ota.status --es status "Install APK successful, but Invoke Error code $exitCode, Updating server..." --ez chk false --ez dnl false --ez inst false > /dev/null
								updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 4, \"errorCode\": 2,\"errorDesc\": \"Invokation code: $exitCode\"}"
								am broadcast -a com.micronet.ota.status --es status "Install APK successful, but Invoke Error code $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
								rm /data/local/tmp/downloadedApplication.apk
								sleep $interval
						fi
				fi
			else
				am broadcast -a com.micronet.ota.status --es status "Install APK Failed! Error code $exitCode, Updating server..." --ez chk false --ez dnl false --ez inst false > /dev/null
				updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Installation error code: $exitCode\"}"
				am broadcast -a com.micronet.ota.status --es status "Install APK Failed! Error code $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
				rm /data/local/tmp/downloadedApplication.apk
				sleep $interval
		fi
	else
		am broadcast -a com.micronet.ota.status --es status "Move APK Failed! Error code $exitCode, Updating server..." --ez chk false --ez dnl false --ez inst false > /dev/null
		updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Move App error code: $exitCode\"}"
		am broadcast -a com.micronet.ota.status --es status "Move APK Failed! Error code $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
		rm /sdcard/Download/downloadedApplication.apk
		sleep $interval
fi


if [ $status == manualCopyApp ]
	then
		setprop persist.vendor.ota.status manualControl
	else
		setprop persist.vendor.ota.status init
fi