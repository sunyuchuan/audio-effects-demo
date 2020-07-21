#ifndef XMLY_AUDIO_EFFECTS_H_
#define XMLY_AUDIO_EFFECTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct XmlyEffectContext_T XmlyEffectContext;

/**
 * @brief 创建 XmlyEffectContext
 *
 * @return XmlyEffectContext*
 */
XmlyEffectContext *create_xmly_effect();

/**
 * @brief 初始化XmlyEffectContext
 *
 * @param ctx
 * @return int
 */
int init_xmly_effect(XmlyEffectContext *ctx);

/**
 * @brief 设置参数
 *
 * @param ctx XmlyEffectContext
 * @param key 关键字
 * @param value 值
 * @param flags
 * @return int
 */
int set_xmly_effect(XmlyEffectContext *ctx, const char *key, const char *value,
                    int flags);

/**
 * @brief 传入需要处理的数据
 *
 * @param ctx XmlyEffectContext
 * @param samples 数据
 * @param nb_samples 样本点个数
 * @return int 返回处理的样本点个数
 */
int xmly_send_samples(XmlyEffectContext *ctx, const void *samples,
                      const size_t nb_samples);

/**
 * @brief 去除处理后的数据
 *
 * @param ctx XmlyEffectContext
 * @param samples 处理后的数据
 * @param max_nb_samples buffer的容量
 * @return int 实际填充的样本点个数
 */
int xmly_receive_samples(XmlyEffectContext *ctx, void *samples,
                         const size_t max_nb_samples);

/**
 * @brief 释放 XmlyEffectContext
 *
 * @param ctx
 */
void free_xmly_effect(XmlyEffectContext **ctx);

#ifdef __cplusplus
}
#endif
#endif  // XMLY_AUDIO_EFFECTS_H_
