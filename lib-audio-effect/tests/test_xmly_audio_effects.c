#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <termio.h>
#include <unistd.h>
#include "effects.h"
#include "file_helper.h"
#include "logger.h"
#include "xmly_audio_effects.h"

#define MAX_NB_MSG 10

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

    printf("test_xmly_audio_effects.c:%d %s msg = %d %d %d.\n", __LINE__,
           __func__, msg, msg_queue.msg_read, msg_queue.msg_write);
}

static int message_get() {
    if (msg_queue.msg_read == msg_queue.msg_write) return 0;
    int msg = msg_queue.msg[msg_queue.msg_read++];
    if (MAX_NB_MSG == msg_queue.msg_read) msg_queue.msg_read = 0;
    printf("test_xmly_audio_effects.c:%d %s msg = %d.\n", __LINE__, __func__,
           msg);
    return msg;
}

static int open_input_output(int argc, char **argv) {
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
    printf("1.无环境声          2.山谷\n");
    printf("3.礼堂              4.教室\n");
    printf("5.现场演出\n");
    printf("Q.原声              W.机器人\n");
    printf("E.小黄人            R.明亮\n");
    printf("T.男声              Y.女声\n");
    printf("Esc.退出\n");
    printf("======================================================\n\n");
}

static int process_message(XmlyEffectContext *ctx) {
    int msg = message_get();
    if (0 == msg) return 0;

    int ret = 0;
    switch (msg) {
        case 91:  // 按下 [
            AeLogI("NoiseSuppression_Switch = 1\n");
            ret = set_xmly_effect(ctx, "NoiseSuppression", "On", 0);
            break;
        case 93:  // 按下 ]
            AeLogI("NoiseSuppression_Switch = 0\n");
            ret = set_xmly_effect(ctx, "NoiseSuppression", "Off", 0);
            break;
        case 49:  // 按下 1
            AeLogI("Environment = None\n");
            ret = set_xmly_effect(ctx, "Environment", "None", 0);
            break;
        case 50:  // 按下 2
            AeLogI("Environment = Valley\n");
            ret = set_xmly_effect(ctx, "Environment", "Valley", 0);
            break;
        case 51:  // 按下 3
            AeLogI("Environment = Church\n");
            ret = set_xmly_effect(ctx, "Environment", "Church", 0);
            break;
        case 52:  // 按下 4
            AeLogI("Environment = Classroom\n");
            ret = set_xmly_effect(ctx, "Environment", "Classroom", 0);
            break;
        case 53:  // 按下 5
            AeLogI("Environment = Live\n");
            ret = set_xmly_effect(ctx, "Environment", "Live", 0);
            break;
        case 113:  // 按下 Q
            AeLogI("Morph = Original\n");
            ret = set_xmly_effect(ctx, "Morph", "Original", 0);
            break;
        case 119:  // 按下 W
            AeLogI("Morph = Robot\n");
            ret = set_xmly_effect(ctx, "Morph", "Robot", 0);
            break;
        case 101:  // 按下 E
            AeLogI("Morph = Mimions\n");
            ret = set_xmly_effect(ctx, "Morph", "Mimions", 0);
            break;
        case 114:  // 按下 R
            AeLogI("Morph = Bright\n");
            ret = set_xmly_effect(ctx, "Morph", "Bright", 0);
            break;
        case 116:  // 按下 T
            AeLogI("Morph = Man\n");
            ret = set_xmly_effect(ctx, "Morph", "Man", 0);
            break;
        case 121:  // 按下 Y
            AeLogI("Morph = Women\n");
            ret = set_xmly_effect(ctx, "Morph", "Women", 0);
            break;
        default:
            break;
    }
    return ret;
}

void *xmly_effects_thread(void *arg) {
    int ret = 0;
    int buffer_size = 1024;
    short buffer[buffer_size];

    XmlyEffectContext *ctx = create_xmly_effect();
    ret = init_xmly_effect(ctx);
    if (ret < 0) {
        AeLogE("test_xmly_audio_effects.c:%d %s init_xmly_effect error.\n",
               __LINE__, __func__);
        goto end;
    }

    while (!abort_request) {
        // 处理消息
        ret = process_message(ctx);
        if (ret < 0) break;
        // 读取数据
        ret = fread(buffer, sizeof(short), buffer_size, pcm_reader);
        if (ret <= 0) {
            AeLogE("test_xmly_audio_effects.c:%d %s fread error(ret = %d).\n",
                   __LINE__, __func__, ret);
            fseek(pcm_reader, 0L, SEEK_SET);
            continue;
        }
        // 传入数据
        ret = xmly_send_samples(ctx, buffer, buffer_size);
        if (ret < 0) break;
        // 取出数据
        ret = xmly_receive_samples(ctx, buffer, buffer_size);
        while (ret > 0) {
            fwrite(buffer, sizeof(short), ret, pcm_writer);
            ret = xmly_receive_samples(ctx, buffer, buffer_size);
        }
        usleep(1000);
    }

end:
    if (ctx) free_xmly_effect(&ctx);
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
    AeSetLogLevel(kLogLevelAll);
    AeSetLogMode(kLogModeScreen);

    if (argc < 2) {
        AeLogW("Usage %s input_pcm_file output_pcm_file\n", argv[0]);
        return 0;
    }

    // 打开输入输出文件
    open_input_output(argc, argv);
    memset(&msg_queue, 0, sizeof(msg_queue));

    pthread_t xmly_effects_tid = 0;
    int ret =
        pthread_create(&xmly_effects_tid, NULL, xmly_effects_thread, NULL);
    if (ret) {
        AeLogE("Error:unable to create thread, %d\n", ret);
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