#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "math/spl_math.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/util.h"

#define COMB_NUM 8
#define ALLPASS_NUM 4
#define FIXED_GAIN 492
#define SCALE_DRY 2
#define SCALE_DAMP 13107
#define SCALE_ROOM 9175
#define OFFSET_ROOM 22938

typedef struct {
    fifo *fifo_in;
    fifo *fifo_out;
    SdlMutex *sdl_mutex;
    int comb_state[COMB_NUM];
    int *comb_buf[COMB_NUM];
    short comb_buf_len[COMB_NUM];
    short comb_buf_idx[COMB_NUM];
    int *allpass_buf[ALLPASS_NUM];
    short allpass_buf_len[ALLPASS_NUM];
    short allpass_buf_idx[ALLPASS_NUM];
    // comb filter related arguments
    short damp1;
    short damp2;
    short comb_feedback;
    // all-pass filter related arguments
    short allpass_feedback;
    short gain;
    // environment arguments
    short dry;
    bool is_reverb_on;
} priv_t;

static int xmly_reverb_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
        for (int i = 0; i < COMB_NUM; ++i) {
            if (priv->comb_buf[i]) {
                free(priv->comb_buf[i]);
                priv->comb_buf[i] = NULL;
            }
        }
        for (int i = 0; i < ALLPASS_NUM; ++i) {
            if (priv->allpass_buf[i]) {
                free(priv->allpass_buf[i]);
                priv->allpass_buf[i] = NULL;
            }
        }
    }
    return 0;
}

static void init_self_parameter(priv_t *priv) {
    int tmp = 32767.0f * 0.75f * SCALE_DAMP;
    priv->damp1 = tmp >> 15;
    priv->damp2 = 32767 - priv->damp1;
    tmp = 32767.0f * 0.5f * SCALE_ROOM + (int)(OFFSET_ROOM << 15);
    priv->comb_feedback = tmp >> 15;
    priv->allpass_feedback = 32767.0f * 0.5f;
    priv->gain = FIXED_GAIN;
    short tmp1 = 32767.0f * 0.4f;
    priv->dry = tmp1 * SCALE_DRY;
    priv->is_reverb_on = false;
}

static int xmly_reverb_init(EffectContext *ctx, int argc, char **argv) {
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
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
    // LIVE
    priv->comb_buf_len[0] = 916;
    priv->comb_buf_len[1] = 988;
    priv->comb_buf_len[2] = 1077;
    priv->comb_buf_len[3] = 1156;
    priv->comb_buf_len[4] = 1222;
    priv->comb_buf_len[5] = 1291;
    priv->comb_buf_len[6] = 1357;
    priv->comb_buf_len[7] = 1417;
    priv->allpass_buf_len[0] = 1500;
    priv->allpass_buf_len[1] = 2500;
    priv->allpass_buf_len[2] = 3000;
    priv->allpass_buf_len[3] = 3500;
    for (short i = 0; i < COMB_NUM; ++i) {
        priv->comb_buf[i] = (int *)calloc(priv->comb_buf_len[i], sizeof(int));
        if (NULL == priv->comb_buf[i]) {
            ret = AEERROR_NOMEM;
            goto end;
        }
    }
    for (short i = 0; i < ALLPASS_NUM; ++i) {
        priv->allpass_buf[i] =
            (int *)calloc(priv->allpass_buf_len[i], sizeof(int));
        if (NULL == priv->allpass_buf[i]) {
            ret = AEERROR_NOMEM;
            goto end;
        }
    }
    init_self_parameter(priv);

end:
    if (ret < 0) xmly_reverb_close(ctx);
    return ret;
}

static void reverb_set_mode(priv_t *priv, const char *mode) {
    LogInfo("%s mode(%s).\n", __func__, mode);
    priv->is_reverb_on = true;
    if (0 == strcasecmp(mode, "KTV")) {
        priv->comb_buf_len[0] = 316;
        priv->comb_buf_len[1] = 388;
        priv->comb_buf_len[2] = 477;
        priv->comb_buf_len[3] = 556;
        priv->comb_buf_len[4] = 622;
        priv->comb_buf_len[5] = 691;
        priv->comb_buf_len[6] = 757;
        priv->comb_buf_len[7] = 817;
        priv->allpass_buf_len[0] = 1500;
        priv->allpass_buf_len[1] = 2500;
        priv->allpass_buf_len[2] = 3000;
        priv->allpass_buf_len[3] = 3500;
    } else if (0 == strcasecmp(mode, "LIVE")) {
        priv->comb_buf_len[0] = 916;
        priv->comb_buf_len[1] = 988;
        priv->comb_buf_len[2] = 1077;
        priv->comb_buf_len[3] = 1156;
        priv->comb_buf_len[4] = 1222;
        priv->comb_buf_len[5] = 1291;
        priv->comb_buf_len[6] = 1357;
        priv->comb_buf_len[7] = 1417;
        priv->allpass_buf_len[0] = 1500;
        priv->allpass_buf_len[1] = 2500;
        priv->allpass_buf_len[2] = 3000;
        priv->allpass_buf_len[3] = 3500;
    } else if (0 == strcasecmp(mode, "SMALL_ROOM")) {
        priv->comb_buf_len[0] = 616;
        priv->comb_buf_len[1] = 688;
        priv->comb_buf_len[2] = 777;
        priv->comb_buf_len[3] = 856;
        priv->comb_buf_len[4] = 922;
        priv->comb_buf_len[5] = 991;
        priv->comb_buf_len[6] = 1057;
        priv->comb_buf_len[7] = 1117;
        priv->allpass_buf_len[0] = 1500;
        priv->allpass_buf_len[1] = 2500;
        priv->allpass_buf_len[2] = 3000;
        priv->allpass_buf_len[3] = 3500;
    } else {
        priv->is_reverb_on = false;
        priv->comb_buf_len[0] = 1000;
        priv->comb_buf_len[1] = 1050;
        priv->comb_buf_len[2] = 1100;
        priv->comb_buf_len[3] = 1150;
        priv->comb_buf_len[4] = 1200;
        priv->comb_buf_len[5] = 1250;
        priv->comb_buf_len[6] = 1300;
        priv->comb_buf_len[7] = 1350;
        priv->allpass_buf_len[0] = 110;
        priv->allpass_buf_len[1] = 170;
        priv->allpass_buf_len[2] = 220;
        priv->allpass_buf_len[3] = 270;
    }
}

static int xmly_reverb_set(EffectContext *ctx, const char *key, int flags) {
    assert(NULL != ctx);

    priv_t *priv = ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);
        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, "mode")) {
            reverb_set_mode(priv, entry->value);
        } else if (0 == strcasecmp(entry->key, "room_size")) {
            float room_size = atof(entry->value);
            room_size = FFMIN(1.0, FFMAX(0.0, room_size));
            short tmp = room_size * 32767;
            priv->comb_feedback =
                (tmp * SCALE_ROOM + (int)(OFFSET_ROOM << 15)) >> 15;
            LogInfo("comb_feedback = %d\n", priv->comb_feedback);
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    }
    return 0;
}

static int xmly_reverb_send(EffectContext *ctx, const void *samples,
                            const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int xmly_reverb_receive(EffectContext *ctx, void *samples,
                               const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);
    assert(NULL != priv->fifo_out);

    if (priv->is_reverb_on) {
        sdl_mutex_lock(priv->sdl_mutex);
        size_t nb_samples = fifo_read(priv->fifo_in, samples, max_nb_samples);
        int16_t *x = samples;
        for (size_t i = 0; i < nb_samples; ++i) {
            int32_t tmp32no0 = (priv->gain * x[i]) >> 1;
            int32_t tmp32no1 = tmp32no0;
            int32_t tmp32no2 = tmp32no0;
            int32_t tmp32no3 = tmp32no0;
            // comb filter 0-3
            int32_t comb_out0 = priv->comb_buf[0][priv->comb_buf_idx[0]];
            int32_t comb_out1 = priv->comb_buf[1][priv->comb_buf_idx[1]];
            int32_t comb_out2 = priv->comb_buf[2][priv->comb_buf_idx[2]];
            int32_t comb_out3 = priv->comb_buf[3][priv->comb_buf_idx[3]];
            int32_t comb_out = comb_out0 + comb_out1 + comb_out2 + comb_out3;

            int32_t tmp32no4 =
                SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[0]);
            int32_t tmp32no5 =
                SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[1]);
            int32_t tmp32no6 =
                SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[2]);
            int32_t tmp32no7 =
                SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[3]);

            int32_t tmp32no8 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out0);
            int32_t tmp32no9 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out1);
            int32_t tmp32no10 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out2);
            int32_t tmp32no11 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out3);

            priv->comb_state[0] = (tmp32no4 + tmp32no8) << 1;
            priv->comb_state[1] = (tmp32no5 + tmp32no9) << 1;
            priv->comb_state[2] = (tmp32no6 + tmp32no10) << 1;
            priv->comb_state[3] = (tmp32no7 + tmp32no11) << 1;

            tmp32no4 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[0]);
            tmp32no5 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[1]);
            tmp32no6 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[2]);
            tmp32no7 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[3]);

            priv->comb_buf[0][priv->comb_buf_idx[0]] = (tmp32no4 + tmp32no0)
                                                       << 1;
            priv->comb_buf[1][priv->comb_buf_idx[1]] = (tmp32no5 + tmp32no1)
                                                       << 1;
            priv->comb_buf[2][priv->comb_buf_idx[2]] = (tmp32no6 + tmp32no2)
                                                       << 1;
            priv->comb_buf[3][priv->comb_buf_idx[3]] = (tmp32no7 + tmp32no3)
                                                       << 1;

            // comb filter 4-7
            comb_out0 = priv->comb_buf[4][priv->comb_buf_idx[4]];
            comb_out1 = priv->comb_buf[5][priv->comb_buf_idx[5]];
            comb_out2 = priv->comb_buf[6][priv->comb_buf_idx[6]];
            comb_out3 = priv->comb_buf[7][priv->comb_buf_idx[7]];
            comb_out += (comb_out0 + comb_out1 + comb_out2 + comb_out3);

            tmp32no4 = SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[4]);
            tmp32no5 = SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[5]);
            tmp32no6 = SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[6]);
            tmp32no7 = SPL_MUL_16_32_RSFT16(priv->damp1, priv->comb_state[7]);

            tmp32no8 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out0);
            tmp32no9 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out1);
            tmp32no10 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out2);
            tmp32no11 = SPL_MUL_16_32_RSFT16(priv->damp2, comb_out3);

            priv->comb_state[4] = (tmp32no4 + tmp32no8) << 1;
            priv->comb_state[5] = (tmp32no5 + tmp32no9) << 1;
            priv->comb_state[6] = (tmp32no6 + tmp32no10) << 1;
            priv->comb_state[7] = (tmp32no7 + tmp32no11) << 1;

            tmp32no4 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[4]);
            tmp32no5 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[5]);
            tmp32no6 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[6]);
            tmp32no7 =
                SPL_MUL_16_32_RSFT16(priv->comb_feedback, priv->comb_state[7]);

            priv->comb_buf[4][priv->comb_buf_idx[4]] = (tmp32no4 + tmp32no0)
                                                       << 1;
            priv->comb_buf[5][priv->comb_buf_idx[5]] = (tmp32no5 + tmp32no1)
                                                       << 1;
            priv->comb_buf[6][priv->comb_buf_idx[6]] = (tmp32no6 + tmp32no2)
                                                       << 1;
            priv->comb_buf[7][priv->comb_buf_idx[7]] = (tmp32no7 + tmp32no3)
                                                       << 1;

            if (++priv->comb_buf_idx[0] == priv->comb_buf_len[0])
                priv->comb_buf_idx[0] = 0;
            if (++priv->comb_buf_idx[1] == priv->comb_buf_len[1])
                priv->comb_buf_idx[1] = 0;
            if (++priv->comb_buf_idx[2] == priv->comb_buf_len[2])
                priv->comb_buf_idx[2] = 0;
            if (++priv->comb_buf_idx[3] == priv->comb_buf_len[3])
                priv->comb_buf_idx[3] = 0;
            if (++priv->comb_buf_idx[4] == priv->comb_buf_len[4])
                priv->comb_buf_idx[4] = 0;
            if (++priv->comb_buf_idx[5] == priv->comb_buf_len[5])
                priv->comb_buf_idx[5] = 0;
            if (++priv->comb_buf_idx[6] == priv->comb_buf_len[6])
                priv->comb_buf_idx[6] = 0;
            if (++priv->comb_buf_idx[7] == priv->comb_buf_len[7])
                priv->comb_buf_idx[7] = 0;

            // all-pass filter 0
            tmp32no0 = priv->allpass_buf[0][priv->allpass_buf_idx[0]];
            tmp32no1 = SPL_MUL_16_32_RSFT16(priv->allpass_feedback, tmp32no0);
            priv->allpass_buf[0][priv->allpass_buf_idx[0]] =
                comb_out + (tmp32no1 << 1);
            tmp32no2 = SPL_MUL_16_32_RSFT16(
                priv->allpass_feedback,
                priv->allpass_buf[0][priv->allpass_buf_idx[0]]);
            int32_t allpass_out32 = tmp32no0 - (tmp32no2 << 1);

            // all-pass filter 1
            tmp32no0 = priv->allpass_buf[1][priv->allpass_buf_idx[1]];
            tmp32no1 = SPL_MUL_16_32_RSFT16(priv->allpass_feedback, tmp32no0);
            priv->allpass_buf[1][priv->allpass_buf_idx[1]] =
                allpass_out32 + (tmp32no1 << 1);
            tmp32no2 = SPL_MUL_16_32_RSFT16(
                priv->allpass_feedback,
                priv->allpass_buf[1][priv->allpass_buf_idx[1]]);
            allpass_out32 = tmp32no0 - (tmp32no2 << 1);

            // all-pass filter 2
            tmp32no0 = priv->allpass_buf[2][priv->allpass_buf_idx[2]];
            tmp32no1 = SPL_MUL_16_32_RSFT16(priv->allpass_feedback, tmp32no0);
            priv->allpass_buf[2][priv->allpass_buf_idx[2]] =
                allpass_out32 + (tmp32no1 << 1);
            tmp32no2 = SPL_MUL_16_32_RSFT16(
                priv->allpass_feedback,
                priv->allpass_buf[2][priv->allpass_buf_idx[2]]);
            allpass_out32 = tmp32no0 - (tmp32no2 << 1);

            // all-pass filter 3
            tmp32no0 = priv->allpass_buf[3][priv->allpass_buf_idx[3]];
            tmp32no1 = SPL_MUL_16_32_RSFT16(priv->allpass_feedback, tmp32no0);
            priv->allpass_buf[3][priv->allpass_buf_idx[3]] =
                allpass_out32 + (tmp32no1 << 1);
            tmp32no2 = SPL_MUL_16_32_RSFT16(
                priv->allpass_feedback,
                priv->allpass_buf[3][priv->allpass_buf_idx[3]]);
            allpass_out32 = tmp32no0 - (tmp32no2 << 1);

            if (++priv->allpass_buf_idx[0] == priv->allpass_buf_len[0])
                priv->allpass_buf_idx[0] = 0;
            if (++priv->allpass_buf_idx[1] == priv->allpass_buf_len[1])
                priv->allpass_buf_idx[1] = 0;
            if (++priv->allpass_buf_idx[2] == priv->allpass_buf_len[2])
                priv->allpass_buf_idx[2] = 0;
            if (++priv->allpass_buf_idx[3] == priv->allpass_buf_len[3])
                priv->allpass_buf_idx[3] = 0;

            tmp32no3 = FFMAX(-1073741824, FFMIN(1073741823, x[i] * priv->dry +
                                                                allpass_out32));
            x[i] = tmp32no3 >> 15;
        }
        fifo_write(priv->fifo_out, samples, nb_samples);
        sdl_mutex_unlock(priv->sdl_mutex);
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, samples, max_nb_samples);
            fifo_write(priv->fifo_out, samples, nb_samples);
        }
    }

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;
    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_xmly_reverb_fn(void) {
    static EffectHandler handler = {.name = "xmly_reverb",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = xmly_reverb_init,
                                    .set = xmly_reverb_set,
                                    .send = xmly_reverb_send,
                                    .receive = xmly_reverb_receive,
                                    .close = xmly_reverb_close};
    return &handler;
}
