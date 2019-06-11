#include <string.h>
#include <sys/time.h>
#include "effects.h"
#include "file_helper.h"
#include "logger.h"

int main(int argc, char **argv) {
    AeSetLogLevel(kLogLevelAll);
    AeSetLogMode(kLogModeScreen);

    if (argc < 2) {
        AeLogW("Usage %s input_pcm_file output_pcm_file\n", argv[0]);
        return 0;
    }

    int ret = 0;
    size_t buffer_size = 1024;
    short buffer[buffer_size];
    FILE *pcm_reader = NULL;
    FILE *pcm_writer = NULL;
    size_t nb_effects = 2;
    EffectContext *effects[nb_effects];
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    // 打开输入文件
    ret = OpenFile(&pcm_reader, argv[1], 0);
    if (ret < 0) goto end;
    // 打开输出文件
    ret = OpenFile(&pcm_writer, argv[2], 1);
    if (ret < 0) goto end;

    // 创建多个音效
    memset(effects, 0, nb_effects * sizeof(EffectContext *));
    effects[0] = create_effect(find_effect("xmly_echo"));
    effects[1] = create_effect(find_effect("equalizer"));

    for (size_t i = 0; i < nb_effects; ++i) {
        ret = init_effect(effects[i], 0, NULL);
        if (ret < 0) goto end;
    }

    // 设置音效模式
    ret = set_effect(effects[0], "mode", "Valley", 0);
    if (ret < 0) goto end;
    ret = set_effect(effects[1], "mode", "OldRadio", 0);
    if (ret < 0) goto end;

    // 音效处理
    while (buffer_size ==
           fread(buffer, sizeof(short), buffer_size, pcm_reader)) {
        ret = send_samples(effects[0], buffer, buffer_size);
        if (ret < 0) goto end;
        for (size_t i = 0; i < nb_effects - 1; ++i) {
            ret = receive_samples(effects[i], buffer, buffer_size);
            while (ret > 0) {
                ret = send_samples(effects[i + 1], buffer, buffer_size);
                if (ret < 0) break;
                ret = receive_samples(effects[i], buffer, buffer_size);
            }
        }
        ret = receive_samples(effects[nb_effects - 1], buffer, buffer_size);
        while (ret > 0) {
            fwrite(buffer, sizeof(short), buffer_size, pcm_writer);
            ret = receive_samples(effects[nb_effects - 1], buffer, buffer_size);
        }
    }

end:
    if (pcm_reader) {
        fclose(pcm_reader);
        pcm_reader = NULL;
    }
    if (pcm_writer) {
        fclose(pcm_writer);
        pcm_writer = NULL;
    }
    for (size_t i = 0; i < nb_effects; ++i) {
        if (effects[i]) {
            free_effect(effects[i]);
            effects[i] = NULL;
        }
    }
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    AeLogI("time consuming %ld us\n", timer);
    return 0;
}