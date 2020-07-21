//
// Created by layne on 19-5-16.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "tools/util.h"
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
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
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
    if (0 == strcasecmp(mode, "original")) {
        LogInfo("%s set original.\n", __func__);
        morph_core_set_type(priv->morph, ORIGINAL);
    } else if (0 == strcasecmp(mode, "bright")) {
        LogInfo("%s set bright.\n", __func__);
        morph_core_set_type(priv->morph, BRIGHT);
    } else if (0 == strcasecmp(mode, "robot")) {
        LogInfo("%s set robot.\n", __func__);
        morph_core_set_type(priv->morph, ROBOT);
    } else if (0 == strcasecmp(mode, "man")) {
        LogInfo("%s set man.\n", __func__);
        morph_core_set_type(priv->morph, MAN);
    } else if (0 == strcasecmp(mode, "women")) {
        LogInfo("%s set women.\n", __func__);
        morph_core_set_type(priv->morph, WOMEN);
    }
}

static int voice_morph_set(EffectContext *ctx, const char *key, int flags) {
    assert(NULL != ctx);

    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);
        if (0 == strcasecmp(entry->key, "mode")) {
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

    return morph_core_receive(priv->morph, samples, max_nb_samples,
                              &ctx->return_max_nb_samples);
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
