//
// Created by sunyc on 20-07-17.
//
#include "log.h"
#include "xm_audio_effects.h"
#include "utils.h"
#include <assert.h>
#include <pthread.h>

#define JNI_CLASS_AUDIO_EFFECTS "com/xmly/audio_effects/XmAudioEffects"

typedef struct xm_audio_effects_fields_t {
    pthread_mutex_t mutex;
    jclass clazz;
    jfieldID field_mNativeXmAudioEffects;
} xm_audio_effects_fields_t;

static xm_audio_effects_fields_t g_clazz;
static JavaVM* g_jvm;

static jlong
jni_mNativeXMAudioEffects_get(JNIEnv *env, jobject thiz)
{
    return (*env)->GetLongField(env, thiz, g_clazz.field_mNativeXmAudioEffects);
}

static void
jni_mNativeXMAudioEffects_set(JNIEnv *env, jobject thiz, jlong value)
{
    (*env)->SetLongField(env, thiz, g_clazz.field_mNativeXmAudioEffects, value);
}

static XmEffectContext *
jni_get_xm_audio_effects(JNIEnv *env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XmEffectContext *ctx = (XmEffectContext *) (intptr_t) jni_mNativeXMAudioEffects_get(env, thiz);
    if (ctx) {
        xmae_inc_ref(ctx);
    }

    pthread_mutex_unlock(&g_clazz.mutex);
    return ctx;
}

static XmEffectContext *
jni_set_xm_audio_effects(JNIEnv *env, jobject thiz, XmEffectContext *ctx)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XmEffectContext *oldctx = (XmEffectContext *) (intptr_t) jni_mNativeXMAudioEffects_get(env, thiz);
    if (ctx) {
        xmae_inc_ref(ctx);
    }
    jni_mNativeXMAudioEffects_set(env, thiz, (intptr_t) ctx);

    pthread_mutex_unlock(&g_clazz.mutex);

    if (oldctx != NULL) {
        xmae_dec_ref_p(&oldctx);
    }

    return oldctx;
}

static void
XMAudioEffects_release(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);

    XmEffectContext *ctx = jni_get_xm_audio_effects(env, thiz);
    if(ctx == NULL) {
        LOGW("XMAudioEffects_release ctx is NULL\n");
        goto LABEL_RETURN;
    }

    jni_set_xm_audio_effects(env, thiz, NULL);
LABEL_RETURN:
    xmae_dec_ref_p(&ctx);
}

static int
XMAudioEffects_receiveSamples(JNIEnv *env, jobject thiz,
        jshortArray samples, jint max_nb_samples) {
    LOGI("%s\n", __func__);

    int ret = -1;
    XmEffectContext *ctx = jni_get_xm_audio_effects(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AEjni: receiveSamples: null ctx", LABEL_RETURN);

    jshort *samples_ = (*env)->GetShortArrayElements(env, samples, NULL);

    ret = xm_effect_receive_samples(ctx, samples_, max_nb_samples);

    if (samples_) (*env)->ReleaseShortArrayElements(env, samples, samples_, 0);
LABEL_RETURN:
    xmae_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioEffects_sendSamples(JNIEnv *env, jobject thiz,
        jshortArray samples, jint nb_samples) {
    LOGI("%s\n", __func__);

    int ret = -1;
    XmEffectContext *ctx = jni_get_xm_audio_effects(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AEjni: sendSamples: null ctx", LABEL_RETURN);

    jshort *samples_ = (*env)->GetShortArrayElements(env, samples, NULL);
    ret = xm_effect_send_samples(ctx, samples_, nb_samples);

    if (samples_) (*env)->ReleaseShortArrayElements(env, samples, samples_, 0);
LABEL_RETURN:
    xmae_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioEffects_setEffects(JNIEnv *env, jobject thiz,
        jstring key, jstring value, jint flags) {
    LOGI("%s\n", __func__);

    int ret = -1;
    XmEffectContext *ctx = jni_get_xm_audio_effects(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AEjni: setEffects: null ctx", LABEL_RETURN);

    const char *key_ = (*env)->GetStringUTFChars(env, key, 0);
    const char *value_ = (*env)->GetStringUTFChars(env, value, 0);

    ret = set_xm_effect(ctx, key_, value_, flags);

    if (key_) (*env)->ReleaseStringUTFChars(env, key, key_);
    if (value_) (*env)->ReleaseStringUTFChars(env, value, value_);
LABEL_RETURN:
    xmae_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioEffects_init(JNIEnv *env, jobject thiz,
        jint sampleRate, jint nbChannels) {
    LOGI("%s\n", __func__);

    int ret = -1;
    XmEffectContext *ctx = jni_get_xm_audio_effects(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AEjni: init: null ctx", LABEL_RETURN);

    ret = init_xm_effect_context(ctx, sampleRate, nbChannels);
LABEL_RETURN:
    xmae_dec_ref_p(&ctx);
    return ret;
}

static void
XMAudioEffects_close_log_file(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    AeCloseLogFile();
}

static void
XMAudioEffects_set_log(JNIEnv *env, jobject thiz,
        jint logMode, jint logLevel,  jstring outLogPath)
{
    LOGI("%s\n", __func__);
    AeSetLogMode(logMode);
    AeSetLogLevel(logLevel);

    if(logMode == LOG_MODE_FILE) {
        if(outLogPath == NULL) {
            LOGE("logMode is LOG_MODE_FILE, and outLogPath is NULL, return\n");
            return;
        } else {
            const char *out_log_path_ = (*env)->GetStringUTFChars(env, outLogPath, 0);
            AeSetLogPath(out_log_path_);
            if (out_log_path_) (*env)->ReleaseStringUTFChars(env, outLogPath, out_log_path_);
        }
    }
}

static void
XMAudioEffects_setup(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    XmEffectContext *ctx = create_xm_effect_context();
    JNI_CHECK_GOTO(ctx, env, "java/lang/OutOfMemoryError", "AEjni: native_setup: create failed", LABEL_RETURN);

    jni_set_xm_audio_effects(env, thiz, ctx);
LABEL_RETURN:
    xmae_dec_ref_p(&ctx);
}

static JNINativeMethod g_methods[] = {
    { "native_setup", "()V", (void *) XMAudioEffects_setup },
    { "native_set_log", "(IILjava/lang/String;)V", (void *) XMAudioEffects_set_log },
    { "native_close_log_file", "()V", (void *) XMAudioEffects_close_log_file },
    { "native_init", "(II)V", (void *) XMAudioEffects_init },
    { "native_setEffects", "(Ljava/lang/String;Ljava/lang/String;I)I", (void *) XMAudioEffects_setEffects },
    { "native_sendSamples", "([SI)I", (void *) XMAudioEffects_sendSamples },
    { "native_receiveSamples", "([SI)I", (void *) XMAudioEffects_receiveSamples },
    { "native_release", "()V", (void *) XMAudioEffects_release },
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = NULL;

    g_jvm = vm;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != NULL);

    pthread_mutex_init(&g_clazz.mutex, NULL);

    IJK_FIND_JAVA_CLASS(env, g_clazz.clazz, JNI_CLASS_AUDIO_EFFECTS);
    (*env)->RegisterNatives(env, g_clazz.clazz, g_methods, NELEM(g_methods));

    g_clazz.field_mNativeXmAudioEffects = (*env)->GetFieldID(env, g_clazz.clazz, "mNativeXmAudioEffects", "J");

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *jvm, void *reserved)
{
    pthread_mutex_destroy(&g_clazz.mutex);
}
