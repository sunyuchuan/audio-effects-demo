package com.xmly.audio_effects;

import android.os.Build;
import android.util.Log;

/**
 * 为录制的人声pcm数据添加特效
 *
 * Created by sunyc on 19-11-19.
 */
public class XmAudioEffects {
    private static final String TAG = "XmAudioEffects";
    private static final int DefaultSampleRate = 44100;
    private static final int DefaultChannelNumber = 1;
    // 日志输出模式
    // 不输出
    public static final int LOG_MODE_NONE = 0;
    // 输出到指定文件
    public static final int LOG_MODE_FILE = 1;
    // 输出到android日志系统logcat
    public static final int LOG_MODE_ANDROID = 2;
    // 在电脑上调试测试代码时,直接打印log到终端屏幕
    public static final int LOG_MODE_SCREEN = 3;
    //日志级别
    public static final int LOG_LEVEL_TRACE = 0;
    public static final int LOG_LEVEL_DEBUG = 1;
    public static final int LOG_LEVEL_VERBOSE = 2;
    public static final int LOG_LEVEL_INFO = 3;
    public static final int LOG_LEVEL_WARNING = 4;
    public static final int LOG_LEVEL_ERROR = 5;
    public static final int LOG_LEVEL_FATAL = 6;
    public static final int LOG_LEVEL_PANIC = 7;
    public static final int LOG_LEVEL_QUIET = 8;
    //是否加载过so
    private static boolean mIsLibLoaded = false;
    //本地XmAudioGenerator对象
    private long mNativeXmAudioEffects = 0;

    private static final LibLoader sLocalLibLoader = new LibLoader() {
        @Override
        public void loadLibrary(String libName) throws UnsatisfiedLinkError, SecurityException {
            String ABI = Build.CPU_ABI;
            Log.i(TAG, "ABI " + ABI + " libName " +libName);
            System.loadLibrary(libName + "-" + ABI);
        }
    };

    private static void loadLibrariesOnce(LibLoader libLoader) {
        synchronized (XmAudioEffects.class) {
            if (!mIsLibLoaded) {
                if (libLoader == null)
                    libLoader = sLocalLibLoader;

                libLoader.loadLibrary("ijkffmpeg");
                libLoader.loadLibrary("xmaudio_effects");
                mIsLibLoaded = true;
            }
        }
    }

    private void init() {
        setLogModeAndLevel(LOG_MODE_ANDROID, LOG_LEVEL_DEBUG, null);
        try {
            native_setup();
            native_init(DefaultSampleRate, DefaultChannelNumber);
        } catch (OutOfMemoryError e) {
            e.printStackTrace();
        }
    }

    public XmAudioEffects()
    {
        loadLibrariesOnce(sLocalLibLoader);
        init();
    }

    public XmAudioEffects(LibLoader libLoader)
    {
        loadLibrariesOnce(libLoader);
        init();
    }

    /**
     * native层代码的日志控制
     * @param logMode 日志打印模式:
     *                LOG_MODE_FILE 输出日志到文件
     *                LOG_MODE_ANDROID 输出到android的日志系统logcat
     *                LOG_MODE_SCREEN ubuntu等电脑操作系统打印在终端窗口上显示
     * @param logLevel 日志级别 LOG_LEVEL_DEBUG等
     * @param outLogPath 如果输出到文件,需要设置文件路径
     */
    public void setLogModeAndLevel(int logMode, int logLevel, String outLogPath) {
        if (logMode == LOG_MODE_FILE && outLogPath == null) {
            Log.e(TAG, "Input Params is inValid, exit");
            return;
        }

        native_set_log(logMode, logLevel, outLogPath);
    }

    /**
     * 关闭native日志输出
     */
    public void closeLogFile() {
        try {
            native_close_log_file();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
    }

    /**
     * 初始化
     */
    public void initEffects(int sampleRate, int nbChannels) {
        try {
            native_init(sampleRate, nbChannels);
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
    }

    /**
     * 配置特效类型
     */
    public int setEffects(String key, String value, int flag) {
        int ret = -1;
        if (key == null || value == null) return ret;
        try {
            ret = native_setEffects(key, value, flag);
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
        return ret;
    }

    /**
     * 发送原始样本数据
     */
    public int sendSamples(short[] samples, int nb_samples) {
        int ret = -1;
        if (samples == null || nb_samples <= 0) return ret;
        try {
            ret = native_sendSamples(samples, nb_samples);
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
        return ret;
    }

    /**
     * 接收添加特效后的样本数据
     */
    public int reveiveSamples(short[] buffer, int max_nb_samples) {
        int ret = -1;
        if (buffer == null || max_nb_samples <= 0) return ret;
        try {
            ret = native_receiveSamples(buffer, max_nb_samples);
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
        return ret;
    }

    /**
     * 释放native内存
     */
    public void release() {
        try {
            native_release();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
    }

    private native void native_release();
    private native int native_receiveSamples(short[] buffer, int max_nb_samples);
    private native int native_sendSamples(short[] samples, int nb_samples);
    private native int native_setEffects(String key, String value, int flag);
    private native void native_init(int sampleRate, int nbChannels);
    private native void native_set_log(int logMode, int logLevel, String outLogPath);
    private native void native_close_log_file();
    private native void native_setup();

    @Override
    protected void finalize() throws Throwable {
        Log.i(TAG, "finalize");
        try {
            native_release();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        } finally {
            super.finalize();
        }
    }
}