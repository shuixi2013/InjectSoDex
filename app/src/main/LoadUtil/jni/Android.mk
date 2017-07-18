LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := proxybinder.cpp
LOCAL_MODULE:= proxybinder

LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog

LOCAL_SHARED_LIBRARIES :=     \
        libcutils             \
        libutils              \
        libbinder             \
        libandroid_runtime    \
        liblog                \
	    libdl

#LOCAL_LDFLAGS := -shared


LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
