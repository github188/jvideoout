LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := JVideoOut
LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_SRC_FILES := $(wildcard *.c opengl/*.c)
LOCAL_CFLAGS    := -Wall -std=gnu99
#LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv2
include $(BUILD_STATIC_LIBRARY)
