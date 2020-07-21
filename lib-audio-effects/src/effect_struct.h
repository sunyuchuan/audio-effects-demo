#ifndef AUDIO_EFFECT_EFFECT_STRUCT_H_
#define AUDIO_EFFECT_EFFECT_STRUCT_H_
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include "effects.h"
#include "tools/dict.h"

typedef const EffectHandler *(*effect_fn_t)(void);

struct EffectHandler_T {
    const char *name;
    const char *usage;
    size_t priv_size;

    int (*init)(EffectContext *ctx, int argc, char **argv);
    int (*set)(EffectContext *ctx, const char *key, int flags);
    int (*send)(EffectContext *ctx, const void *samples,
                const size_t nb_samples);
    int (*receive)(EffectContext *ctx, void *samples, const size_t nb_samples);
    int (*close)(EffectContext *ctx);
};

struct EffectContext_T {
    EffectHandler handler;
    AVDictionary *options;
    atomic_bool return_max_nb_samples;
    void *priv;
};

#endif  // AUDIO_EFFECT_EFFECT_STRUCT_H_
