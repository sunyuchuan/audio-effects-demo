#include "xmly_audio_effects.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "effects.h"
#include "logger.h"

enum EffectType {
    NoiseSuppression = 0,
    Equalizer,
    XmlyEcho,
    XmlyReverb,
    Minions,
    VoiceMorph
};

struct XmlyEffectContext_T {
    size_t nb_effects;
    EffectContext *effects[VoiceMorph - NoiseSuppression + 1];
};

XmlyEffectContext *create_xmly_effect() {
    XmlyEffectContext *self =
        (XmlyEffectContext *)calloc(1, sizeof(XmlyEffectContext));
    if (NULL == self) return NULL;

    self->nb_effects = VoiceMorph - NoiseSuppression + 1;
    // 创建多个音效
    memset(self->effects, 0, self->nb_effects * sizeof(XmlyEffectContext *));
    self->effects[NoiseSuppression] =
        create_effect(find_effect("noise_suppression"));
    self->effects[Equalizer] = create_effect(find_effect("equalizer"));
    self->effects[XmlyEcho] = create_effect(find_effect("xmly_echo"));
    self->effects[XmlyReverb] = create_effect(find_effect("xmly_reverb"));
    self->effects[Minions] = create_effect(find_effect("minions"));
    self->effects[VoiceMorph] = create_effect(find_effect("voice_morph"));

    return self;
}

int init_xmly_effect(XmlyEffectContext *ctx) {
    int error = 0;
    for (short i = NoiseSuppression; i <= VoiceMorph; ++i) {
        if (NULL != ctx->effects[i]) {
            int ret = init_effect(ctx->effects[i], 0, NULL);
            if (ret < 0) {
                AeLogE("xmly_audio_effects.c:%d %s i = %d ret = %d.\n",
                       __LINE__, __func__, i, ret);
                error = ret;
            }
        }
    }
    return error;
}

static int noise_suppression_set(XmlyEffectContext *ctx, const char *key,
                                 const char *value, int flags) {
    AeLogI("xmly_audio_effects.c:%d %s key = %s value = %s.\n", __LINE__,
           __func__, key, value);

    int value_length = strlen(value);
    if (value_length >= 2 &&
        (0 == strncmp(value, "On", 2) || 0 == strncmp(value, "on", 2)))
        return set_effect(ctx->effects[NoiseSuppression],
                          "noise_suppression_switch", "1", flags);
    else if (value_length >= 3 &&
             (0 == strncmp(value, "Off", 3) || 0 == strncmp(value, "off", 3)))
        return set_effect(ctx->effects[NoiseSuppression],
                          "noise_suppression_switch", "0", flags);
    return 0;
}

static int environment_set(XmlyEffectContext *ctx, const char *value,
                           int flags) {
    int value_length = strlen(value);
    if (value_length >= 4 &&
        (0 == strncmp(value, "None", 4) || 0 == strncmp(value, "none", 4))) {
        // 无环境音
        set_effect(ctx->effects[XmlyEcho], "echo_mode", "no_echo", flags);
        set_effect(ctx->effects[XmlyReverb], "reverb_mode", "no_reverb", flags);
    } else if (value_length >= 6 && (0 == strncmp(value, "Valley", 6) ||
                                     0 == strncmp(value, "valley", 6))) {
        // 山谷
        set_effect(ctx->effects[XmlyReverb], "reverb_mode", "no_reverb", flags);
        set_effect(ctx->effects[XmlyEcho], "echo_mode", "valley", flags);
    } else if (value_length >= 6 && (0 == strncmp(value, "Church", 6) ||
                                     0 == strncmp(value, "church", 6))) {
        // 教堂
        set_effect(ctx->effects[XmlyReverb], "reverb_mode", "no_reverb", flags);
        set_effect(ctx->effects[XmlyEcho], "echo_mode", "church", flags);
    } else if (value_length >= 9 && (0 == strncmp(value, "Classroom", 9) ||
                                     0 == strncmp(value, "classroom", 9))) {
        // 教室
        set_effect(ctx->effects[XmlyReverb], "reverb_mode", "no_reverb", flags);
        set_effect(ctx->effects[XmlyEcho], "echo_mode", "classroom", flags);
    } else if (value_length >= 4 && (0 == strncmp(value, "Live", 4) ||
                                     0 == strncmp(value, "live", 4))) {
        // 现场演出
        set_effect(ctx->effects[XmlyEcho], "echo_mode", "no_echo", flags);
        set_effect(ctx->effects[XmlyReverb], "reverb_mode", "LIVE", flags);
    }
    return 0;
}

static int morph_set(XmlyEffectContext *ctx, const char *value, int flags) {
    int value_length = strlen(value);
    if (value_length >= 8 && (0 == strncmp(value, "Original", 8) ||
                              0 == strncmp(value, "original", 8))) {
        // 无变声
        set_effect(ctx->effects[Minions], "minions_on", "0", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "original", flags);
    } else if (value_length >= 5 && (0 == strncmp(value, "Robot", 5) ||
                                     0 == strncmp(value, "robot", 5))) {
        // 机器人
        set_effect(ctx->effects[Minions], "minions_on", "0", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "robot", flags);
    } else if (value_length >= 7 && (0 == strncmp(value, "Mimions", 7) ||
                                     0 == strncmp(value, "mimions", 7))) {
        // 小黄人
        set_effect(ctx->effects[Minions], "minions_on", "1", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "original", flags);
    } else if (value_length >= 6 && (0 == strncmp(value, "Bright", 8) ||
                                     0 == strncmp(value, "bright", 6))) {
        // 明亮
        set_effect(ctx->effects[Minions], "minions_on", "0", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "bright", flags);
    } else if (value_length >= 3 && (0 == strncmp(value, "Man", 8) ||
                                     0 == strncmp(value, "man", 3))) {
        // 男声
        set_effect(ctx->effects[Minions], "minions_on", "0", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "man", flags);
    } else if (value_length >= 5 && (0 == strncmp(value, "Women", 5) ||
                                     0 == strncmp(value, "women", 5))) {
        // 女声
        set_effect(ctx->effects[Minions], "minions_on", "0", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "women", flags);
    }
    return 0;
}

static int equalizer_set(XmlyEffectContext *ctx, const char *value, int flags) {
    return set_effect(ctx->effects[Equalizer], "mode", value, flags);
}

int set_xmly_effect(XmlyEffectContext *ctx, const char *key, const char *value,
                    int flags) {
    AeLogI("xmly_audio_effects.c:%d %s key = %s value = %s.\n", __LINE__,
           __func__, key, value);
    assert(NULL != ctx);

    int key_length = strlen(key);
    if (key_length >= 16 && strncmp(key, "NoiseSuppression", 16) == 0) {
        return noise_suppression_set(ctx, key, value, flags);
    } else if (key_length >= 11 && strncmp(key, "Environment", 11) == 0) {
        return environment_set(ctx, value, flags);
    } else if (key_length >= 5 && strncmp(key, "Morph", 5) == 0) {
        return morph_set(ctx, value, flags);
    } else if (key_length >= 9 && strncmp(key, "Equalizer", 9) == 0) {
        return equalizer_set(ctx, value, flags);
    }
    return 0;
}

int xmly_send_samples(XmlyEffectContext *ctx, const void *samples,
                      const size_t nb_samples) {
    return send_samples(ctx->effects[NoiseSuppression], samples, nb_samples);
}

int xmly_receive_samples(XmlyEffectContext *ctx, void *samples,
                         const size_t max_nb_samples) {
    int ret = 0;
    for (short i = NoiseSuppression; i < VoiceMorph; ++i) {
        if (NULL == ctx->effects[i]) continue;
        ret = receive_samples(ctx->effects[i], samples, max_nb_samples);
        while (ret > 0) {
            ret = send_samples(ctx->effects[i + 1], samples, ret);
            if (ret < 0) break;
            ret = receive_samples(ctx->effects[i], samples, max_nb_samples);
        }
    }
    if (ctx->effects[VoiceMorph]) {
        ret =
            receive_samples(ctx->effects[VoiceMorph], samples, max_nb_samples);
    }
    return ret;
}

void free_xmly_effect(XmlyEffectContext **ctx) {
    if (NULL == ctx || NULL == *ctx) return;
    XmlyEffectContext *self = *ctx;

    for (short i = NoiseSuppression; i <= VoiceMorph; ++i) {
        if (self->effects[i]) {
            free_effect(self->effects[i]);
            self->effects[i] = NULL;
        }
    }
    free(*ctx);
    *ctx = NULL;
}
