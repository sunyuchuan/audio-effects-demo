//
// Created by layne on 19-5-16.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "logger.h"
#include "voice_morph/morph_core.h"

typedef struct {
    MorphCore *morph;
} priv_t;

static int voice_morph_close(EffectContext *ctx) {
    assert(NULL != ctx);
    if (NULL == ctx->priv) return 0;
    priv_t *priv = (priv_t *)ctx->priv;
    if (priv->morph) morph_core_free(&priv->morph);
    return 0;
}

static int voice_morph_init(EffectContext *ctx, int argc, char **argv) {
    AeLogI("voice_morph.c:%d %s.\n", __LINE__, __func__);
    for (int i = 0; i < argc; ++i) {
        AeLogI("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);

    int ret = 0;
    // 创建 MorphCore
    priv->morph = morph_core_create();
    if (NULL == priv->morph) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    ret = morph_core_init(priv->morph);

end:
    if (ret < 0) voice_morph_close(ctx);
    return ret;
}

static void voice_morph_set_mode(priv_t *priv, const char *mode) {
    int mode_length = strlen(mode);
    if (mode_length >= 8 && 0 == strncmp(mode, "original", 8)) {
        AeLogI("voice_morph.c:%d %s set original.\n", __LINE__, __func__);
        morph_core_set_type(priv->morph, ORIGINAL);
    } else if (mode_length >= 6 && 0 == strncmp(mode, "bright", 6)) {
        AeLogI("voice_morph.c:%d %s set bright.\n", __LINE__, __func__);
        morph_core_set_type(priv->morph, BRIGHT);
    } else if (mode_length >= 5 && 0 == strncmp(mode, "robot", 5)) {
        AeLogI("voice_morph.c:%d %s set robot.\n", __LINE__, __func__);
        morph_core_set_type(priv->morph, ROBOT);
    } else if (mode_length >= 3 && 0 == strncmp(mode, "man", 3)) {
        AeLogI("voice_morph.c:%d %s set man.\n", __LINE__, __func__);
        morph_core_set_type(priv->morph, MAN);
    } else if (mode_length >= 5 && 0 == strncmp(mode, "women", 5)) {
        AeLogI("voice_morph.c:%d %s set women.\n", __LINE__, __func__);
        morph_core_set_type(priv->morph, WOMEN);
    }
}

static int voice_morph_set(EffectContext *ctx, const char *key, int flags) {
    AeLogI("voice_morph.c:%d %s.\n", __LINE__, __func__);
    assert(NULL != ctx);

    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        AeLogI("key = %s val = %s\n", entry->key, entry->value);
        int key_length = strlen(entry->key);
        if (key_length >= 4 && 0 == strncmp(entry->key, "mode", 4)) {
            voice_morph_set_mode(ctx->priv, entry->value);
        }
    }
    return 0;
}

static int voice_morph_send(EffectContext *ctx, const void *samples,
                            const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->morph);

    return morph_core_send(priv->morph, samples, nb_samples);
}

static int voice_morph_receive(EffectContext *ctx, void *samples,
                               const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->morph);

    return morph_core_receive(priv->morph, samples, max_nb_samples);
}

const EffectHandler *effect_voice_morph_fn(void) {
    static EffectHandler handler = {.name = "voice_morph",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = voice_morph_init,
                                    .set = voice_morph_set,
                                    .send = voice_morph_send,
                                    .receive = voice_morph_receive,
                                    .close = voice_morph_close};
    return &handler;
}
