LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= iodriver.c control.c accel.c util.c tty.c queue.c frame.c j1708.c
LOCAL_MODULE := iodriver
LOCAL_MODULE_OWNER  := Micronet
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -std=c99 -DUSE_THREADS -D__ANDROID__
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libcutils libc liblog libutils
#LOCAL_SHARED_LIBRARIES := libcutils libc liblog libutils
LOCAL_C_INCLUDES += $(JNI_H_INCLUDE)
LOCAL_MODULE_PATH   := $(TARGET_ROOT_OUT_SBIN)
LOCAL_POST_INSTALL_CMD += ln -sf /sbin/iodriver $(TARGET_OUT)/bin/iodriver; 
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE        := recovery.iodriver
LOCAL_MODULE_OWNER  := Micronet
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := EXECUTABLES
LOCAL_FORCE_STATIC_EXECUTABLE := true
#LOCAL_SRC_FILES     := ../../../../$(TARGET_OUT_EXECUTABLES)/iodriver
LOCAL_SRC_FILES     := ../../../../$(TARGET_ROOT_OUT_SBIN)/iodriver
LOCAL_MODULE_PATH   := $(TARGET_RECOVERY_ROOT_OUT)/sbin
include $(BUILD_PREBUILT)

#$(shell pushd $(LOCAL_PATH); \
#	cd ../../../../$(TARGET_ROOT_OUT); \
#	ln -sf sbin/iodriver ../$(TARGET_COPY_OUT_SYSTEM)/bin/iodriver; \
#	popd)


