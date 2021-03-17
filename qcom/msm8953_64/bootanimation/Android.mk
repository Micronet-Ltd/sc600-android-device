LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE        := bootanimation.zip
LOCAL_MODULE_CLASS  := ETC
LOCAL_MODULE_TAGS   := optional
LOCAL_SRC_FILES     := bootanimation.zip
LOCAL_MODULE_PATH   := $(PRODUCT_OUT)/system/media
include $(BUILD_PREBUILT)

