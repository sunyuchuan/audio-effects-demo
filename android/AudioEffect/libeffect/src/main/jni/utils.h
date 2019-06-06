#ifndef UTILS_H_
#define UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <android/log.h>
#include <jni.h>

#define TAG "System.out"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)

#define SUCCESS 0
#define INVALID_OBJECT -1
#define NO_FOUND_FIELD_OBJECT -2

inline void jniThrowException(JNIEnv* env, const char* className,
                              const char* msg) {
    jclass newExcCls = env->FindClass(className);
    if (!newExcCls) {
        LOGE("找不到异常类：%s", className);
        return;
    }
    if (env->ThrowNew(newExcCls, msg) != JNI_OK) {
        LOGE("无法抛出异常:%s 描述:%s", className, msg);
        return;
    }
}

inline void jniThrowIllegalArgumentException(JNIEnv* env, const char* msg) {
    jniThrowException(env, "java/lang/IllegalArgumentException", msg);
}
#ifdef __cplusplus
}
#endif

#endif  // UTILS_H_
