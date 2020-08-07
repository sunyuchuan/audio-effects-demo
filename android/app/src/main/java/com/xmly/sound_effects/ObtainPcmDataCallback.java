package com.xmly.sound_effects;

/**
 * Created by sunyc on 20-8-7.
 */

public interface ObtainPcmDataCallback {
    int onObtainBuffer(byte[] data, int length);
}
