#include "logger.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
    #include <sys/types.h>
    #include <unistd.h>
    #define gettid() getpid()
#elif __linux__
    #include <sys/syscall.h>
    #define gettid() syscall(__NR_gettid)
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "ap-log"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)
#endif

#define NONE "\e[0m"
#define BLACK "\e[0;30m"
#define L_BLACK "\e[1;30m"
#define RED "\e[0;31m"
#define L_RED "\e[1;31m"
#define GREEN "\e[0;32m"
#define L_GREEN "\e[1;32m"
#define YELLOW "\e[0;33m"
#define L_YELLOW "\e[1;33m"
#define BLUE "\e[0;34m"
#define L_BLUE "\e[1;34m"
#define PURPLE "\e[0;35m"
#define L_PURPLE "\e[1;35m"
#define CYAN "\e[0;36m"
#define L_CYAN "\e[1;36m"
#define GRAY "\e[0;37m"
#define WHITE "\e[1;37m"

#define MAX_BUFFER_SIZE 400


static enum AeLogWriterMode ae_log_mode = kLogModeNone;
static enum AeLogWriterLevel ae_log_level = kLogLevelError;
static pthread_mutex_t ae_log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *ae_log_file = NULL;
static char ae_buffer[MAX_BUFFER_SIZE];

//获取系统当前时间
static char *GetCurrentTime() {
    static time_t cur_time;
    static struct tm *system_time;
    static char system_time_in_string[20];

    time(&cur_time);
    system_time = localtime(&cur_time);
    strftime(system_time_in_string, sizeof(system_time_in_string),
             "%02m-%02d %H:%M:%S", system_time);
    return system_time_in_string;
}

static void Write2File(const char level, const char *info, va_list vl) {
    if (NULL == ae_log_file) return;
    sprintf(ae_buffer, "%s %d-%d/%c/", GetCurrentTime(), getpid(), gettid(),
            level);
    vsprintf(ae_buffer + strlen(ae_buffer), info, vl);
    vfprintf(ae_log_file, ae_buffer, vl);
}

static void Log2Screen(const char level, const char *info, va_list vl) {
    sprintf(ae_buffer, "%s %d-%d/%c/", GetCurrentTime(), getpid(), gettid(),
            level);
    vsprintf(ae_buffer + strlen(ae_buffer), info, vl);
    switch (level) {
        case 'D':
            printf(L_BLUE "%s" NONE, ae_buffer);
            break;
        case 'I':
            printf(L_GREEN "%s" NONE, ae_buffer);
            break;
        case 'W':
            printf(L_YELLOW "%s" NONE, ae_buffer);
            break;
        case 'E':
            printf(L_RED "%s" NONE, ae_buffer);
            break;
        default:
            printf("%s", ae_buffer);
            break;
    }
}

int AeSetLogPath(const char *path) {
    int ret = 0;

    pthread_mutex_lock(&ae_log_lock);

    if (ae_log_file) {
        fclose(ae_log_file);
        ae_log_file = NULL;
    }
    ae_log_file = fopen(path, "wb");
    if (!ae_log_file) {
        ret = ERROR_OPEN_LOG_FILE;
    } else if (setvbuf(ae_log_file, NULL, _IONBF, 0) != 0) {
        ret = ERROR_OPEN_LOG_FILE;
    }

    pthread_mutex_unlock(&ae_log_lock);
    return ret;
}

void AeSetLogMode(const enum AeLogWriterMode mode) {
    pthread_mutex_lock(&ae_log_lock);
    ae_log_mode = mode;
    pthread_mutex_unlock(&ae_log_lock);
}

void AeSetLogLevel(const enum AeLogWriterLevel level) {
    pthread_mutex_lock(&ae_log_lock);
    ae_log_level = level;
    pthread_mutex_unlock(&ae_log_lock);
}

void AeLogD(const char *info, ...) {
    if (kLogModeNone == ae_log_mode || ae_log_level > kLogLevelDebug) return;

    pthread_mutex_lock(&ae_log_lock);
    va_list vl;
    va_start(vl, info);
    if (kLogModeFile == ae_log_mode) {
        Write2File('D', info, vl);
    } else if (kLogModeAndroid == ae_log_mode) {
#ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_DEBUG, TAG, info, vl);
#endif
    } else if (kLogModeScreen == ae_log_mode) {
        Log2Screen('D', info, vl);
    }
    va_end(vl);
    pthread_mutex_unlock(&ae_log_lock);
}

void AeLogI(const char *info, ...) {
    if (kLogModeNone == ae_log_mode || ae_log_level > kLogLevelInfo) return;

    pthread_mutex_lock(&ae_log_lock);
    va_list vl;
    va_start(vl, info);
    if (kLogModeFile == ae_log_mode) {
        Write2File('I', info, vl);
    } else if (kLogModeAndroid == ae_log_mode) {
#ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_INFO, TAG, info, vl);
#endif
    } else if (kLogModeScreen == ae_log_mode) {
        Log2Screen('I', info, vl);
    }
    va_end(vl);
    pthread_mutex_unlock(&ae_log_lock);
}

void AeLogW(const char *info, ...) {
    if (kLogModeNone == ae_log_mode || ae_log_level > kLogLevelWarning) return;

    pthread_mutex_lock(&ae_log_lock);
    va_list vl;
    va_start(vl, info);
    if (kLogModeFile == ae_log_mode) {
        Write2File('W', info, vl);
    } else if (kLogModeAndroid == ae_log_mode) {
#ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_WARN, TAG, info, vl);
#endif
    } else if (kLogModeScreen == ae_log_mode) {
        Log2Screen('W', info, vl);
    }
    va_end(vl);
    pthread_mutex_unlock(&ae_log_lock);
}

void AeLogE(const char *info, ...) {
    if (kLogModeNone == ae_log_mode || ae_log_level > kLogLevelError) return;

    pthread_mutex_lock(&ae_log_lock);
    va_list vl;
    va_start(vl, info);
    if (kLogModeFile == ae_log_mode) {
        Write2File('E', info, vl);
    } else if (kLogModeAndroid == ae_log_mode) {
#ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_ERROR, TAG, info, vl);
#endif
    } else if (kLogModeScreen == ae_log_mode) {
        Log2Screen('E', info, vl);
    }
    va_end(vl);
    pthread_mutex_unlock(&ae_log_lock);
}
