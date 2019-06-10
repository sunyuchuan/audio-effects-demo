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

    ctx = create_effect(find_effect("equalizer"));
    ret = init_effect(ctx, 0, NULL);
    if (ret < 0) goto end;

    // 关闭均衡器
    set_effect(ctx, "mode", "None", 0);
    // 设置音效模式
    set_effect(ctx, "mode", "OldRadio", 0);
    // set_effect(ctx, "mode", "CleanVoice", 0);
    // set_effect(ctx, "mode", "Bass", 0);
    // set_effect(ctx, "mode", "LowVoice", 0);
    // set_effect(ctx, "mode", "Penetrating", 0);
    // set_effect(ctx, "mode", "Magnetic", 0);
    // set_effect(ctx, "mode", "SoftPitch", 0);

    // 增益均衡处理
    while (buffer_size ==
           fread(buffer, sizeof(short), buffer_size, pcm_reader)) {
        // 传入数据
        ret = send_samples(ctx, buffer, buffer_size);
        if (ret < 0) break;

        // 取出数据
        ret = receive_samples(ctx, buffer, buffer_size);
        while (ret > 0) {
            fwrite(buffer, sizeof(short), ret, pcm_writer);
            ret = receive_samples(ctx, buffer, buffer_size);
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
    if (ctx) {
        free_effect(ctx);
        ctx = NULL;
    }
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    AeLogI("time consuming %ld us\n", timer);
    return 0;
}