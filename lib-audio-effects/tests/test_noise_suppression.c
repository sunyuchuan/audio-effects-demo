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
    size_t buffer_size = 1023;
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

    ctx = create_effect(find_effect("noise_suppression"), 44100, 1);
    ret = init_effect(ctx, 0, NULL);
    if (ret < 0) goto end;

    LogWarning("%s", show_usage(ctx));

    /******** 设置参数(可以不设置，采用默认参数) ********/
    set_effect(ctx, "low2mid_in_Hz", "4300", 0);
    set_effect(ctx, "mid2high_in_Hz", "21500", 0);
    set_effect(ctx, "all_band_gain_threshold", "0.0", 0);
    set_effect(ctx, "low_gain", "1.0", 0);
    set_effect(ctx, "mid_gain", "0.75", 0);
    set_effect(ctx, "high_gain", "0.4", 0);
    set_effect(ctx, "mid_freq_gain", "0.5", 0);
    set_effect(ctx, "is_enhance_mid_freq", "0", 0);
    /****************************************************/

    set_effect(ctx, "return_max_nb_samples", "True", 0);

    // 设置降噪开关
    set_effect(ctx, "Switch", "On", 0);

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
    LogInfo("time consuming %ld us\n", timer);
    return 0;
}