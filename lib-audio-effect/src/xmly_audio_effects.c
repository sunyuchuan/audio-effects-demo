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

static int environment_set(XmlyEffectContext *ctx, const char *value,
                           int flags) {
    if (0 == strcasecmp(value, "None")) {
        // 无环境音
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
    } else if (0 == strcasecmp(value, "Valley")) {
        // 山谷
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[XmlyEcho], "mode", "Valley", flags);
    } else if (0 == strcasecmp(value, "Church")) {
        // 教堂
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[XmlyEcho], "mode", "Church", flags);
    } else if (0 == strcasecmp(value, "Classroom")) {
        // 教室
        set_effect(ctx->effects[XmlyReverb], "mode", "None", flags);
        set_effect(ctx->effects[XmlyEcho], "mode", "Classroom", flags);
    } else if (0 == strcasecmp(value, "Live")) {
        // 现场演出
        set_effect(ctx->effects[XmlyEcho], "mode", "None", flags);
        set_effect(ctx->effects[XmlyReverb], "mode", "Live", flags);
    }
    return 0;
}

static int morph_set(XmlyEffectContext *ctx, const char *value, int flags) {
    if (0 == strcasecmp(value, "Original")) {
        // 无变声
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Original", flags);
    } else if (0 == strcasecmp(value, "Robot")) {
        // 机器人
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Robot", flags);
    } else if (0 == strcasecmp(value, "Mimions")) {
        // 小黄人
        set_effect(ctx->effects[Minions], "Switch", "On", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "original", flags);
    } else if (0 == strcasecmp(value, "Bright")) {
        // 明亮
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Bright", flags);
    } else if (0 == strcasecmp(value, "Man")) {
        // 男声
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Man", flags);
    } else if (0 == strcasecmp(value, "Women")) {
        // 女声
        set_effect(ctx->effects[Minions], "Switch", "Off", flags);
        set_effect(ctx->effects[VoiceMorph], "mode", "Women", flags);
    }
    return 0;
}

int set_xmly_effect(XmlyEffectContext *ctx, const char *key, const char *value,
                    int flags) {
    AeLogI("xmly_audio_effects.c:%d %s key = %s value = %s.\n", __LINE__,
           __func__, key, value);
    assert(NULL != ctx);

    if (0 == strcasecmp(key, "NoiseSuppression")) {
        return set_effect(ctx->effects[NoiseSuppression], key, value, flags);
    } else if (0 == strcasecmp(key, "Environment")) {
        return environment_set(ctx, value, flags);
    } else if (0 == strcasecmp(key, "Morph")) {
        return morph_set(ctx, value, flags);
    } else if (0 == strcasecmp(key, "Equalizer")) {
        return set_effect(ctx->effects[Equalizer], "mode", value, flags);
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
