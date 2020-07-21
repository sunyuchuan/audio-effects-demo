#include "xm_audio_effects.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "voice_effect.h"
#include "log.h"
#include "tools/util.h"
#include <pthread.h>

enum EffectType {
    NoiseSuppression = 0,
    Beautify,
    XmlyEcho,
    XmlyReverb,
    Minions,
    VoiceMorph,
    MAX_NB_EFFECTS
};

struct XmEffectContext {
    volatile int ref_count;
    pthread_mutex_t mutex;
    size_t nb_effects;
    EffectContext *effects[MAX_NB_EFFECTS];
};

static int special_set(XmEffectContext *ctx, const char *value, int flags) {
    if (0 == strcasecmp(value, "Original")) {
        // 无变声
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Original", flags);
    } else if (0 == strcasecmp(value, "Church")) {
        // 教堂
        set_effect(ctx->effects[XmlyEcho], "mode", "Church", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Original", flags);
    } else if (0 == strcasecmp(value, "Live")) {
        // 现场演出
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "Live", flags);
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Original", flags);
    } else if (0 == strcasecmp(value, "Robot")) {
        // 机器人
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Robot", flags);
    } else if (0 == strcasecmp(value, "Mimions")) {
        // 小黄人
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[Minions], "Switch", "On", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "original", flags);
    } else if (0 == strcasecmp(value, "Man")) {
        // 男声
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Man", flags);
    } else if (0 == strcasecmp(value, "Women")) {
        // 女声
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Women", flags);
    }
    return 0;
}

static void set_return_samples(XmEffectContext *ctx,
        const char *value, int flags) {
    set_effect(ctx->effects[NoiseSuppression], "return_max_nb_samples", value, flags);
    set_effect(ctx->effects[Beautify], "return_max_nb_samples", value, flags);
    set_effect(ctx->effects[XmlyEcho], "return_max_nb_samples", value, flags);
    set_effect(ctx->effects[XmlyReverb], "return_max_nb_samples", value, flags);
    set_effect(ctx->effects[Minions], "return_max_nb_samples", value, flags);
    set_effect(ctx->effects[VoiceMorph], "return_max_nb_samples", value, flags);
}

void xmae_inc_ref(XmEffectContext *self)
{
    assert(self);
    __sync_fetch_and_add(&self->ref_count, 1);
}

void xmae_dec_ref(XmEffectContext *self)
{
    if (!self)
        return;

    int ref_count = __sync_sub_and_fetch(&self->ref_count, 1);
    if (ref_count == 0) {
        LogInfo("%s xmae_dec_ref(): ref=0\n", __func__);
        free_xm_effect_context(&self);
    }
}

void xmae_dec_ref_p(XmEffectContext **self)
{
    if (!self || !*self)
        return;

    xmae_dec_ref(*self);
    *self = NULL;
}

void free_xm_effect_context(XmEffectContext **ctx) {
    LogInfo("%s.\n", __func__);
    if (NULL == ctx || NULL == *ctx) return;
    XmEffectContext *self = *ctx;

    for (short i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (self->effects[i]) {
            free_effect(self->effects[i]);
            self->effects[i] = NULL;
        }
    }
    pthread_mutex_destroy(&(*ctx)->mutex);
    free(*ctx);
    *ctx = NULL;
}

int xm_effect_receive_samples(XmEffectContext *ctx,
        void *samples, const size_t max_nb_samples) {
    int ret = 0;
    for (short i = 0; i < MAX_NB_EFFECTS - 1; ++i) {
        if (NULL == ctx->effects[i]) continue;
        ret = receive_samples(ctx->effects[i], samples, max_nb_samples);
        while (ret > 0) {
            ret = send_samples(ctx->effects[i + 1], samples, ret);
            if (ret < 0) break;
            ret = receive_samples(ctx->effects[i], samples, max_nb_samples);
        }
    }
    if (ctx->effects[MAX_NB_EFFECTS - 1]) {
        ret = receive_samples(ctx->effects[MAX_NB_EFFECTS - 1],
                    samples, max_nb_samples);
    }
    return ret;
}

int xm_effect_send_samples(XmEffectContext *ctx,
        const void *samples, const size_t nb_samples) {
    return send_samples(ctx->effects[0], samples, nb_samples);
}

int set_xm_effect(XmEffectContext *ctx,
        const char *key, const char *value, int flags) {
    LogInfo("%s key = %s value = %s.\n", __func__, key, value);
    assert(NULL != ctx);

    if (0 == strcasecmp(key, "NoiseSuppression")) {
        return set_effect(ctx->effects[NoiseSuppression], key, value, flags);
    } else if (0 == strcasecmp(key, "Beautify")) {
        return set_effect(ctx->effects[Beautify], "mode", value, flags);
    } else if (0 == strcasecmp(key, "Special")) {
        return special_set(ctx, value, flags);
    }
    if (0 == strcasecmp(key, "return_max_nb_samples")) {
        set_return_samples(ctx, value, flags);
    }
    return 0;
}

int init_xm_effect_context(XmEffectContext *ctx,
        int sample_rate, int nb_channels) {
    int error = 0;

    for (short i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (ctx->effects[i]) {
            free_effect(ctx->effects[i]);
            ctx->effects[i] = NULL;
        }
    }

    ctx->effects[NoiseSuppression] =
        create_effect(find_effect("noise_suppression"), sample_rate, nb_channels);
    ctx->effects[Beautify] = create_effect(find_effect("beautify"), sample_rate, nb_channels);
    ctx->effects[XmlyEcho] = create_effect(find_effect("xmly_echo"), sample_rate, nb_channels);
    ctx->effects[XmlyReverb] = create_effect(find_effect("xmly_reverb"), sample_rate, nb_channels);
    ctx->effects[Minions] = create_effect(find_effect("minions"), sample_rate, nb_channels);
    ctx->effects[VoiceMorph] = create_effect(find_effect("voice_morph"), sample_rate, nb_channels);

    for (short i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (NULL != ctx->effects[i]) {
            int ret = init_effect(ctx->effects[i], 0, NULL);
            if (ret < 0) {
                LogError("%s i = %d ret = %d.\n", __func__, i, ret);
                error = ret;
            }
        }
    }
    return error;
}

XmEffectContext *create_xm_effect_context() {
    XmEffectContext *self =
        (XmEffectContext *)calloc(1, sizeof(XmEffectContext));
    if (NULL == self) return NULL;

    self->nb_effects = MAX_NB_EFFECTS;
    memset(self->effects, 0, MAX_NB_EFFECTS * sizeof(EffectContext *));

    xmae_inc_ref(self);
    pthread_mutex_init(&self->mutex, NULL);
    return self;
}
