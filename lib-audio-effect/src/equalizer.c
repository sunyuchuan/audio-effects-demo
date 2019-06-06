//
// Created by layne on 19-4-28.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsp_tools/iir_design/iir_design.h"
#include "effect_struct.h"
#include "error_def.h"
#include "logger.h"
#include "tools/conversion.h"
#include "tools/dict.h"
#include "tools/fifo.h"
#include "tools/mem.h"
#include "tools/sdl_mutex.h"

#define MIN_BUFFER_SIZE 2048
#define SAMPLE_RATE 44100

typedef struct {
    fifo *f;
    SdlMutex *sdl_mutex;
    short effect_mode;
    size_t nb_effect_bands;
    Band *effect_bands;
    float *flp_buffer;
    size_t flp_buffer_size;
    short is_equalizer_on;
} priv_t;

static int equalizer_close(EffectContext *ctx) {
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->f) fifo_delete(&priv->f);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
        if (priv->flp_buffer) av_freep(&priv->flp_buffer);
        if (priv->effect_bands) av_freep(&priv->effect_bands);
    }
    return 0;
}

static void create_base_effect(priv_t *priv) {
    priv->nb_effect_bands = 4;
    priv->effect_bands =
        (Band *)av_mallocz(priv->nb_effect_bands * sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_butterworth_highpass(priv->effect_bands, SAMPLE_RATE, 50.0f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 200.f, 2.0f,
                        1.66f);
    iir_2nd_coeffs_peak(priv->effect_bands + 2, SAMPLE_RATE, 80.f, 2.0f, 1.43f);
    iir_2nd_coeffs_high_shelf(priv->effect_bands + 3, SAMPLE_RATE, 4300.0f,
                              0.7f, 1.74f);
}

// 老式收音机
static void create_old_radio(priv_t *priv) {
    priv->nb_effect_bands = 6;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_low_shelf(priv->effect_bands, SAMPLE_RATE, 42.0f, 0.7f,
                             0.562f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 80.f, 4.0f,
                        0.0316f);
    iir_2nd_coeffs_peak(priv->effect_bands + 2, SAMPLE_RATE, 200.0f, 4.0f,
                        0.224f);
    iir_2nd_coeffs_peak(priv->effect_bands + 3, SAMPLE_RATE, 672.0f, 4.0f,
                        3.55f);
    iir_2nd_coeffs_peak(priv->effect_bands + 4, SAMPLE_RATE, 2032.0f, 4.0f,
                        0.251f);
    iir_2nd_coeffs_high_shelf(priv->effect_bands + 5, SAMPLE_RATE, 14000.0f,
                              0.7f, 0.0794f);
}

// 清晰人声
static void create_clean_voice(priv_t *priv) {
    priv->nb_effect_bands = 4;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_butterworth_highpass(priv->effect_bands, SAMPLE_RATE, 50.0f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 200.f, 2.0f,
                        1.66f);
    iir_2nd_coeffs_peak(priv->effect_bands + 2, SAMPLE_RATE, 80.f, 2.0f, 1.43f);
    iir_2nd_coeffs_high_shelf(priv->effect_bands + 3, SAMPLE_RATE, 4300.0f,
                              0.7f, 1.74f);
}

// 低音
static void create_bass_effect(priv_t *priv) {
    priv->nb_effect_bands = 3;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_butterworth_highpass(priv->effect_bands, SAMPLE_RATE, 50.0f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 150.f, 2.0f,
                        1.95f);
    iir_2nd_coeffs_peak(priv->effect_bands + 2, SAMPLE_RATE, 500.0f, 2.0f,
                        1.74f);
}

// 低沉
static void create_low_voice(priv_t *priv) {
    priv->nb_effect_bands = 4;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_butterworth_highpass(priv->effect_bands, SAMPLE_RATE, 50.0f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 200.f, 2.0f,
                        1.53f);
    iir_2nd_coeffs_peak(priv->effect_bands + 2, SAMPLE_RATE, 1250.0f, 2.0f,
                        0.813f);
    iir_2nd_coeffs_high_shelf(priv->effect_bands + 3, SAMPLE_RATE, 7500.0f,
                              0.7f, 0.708f);
}

// 穿透
static void create_penetrating_effect(priv_t *priv) {
    priv->nb_effect_bands = 2;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_peak(priv->effect_bands, SAMPLE_RATE, 2000.f, 2.0f, 1.51f);
    iir_2nd_coeffs_high_shelf(priv->effect_bands + 1, SAMPLE_RATE, 4300.0f,
                              0.7f, 1.74f);
}

// 磁性
static void create_magnetic_effect(priv_t *priv) {
    priv->nb_effect_bands = 3;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_butterworth_highpass(priv->effect_bands, SAMPLE_RATE, 50.0f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 500.f, 2.0f,
                        1.74f);
    iir_2nd_coeffs_peak(priv->effect_bands + 2, SAMPLE_RATE, 1200.0f, 2.0f,
                        1.55f);
}

// 柔和高音
static void create_soft_pitch(priv_t *priv) {
    priv->nb_effect_bands = 3;
    priv->effect_bands = (Band *)calloc(priv->nb_effect_bands, sizeof(Band));
    if (NULL == priv->effect_bands) return;

    iir_2nd_coeffs_peak(priv->effect_bands, SAMPLE_RATE, 300.f, 2.0f, 1.2f);
    iir_2nd_coeffs_peak(priv->effect_bands + 1, SAMPLE_RATE, 1200.f, 2.0f,
                        1.82f);
    iir_2nd_coeffs_high_shelf(priv->effect_bands + 2, SAMPLE_RATE, 4300.0f,
                              0.7f, 2.19f);
}

static int equalizer_init(EffectContext *ctx, int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        AeLogI("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    // 创建 fifo
    priv->f = fifo_create(sizeof(short));
    if (NULL == priv->f) {
        equalizer_close(ctx);
        return AEERROR_NOMEM;
    }

    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        equalizer_close(ctx);
        return AEERROR_NOMEM;
    }

    // 为 flp_buffer 分配空间
    priv->flp_buffer = av_mallocz(sizeof(float) * MIN_BUFFER_SIZE);
    if (NULL == priv->flp_buffer) {
        equalizer_close(ctx);
        return AEERROR_NOMEM;
    }
    priv->flp_buffer_size = MIN_BUFFER_SIZE;
    priv->is_equalizer_on = 0;

    // 创建基本的均衡器
    create_base_effect(priv);

    return 0;
}

static void equalizer_set_mode(priv_t *priv, const char *mode) {
    AeLogI("equalizer.c:%d %s mode = %s.\n", __LINE__, __func__, mode);
    sdl_mutex_lock(priv->sdl_mutex);

    if (priv->effect_bands) {
        av_freep(&priv->effect_bands);
        priv->nb_effect_bands = 0;
    }
    priv->is_equalizer_on = 1;

    short mode_length = strlen(mode);
    if (mode_length >= 4 && 0 == strncmp(mode, "None", 4)) {
        priv->is_equalizer_on = 0;
    } else if (mode_length >= 8 && 0 == strncmp(mode, "OldRadio", 8)) {
        create_old_radio(priv);
    } else if (mode_length >= 10 && 0 == strncmp(mode, "CleanVoice", 10)) {
        create_clean_voice(priv);
    }
    if (mode_length >= 4 && 0 == strncmp(mode, "Bass", 4)) {
        create_bass_effect(priv);
    }
    if (mode_length >= 8 && 0 == strncmp(mode, "LowVoice", 8)) {
        create_low_voice(priv);
    }
    if (mode_length >= 11 && 0 == strncmp(mode, "Penetrating", 11)) {
        create_penetrating_effect(priv);
    }
    if (mode_length >= 8 && 0 == strncmp(mode, "Magnetic", 8)) {
        create_magnetic_effect(priv);
    }
    if (mode_length >= 9 && 0 == strncmp(mode, "SoftPitch", 9)) {
        create_soft_pitch(priv);
    }
    sdl_mutex_unlock(priv->sdl_mutex);
}

static int equalizer_set(EffectContext *ctx, const char *key, int flags) {
    AeLogI("equalizer.c:%d %s key = %s.\n", __LINE__, __func__, key);
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        int key_length = strlen(entry->key);
        if (key_length >= 4 && 0 == strncmp(entry->key, "mode", 4)) {
            equalizer_set_mode(ctx->priv, entry->value);
        }
    }
    return 0;
}

static int equalizer_send(EffectContext *ctx, const void *samples,
                          const size_t nb_samples) {
    priv_t *priv = (priv_t *)ctx->priv;

    if (priv->f) return fifo_write(priv->f, samples, nb_samples);
    return 0;
}

static void band_rocess(Band *band, float *x, const int len) {
    float b0 = band->coeffs[0];
    float b1 = band->coeffs[1];
    float b2 = band->coeffs[2];
    float a1 = band->coeffs[3];
    float a2 = band->coeffs[4];
    float s1 = band->states[0];
    float s2 = band->states[1];
    float s3 = band->states[2];
    float s4 = band->states[3];

    for (int i = 0; i < len; ++i) {
        float y = b0 * x[i] + b1 * s1 + b2 * s2 - a1 * s3 - a2 * s4;
        s2 = s1;
        s1 = x[i];
        s4 = s3;
        s3 = y;
        x[i] = y;
    }

    band->states[0] = s1;
    band->states[1] = s2;
    band->states[2] = s3;
    band->states[3] = s4;
}

static int equalizer_receive(EffectContext *ctx, void *samples,
                             const size_t nb_samples) {
    priv_t *priv = (priv_t *)ctx->priv;
    if (!priv->f) return AEERROR_NULL_POINT;

    // 读取原始数据
    int ret = fifo_read(priv->f, samples, nb_samples);
    if (priv->is_equalizer_on) {
        if ((size_t)ret > priv->flp_buffer_size) {
            priv->flp_buffer_size = ret;
            priv->flp_buffer = av_realloc(
                priv->flp_buffer, sizeof(float) * priv->flp_buffer_size);
        }
        if (!priv->flp_buffer) return AEERROR_NULL_POINT;

        // 原始数据转成float类型
        S16ToFloat(samples, priv->flp_buffer, ret);

        // 均衡器音效处理
        sdl_mutex_lock(priv->sdl_mutex);
        for (uint16_t i = 0; i < priv->nb_effect_bands; ++i) {
            band_rocess(priv->effect_bands + i, priv->flp_buffer, ret);
        }
        sdl_mutex_unlock(priv->sdl_mutex);

        // 处理后的数据转换成short类型
        FloatToS16(priv->flp_buffer, samples, ret);
    }
    return ret;
}

const EffectHandler *effect_equalizer_fn(void) {
    static EffectHandler handler = {.name = "equalizer",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = equalizer_init,
                                    .set = equalizer_set,
                                    .send = equalizer_send,
                                    .receive = equalizer_receive,
                                    .close = equalizer_close};
    return &handler;
}
