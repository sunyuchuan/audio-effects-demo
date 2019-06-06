#include <assert.h>
#include <stdio.h>
#include "effect_struct.h"
#include "error_def.h"
#include "logger.h"
#include "tools/fifo.h"

typedef struct {
    fifo *f;
} priv_t;

static int echo_close(EffectContext *ctx) {
    AeLogI("echo.c:%d %s.\n", __LINE__, __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->f) fifo_delete(&priv->f);
    }
    return 0;
}

static int echo_init(EffectContext *ctx, int argc, char **argv) {
    AeLogI("echo.c:%d %s.\n", __LINE__, __func__);
    for (int i = 0; i < argc; ++i) {
        AeLogI("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
    priv->f = fifo_create(sizeof(int16_t));
    if (NULL == priv->f) {
        ret = AEERROR_NOMEM;
        goto end;
    }
end:
    if (ret < 0) echo_close(ctx);
    return ret;
}

static int echo_set(EffectContext *ctx, const char *key, int flags) {
    return 0;
}

static int echo_send(EffectContext *ctx, const void *samples,
                     const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->f);

    return fifo_write(priv->f, samples, nb_samples);
}

static int echo_receive(EffectContext *ctx, void *samples,
                        const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->f);

    // 读取原始数据
    return fifo_read(priv->f, samples, max_nb_samples);
}

const EffectHandler *effect_echo_fn(void) {
    static EffectHandler handler = {.name = "echo",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = echo_init,
                                    .set = echo_set,
                                    .send = echo_send,
                                    .receive = echo_receive,
                                    .close = echo_close};
    return &handler;
}
