#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ERROR_OPEN_LOG_FILE -1000

// 日志输出方式
enum AeLogWriterMode {
    kLogModeNone = 0,  // 不输出
    kLogModeFile,      // 写文件
    kLogModeAndroid,   // 导出到Android Logcat
    kLogModeScreen     // 输出到屏幕
};

//日志级别
enum AeLogWriterLevel {
    kLogLevelAll = 0,  //所有信息都写日志
    kLogLevelDebug,    // 写Debug信息
    kLogLevelInfo,     // 写运行信息
    kLogLevelWarning,  //写警告信息
    kLogLevelError,    //写错误信息
    kLogLevelOff       //不写日志
};

int AeSetLogPath(const char *path);
void AeSetLogMode(const enum AeLogWriterMode mode);
void AeSetLogLevel(const enum AeLogWriterLevel level);
void AeLogD(const char *info, ...);
void AeLogI(const char *info, ...);
void AeLogW(const char *info, ...);
void AeLogE(const char *info, ...);

#ifdef __cplusplus
}
#endif

#endif  // LOGGER_H_
