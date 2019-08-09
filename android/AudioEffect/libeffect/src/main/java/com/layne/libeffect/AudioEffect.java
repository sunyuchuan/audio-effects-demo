package com.layne.libeffect;

import android.os.Build;
import android.util.Log;

import static android.content.ContentValues.TAG;

public class AudioEffect {
    static {
        String ABI = Build.CPU_ABI;
        Log.i(TAG, "ABI " + ABI);

        System.loadLibrary("ijkffmpeg" + "-" + ABI);
        System.loadLibrary("audio-effect" + "-" + ABI);

//        System.loadLibrary("ijkffmpeg-armeabi-v7a");
//        System.loadLibrary("audio-effect-armeabi-v7a");
        register();
    }

    private final long mObject;

    public AudioEffect() {
        this.mObject = getNativeBean();
        init_xmly_effect();
    }

    private static native int register();

    private static native long getNativeBean();

    @Override
    protected void finalize() {
        releaseNativeBean();
    }

    private native void releaseNativeBean();

    // 以下方法引入自"xmly_audio_effects.h"

    public native int init_xmly_effect();

    public native int set_xmly_effect(String key, String value, int flags);
    public native int xmly_send_samples(short[] nearend, int nb_samples);

    public native int xmly_receive_samples(short[] samples, int max_nb_samples);
}
