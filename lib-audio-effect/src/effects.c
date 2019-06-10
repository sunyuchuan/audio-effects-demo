//
// Created by layne on 19-4-27.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "tools/mem.h"
#include "tools/util.h"

#define EFFECT(f) extern const EffectHandler *effect_##f##_fn(void);
#include "effect_name.h"
#undef EFFECT

static effect_fn_t all_effect_fns[] = {
#define EFFECT(f) effect_##f##_fn,
#include "effect_name.h"
#undef EFFECT
    NULL};

const effect_fn_t *get_all_effect_fns(void) { return all_effect_fns; }

const EffectHandler *find_effect(char const *name) {
    const effect_fn_t *fns = get_all_effect_fns();
    for (int i = 0; NULL != fns[i]; ++i) {
        const EffectHandler *eh = fns[i]();
        if (eh && eh->name && strcasecmp(eh->name, name) == 0) return eh;
    }
    return NULL;
}

EffectContext *create_effect(const EffectHandler *handler) {
    if (NULL == handler) return NULL;
    EffectContext *self = (EffectContext *)calloc(1, sizeof(EffectContext));
    self->handler = *handler;
    self->priv = av_mallocz(handler->priv_size);
    return self;
}

const char *show_usage(EffectContext *ctx) {
    assert(NULL != ctx);
    return ctx->handler.usage;
}

int init_effect(EffectContext *ctx, int argc, char **argv) {
    assert(NULL != ctx);
    int result;
    char **argv2 = calloc((argc + 1), sizeof(*argv2));
    argv2[0] = (char *)ctx->handler.name;
    memcpy(argv2 + 1, argv, argc * sizeof(*argv2));
    result = ctx->handler.init(ctx, argc + 1, argv2);
    free(argv2);
    return result;
}

int set_effect(EffectContext *ctx, const char *key, const char *value,
               int flags) {
    assert(NULL != ctx);
    ae_dict_set(&ctx->options, key, value, flags);
    return ctx->handler.set(ctx, key, flags);
}

int send_samples(EffectContext *ctx, const void *samples,
                 const size_t nb_samples) {
    assert(NULL != ctx);
    return ctx->handler.send(ctx, samples, nb_samples);
}

int receive_samples(EffectContext *ctx, void *samples,
                    const size_t max_nb_samples) {
    assert(NULL != ctx);
    return ctx->handler.receive(ctx, samples, max_nb_samples);
}

void free_effect(EffectContext *ctx) {
    if (NULL == ctx) return;
    ctx->handler.close(ctx);
    if (ctx->priv) av_freep(&ctx->priv);
    if (ctx->options) ae_dict_free(&ctx->options);
    free(ctx);
}
