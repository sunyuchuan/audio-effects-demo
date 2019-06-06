LOCAL_PATH := $(call my-dir)

MY_APP_JNI_ROOT := $(realpath $(LOCAL_PATH))
MY_LIB_PATH := $(MY_APP_JNI_ROOT)/effect-android-lib/$(TARGET_ARCH_ABI)
MY_INC_PATH := $(MY_APP_JNI_ROOT)/effect-android-lib/include

# libffmpeg
include $(CLEAR_VARS)
LOCAL_MODULE := ijkffmpeg-$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := $(MY_LIB_PATH)/libijkffmpeg-$(TARGET_ARCH_ABI).so
include $(PREBUILT_SHARED_LIBRARY)

# libeffect
include $(CLEAR_VARS)
LOCAL_MODULE := effect
LOCAL_SRC_FILES := $(MY_LIB_PATH)/lib/libeffect.a
include $(PREBUILT_STATIC_LIBRARY)

# audio-effect-jni
include $(CLEAR_VARS)
LOCAL_CFLAGS += -std=c99
LOCAL_CXXFLAGS += -std=c++11
LOCAL_LDLIBS += -llog -lstdc++

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(MY_INC_PATH)

LOCAL_SRC_FILES := JniAudioEffect.cpp \

LOCAL_SHARED_LIBRARIES := ijkffmpeg-$(TARGET_ARCH_ABI)
LOCAL_STATIC_LIBRARIES := effect

LOCAL_MODULE := audio-effect-$(TARGET_ARCH_ABI)
include $(BUILD_SHARED_LIBRARY)