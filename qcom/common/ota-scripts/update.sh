#!/bin/sh
#runs on property persist.vendor.ota.status = update

writeLog()
{
	echo $(date "+%Y-%m-%d %H:%M:%S") $1 >> /sdcard/Download/updater.log
	return
}

waitForIgnitionOff()
{
writeLog "Update waiting for Ignition off..."

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

nextPing=$(($(date +%s) + 1800))
imeiNum=$(cat /sdcard/Download/imei.txt)

while true
	do
		readState=$(cat /sys/class/switch_dock/dock/state)
		if [[ $readState -eq 11 ]] || [[ $readState -eq 7 ]] # ignition on
			then 
				sleep 10s
				difference=$(( nextPing - $(date +%s) ))
				if [[ $difference -lt 0 ]]
					then
						nextPing=$(($(date +%s) + 1800))
						curl -f -k -X 'GET' "https://$baseUrl/api/campaigns/for-device/$imeiNum" -H 'accept: application/json' -H "$token" -o /dev/null
				fi
			else
				doReboot
				break
		fi
	done
}

checkTime()
{
writeLog "Checking time"
PreferredUpdateTime=$(sed -n 's/PreferredUpdateTime://p' /sdcard/Download/manifest.txt)
currentTime=$(date +%s)
waitingTime=$((PreferredUpdateTime - currentTime))
if [[ -n $PreferredUpdateTime ]]
	then
		shouldWaitForIgnition=$(date -d @$PreferredUpdateTime +%H%M)
fi

if [[ -z $PreferredUpdateTime ]]
	then 
		writeLog "Update without target time"
		doReboot
	else
		if [[ "$shouldWaitForIgnition" == "0000" ]]
			then
				waitForIgnitionOff
				return
		elif [[ $waitingTime -gt 0 ]]
			then
				writeLog "Wait for time, $waitingTime seconds"
				#echo 'date '+%s' -d '+ $waitingTime minutes'' > /sys/class/rtc/rtc0/wakealarm
				sleep $waitingTime
				doReboot
				return
		else 
# 			if [  $waitingTime -lt -30 ] 
# 				then
# 					writeLog "over 30 min passed"
# 					# -2: update time is over more then for 30min
# 					#echo -2 > result
# 					#curl -k -f -F "files=@/sdcard/Download/updater.log" $CallBackUrl
# 					setprop persist.vendor.ota.status init
# 					#!!!! set time in manifest to next day
# 					#!!!!!find new waiting time
# 					#!!!!set wake to new waitingtime
# 					#!!!!sleep newWaitingTime
# 					#!!!!waitForIgnitionOff
# 				else 
				writeLog "Ready to install"
				doReboot
				return
				#fi
		fi
fi
}

doReboot()
{
	setprop persist.vendor.ota.status updateStarted
	writeLog "reboot..."
	sleep 5s
	reboot recovery
}

# start script
writeLog "Starting Update: Get status"
status=$(getprop persist.vendor.ota.status)
writeLog "Starting Update: $status"
if [[ "$status" == "manualUpdate" ]]
	then
		doReboot
		return
	else
		checkTime
		return
fi