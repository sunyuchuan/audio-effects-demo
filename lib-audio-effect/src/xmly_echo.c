#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "logger.h"
#include "math/spl_math.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/util.h"

#define FRAME_LEN 1024
#define MAX_INPUT_FRAME_NUM 8
#define MAX_INPUT_SAMPLE_NUM (MAX_INPUT_FRAME_NUM * FRAME_LEN)
#define MAX_DELAY_FRAME_NUM 15  // 15*1024/44100 = 0.35s
#define MAX_ECHO_LEVEL 3
#define MAX_DELAY_BUFFER_LEN                                              \
    ((MAX_DELAY_FRAME_NUM * MAX_ECHO_LEVEL + (MAX_INPUT_FRAME_NUM + 1)) * \
     FRAME_LEN)

typedef struct {
    fifo *fifo_in;
    fifo *fifo_out;
    SdlMutex *sdl_mutex;
    short is_echo_on;
    short delay_frame_num;
    short weight[MAX_ECHO_LEVEL];
    int16_t delay_buf[MAX_ECHO_LEVEL][MAX_DELAY_BUFFER_LEN];
    int res_len[MAX_ECHO_LEVEL];
} priv_t;

static int xmly_echo_close(EffectContext *ctx) {
    AeLogI("xmly_echo.c:%d %s.\n", __LINE__, __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
    }
    return 0;
}

static void init_self_parameter(priv_t *priv) {
    assert(NULL != priv);
    priv->is_echo_on = 0;
    priv->delay_frame_num = 5;

    int exp_w = 0x3fffffff;
    for (int i = 0; i < MAX_ECHO_LEVEL; i++) {
        exp_w = SPL_MUL_16_32_RSFT16(0x4000, exp_w);
        priv->weight[i] = exp_w >> 14;
        exp_w <<= 1;
    }

    for (int i = 0; i < MAX_ECHO_LEVEL; ++i) {
        memset(priv->delay_buf[i], 0, sizeof(int16_t) * MAX_DELAY_BUFFER_LEN);
        priv->res_len[i] = priv->delay_frame_num * (i + 1) * FRAME_LEN;
    }
}

static int xmly_echo_init(EffectContext *ctx, int argc, char **argv) {
    AeLogI("xmly_echo.c:%d %s.\n", __LINE__, __func__);
    for (int i = 0; i < argc; ++i) {
        AeLogI("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
    priv->fifo_in = fifo_create(sizeof(int16_t));
    if (NULL == priv->fifo_in) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->fifo_out = fifo_create(sizeof(int16_t));
    if (NULL == priv->fifo_out) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    init_self_parameter(priv);

end:
    if (ret < 0) xmly_echo_close(ctx);
    return ret;
}

static void set_delay(priv_t *priv, int delay) {
    priv->delay_frame_num = FFMAX(0, FFMIN(MAX_DELAY_FRAME_NUM, delay));
    for (int i = 0; i < MAX_ECHO_LEVEL; i++) {
        memset(priv->delay_buf[i], 0, sizeof(int16_t) * MAX_DELAY_BUFFER_LEN);
        priv->res_len[i] = priv->delay_frame_num * (i + 1) * FRAME_LEN;
    }
}

static void echo_set_mode(priv_t *priv, const char *mode) {
    if (0 == strcasecmp(mode, "None")) {
        priv->is_echo_on = 0;
        return;
    }

    int delay = 0;
    priv->is_echo_on = 1;
    if (0 == strcasecmp(mode, "Classroom")) {
        delay = 1;
    } else if (0 == strcasecmp(mode, "Church")) {
        delay = 3;
    } else if (0 == strcasecmp(mode, "Valley")) {
        delay = 15;
    }
    set_delay(priv, delay);
}

static int xmly_echo_set(EffectContext *ctx, const char *key, int flags) {
    AeLogI("xmly_echo.c:%d %s.\n", __LINE__, __func__);
    assert(NULL != ctx);

    priv_t *priv = ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        AeLogI("key = %s val = %s\n", entry->key, entry->value);

        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, "mode")) {
            echo_set_mode(priv, entry->value);
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    }
    return 0;
}

static int xmly_echo_send(EffectContext *ctx, const void *samples,
                          const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int xmly_echo_receive(EffectContext *ctx, void *samples,
                             const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);
    assert(max_nb_samples < MAX_INPUT_SAMPLE_NUM);

    sdl_mutex_lock(priv->sdl_mutex);
    if (priv->is_echo_on) {
        size_t nb_samples = fifo_read(priv->fifo_in, samples, max_nb_samples);
        for (size_t i = 0; i < MAX_ECHO_LEVEL; ++i) {
            memcpy(priv->delay_buf[i] + priv->res_len[i], samples,
                   sizeof(int16_t) * nb_samples);
            priv->res_len[i] += nb_samples;
        }

        int16_t *d0 = priv->delay_buf[0];
        int16_t *d1 = priv->delay_buf[1];
        int16_t *d2 = priv->delay_buf[2];
        int32_t sum = 0;
        for (size_t i = 0; i < nb_samples; ++i) {
            sum = (d0[i] * priv->weight[0] + d1[i] * priv->weight[1] +
                   d2[i] * priv->weight[2]) >>
                  1;
            sum += ((int16_t *)samples)[i] << 14;
            sum = FFMIN(0x1fffffff, FFMAX(-536870912, sum));
            ((int16_t *)samples)[i] = sum >> 14;
        }
        fifo_write(priv->fifo_out, samples, nb_samples);

        for (short i = 0; i < MAX_ECHO_LEVEL; ++i) {
            memmove(priv->delay_buf[i], &priv->delay_buf[i][nb_samples],
                    sizeof(int16_t) * (priv->res_len[i] - nb_samples));
            priv->res_len[i] -= nb_samples;
        }
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, samples, max_nb_samples);
            fifo_write(priv->fifo_out, samples, nb_samples);
        }
    }
    sdl_mutex_unlock(priv->sdl_mutex);

    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_xmly_echo_fn(void) {
    static EffectHandler handler = {.name = "xmly_echo",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = xmly_echo_init,
                                    .set = xmly_echo_set,
                                    .send = xmly_echo_send,
                                    .receive = xmly_echo_receive,
                                    .close = xmly_echo_close};
    return &handler;
}
