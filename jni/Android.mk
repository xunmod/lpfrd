LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := lpfrd
LOCAL_SRC_FILES := lpfrd.c
LOCAL_CFLAGS := -std=c99 -fstack-protector-strong # -fsanitize=address
LOCAL_LDFLAGS := -static # -fsanitize=address
# LOCAL_CFLAGS += -g
# LOCAL_STRIP_MODE := none
include $(BUILD_EXECUTABLE)
