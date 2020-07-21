LOCAL_PATH := $(call my-dir)

MY_APP_JNI_ROOT := $(realpath $(LOCAL_PATH))
MY_LIB_PATH := $(MY_APP_JNI_ROOT)/lib-audio-effects/audio-effects-android-lib/$(TARGET_ARCH_ABI)
MY_INC_PATH := $(MY_APP_JNI_ROOT)/lib-audio-effects/audio-effects-android-lib/include

# libffmpeg
include $(CLEAR_VARS)
LOCAL_MODULE := ijkffmpeg-$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := $(MY_LIB_PATH)/libijkffmpeg-$(TARGET_ARCH_ABI).so
include $(PREBUILT_SHARED_LIBRARY)

# libaudio_effects
include $(CLEAR_VARS)
LOCAL_MODULE := audio_effects
LOCAL_SRC_FILES := $(MY_LIB_PATH)/libaudio_effects.a
include $(PREBUILT_STATIC_LIBRARY)

# libxmaudio_effects
include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -mfloat-abi=softfp
endif
LOCAL_CFLAGS += -std=c99
LOCAL_CFLAGS += -Wno-deprecated-declarations
LOCAL_LDLIBS += -llog -landroid

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(MY_INC_PATH)

LOCAL_SRC_FILES := lib-audio-effects/xm_audio_effects_jni.c

LOCAL_SHARED_LIBRARIES := ijkffmpeg-$(TARGET_ARCH_ABI)
LOCAL_STATIC_LIBRARIES := audio_effects

LOCAL_MODULE := xmaudio_effects-$(TARGET_ARCH_ABI)
include $(BUILD_SHARED_LIBRARY)