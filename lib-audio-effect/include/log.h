#ifndef LOG_H_
#define LOG_H_

#define ERROR_OPEN_LOG_FILE -1000

// 日志输出方式
typedef enum {
    LOG_MODE_NONE = 0,  // 不输出
    LOG_MODE_FILE,      // 写文件
    LOG_MODE_ANDROID,   // 导出到Android Logcat
    LOG_MODE_SCREEN     // 输出到屏幕
} LogMode;

// 日志级别
typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_PANIC,
    LOG_LEVEL_QUIET
} LogLevel;

#ifdef __cplusplus
extern "C" {
#endif

int AeSetLogPath(const char *path);
void AeSetLogMode(const LogMode mode);
void AeSetLogLevel(const LogLevel level);
void AePrintLog(const LogLevel level, const char *filename, const int line,
                const char *format, ...);

#define LogTrace(format, ...) \
    AePrintLog(LOG_LEVEL_TRACE, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogDebug(format, ...) \
    AePrintLog(LOG_LEVEL_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogVerbose(format, ...) \
    AePrintLog(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogInfo(format, ...) \
    AePrintLog(LOG_LEVEL_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogWarning(format, ...) \
    AePrintLog(LOG_LEVEL_WARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogError(format, ...) \
    AePrintLog(LOG_LEVEL_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogFatal(format, ...)                                   \
    do {                                                        \
        AePrintLog(LOG_LEVEL_FATAL, __FILE__, __LINE__, format, \
                   ##__VA_ARGS__);                              \
    } while (0)

#define LogPanic(format, ...)                                   \
    do {                                                        \
        AePrintLog(LOG_LEVEL_PANIC, __FILE__, __LINE__, format, \
                   ##__VA_ARGS__);                              \
    } while (0)

#ifdef __cplusplus
}
#endif
#endif  // LOG_H_
