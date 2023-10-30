#!/bin/sh
#runs on property persist.vendor.ota.status = download

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

getDeviceVersion()
{
echo $(getprop ro.build.display.id | cut -d '.' -f 4- | cut -d ' ' -f 1)
}

testForUpdateFiles()
{
rm -f /cache/result
sdcardName=$(ls /storage/ | grep -)
sdcardPath=/storage/$sdcardName
if [[ -f /cache/msm8953_64-incremental-ota.zip ]]
then
return
fi

if [[ -n $sdcardName ]]
	then
		if [[ -f $sdcardPath/msm8953_64-incremental-ota.zip ]]
		then
		return
		fi
		
		if [[ -f $sdcardPath/Android/data/com.redbend.client/files/msm8953_64-ota-eng.micronet.zip ]]
		then
		return
		fi
fi
echo 1
return
}

downloadPackage()
{
	am broadcast -a com.micronet.ota.status --es status "Please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
	if [[ $updateType -eq 2 ]]
		then 
			specialCommand=$(sed -n 's/Command://p' /sdcard/Download/manifest.txt)
			packageName=$(sed -n 's/PackageNameDeltaSourceVersion://p' /sdcard/Download/manifest.txt)
			if [[ "$specialCommand" == Delete ]]
				then
					writeLog "Deleting $packageName"
					am broadcast -a com.micronet.ota.status --es status "Deleting $packageName, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
					pm uninstall $packageName
					exitCode=$?
					writeLog "Deleting $packageName result: $exitCode"
					if [[ $exitCode -eq 0 ]]
						then 
							am broadcast -a com.micronet.ota.status --es status "Uninstall of $packageName completed, updating server..." --ez chk flase --ez dnl false --ez inst false  > /dev/null
							updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 1}"
							if [[ "$status" == "manualDownload" ]]
								then
									am broadcast -a com.micronet.ota.status --es status "Uninstall of $packageName completed" --ez chk true --ez dnl false --ez inst false  > /dev/null
								else
									setprop persist.vendor.ota.status init
							fi
						else
							am broadcast -a com.micronet.ota.status --es status "Uninstall of $packageName failed, Error code: $exitCode, updating Server..." --ez chk false --ez dnl false --ez inst false  > /dev/null
							updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Deletion code: $exitCode\"}"
							if [[ "$status" == "manualDownload" ]]
								then
									am broadcast -a com.micronet.ota.status --es status "Uninstall of $packageName failed, Error code: $exitCode" --ez chk true --ez dnl false --ez inst false  > /dev/null
								else
									sleep $interval
									setprop persist.vendor.ota.status init
						fi
					fi
					return
			fi
			writeLog "Update Type Application: $downloadLink"
			am broadcast -a com.micronet.ota.status --es status "Downloading App, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
			updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 3}"
			curl -f -k -o /sdcard/Download/downloadedApplication.apk $downloadLink
			exitCode=$?
			if [[ $exitCode -eq 0 ]]
				then
					updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 7}"
					writeLog "App Download Successful"
					calcChecksum=$(md5sum -b /sdcard/Download/downloadedApplication.apk)
					if [[ "$checksum" != "$calcChecksum" ]]
						then
							writeLog "Checksum error. Expected $checksum, got $calcChecksum"
							updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Checksum error. Expected $checksum, got $calcChecksum\"}"
							rm /sdcard/Download/downloadedApplication.apk
							if [ $status == manualDownload ]
								then
									am broadcast -a com.micronet.ota.status --es status "Checksum error. Expected $checksum, got $calcChecksum" --ez chk true --ez dnl false --ez inst false  > /dev/null
								else
									sleep $interval
									setprop persist.vendor.ota.status init
							fi
							return
					fi
					if [[ "$status" == "manualDownload" ]]
						then
							am broadcast -a com.micronet.ota.status --es status "Download completed, ready to install!" --ez chk false --ez dnl false --ez inst true  > /dev/null
						else
							setprop persist.vendor.ota.status copyApp
					fi
				else
					updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 2,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Download error code: $exitCode\"}"
					writeLog "App Download Code: $exitCode"
					if [[ "$status" == "manualDownload" ]]
						then
							am broadcast -a com.micronet.ota.status --es status "Download error code: $exitCode" --ez chk true --ez dnl flase --ez inst false > /dev/null
						else
							sleep $interval
							setprop persist.vendor.ota.status init
					fi
			fi
	elif [[ $updateType -eq 1 ]]
		then
			writeLog "Update Type OS"
			SourceVersion=$(sed -n 's/SourceVersion://p' /sdcard/Download/manifest.txt)
			if [[ -z $SourceVersion ]] || [[ "$SourceVersion" == "$deviceVersion" ]]
				then
					writeLog "Source And Device Versions Match, downloading..."
					SizeOfOtaPackage=$(sed -n 's/SizeOfOtaPackage://p' /sdcard/Download/manifest.txt)
					sizeNeeded=$(( $SizeOfOtaPackage / 512 ))
					cacheFreeSpace=$(df /cache | tail -1 | awk '{print $4}')
					sdcardName=$(ls /storage/ | grep -)
					sdcardPath=/storage/$sdcardName
					if [[ $cacheFreeSpace -lt $sizeNeeded ]]
						then
							if [[ -n $sdcardName ]]
								then
									sdFreeSpace=$(df /mnt/media_rw/$sdcardName | tail -1 | awk '{print $4}')
									if [[ $sdFreeSpace -lt $sizeNeeded ]]
										then
											writeLog "Not enough Space!"
											am broadcast -a com.micronet.ota.status --es status "Not enough Space on Device/SD-Card!" --ez chk true --ez dnl false --ez inst false > /dev/null
											updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Not enough space to Download!\"}"
											if [[ "$status" == "download" ]]
												then
													sleep $interval
													setprop persist.vendor.ota.status init
											fi
											return
										else
											am broadcast -a com.micronet.ota.status --es status "Downloading OS, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
											updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 3}"
											curl -f -k -o /$sdcardPath/temp.tar $downloadLink
											exitCode=$?
											writeLog "Downloading code - $exitCode"
											if [[ $exitCode -eq 0 ]]
												then
													calcChecksum=$(md5sum -b /$sdcardPath/temp.tar)
													if [[ "$checksum" != "$calcChecksum" ]]
														then
															writeLog "Checksum error. Expected $checksum, got $calcChecksum"
															updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Checksum error. Expected $checksum, got $calcChecksum\"}"
															rm /$sdcardPath/temp.tar
															if [[ "$status" == "manualDownload" ]]
																then
																	am broadcast -a com.micronet.ota.status --es status "Checksum error. Expected $checksum, got $calcChecksum" --ez chk true --ez dnl false --ez inst false  > /dev/null
																else
																	am broadcast -a com.micronet.ota.status --es status "Checksum error. Expected $checksum, got $calcChecksum" --ez chk true --ez dnl false --ez inst false  > /dev/null
																	sleep $interval
																	setprop persist.vendor.ota.status init
															fi
															return
													fi
													am broadcast -a com.micronet.ota.status --es status "Extracting, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
													updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 7}"
													tar -C /$sdcardPath/ -xvf /$sdcardPath/temp.tar
													exitCode=$?
													writeLog "tar code - $exitCode"
													if [[ $exitCode -eq 0 ]]
														then
															if [[ -f /$sdcardPath/msm8953_64-ota-eng.micronet.zip ]]
																then
																	mkdir -p /$sdcardPath/Android/data/com.redbend.client/files/
																	mv -f /$sdcardPath/msm8953_64-ota-eng.micronet.zip /$sdcardPath/Android/data/com.redbend.client/files/
															fi
															updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 8}"
															preRebootTest=$(testForUpdateFiles)
															if [[ $preRebootTest -eq 1 ]]
																then
																	updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"No Update Files Found!\"}"
																	cd /$sdcardPath
																	tar -t -f /$sdcardPath/temp.tar | while read -r p
																		do
																			rm $p
																		done
																	rm -f /$sdcardPath/temp.tar
																	if [[ "$status" == "manualDownload" ]]
																		then
																			am broadcast -a com.micronet.ota.status --es status "No update files found!" --ez chk true --ez dnl false --ez inst false > /dev/null
																		else
																			writeLog "No Update files Found!"
																			am broadcast -a com.micronet.ota.status --es status "No update files found!" --ez chk true --ez dnl false --ez inst flase > /dev/null
																			sleep $interval
																			setprop persist.vendor.ota.status init
																	fi
																	return
																else
																	rm -f /$sdcardPath/temp.tar
															fi
															if [[ "$status" == "manualDownload" ]]
																then
																	am broadcast -a com.micronet.ota.status --es status "Download completed, ready to install!" --ez chk false --ez dnl false --ez inst true > /dev/null
																else
																	writeLog "starting Update"
																	am broadcast -a com.micronet.ota.status --es status "Ready to install!" --ez chk false --ez dnl false --ez inst true > /dev/null
																	setprop persist.vendor.ota.status update
															fi
														else
															rm -f /$sdcardPath/temp.tar
															am broadcast -a com.micronet.ota.status --es status "TAR error code: $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
															updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"TAR error code $exitCode\"}"
															if [[ "$status" == "download" ]]
																then
																	sleep $interval
																	setprop persist.vendor.ota.status init
															fi
													fi
												else
													am broadcast -a com.micronet.ota.status --es status "Dowanload error code: $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
													updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Download error code $exitCode\"}"
													if [[ "$status" == "download" ]]
														then
															sleep $interval
															setprop persist.vendor.ota.status init
													fi
											fi
									fi
								else
									writeLog "Not enough Space on Device and no SD-Card!"
									am broadcast -a com.micronet.ota.status --es status "Not enough Space on Device and no SD-Card!" --ez chk true --ez dnl false --ez inst false > /dev/null
									updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Not enough Space on Device and no SD-Card!\"}"
									if [[ "$status" == "download" ]]
										then
											sleep $interval
											setprop persist.vendor.ota.status init
									fi
							fi
						else
							updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 3}"
							am broadcast -a com.micronet.ota.status --es status "Downloading OS, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
							curl -f -k -o /cache/temp.tar $downloadLink
							exitCode=$?
							writeLog "Downloading code - $exitCode"
							if [[ $exitCode -eq 0 ]]
								then
									calcChecksum=$(md5sum -b /cache/temp.tar)
									if [[ "$checksum" != "$calcChecksum" ]]
										then
											writeLog "Checksum error. Expected $checksum, got $calcChecksum"
											updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Checksum error. Expected $checksum, got $calcChecksum\"}"
											rm /cache/temp.tar
											if [[ "$status" == "manualDownload" ]]
												then
													am broadcast -a com.micronet.ota.status --es status "Checksum error. Expected $checksum, got $calcChecksum" --ez chk true --ez dnl false --ez inst false  > /dev/null
												else
													am broadcast -a com.micronet.ota.status --es status "Checksum error. Expected $checksum, got $calcChecksum" --ez chk true --ez dnl false --ez inst false  > /dev/null
													sleep $interval
													setprop persist.vendor.ota.status init
											fi
											return
									fi
									updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 7}"
									am broadcast -a com.micronet.ota.status --es status "Extracting, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
									tar -C /cache/ -xvf /cache/temp.tar
									exitCode=$?
									writeLog "tar code - $exitCode"
									if [[ $exitCode -eq 0 ]]
										then
											updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 8}"
											preRebootTest=$(testForUpdateFiles)
											if [[ $preRebootTest -eq 1 ]]
												then
													updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"No Update Files Found!\"}"
													cd /cache
													tar -t -f /cache/temp.tar | while read -r p
														do
															rm $p
														done
													rm -f /cache/temp.tar
													if [[ "$status" == "manualDownload" ]]
														then
															am broadcast -a com.micronet.ota.status --es status "No update files found!" --ez chk true --ez dnl false --ez inst false > /dev/null
														else
															writeLog "No Update files Found!"
															am broadcast -a com.micronet.ota.status --es status "No update files found!" --ez chk true --ez dnl false --ez inst false > /dev/null
															sleep $interval
															setprop persist.vendor.ota.status init
													fi
													return
												else
													rm -f /cache/temp.tar
											fi
											if [[ "$status" == "manualDownload" ]]
												then
													am broadcast -a com.micronet.ota.status --es status "Download completed, ready to install!" --ez chk false --ez dnl false --ez inst true > /dev/null
												else
													writeLog "starting Update"
													am broadcast -a com.micronet.ota.status --es status "Ready to install!" --ez chk false --ez dnl false --ez inst true > /dev/null
													setprop persist.vendor.ota.status update
											fi
										else
											rm -f /cache/temp.tar
											am broadcast -a com.micronet.ota.status --es status "TAR error code: $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
											updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"TAR error code $exitCode\"}"
											if [[ "$status" == "download" ]]
												then
													sleep $interval
													setprop persist.vendor.ota.status init
											fi
									fi
								else
									am broadcast -a com.micronet.ota.status --es status "Dowanload error code: $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
									updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 1,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Download error code $exitCode\"}"
									if [[ "$status" == "download" ]]
										then
											sleep $interval
											setprop persist.vendor.ota.status init
									fi
							fi
					fi
				else
					writeLog "Source And Device Versions Mismatch: $SourceVersion and $deviceVersion"
					am broadcast -a com.micronet.ota.status --es status "Source And Device Versions Mismatch: $SourceVersion and $deviceVersion" --ez chk true --ez dnl false --ez inst false > /dev/null
					if [[ "$status" == "download" ]]
						then
							sleep $interval
							setprop persist.vendor.ota.status init
					fi
			fi
	elif [[ $updateType -eq 3 ]]
		then
			#save data
			specialCommand=$(sed -n 's/Command://p' /sdcard/Download/manifest.txt)
			writeLog "Save Data from $downloadLink"
			writeLog "Save Data to $specialCommand"
			updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 3}"
			am broadcast -a com.micronet.ota.status --es status "Downloading Data, please wait..." --ez chk false --ez dnl false --ez inst false > /dev/null
			curl -v -f -k -o /sdcard/Download/data.tar $downloadLink
			exitCode=$?
			writeLog "Download Data: $exitCode"
			if [[ $exitCode -eq 0 ]]
				then
					calcChecksum=$(md5sum -b /sdcard/Download/data.tar)
					if [[ "$checksum" != "$calcChecksum" ]]
						then
							writeLog "Checksum error. Expected $checksum, got $calcChecksum"
							updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Checksum error. Expected $checksum, got $calcChecksum\"}"
							rm /cache/temp.tar
							if [[ "$status" == "manualDownload" ]]
								then
									am broadcast -a com.micronet.ota.status --es status "Checksum error. Expected $checksum, got $calcChecksum" --ez chk true --ez dnl false --ez inst false  > /dev/null
								else
									sleep $interval
									setprop persist.vendor.ota.status init
							fi
							return
					fi
					updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 7}"
					am broadcast -a com.micronet.ota.status --es status "Moving file..."
					cd /
					mkdir -p $specialCommand
					mv /sdcard/Download/data.tar $specialCommand
					exitCode=$?
					writeLog "Move Data: $exitCode"
					if [[ $exitCode -eq 0 ]]
						then
							tar -C $specialCommand -xvf $specialCommand/data.tar
							exitCode=$?
							writeLog "TAR exit code: $exitCode"
							rm $specialCommand/data.tar
							if [[ $exitCode -eq 0 ]]
								then
									am broadcast -a com.micronet.ota.status --es status "Downloading and moving Successful, updating Server..." --ez chk false --ez dnl false --ez inst false > /dev/null
									updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 1}"
									am broadcast -a com.micronet.ota.status --es status "Downloading and moving Successful." --ez chk true --ez dnl false --ez inst false > /dev/null
								else
									am broadcast -a com.micronet.ota.status --es status "TAR error: $exitCode, updating Server..." --ez chk false --ez dnl false --ez inst false > /dev/null
									updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"TAR error code $exitCode\"}"
									am broadcast -a com.micronet.ota.status --es status "TAR error: $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
									sleep $interval
							fi
							if [[ "$status" == "download" ]]
								then
									setprop persist.vendor.ota.status init
							fi
						else
							am broadcast -a com.micronet.ota.status --es status "Move error code: $exitCode, updating Server..." --ez chk false --ez dnl false --ez inst false > /dev/null
							updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Move error code $exitCode\"}"
							am broadcast -a com.micronet.ota.status --es status "Move error code: $exitCode." --ez chk true --ez dnl false --ez inst false > /dev/null
							if [[ "$status" == "download" ]]
								then
									sleep $interval
									setprop persist.vendor.ota.status init
							fi
					fi
				else
					am broadcast -a com.micronet.ota.status --es status "Download error code: $exitCode, updating Server..." --ez chk false --ez dnl false --ez inst false > /dev/null
					updateStatus "{\"deviceId\": \"$imeiNum\",\"campaignId\": $CampaignID,\"campaignType\": 3,\"state\": 4, \"errorCode\": 1,\"errorDesc\": \"Download error code $exitCode\"}"
					am broadcast -a com.micronet.ota.status --es status "Download error code: $exitCode" --ez chk true --ez dnl false --ez inst false > /dev/null
					if [[ "$status" == "download" ]]
						then
							sleep $interval
							setprop persist.vendor.ota.status init
					fi
			fi
	else
			writeLog "Nothing to do."
			am broadcast -a com.micronet.ota.status --es status "Nothing to do." --ez chk true --ez dnl false --ez inst false > /dev/null
	fi
}

# script starts from here
downloadLink=$(sed -n 's/UrlToOtaPackage://p' /sdcard/Download/manifest.txt)
updateType=$(sed -n 's/UpdateType://p' /sdcard/Download/manifest.txt)
checksum=$(sed -n 's/Checksum://p' /sdcard/Download/manifest.txt)
PreferredDownloadTime=$(sed -n 's/PreferredDownloadTime://p' /sdcard/Download/manifest.txt) #format: mmddhhm
CampaignID=$(sed -n 's/CampaignID://p' /sdcard/Download/manifest.txt)
imeiNum=$(cat /sdcard/Download/imei.txt)
deviceVersion=$(getDeviceVersion)
status=$(getprop persist.vendor.ota.status)

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

currentTime=$(date +%s)
waitingTime=$((PreferredDownloadTime - currentTime))

if [[ "$status" == "manualDownload" ]]
	then
		downloadPackage
	else
		if [[ -z $PreferredDownloadTime ]]
			then
				downloadPackage
			else
				if [[ $waitingTime -gt 0 ]]
					then
						writeLog "waiting to download: $waitingTime seconds"
						sleep $waitingTime
						downloadPackage
					else
						downloadPackage
				fi
		fi
fi
