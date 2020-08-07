package com.xmly.sound_effects;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

public class SearchPCMEncoderAAC {
    private final static String TAG = "SearchPCMEncoderAAC";
    //比特率64kbits
    private final static int KEY_BIT_RATE = 64000;
    //一次编码的数据大小 1024(采样点个数) * 2(采样深度16位，2字节)
    private final static int DEFAULT_FRAME_SIZE = 1024 * 2;
    //采样率
    private final static int DEFAULT_SAMPLE_RATE = 16000;
    //声道数
    private final static int DEFAULT_NB_CHANNEL = 1;
    private MediaCodec mMediaCodec;
    private ByteBuffer[] mEncodeInputBuffers;
    private ByteBuffer[] mEncodeOutputBuffers;
    private MediaCodec.BufferInfo mEncodeBufferInfo;
    private FileOutputStream mFos;
    private ByteArrayOutputStream mBos;
    //采样率
    private int mSampleRate = 16000;
    //声道数
    private int mNbChannels = 1;
    private boolean mFlushEncoder = false;

    public SearchPCMEncoderAAC(String filePath, int sample_rate, int channels) {
        mSampleRate = sample_rate;
        mNbChannels = channels;
        init(filePath);
    }

    /**
     * 获取编码器编码一次需要的数据长度
     * @return 帧大小，单位是字节
     */
    public int getFrameSizeInByte() {
        return mNbChannels * DEFAULT_FRAME_SIZE;
    }

    /**
     * 初始化AAC编码器
     */
    private void init(String filePath) {
        setOutputPath(filePath);
        mFlushEncoder = false;
        try {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                return;
            }

            // AAC 硬编码器
            try {
                mMediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
            } catch (Exception e) {
                e.printStackTrace();
                return;
            }

            MediaFormat format = new MediaFormat();
            format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_AUDIO_AAC);
            format.setInteger(MediaFormat.KEY_CHANNEL_COUNT, mNbChannels); //声道数（这里是数字）
            format.setInteger(MediaFormat.KEY_SAMPLE_RATE, mSampleRate); //采样率
            format.setInteger(MediaFormat.KEY_BIT_RATE, KEY_BIT_RATE); //目标码率
            format.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);

            mEncodeBufferInfo = new MediaCodec.BufferInfo();//记录编码完成的buffer的信息
            mMediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);// MediaCodec.CONFIGURE_FLAG_ENCODE 标识为编码器
            mMediaCodec.start();

            mEncodeInputBuffers = mMediaCodec.getInputBuffers();
            mEncodeOutputBuffers = mMediaCodec.getOutputBuffers();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void setOutputPath(String filePath) {
        mBos = new ByteArrayOutputStream();
        if (TextUtils.isEmpty(filePath)) {
            return;
        }
        File file = new File(filePath);
        try {
            mFos = new FileOutputStream(file);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public int encodeData(byte[] data, int length) throws IOException {
        int ret = -1;
        if (mMediaCodec == null) {
            return ret;
        }

        ByteBuffer inputBuffer;
        ByteBuffer outputBuffer;

        if (!mFlushEncoder) {
            if (data == null || length <= 0) {
                mFlushEncoder = true;
            }

            //  <0一直等待可用的byteBuffer 索引;=0 马上返回索引 ;>0 等待相应的毫秒数返回索引
            int inputBufferIndex = mMediaCodec.dequeueInputBuffer(-1); //一直等待（阻塞）
            if (inputBufferIndex >= 0) {
                if (mFlushEncoder) {
                    mMediaCodec.queueInputBuffer(inputBufferIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    Log.d(TAG, "flush encoder");
                } else {
                    inputBuffer = mEncodeInputBuffers[inputBufferIndex];
                    inputBuffer.clear();
                    inputBuffer.put(data, 0, length);
                    mMediaCodec.queueInputBuffer(inputBufferIndex, 0, length, 0, 0);
                }
            }
        }

        //获取已经编码成的buffer的索引  0表示马上获取 ，>0表示最多等待多少毫秒获取
        int outputBufferIndex = mMediaCodec.dequeueOutputBuffer(mEncodeBufferInfo, 0);
        while (outputBufferIndex >= 0) {
            //------------添加头信息--------------
            int outBufferSize = mEncodeBufferInfo.size;
            int outPacketSize = outBufferSize + 7; // 7 is ADTS size
            byte[] outData = new byte[outPacketSize];

            outputBuffer = mEncodeOutputBuffers[outputBufferIndex];
            outputBuffer.position(mEncodeBufferInfo.offset);
            outputBuffer.limit(mEncodeBufferInfo.offset + mEncodeBufferInfo.size);

            // it's a codec config such as channel/samplerate and so on
            if ((mEncodeBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                Log.d(TAG, "codec special data");
                outputBuffer.get(outData, 0, outBufferSize);
                Log.d(TAG, "codec special data size " + outBufferSize);
                int objectType = ((outData[0] & 0xf8) >> 3);
                Log.d(TAG, "start 5 bit object type " + objectType);
                if (objectType != 31) {
                    Log.d(TAG, "frequency index is " + (((outData[0] & 0x07) << 1)|(outData[1] & 0x80)));
                }
                Log.d(TAG, "channel config " + ((outData[1] & 0x78) >> 3));
                ret = 0;
            } else if ((mEncodeBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                Log.d(TAG, "output buffer eof quit");
                ret = 1;
                break;
            } else {
                if (mFlushEncoder) {
                    Log.d(TAG, "output buffer have data");
                }
                addADTStoPacket(outData, outPacketSize);
                outputBuffer.get(outData, 7, outBufferSize);
                try {
                    mBos.write(outData);
                } catch (IOException e) {
                    e.printStackTrace();
                }
                try {
                    if (mFos != null) {
                        mFos.write(mBos.toByteArray());
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
                //写完以后重置输出流，否则数据会重复
                try {
                    mBos.flush();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                mBos.reset();
                ret = 0;
            }
            mMediaCodec.releaseOutputBuffer(outputBufferIndex, false);
            outputBufferIndex = mMediaCodec.dequeueOutputBuffer(mEncodeBufferInfo, 0);
        }
        return ret;
    }

    /**
     * 添加ADTS头
     */
    private void addADTStoPacket(byte[] packet, int packetLen) {
        int profile = 2; // AAC LC
        int freqIdx = 0x8; // 16KHz
        switch (mSampleRate) {
            case 96000:
                freqIdx = 0x0;
                break;
            case 88200:
                freqIdx = 0x1;
                break;
            case 64000:
                freqIdx = 0x2;
                break;
            case 48000:
                freqIdx = 0x3;
                break;
            case 44100:
                freqIdx = 0x4;
                break;
            case 32000:
                freqIdx = 0x5;
                break;
            case 24000:
                freqIdx = 0x6;
                break;
            case 22050:
                freqIdx = 0x7;
                break;
            case 16000:
                freqIdx = 0x8;
                break;
            case 12000:
                freqIdx = 0x9;
                break;
            case 11025:
                freqIdx = 0xa;
                break;
            case 8000:
                freqIdx = 0xb;
                break;
            case 7350:
                freqIdx = 0xc;
                break;
            default:
                freqIdx = 0x8;
                break;
        }
        int chanCfg = mNbChannels;
        // fill in ADTS data
        packet[0] = (byte) 0xFF;
        packet[1] = (byte) 0xF9;
        packet[2] = (byte) (((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));
        packet[3] = (byte) (((chanCfg & 3) << 6) + (packetLen >> 11));
        packet[4] = (byte) ((packetLen & 0x7FF) >> 3);
        packet[5] = (byte) (((packetLen & 7) << 5) + 0x1F);
        packet[6] = (byte) 0xFC;
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public void close() {
        try {
            if (mMediaCodec != null) {
                mMediaCodec.stop();
                mMediaCodec.release();
            }
            if (mBos != null) {
                mBos.flush();
                mBos.close();
            }
            if (mFos != null) {
                mFos.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

}
