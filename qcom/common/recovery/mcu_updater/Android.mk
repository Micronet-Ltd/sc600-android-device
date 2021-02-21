# Copyright 2009 The Android Open Source Project

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := mcu_update.cpp flash.cpp partitions.cpp fastbootd.cpp utils.cpp virtual_partitions.cpp
LOCAL_MODULE := mcu_ua
LOCAL_C_INCLUDES += external/zlib/ \
		    bootable/recovery
LOCAL_STATIC_LIBRARIES := libz
LOCAL_STATIC_LIBRARIES += libcutils libstdc++ libc

LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
