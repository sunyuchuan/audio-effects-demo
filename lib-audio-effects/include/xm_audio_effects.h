#ifndef XM_AUDIO_EFFECTS_H_
#define XM_AUDIO_EFFECTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct XmEffectContext XmEffectContext;

/**
 * @brief Reference count subtract 1
 *
 * @param self XmEffectContext
 */
void xmae_dec_ref(XmEffectContext *self);

/**
 * @brief Reference count subtract 1
 *
 * @param self XmEffectContext
 */
void xmae_dec_ref_p(XmEffectContext **self);

/**
 * @brief Reference count plus 1
 *
 * @param self XmEffectContext
 */
void xmae_inc_ref(XmEffectContext *self);

/**
 * @brief free XmEffectContext
 *
 * @param ctx XmEffectContext **
 */
void free_xm_effect_context(XmEffectContext **ctx);

/**
 * @brief Get the data after adding special effects
 *
 * @param ctx XmEffectContext
 * @param samples the data after adding special effects
 * @param max_nb_samples buffer size
 * @return effective buffer size
 */
int xm_effect_receive_samples(XmEffectContext *ctx,
    void *samples, const size_t max_nb_samples);

/**
 * @brief send raw samples
 *
 * @param ctx XmEffectContext
 * @param samples pcm data
 * @param nb_samples number of samples
 * @return the number of processed data
 */
int xm_effect_send_samples(XmEffectContext *ctx,
    const void *samples, const size_t nb_samples);

/**
 * @brief set effect
 *
 * @param ctx XmEffectContext
 * @param key
 * @param value
 * @param flags
 * @return Less than 0 fail
 */
int set_xm_effect(XmEffectContext *ctx,
    const char *key, const char *value, int flags);

/**
 * @brief init XmEffectContext
 *
 * @param ctx XmEffectContext *
 * @param sample_rate sample rate of pcm data
 * @param nb_channels number channels of pcm data
 * @return Less than 0 fail
 */
int init_xm_effect_context(XmEffectContext *ctx,
        int sample_rate, int nb_channels);

/**
 * @brief create XmEffectContext
 *
 * @return XmEffectContext*
 */
XmEffectContext *create_xm_effect_context();

#ifdef __cplusplus
}
#endif
#endif  // XM_AUDIO_EFFECTS_H_
