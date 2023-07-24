#!/bin/sh
#runs on boot_completed and on persist.vendor.ota.status=init

writeLog()
{
	echo $(date "+%Y-%m-%d %H:%M:%S") $1 >> /sdcard/Download/updater.log
	return
}

getDeviceVersion()
{
echo $(getprop ro.build.display.id | cut -d '.' -f 4- | cut -d ' ' -f 1)
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

doUpdate()
{
	writeLog "Querying Update Type"
	curl -f -k -X 'GET' "https://$baseUrl/api/campaigns/for-device/$imeiNum" -H 'accept: application/json' -H "$token" -o /sdcard/Download/manifest.txt
	exitCode=$?
	if [ $exitCode != 0 ]
		then
			writeLog "Getting manifest Error code $exitCode"
			am broadcast -a com.micronet.ota.status --es status "Getting manifest Error code $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
			return
	fi
	if [ $( stat -c%s /sdcard/Download/manifest.txt ) == 0 ]
		then
			writeLog "No Update"
			am broadcast -a com.micronet.ota.status --es status "No Update" --ez chk true --ez dnl false --ez inst false > /dev/null
			return
	fi
	
	updateType=$(sed -n 's/UpdateType://p' /sdcard/Download/manifest.txt)
	CampaignID=$(sed -n 's/CampaignID://p' /sdcard/Download/manifest.txt)
	downloadLink=$(sed -n 's/UrlToOtaPackage://p' /sdcard/Download/manifest.txt)
	SourceVersion=$(sed -n 's/SourceVersion://p' /sdcard/Download/manifest.txt)
	TargetVersion=$(sed -n 's/TargetVersion://p' /sdcard/Download/manifest.txt)
	specialCommand=$(sed -n 's/Command://p' /sdcard/Download/manifest.txt)
	writeLog "CampaignID is $CampaignID"
	if [ $updateType == 2 ]
		then 
			writeLog "Update Type Application: $downloadLink"
			writeLog "Special Command: $specialCommand"
			am broadcast -a com.micronet.ota.status --es status "Found update type Application! Special command? $specialCommand" --ez chk false --ez dnl true --ez inst false > /dev/null
	elif [ $updateType == 1 ]
		then
			writeLog "Update Type OS"
			if [ "$TargetVersion" == "$deviceVersion" ]
				then
					writeLog "$TargetVersion already Installed"
					am broadcast -a com.micronet.ota.status --es status "$TargetVersion already Installed" --ez chk true --ez dnl false --ez inst false > /dev/null
					updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 2}"
					return
			elif [[ -z $SourceVersion ]] || [ "$SourceVersion" == "$deviceVersion" ]
				then
					writeLog "Source And Device Versions Match"
					am broadcast -a com.micronet.ota.status --es status "Found update type OS!" --ez chk false --ez dnl true --ez inst false > /dev/null
				else
					writeLog "Source And Device Versions Mismatch: $SourceVersion and $deviceVersion"
					am broadcast -a com.micronet.ota.status --es status "Source And Device Versions Mismatch: $SourceVersion and $deviceVersion" --ez chk true --ez dnl false --ez inst false > /dev/null
					updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Source version mismatch\"}"
					return
			fi
	elif [ $updateType == 4 ]
		then
			writeLog "Getting inventory"
			rm /sdcard/Download/1.txt
			rm /sdcard/Download/2.txt
			rm /sdcard/Download/inventory.txt
			am broadcast -a com.micronet.ota.status --es status "Getting inventory..." > /dev/null
			LM=$(dumpsys package lm.smartcam.androidapp | grep versionName)
			exitCode=$?
			if [ $exitCode == 0 ]
				then
					writeLog "LM Found"
					echo lm.smartcam.androidapp > /sdcard/Download/1.txt
			fi
			pm list packages -3 | sed -e "s/^package://" >> /sdcard/Download/1.txt
			cat /sdcard/Download/1.txt | while read -r p || [[ -n "$p" ]]
				do
					dumpsys package $p | grep versionName | sed -e 's/^[ \t]*versionName=//' | head -n 1 >>/sdcard/Download/2.txt
				done
			paste -d":" /sdcard/Download/1.txt /sdcard/Download/2.txt > /sdcard/Download/inventory.txt
			qqq=$(cat /sdcard/Download/inventory.txt | sed "s/:/\":\"/" | tr -s "\n" "}" | sed "s/}/\"},{\"/g" )
			am broadcast -a com.micronet.ota.status --es status "Uploading Inventory..." > /dev/null
			curl -k -X 'PUT' "https://$baseUrl/api/campaigns/inventory" -H 'accept: */*' -H 'Content-Type: application/json' -H "$token" -d "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 4,\"state\": 1, \"packages\": [{\"${qqq%}OS\":\"$deviceVersion\"}]}"
			rm /sdcard/Download/1.txt
			rm /sdcard/Download/2.txt
			rm /sdcard/Download/inventory.txt
			am broadcast -a com.micronet.ota.status --es status "Inventory Sent" --ez chk true --ez dnl false --ez inst false > /dev/null
			return
	elif [ $updateType == 3 ]
		then
			writeLog "Update type Data"
			am broadcast -a com.micronet.ota.status --es status "Found update type Data!" --ez chk false --ez dnl true --ez inst false > /dev/null
	elif [ $updateType == 5 ]
		then
			writeLog "Update type Log"
			am broadcast -a com.micronet.ota.status --es status "Found update type Log!" --ez chk false --ez dnl false --ez inst false > /dev/null
			if [[ $status != manual* ]]
				then
					setprop persist.vendor.ota.status logCopy
				else
					setprop persist.vendor.ota.status manualLogCopy
			fi
			echo 1
			return
	elif [ $updateType == 7 ]
		then
			writeLog "Update type Get"
			am broadcast -a com.micronet.ota.status --es status "Found update type Get!" --ez chk false --ez dnl false --ez inst false > /dev/null
			if [[ $status != manual* ]]
				then
					setprop persist.vendor.ota.status get
				else
					setprop persist.vendor.ota.status manualGet
			fi
			echo 1
			return
	elif [ $updateType == 6 ]
		then
			am start-activity -n com.micronet.operations/com.micronet.operations.MainActivity --ei operation $specialCommand
			updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 6,\"state\": 1}"
			return
	fi
	
	if [[ $status != manual* ]]
		then
			setprop persist.vendor.ota.status download
			echo 1
	fi
}

# Start script
am broadcast -a com.micronet.ota.status --es status "Please Wait 20 seconds" --ez chk false --ez dnl false --ez inst false > /dev/null
writeLog "Starting script"
sleep 20s
status=$(getprop persist.vendor.ota.status)
writeLog "Starting script: Status is $status"
if [ $status == manualInit ]
	then
		writeLog "Manual mode"
elif [ $status == init ]
	then
		sleep 15s
		writeLog "Automatic mode"
elif [ $status == manualUpdateStarted ]
	then
		sleep 30s
		setprop persist.vendor.ota.status manualUpdateResult
		return
elif [ $status == updateStarted ]
	then
		sleep 30s
		setprop persist.vendor.ota.status updateResult
		return
elif [ $status == manualControl ]
	then
		return
elif [ $status == updateResult ]
	then
		am broadcast -a com.micronet.ota.status --es status "Uploading results, $status, wait 7min!" --ez chk false --ez dnl false --ez inst false > /dev/null
		sleep 7m
elif [ $status == manualUpdateResult ]
	then
		am broadcast -a com.micronet.ota.status --es status "Uploading results, $status, wait 7min!" --ez chk false --ez dnl false --ez inst false > /dev/null
		sleep 7m
fi

writeLog "get current version"
export deviceVersion=$(getDeviceVersion)
writeLog "got version $deviceVersion"
while true
	do
		export imeiNum=$( service call iphonesubinfo 1 | cut -c 52-66 | tr -d '.[:space:]' )
		if [[ ${%imeiNum} -eq 15 ]] && [[ $imeiNum == 35540978* ]]
			then
				echo $imeiNum > /sdcard/Download/imei.txt
				break
			else
				if [[ -f  /sdcard/Download/imei.txt ]] && [ $( stat -c%s /sdcard/Download/imei.txt ) == 16 ]
					then
						imeiNum=$(cat /sdcard/Download/imei.txt)
						break
					else
						sleep 10s
				fi
		fi
	done
echo $imeiNum > /sdcard/Download/imeiNum.txt


if [ $( stat -c%s /sdcard/Download/updater.log ) -gt 1000000 ]
	then
		rm /sdcard/Download/updater.log
		writeLog "Reached max size, clearing log"
fi
writeLog "start querying for manifest"

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

am broadcast -a com.micronet.ota.status --es status "start querying for manifest" --ez chk false --ez dnl false --ez inst false > /dev/null
if [ $status == manualInit ]
	then
		doUpdate
	else
		while true
			do
				exitCode=$(doUpdate)
				if [ $exitCode == 1 ]
					then
						break
				fi
				sleep $interval
			done			
fi