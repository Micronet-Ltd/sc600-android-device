ALLOW_MISSING_DEPENDENCIES=true
#TARGET_HAS_LOW_RAM := true
# Enable AVB 2.0
ifneq ($(wildcard kernel/msm-4.9),)
#BOARD_AVB_ENABLE ?= false
BOARD_AVB_ENABLE := true
# Enable chain partition for system, to facilitate system-only OTA in Treble.
BOARD_AVB_SYSTEM_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_SYSTEM_ALGORITHM := SHA256_RSA2048
BOARD_AVB_SYSTEM_ROLLBACK_INDEX := 0
BOARD_AVB_SYSTEM_ROLLBACK_INDEX_LOCATION := 2
endif

PRODUCT_PROPERTY_OVERRIDES += persist.vendor.bluetooth.modem_nv_support=true

TARGET_USES_AOSP := false
TARGET_USES_AOSP_FOR_AUDIO := false
TARGET_USES_QCOM_BSP := false

ifeq ($(TARGET_USES_AOSP),true)
TARGET_DISABLE_DASH := true
endif

DEVICE_PACKAGE_OVERLAYS := device/qcom/msm8953_64/overlay

TARGET_USES_NQ_NFC := false

ifeq ($(TARGET_USES_NQ_NFC),false)
NXP_NFC_HOST := $(TARGET_PRODUCT)
NXP_NFC_HW := pn7150
NXP_NFC_PLATFORM := pn54x
NXP_VENDOR_DIR := nxp
# These are the hardware-specific features
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:system/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:system/etc/permissions/android.hardware.nfc.hcef.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml

# NFC config files
PRODUCT_COPY_FILES += \
    vendor/$(NXP_VENDOR_DIR)/nfc/hw/$(NXP_NFC_HW)/libnfc-nci.conf:vendor/etc/libnfc-nci.conf \
    vendor/$(NXP_VENDOR_DIR)/nfc/hw/$(NXP_NFC_HW)/libnfc-nxp.conf:vendor/etc/libnfc-nxp.conf \
# NFC packages
PRODUCT_PACKAGES += \
    libnfc-nci \
    NfcNci \
    android.hardware.nfc@1.0-impl \
    libpn548ad_fw.so \
    nfc_nci.$(NXP_NFC_PLATFORM)

PRODUCT_PACKAGES += \
    android.hardware.nfc@1.1-service

ifeq ($(ENABLE_TREBLE), true)
PRODUCT_PACKAGES += \
	vendor.nxp.nxpnfc@1.0-impl \
	vendor.nxp.nxpnfc@1.0-service
endif

PRODUCT_PROPERTY_OVERRIDES += \
	ro.hardware.nfc_nci=$(NXP_NFC_PLATFORM)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.product.first_api_level=23
endif

ifneq ($(wildcard kernel/msm-3.18),)
    TARGET_KERNEL_VERSION := 3.18
    $(warning "Build with 3.18 kernel.")
else ifneq ($(wildcard kernel/msm-4.9),)
    TARGET_KERNEL_VERSION := 4.9
    $(warning "Build with 4.9 kernel")
else
    $(warning "Unknown kernel")
endif

TARGET_ENABLE_QC_AV_ENHANCEMENTS := true

# Default vendor configuration.
ifeq ($(ENABLE_VENDOR_IMAGE),)
ENABLE_VENDOR_IMAGE := true
endif

# Disable QTIC until it's brought up in split system/vendor
# configuration to avoid compilation breakage.
ifeq ($(ENABLE_VENDOR_IMAGE), true)
#TARGET_USES_QTIC := false
endif

# Default A/B configuration.
ENABLE_AB ?= false

# Enable features in video HAL that can compile only on this platform
TARGET_USES_MEDIA_EXTENSIONS := true

-include $(QCPATH)/common/config/qtic-config.mk

PRODUCT_COPY_FILES += vendor/verizon/dmacc-init.txt:$(TARGET_COPY_OUT_VENDOR)/verizon/dmclient/init/dmacc-init.txt
# media_profiles and media_codecs xmls for msm8953
ifeq ($(TARGET_ENABLE_QC_AV_ENHANCEMENTS), true)
PRODUCT_COPY_FILES += \
    device/qcom/msm8953_32/media/media_profiles_8953.xml:system/etc/media_profiles.xml \
    device/qcom/msm8953_32/media/media_profiles_8953.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_vendor.xml \
    device/qcom/msm8953_32/media/media_codecs.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs.xml \
    device/qcom/msm8953_32/media/media_codecs_vendor.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_vendor.xml \
    device/qcom/msm8953_32/media/media_codecs_8953.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_8953.xml \
    device/qcom/msm8953_32/media/media_codecs_performance_8953.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance.xml \
    device/qcom/msm8953_32/media/media_codecs_performance_8953.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance_8953.xml \
    device/qcom/msm8953_32/media/media_profiles_8953_v1.xml:system/etc/media_profiles_8953_v1.xml \
    device/qcom/msm8953_32/media/media_profiles_8953_v1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_8953_v1.xml \
    device/qcom/msm8953_32/media/media_codecs_8953_v1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_8953_v1.xml \
    device/qcom/msm8953_32/media/media_codecs_performance_8953_v1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance_8953_v1.xml \
    device/qcom/msm8953_32/media/media_codecs_vendor_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_vendor_audio.xml
endif
# video seccomp policy files
# copy to system/vendor as well (since some devices may symlink to system/vendor and not create an actual partition for vendor)
PRODUCT_COPY_FILES += \
    device/qcom/msm8953_32/seccomp/mediacodec-seccomp.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy \
    device/qcom/msm8953_32/seccomp/mediaextractor-seccomp.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediaextractor.policy

PRODUCT_PROPERTY_OVERRIDES += \
           dalvik.vm.heapminfree=4m \
           dalvik.vm.heapstartsize=16m \
           vendor.vidc.disable.split.mode=1
PRODUCT_PROPERTY_OVERRIDES += persist.vendor.ota.status=init

$(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)
$(call inherit-product, device/qcom/common/common64.mk)

# set media volume to default 70%
PRODUCT_PROPERTY_OVERRIDES += \
	ro.config.media_vol_default=10
	
PRODUCT_NAME := msm8953_64
PRODUCT_DEVICE := msm8953_64
PRODUCT_BRAND := TREQ
#PRODUCT_MODEL := msm8953 for arm64
#PRODUCT_VARIANT := $(shell echo $${PRODUCT_VARIANT})
PRODUCT_VARIANT := smartcam
RODUCT_EXT_APK  := $(shell echo $${PRODUCT_EXT_APK})
PRODUCT_RB_OTA	:= $(shell echo $${PRODUCT_RB_OTA})
PRODUCT_BOARD_V	:= $(shell echo $${PRODUCT_BOARD_VARIANT})
ifeq ($(PRODUCT_VARIANT),smartcam)
PRODUCT_MODEL := MSCAM
DEVICE_NAME   := MSCAM
ifeq ($(PRODUCT_EXT_APK),lm)
ifeq ($(PRODUCT_RB_OTA), enabled)
PRODUCT_VER    := 11.1.7.01
else
PRODUCT_VER    := 31.1.7.01
endif
PRODUCT_EXT_APK := lm
else
ifeq ($(PRODUCT_RB_OTA), enabled)
PRODUCT_VER    := 10.1.7.01
else
PRODUCT_VER    := 30.1.7.01
endif
PRODUCT_EXT_APK :=
endif
PRODUCT_VARIANT := smartcam
ifeq ($(TARGET_BUILD_VARIANT),user)
ifeq ($(PRODUCT_BOARD_V), sb)
    KERNEL_DEFCONFIG := msm8953-sb-perf_defconfig
else
    KERNEL_DEFCONFIG := msm8953-perf_defconfig
endif
else
ifeq ($(PRODUCT_BOARD_V), sb)
    KERNEL_DEFCONFIG := msm8953-sb_defconfig
else
    KERNEL_DEFCONFIG := msm8953_defconfig
endif
endif
x := $(shell echo "Config selected:" ${KERNEL_DEFCONFIG})
$(warning $x)

PRODUCT_GMS_COMMON ?= false
else
PRODUCT_MODEL := SmarTab-8
DEVICE_NAME   := SmarTab-8
ifeq ($(TARGET_BUILD_VARIANT),user)
ifeq ($(PRODUCT_RB_OTA), enabled)
PRODUCT_VER    := 01.1.7.01
else
PRODUCT_VER    := 21.1.7.01
endif
PRODUCT_GMS_COMMON := true
DISPLAY_BUILD_NUMBER := true
else
ifeq ($(PRODUCT_RB_OTA), enabled)
PRODUCT_VER    := 00.1.7.01
else
PRODUCT_VER    := 20.1.7.01
endif
PRODUCT_GMS_COMMON ?= false
endif
endif
BUILD_DT       := $(shell date +%s)
PRODUCT_DT     := date -d @$(BUILD_DT)
BUILD_NUMBER   := $(shell echo $${USER:0:8}).$(PRODUCT_MODEL)_$(PRODUCT_VER)_$(shell $(PRODUCT_DT) +%Y%m%d.%H%M)

#add by zzj for GMS
ifeq ($(PRODUCT_GMS_COMMON),true)
$(warning "Building GMS version.")
$(call inherit-product, vendor/google/products/gms.mk )
PRODUCT_PROPERTY_OVERRIDES += \
	ro.com.google.clientidbase=android-uniscope


#add by zzj for GMS
endif


PRODUCT_BOOT_JARS += tcmiface

# Disable Vulkan feature level 1
TARGET_NOT_SUPPORT_VULKAN_FEATURE_LEVEL_1 := true

# Kernel modules install path
KERNEL_MODULES_INSTALL := dlkm
KERNEL_MODULES_OUT := out/target/product/$(PRODUCT_NAME)/$(KERNEL_MODULES_INSTALL)/lib/modules

ifneq ($(strip $(QCPATH)),)
PRODUCT_BOOT_JARS += WfdCommon
#PRODUCT_BOOT_JARS += com.qti.dpmframework
#PRODUCT_BOOT_JARS += dpmapi
#PRODUCT_BOOT_JARS += com.qti.location.sdk
#Android oem shutdown hook
PRODUCT_BOOT_JARS += oem-services
endif

DEVICE_MANIFEST_FILE := device/qcom/msm8953_64/manifest.xml
DEVICE_MATRIX_FILE   := device/qcom/common/compatibility_matrix.xml
DEVICE_FRAMEWORK_MANIFEST_FILE := device/qcom/msm8953_64/framework_manifest.xml
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := \
    device/qcom/common/vendor_framework_compatibility_matrix.xml
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
    device/qcom/msm8953_64/vendor_framework_compatibility_matrix.xml

# default is nosdcard, S/W button enabled in resource
PRODUCT_CHARACTERISTICS := nosdcard

# When can normal compile this module,  need module owner enable below commands
# font rendering engine feature switch
#-include $(QCPATH)/common/config/rendering-engine.mk
#ifneq (,$(strip $(wildcard $(PRODUCT_RENDERING_ENGINE_REVLIB))))
#    MULTI_LANG_ENGINE := REVERIE
#    MULTI_LANG_ZAWGYI := REVERIE
#endif

ifneq ($(TARGET_DISABLE_DASH), true)
#    PRODUCT_BOOT_JARS += qcmediaplayer
endif

#
# system prop for opengles version
#
# 196608 is decimal for 0x30000 to report major/minor versions as 3/0
# 196609 is decimal for 0x30001 to report major/minor versions as 3/1
# 196610 is decimal for 0x30002 to report major/minor versions as 3/2
PRODUCT_PROPERTY_OVERRIDES += \
     ro.opengles.version=196610

#FEATURE_OPENGLES_EXTENSION_PACK support string config file
PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml
#Android EGL implementation
PRODUCT_PACKAGES += libGLES_android

$(call inherit-product-if-exists, vendor/verizon/dmclient/dmclient.mk)
PRODUCT_PACKAGES += \
VerizonDemo \
dmclient \
paltest \
devdetail.test \
connmo.test \
devinfo.test \
diagmon.test \
palnettest \
paladmintest \
palbatterytest \
palbuttonupdatetest\
libcurl \
curl \
libmoconnmo \
libmodcmo \
libmodevdetail \
libmodevinfo \
libmodiagmon \
libmodmacc \
libmofumo \
libmoscm \
libpal \
DMClientUpdate \
DMBrowser

# Audio configuration file
-include $(TOPDIR)hardware/qcom/audio/configs/msm8953/msm8953.mk

#Audio DLKM
ifeq ($(TARGET_KERNEL_VERSION), 4.9)
AUDIO_DLKM := audio_apr.ko
AUDIO_DLKM += audio_q6_notifier.ko
AUDIO_DLKM += audio_adsp_loader.ko
AUDIO_DLKM += audio_q6.ko
AUDIO_DLKM += audio_usf.ko
AUDIO_DLKM += audio_pinctrl_wcd.ko
AUDIO_DLKM += audio_swr.ko
AUDIO_DLKM += audio_wcd_core.ko
AUDIO_DLKM += audio_swr_ctrl.ko
AUDIO_DLKM += audio_wsa881x.ko
AUDIO_DLKM += audio_wsa881x_analog.ko
AUDIO_DLKM += audio_platform.ko
AUDIO_DLKM += audio_cpe_lsm.ko
AUDIO_DLKM += audio_hdmi.ko
AUDIO_DLKM += audio_stub.ko
AUDIO_DLKM += audio_wcd9xxx.ko
AUDIO_DLKM += audio_mbhc.ko
AUDIO_DLKM += audio_wcd9335.ko
AUDIO_DLKM += audio_wcd_cpe.ko
AUDIO_DLKM += audio_digital_cdc.ko
AUDIO_DLKM += audio_analog_cdc.ko
AUDIO_DLKM += audio_native.ko
AUDIO_DLKM += audio_machine_sdm450.ko
AUDIO_DLKM += audio_machine_ext_sdm450.ko
AUDIO_DLKM += mpq-adapter.ko
AUDIO_DLKM += mpq-dmx-hw-plugin.ko
PRODUCT_PACKAGES += $(AUDIO_DLKM)
endif

# MIDI feature
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.midi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.midi.xml

PRODUCT_PACKAGES += android.hardware.media.omx@1.0-impl

#ANT+ stack
PRODUCT_PACKAGES += \
    AntHalService \
    libantradio \
    antradio_app

# Display/Graphics
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.mapper@2.0-impl \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service \
    android.hardware.configstore@1.0-service

# bluetooth
PRODUCT_PACKAGES += \
	android.hardware.bluetooth@1.0-service

PRODUCT_PACKAGES += wcnss_service

# FBE support
PRODUCT_COPY_FILES += \
	device/qcom/msm8953_64/init.qti.qseecomd.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.qti.qseecomd.sh

# VB xml
PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/android.software.verified_boot.xml:system/etc/permissions/android.software.verified_boot.xml

# MSM IRQ Balancer configuration file
PRODUCT_COPY_FILES += \
    device/qcom/msm8953_64/msm_irqbalance.conf:$(TARGET_COPY_OUT_VENDOR)/etc/msm_irqbalance.conf \
    device/qcom/msm8953_64/msm_irqbalance_little_big.conf:$(TARGET_COPY_OUT_VENDOR)/etc/msm_irqbalance_little_big.conf
#wlan driver
PRODUCT_COPY_FILES += \
    device/qcom/msm8953_64/WCNSS_qcom_cfg.ini:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/WCNSS_qcom_cfg.ini \
    device/qcom/msm8953_32/WCNSS_wlan_dictionary.dat:persist/WCNSS_wlan_dictionary.dat \
    device/qcom/msm8953_64/WCNSS_qcom_wlan_nv.bin:persist/WCNSS_qcom_wlan_nv.bin \
    device/qcom/msm8953_64/quec_bt_tag_36.txt:persist/quec_bt_tag_36.txt

PRODUCT_PACKAGES += \
    wpa_supplicant_overlay.conf \
    p2p_supplicant_overlay.conf

#for wlan
PRODUCT_PACKAGES += \
    wificond \
    wifilogd
# Feature definition files for msm8953
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepdetector.xml

PRODUCT_PACKAGES += telephony-ext
PRODUCT_BOOT_JARS += telephony-ext

# Defined the locales
PRODUCT_LOCALES += th_TH vi_VN tl_PH hi_IN ar_EG ru_RU tr_TR pt_BR bn_IN mr_IN ta_IN te_IN zh_HK \
        in_ID my_MM km_KH sw_KE uk_UA pl_PL sr_RS sl_SI fa_IR kn_IN ml_IN ur_IN gu_IN or_IN

# When can normal compile this module, need module owner enable below commands
# Add the overlay path
#PRODUCT_PACKAGE_OVERLAYS := $(QCPATH)/qrdplus/Extension/res \
#        $(QCPATH)/qrdplus/globalization/multi-language/res-overlay \
#        $(PRODUCT_PACKAGE_OVERLAYS)
#PRODUCT_PACKAGE_OVERLAYS := $(QCPATH)/qrdplus/Extension/res \
        $(PRODUCT_PACKAGE_OVERLAYS)

# Powerhint configuration file
PRODUCT_COPY_FILES += \
     device/qcom/msm8953_64/powerhint.xml:system/etc/powerhint.xml

#Healthd packages
PRODUCT_PACKAGES += android.hardware.health@2.0-impl \
                   android.hardware.health@2.0-service \
                   libhealthd.msm

PRODUCT_FULL_TREBLE_OVERRIDE := true

PRODUCT_VENDOR_MOVE_ENABLED := true

#for android_filesystem_config.h
PRODUCT_PACKAGES += \
    fs_config_files

# Sensor HAL conf file
 PRODUCT_COPY_FILES += \
     device/qcom/msm8953_64/sensors/hals.conf:$(TARGET_COPY_OUT_VENDOR)/etc/sensors/hals.conf

PRODUCT_COPY_FILES += vendor/verizon/dmclient/platform/cacert/ca-bundle.crt:/system/etc/security/cacerts/ca-bundle.crt \
		vendor/verizon/libomamo/scm/libs/libsqlite.so:/vendor/lib/drm/libsqlite.so \
		vendor/verizon/libomamo/scm/libs/libsqlite64.so:/vendor/lib64/drm/libsqlite.so \
		vendor/verizon/libomamo/scm/libs/libicuuc.so:/vendor/lib/libicuuc.so \
		vendor/verizon/libomamo/scm/libs/libicuuc64.so:/vendor/lib64/libicuuc.so \
		vendor/verizon/libomamo/scm/libs/libicui18n.so:/vendor/lib/libicui18n.so \
		vendor/verizon/libomamo/scm/libs/libicui18n64.so:/vendor/lib64/libicui18n.so 


# Vibrator
PRODUCT_PACKAGES += \
    android.hardware.vibrator@1.0-impl \
    android.hardware.vibrator@1.0-service

# Power
PRODUCT_PACKAGES += \
    android.hardware.power@1.0-service \
    android.hardware.power@1.0-impl

# Camera configuration file. Shared by passthrough/binderized camera HAL
PRODUCT_PACKAGES += camera.device@3.2-impl
PRODUCT_PACKAGES += camera.device@1.0-impl
PRODUCT_PACKAGES += android.hardware.camera.provider@2.4-impl
# Enable binderized camera HAL
PRODUCT_PACKAGES += android.hardware.camera.provider@2.4-service

PRODUCT_PACKAGES += \
    vendor.display.color@1.0-service \
    vendor.display.color@1.0-impl

PRODUCT_PACKAGES += \
    libandroid_net \
    libandroid_net_32

#Enable Lights Impl HAL Compilation
PRODUCT_PACKAGES += android.hardware.light@2.0-impl

#Thermal
PRODUCT_PACKAGES += android.hardware.thermal@1.0-impl \
                    android.hardware.thermal@1.0-service

TARGET_SUPPORT_SOTER := true

#set KMGK_USE_QTI_SERVICE to true to enable QTI KEYMASTER and GATEKEEPER HIDLs
ifeq ($(ENABLE_VENDOR_IMAGE), true)
KMGK_USE_QTI_SERVICE := true
endif

#Enable AOSP KEYMASTER and GATEKEEPER HIDLs
ifneq ($(KMGK_USE_QTI_SERVICE), true)
PRODUCT_PACKAGES += android.hardware.gatekeeper@1.0-impl \
                    android.hardware.gatekeeper@1.0-service \
                    android.hardware.keymaster@3.0-impl \
                    android.hardware.keymaster@3.0-service
endif

#Enable KEYMASTER 4.0 for Android P not for OTA's
ifeq ($(strip $(TARGET_KERNEL_VERSION)), 4.9)
    ENABLE_KM_4_0 := true
endif

ifeq ($(ENABLE_KM_4_0), true)
    DEVICE_MANIFEST_FILE += device/qcom/msm8953_64/keymaster.xml
else
    DEVICE_MANIFEST_FILE += device/qcom/msm8953_64/keymaster_ota.xml
endif

PRODUCT_PROPERTY_OVERRIDES += rild.libpath=/vendor/lib64/libril-qc-qmi-1.so
PRODUCT_PROPERTY_OVERRIDES += vendor.rild.libpath=/vendor/lib64/libril-qc-qmi-1.so

ifeq ($(ENABLE_AB),true)
#A/B related packages
PRODUCT_PACKAGES += update_engine \
                   update_engine_client \
                   update_verifier \
                   bootctrl.msm8953 \
                   brillo_update_payload \
                   android.hardware.boot@1.0-impl \
                   android.hardware.boot@1.0-service
#Boot control HAL test app
PRODUCT_PACKAGES_DEBUG += bootctl
endif

TARGET_MOUNT_POINTS_SYMLINKS := false

SDM660_DISABLE_MODULE = true
# When AVB 2.0 is enabled, dm-verity is enabled differently,
# below definitions are only required for AVB 1.0
ifeq ($(BOARD_AVB_ENABLE),false)
# dm-verity definitions
  PRODUCT_SUPPORTS_VERITY := true
endif

ifeq ($(strip $(TARGET_KERNEL_VERSION)), 4.9)
    # Enable vndk-sp Libraries
    PRODUCT_PACKAGES += vndk_package
    PRODUCT_COMPATIBLE_PROPERTY_OVERRIDE := true
    TARGET_USES_MKE2FS := true
    $(call inherit-product, build/make/target/product/product_launched_with_p.mk)
endif

ifeq ($(strip $(TARGET_KERNEL_VERSION)), 3.18)
    # Enable extra vendor libs
    ENABLE_EXTRA_VENDOR_LIBS := true
    PRODUCT_PACKAGES += vendor-extra-libs
endif

PRODUCT_PACKAGES += populate_board_id.sh
PRODUCT_PACKAGES += update.sh download.sh checkForUpdates.sh copyApp.sh updateResult.sh install.sh logCopy.sh get.sh
PRODUCT_PACKAGES += cansend candump cancalbt start_can.sh set_can_bitrate.sh set_can_mode.sh set_can_masks.sh set_can_filters.sh
ifneq ($(PRODUCT_BOARD_V), sb)
PRODUCT_PACKAGES += iodriver recovery.iodriver
endif

ifeq ($(PRODUCT_EXT_APK),lm)
PRODUCT_PACKAGES += lm.smartcam.androidapp libLMLibEncDec libLMLibJni liblocee liblocee-jni 
endif
#PRODUCT_PACKAGES += modemconfigapp

