#! /bin/sh

error_exit()
{
	echo "Error: $*"
	exit 1
}

if [ -d bootanimation ];then
	cd bootanimation
	zip -r -0 bootanimation.zip ./ || error_exit "make bootanimation.zip"
	mv bootanimation.zip ..
	cd ..
fi

if [ -d shutdownanimation ];then
	cd shutdownanimation
	zip -r -0 shutdownanimation.zip ./ || error_exit "make shutdownanimation.zip"
	mv shutdownanimation.zip ..
	cd ..
fi


