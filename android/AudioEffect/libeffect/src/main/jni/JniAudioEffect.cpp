#include "effect-android-lib/include/log.h"
#include "effect-android-lib/include/xmly_audio_effects.h"
#include "utils.h"

static struct aecm_offsets_t { jfieldID mObject; } gEffectOffsets;

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    register
 * Signature: ()I
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_layne_libeffect_AudioEffect_register(JNIEnv *env, jclass type) {
    gEffectOffsets.mObject = env->GetFieldID(type, "mObject", "J");
    if (NULL == gEffectOffsets.mObject) {
        LOGE("失败，找不到mObject成员ID");
        return NO_FOUND_FIELD_OBJECT;
    }
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_ANDROID);
    LOGD("注册AudioEffect成功.");
    return SUCCESS;
}

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    getNativeBean
 * Signature: ()J
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_layne_libeffect_AudioEffect_getNativeBean(JNIEnv *env, jclass type) {
    XmlyEffectContext *effect_ctx = create_xmly_effect();
    return (jlong)effect_ctx;
}

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    releaseNativeBean
 * Signature: ()V
 */
extern "C" JNIEXPORT void JNICALL
Java_com_layne_libeffect_AudioEffect_releaseNativeBean(JNIEnv *env,
                                                       jobject instance) {
    XmlyEffectContext *effect_ctx = (XmlyEffectContext *)env->GetLongField(
        instance, gEffectOffsets.mObject);
    if (NULL == effect_ctx) {
        LOGW("Invalid mObject Offsets. or may be died.");
        return;
    }
    free_xmly_effect(&effect_ctx);
    env->SetLongField(instance, gEffectOffsets.mObject, -1);
}

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    init_xmly_effect
 * Signature: ()I
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_layne_libeffect_AudioEffect_init_1xmly_1effect(JNIEnv *env,
                                                        jobject instance) {
    XmlyEffectContext *effect_ctx = (XmlyEffectContext *)env->GetLongField(
        instance, gEffectOffsets.mObject);
    if (NULL == effect_ctx) {
        jniThrowIllegalArgumentException(
            env, "Invalid mObject Offsets. or may be died.");
        LOGE("Invalid mObject Offsets. or may be died.");
        return INVALID_OBJECT;
    }

    return init_xmly_effect(effect_ctx);
}

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    set_xmly_effect
 * Signature: (Ljava/lang/String;Ljava/lang/String;I)I
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_layne_libeffect_AudioEffect_set_1xmly_1effect(
    JNIEnv *env, jobject instance, jstring key, jstring value, jint flags) {
    XmlyEffectContext *effect_ctx = (XmlyEffectContext *)env->GetLongField(
        instance, gEffectOffsets.mObject);
    if (NULL == effect_ctx) {
        jniThrowIllegalArgumentException(
            env, "Invalid mObject Offsets. or may be died.");
        LOGE("Invalid mObject Offsets. or may be died.");
        return INVALID_OBJECT;
    }

    const char *key_ = env->GetStringUTFChars(key, 0);
    const char *value_ = env->GetStringUTFChars(value, 0);
    int ret = set_xmly_effect(effect_ctx, key_, value_, flags);

    env->ReleaseStringUTFChars(key, key_);
    env->ReleaseStringUTFChars(value, value_);
    return 0;
}

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    xmly_send_samples
 * Signature: ([SI)I
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_layne_libeffect_AudioEffect_xmly_1send_1samples(JNIEnv *env,
                                                         jobject instance,
                                                         jshortArray samples,
                                                         jint nb_samples) {
    XmlyEffectContext *effect_ctx = (XmlyEffectContext *)env->GetLongField(
        instance, gEffectOffsets.mObject);
    if (NULL == effect_ctx) {
        jniThrowIllegalArgumentException(
            env, "Invalid mObject Offsets. or may be died.");
        LOGE("Invalid mObject Offsets. or may be died.");
        return INVALID_OBJECT;
    }

    jshort *samples_ = env->GetShortArrayElements(samples, NULL);
    int ret = xmly_send_samples(effect_ctx, samples_, nb_samples);
    env->ReleaseShortArrayElements(samples, samples_, 0);
    return ret;
}

/*
 * Class:     com_layne_libeffect_AudioEffect
 * Method:    xmly_receive_samples
 * Signature: ([SI)I
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_layne_libeffect_AudioEffect_xmly_1receive_1samples(
    JNIEnv *env, jobject instance, jshortArray samples, jint max_nb_samples) {
    XmlyEffectContext *effect_ctx = (XmlyEffectContext *)env->GetLongField(
        instance, gEffectOffsets.mObject);
    if (NULL == effect_ctx) {
        jniThrowIllegalArgumentException(
            env, "Invalid mObject Offsets. or may be died.");
        LOGE("Invalid mObject Offsets. or may be died.");
        return INVALID_OBJECT;
    }

    jshort *samples_ = env->GetShortArrayElements(samples, NULL);
    int ret = xmly_receive_samples(effect_ctx, samples_, max_nb_samples);
    env->ReleaseShortArrayElements(samples, samples_, 0);
    return ret;
}
