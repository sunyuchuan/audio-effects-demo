//
// Created by layne on 19-4-27.
//

#ifndef AUDIO_EFFECT_EFFECTS_H_
#define AUDIO_EFFECT_EFFECTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct EffectContext_T EffectContext;
typedef struct EffectHandler_T EffectHandler;

const EffectHandler *find_effect(char const *name);
EffectContext *create_effect(const EffectHandler *handler);
const char *show_usage(EffectContext *ctx);
int init_effect(EffectContext *ctx, int argc, char **argv);
int set_effect(EffectContext *ctx, const char *key, const char *value,
               int flags);
int send_samples(EffectContext *ctx, const void *samples,
                 const size_t nb_samples);
int receive_samples(EffectContext *ctx, void *samples,
                    const size_t max_nb_samples);
void free_effect(EffectContext *ctx);

#ifdef __cplusplus
}
#endif
#endif  // AUDIO_EFFECT_EFFECTS_H_
