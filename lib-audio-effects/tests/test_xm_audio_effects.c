#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include "voice_effect.h"
#include "file_helper.h"
#include "log.h"
#include "xm_audio_effects.h"
#define MAX_NB_MSG 10

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_NB_CHANNELS 1

typedef struct {
    int msg[MAX_NB_MSG];
    short msg_read;
    short msg_write;
} Message;

static FILE *pcm_reader = NULL;
static FILE *pcm_writer = NULL;
static Message msg_queue;
static short abort_request = 0;

static void message_put(const int msg) {
    msg_queue.msg[msg_queue.msg_write++] = msg;
    if (MAX_NB_MSG == msg_queue.msg_write) msg_queue.msg_write = 0;

    printf("test_xm_audio_effects.c:%d %s msg = %d %d %d.\n", __LINE__,
           __func__, msg, msg_queue.msg_read, msg_queue.msg_write);
}

static int message_get() {
    if (msg_queue.msg_read == msg_queue.msg_write) return 0;
    int msg = msg_queue.msg[msg_queue.msg_read++];
    if (MAX_NB_MSG == msg_queue.msg_read) msg_queue.msg_read = 0;
    printf("test_xm_audio_effects.c:%d %s msg = %d.\n", __LINE__, __func__,
           msg);
    return msg;
}

static int open_input_output(int argc, char **argv) {
    if (argc < 3) return -1;
    int ret = 0;
    // 打开输入文件
    ret = OpenFile(&pcm_reader, argv[1], 0);
    if (ret < 0) return ret;
    // 打开输出文件
    ret = OpenFile(&pcm_writer, argv[2], 1);
    if (ret < 0) return ret;

    return ret;
}

static void close_input_output() {
    if (pcm_reader) {
        fclose(pcm_reader);
        pcm_reader = NULL;
    }
    if (pcm_writer) {
        fclose(pcm_writer);
        pcm_writer = NULL;
    }
}

static void show_menu() {
    printf("\n\n===================================================\n");
    printf("[.打开降噪          ].关闭降噪\n");
    printf("Q.原声              W.礼堂\n");
    printf("E.演唱会            R.机器人\n");
    printf("T.小黄人            Y.男声\n");
    printf("U.女声\n");
    printf("A.原声              S.清晰\n");
    printf("D.沉稳              F.低音\n");
    printf("G.明亮              H.磁性\n");
    printf("J.柔和\n");
    printf("Esc.退出\n");
    printf("======================================================\n\n");
}

static int process_message(XmEffectContext *ctx) {
    int msg = message_get();
    if (0 == msg) return 0;

    int ret = 0;
    switch (msg) {
        case 91:  // 按下 [
            LogInfo("NoiseSuppression_Switch = 1\n");
            ret = set_xm_effect(ctx, "NoiseSuppression", "On", 0);
            break;
        case 93:  // 按下 ]
            LogInfo("NoiseSuppression_Switch = 0\n");
            ret = set_xm_effect(ctx, "NoiseSuppression", "Off", 0);
            break;
        case 49:  // 按下 1
            LogInfo("Environment = None\n");
            ret = set_xm_effect(ctx, "Environment", "None", 0);
            break;
        case 50:  // 按下 2
            LogInfo("Environment = Church\n");
            ret = set_xm_effect(ctx, "Environment", "Church", 0);
            break;
        case 51:  // 按下 3
            LogInfo("Environment = Live\n");
            ret = set_xm_effect(ctx, "Environment", "Live", 0);
            break;
        case 113:  // 按下 Q
            LogInfo("Special = Original\n");
            ret = set_xm_effect(ctx, "Special", "Original", 0);
            break;
        case 119:  // 按下 W
            LogInfo("Special = Church\n");
            ret = set_xm_effect(ctx, "Special", "Church", 0);
            break;
        case 101:  // 按下 E
            LogInfo("Special = Live\n");
            ret = set_xm_effect(ctx, "Special", "Live", 0);
            break;
        case 114:  // 按下 R
            LogInfo("Special = Robot\n");
            ret = set_xm_effect(ctx, "Special", "Robot", 0);
            break;
        case 116:  // 按下 T
            LogInfo("Special = Mimions\n");
            ret = set_xm_effect(ctx, "Special", "Mimions", 0);
            break;
        case 121:  // 按下 Y
            LogInfo("Special = Man\n");
            ret = set_xm_effect(ctx, "Special", "Man", 0);
            break;
        case 117:  // 按下 U
            LogInfo("Special = Women\n");
            ret = set_xm_effect(ctx, "Special", "Women", 0);
            break;
        case 97:  // 按下 A
            LogInfo("Beautify = None\n");
            ret = set_xm_effect(ctx, "Beautify", "None", 0);
            break;
        case 115:  // 按下 S
            LogInfo("Beautify = CleanVoice\n");
            ret = set_xm_effect(ctx, "Beautify", "CleanVoice", 0);
            break;
        case 100:  // 按下 D
            LogInfo("Beautify = Bass\n");
            ret = set_xm_effect(ctx, "Beautify", "Bass", 0);
            break;
        case 102:  // 按下 F
            LogInfo("Beautify = LowVoice\n");
            ret = set_xm_effect(ctx, "Beautify", "LowVoice", 0);
            break;
        case 103:  // 按下 G
            LogInfo("Beautify = Penetrating\n");
            ret = set_xm_effect(ctx, "Beautify", "Penetrating", 0);
            break;
        case 104:  // 按下 H
            LogInfo("Beautify = Magnetic\n");
            ret = set_xm_effect(ctx, "Beautify", "Magnetic", 0);
            break;
        case 106:  // 按下 J
            LogInfo("Beautify = SoftPitch\n");
            ret = set_xm_effect(ctx, "Beautify", "SoftPitch", 0);
            break;
        default:
            break;
    }
    return ret;
}

void *xm_effects_thread(__attribute__((unused)) void *arg) {
    int ret = 0;
    int buffer_size = 1024;
    int16_t buffer[buffer_size];

    XmEffectContext *ctx = create_xm_effect_context();
    ret = init_xm_effect_context(ctx, DEFAULT_SAMPLE_RATE, DEFAULT_NB_CHANNELS);
    if (ret < 0) {
        LogError("%s init_xm_effect_context error.\n", __func__);
        goto end;
    }

    set_xm_effect(ctx, "return_max_nb_samples", "True", 0);

    while (!abort_request) {
        // 处理消息
        ret = process_message(ctx);
        if (ret < 0) break;
        // 读取数据
        ret = fread(buffer, sizeof(int16_t), buffer_size, pcm_reader);
        if (ret <= 0) {
            LogError("%s fread error(ret = %d).\n", __func__, ret);
            fseek(pcm_reader, 0L, SEEK_SET);
            continue;
        }
        // 传入数据
        ret = xm_effect_send_samples(ctx, buffer, buffer_size);
        if (ret < 0) break;
        // 取出数据
        ret = xm_effect_receive_samples(ctx, buffer, buffer_size);
        while (ret > 0) {
            fwrite(buffer, sizeof(int16_t), ret, pcm_writer);
            ret = xm_effect_receive_samples(ctx, buffer, buffer_size);
        }
        usleep(1000);
    }

end:
    if (ctx) free_xm_effect_context(&ctx);
    return NULL;
}

static int scan_keyboard() {
    int in;
    struct termios new_settings;
    struct termios stored_settings;
    tcgetattr(0, &stored_settings);
    new_settings = stored_settings;
    new_settings.c_lflag &= (~ICANON);
    new_settings.c_cc[VTIME] = 0;
    tcgetattr(0, &stored_settings);
    new_settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_settings);

    in = getchar();

    tcsetattr(0, TCSANOW, &stored_settings);

    printf(" key down %d\n", in);
    return in;
}

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    if (argc < 2) {
        LogWarning("Usage %s input_pcm_file output_pcm_file\n", argv[0]);
        return 0;
    }

    // 打开输入输出文件
    open_input_output(argc, argv);
    memset(&msg_queue, 0, sizeof(msg_queue));

    pthread_t xmly_effects_tid = 0;
    int ret =
        pthread_create(&xmly_effects_tid, NULL, xm_effects_thread, NULL);
    if (ret) {
        LogError("Error:unable to create thread, %d\n", ret);
        goto end;
    }

    while (!abort_request) {
        show_menu();
        int command = scan_keyboard();
        if (27 == command)
            abort_request = 1;
        else
            message_put(command);
    }

end:
    pthread_join(xmly_effects_tid, NULL);
    // 关闭输入输出文件
    close_input_output();
    return 0;
}