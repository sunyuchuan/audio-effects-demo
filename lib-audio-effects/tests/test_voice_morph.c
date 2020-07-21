#include <sys/time.h>
#include "voice_effect.h"
#include "file_helper.h"
#include "log.h"

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    if (argc < 2) {
        LogWarning("Usage %s input_pcm_file output_pcm_file\n", argv[0]);
        return 0;
    }

    int ret = 0;
    size_t buffer_size = 1024;
    short buffer[buffer_size];
    size_t count = 0;
    FILE *pcm_reader = NULL;
    FILE *pcm_writer = NULL;
    EffectContext *ctx = NULL;
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

    ctx = create_effect(find_effect("voice_morph"), 44100, 1);
    ret = init_effect(ctx, 0, NULL);
    if (ret < 0) goto end;

    // 打开变声模式
    ret = set_effect(ctx, "morph_on", "1", 0);
    if (ret < 0) goto end;

    // 设置变声模式
    ret = set_effect(ctx, "mode", "original", 0);
    if (ret < 0) goto end;

    // 变声处理
    while (buffer_size ==
           fread(buffer, sizeof(short), buffer_size, pcm_reader)) {
        if (200 == count) {
            ret = set_effect(ctx, "mode", "robot", 0);
            if (ret < 0) goto end;
        } else if (400 == count) {
            ret = set_effect(ctx, "mode", "man", 0);
            if (ret < 0) goto end;
        } else if (600 == count) {
            ret = set_effect(ctx, "mode", "women", 0);
            if (ret < 0) goto end;
        } else if (800 == count) {
            ret = set_effect(ctx, "mode", "bright", 0);
            if (ret < 0) goto end;
        } else if (1000 == count) {
            ret = set_effect(ctx, "mode", "original", 0);
            if (ret < 0) goto end;
        }
        // 传入数据
        ret = send_samples(ctx, buffer, buffer_size);
        if (ret < 0) break;

        // 取出数据
        ret = receive_samples(ctx, buffer, buffer_size);
        while (ret > 0) {
            fwrite(buffer, sizeof(short), ret, pcm_writer);
            ret = receive_samples(ctx, buffer, buffer_size);
        }
        ++count;
    }

end:
    if (ret < 0) LogError("error ret = %d\n", ret);
    if (pcm_reader) {
        fclose(pcm_reader);
        pcm_reader = NULL;
    }
    if (pcm_writer) {
        fclose(pcm_writer);
        pcm_writer = NULL;
    }
    if (ctx) {
        free_effect(ctx);
        ctx = NULL;
    }
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);
    return 0;
}