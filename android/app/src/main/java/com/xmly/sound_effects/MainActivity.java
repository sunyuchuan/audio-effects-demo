package com.xmly.sound_effects;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Message;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.RadioGroup;

import com.xmly.audio_effects.XmAudioEffects;
import com.xmly.sound_effects.audio.AudioCapturer;
import com.xmly.sound_effects.audio.AudioPlayer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends AppCompatActivity implements AudioCapturer.OnAudioFrameCapturedListener,
        View.OnClickListener, RadioGroup.OnCheckedChangeListener {
    private final static String TAG = MainActivity.class.getName();
    private static final String speech = "/sdcard/audio_effects_test/speech.pcm";
    private static final String effect = "/sdcard/audio_effects_test/effect.pcm";
    private static final int maxNbSamples = 1024;
    private OutputStream mOsSpeech;
    private OutputStream mOsEffect;
    private AudioPlayer mPlayer;
    private AudioCapturer mCapturer;
    private Button mBtnRecord;
    private Button mBtnPlay;
    private Thread mThread;
    private short[] speechSamples = new short[maxNbSamples];
    private XmAudioEffects mAudioEffects = null;
    private Handler mHandler = null;
    private static final int MSG_PROGRESS = 1;
    private static final int MSG_COMPLETED = 2;
    private volatile boolean abort = false;

    private void getExternalStoragePermission() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                // TODO: show explanation
            } else {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
            }
        }

        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.READ_EXTERNAL_STORAGE)) {
                // TODO: show explanation
            } else {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
            }
        }
    }

    private void getRecordAudioPermission() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.RECORD_AUDIO)) {
                // TODO: show explanation
            } else {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, 1);
            }
        }
    }

    private static boolean createOutputFile(String path) {
        File outFile = new File(path);
        try {
            if (!outFile.getParentFile().exists()) {
                if (!outFile.getParentFile().mkdirs()) {
                    Log.e(TAG, "createJsonFile : mkdir " + outFile + " create failed");
                    return false;
                }
            }
            if (!outFile.exists()) {
                if (!outFile.createNewFile()) {
                    Log.e(TAG, "createJsonFile : " + outFile + " create failed");
                    return false;
                }
            } else {
                outFile.delete();
                if (!outFile.createNewFile()) {
                    Log.e(TAG, "createJsonFile : " + outFile + " create failed");
                    return false;
                }
            }
            outFile.setReadable(true, false);
            outFile.setWritable(true, false);
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        } finally {

        }

        return true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getExternalStoragePermission();
        getRecordAudioPermission();

        createOutputFile(speech);
        createOutputFile(effect);

        mBtnRecord = findViewById(R.id.btn_record);
        mBtnRecord.setOnClickListener(this);

        mBtnPlay = findViewById(R.id.btn_audition);
        mBtnPlay.setOnClickListener(this);

        ((RadioGroup) findViewById(R.id.voice_group)).setOnCheckedChangeListener(this);
        ((RadioGroup) findViewById(R.id.eq_group)).setOnCheckedChangeListener(this);
        ((RadioGroup) findViewById(R.id.ns_group)).setOnCheckedChangeListener(this);

        mPlayer = new AudioPlayer();
        mCapturer = new AudioCapturer();
        mCapturer.setOnAudioFrameCapturedListener(MainActivity.this);
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mAudioEffects != null)
            mAudioEffects.release();
        mAudioEffects = new XmAudioEffects();
        mAudioEffects.setEffects("return_max_nb_samples", "false", 0);

        if (mHandler == null) {
            mHandler = new Handler() {
                @Override
                public void handleMessage(Message msg) {
                    switch (msg.what) {
                        case MSG_PROGRESS:
                            break;
                        case MSG_COMPLETED:
                            stopAudition();
                            break;
                        default:
                            break;
                    }
                }
            };
        }
    }

    @Override
    protected void onStop() {
        if (mAudioEffects != null)
            mAudioEffects.release();
        mAudioEffects = null;

        stopAudition();
        stopRecord();

        super.onStop();
    }

    private void OpenPcmFiles() {
        // 打开录制文件
        File outSpeech = new File(speech);
        if (outSpeech.exists()) outSpeech.delete();
        try {
            mOsSpeech = new FileOutputStream(outSpeech);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }

        // 打开处理后文件
        File outEffect = new File(effect);
        if (outEffect.exists()) outEffect.delete();
        try {
            mOsEffect = new FileOutputStream(outEffect);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }

    private class AuditionRunnable implements Runnable {
        @Override
        public void run() {
            try {
                // 打开播放文件
                File inEffect = new File(effect);
                InputStream isEffect = new FileInputStream(inEffect);

                // 启动播放
                if (!mPlayer.isPlayerStarted()) {
                    mPlayer.startPlayer();
                }

                // 循环读数据，播放音频
                // 创建字节数组
                int bufferSize = 4096;
                byte[] data = new byte[bufferSize];
                abort = false;
                while (!abort) {
                    // 读取内容，放到字节数组里面
                    int readsize = isEffect.read(data);
                    if (readsize <= 0) {
                        Log.w(TAG, "  end of file stop player");
                        mPlayer.stopPlayer();
                        break;
                    } else {
                        mPlayer.play(data, 0, readsize);
                    }
                }
                isEffect.close();
                mHandler.sendMessage(mHandler.obtainMessage(MSG_COMPLETED));
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    private void startRecord() {
        mBtnRecord.setText("停止");
        OpenPcmFiles();
        // 启动录音
        if (!mCapturer.isCaptureStarted()) {
            mCapturer.startCapture();
        }
    }

    private void stopRecord() {
        mBtnRecord.setText("录音");
        // 停止录音
        if (mCapturer.isCaptureStarted()) {
            mCapturer.stopCapture();
        }
        try {
            if (null != mOsEffect) mOsEffect.close();
            if (null != mOsSpeech) mOsSpeech.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void startAudition() {
        mBtnPlay.setText("停止");
        mThread = new Thread(new AuditionRunnable());
        mThread.setPriority(Thread.MAX_PRIORITY);
        mThread.start();
    }

    private void stopAudition() {
        mBtnPlay.setText("试听");
        abort = true;
        if (null != mThread && mThread.isAlive()) {
            mThread.interrupt();
        }
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.btn_record) {
            if (mBtnRecord.getText().toString().contentEquals("录音")) {
                stopAudition();
                startRecord();
            } else if (mBtnRecord.getText().toString().contentEquals("停止")) {
                stopRecord();
            }
        } else if (id == R.id.btn_audition) {
            if (mBtnPlay.getText().toString().contentEquals("试听")) {
                stopRecord();
                startAudition();
            } else if (mBtnPlay.getText().toString().contentEquals("停止")) {
                stopAudition();
            }
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (checkedId == R.id.rb_voice_original) {
            mAudioEffects.setEffects("Special", "Original", 0);
        } else if (checkedId == R.id.rb_voice_robot) {
            mAudioEffects.setEffects("Special", "Robot", 0);
        } else if (checkedId == R.id.rb_voice_minions) {
            mAudioEffects.setEffects("Special", "Mimions", 0);
        } else if (checkedId == R.id.rb_voice_man) {
            mAudioEffects.setEffects("Special", "Man", 0);
        } else if (checkedId == R.id.rb_voice_women) {
            mAudioEffects.setEffects("Special", "Women", 0);
        } else if (checkedId == R.id.rb_eq_none) {
            mAudioEffects.setEffects("Beautify", "None", 0);
        } else if (checkedId == R.id.rb_eq_clean_voice) {
            mAudioEffects.setEffects("Beautify", "CleanVoice", 0);
        } else if (checkedId == R.id.rb_eq_bass) {
            mAudioEffects.setEffects("Beautify", "Bass", 0);
        } else if (checkedId == R.id.rb_eq_low_voice) {
            mAudioEffects.setEffects("Beautify", "LowVoice", 0);
        } else if (checkedId == R.id.rb_eq_penetrating) {
            mAudioEffects.setEffects("Beautify", "Penetrating", 0);
        } else if (checkedId == R.id.rb_eq_magnetic) {
            mAudioEffects.setEffects("Beautify", "Magnetic", 0);
        } else if (checkedId == R.id.rb_eq_soft_pitch) {
            mAudioEffects.setEffects("Beautify", "SoftPitch", 0);
        } else if (checkedId == R.id.rb_ns_close) {
            mAudioEffects.setEffects("NoiseSuppression", "Off", 0);
        } else if (checkedId == R.id.rb_ns_open) {
            mAudioEffects.setEffects("NoiseSuppression", "On", 0);
        }
    }

    @Override
    public void onAudioFrameCaptured(byte[] audioData) {
        try {
            mOsSpeech.write(audioData, 0, audioData.length);
            mOsSpeech.flush();

            mOsEffect.write(audioData, 0, audioData.length);
            mOsEffect.flush();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onAudioFrameCaptured(short[] audioData) {
        try {
            byte[] data = Utils.getByteArrayInLittleOrder(audioData);
            mOsSpeech.write(data, 0, data.length);
            mOsSpeech.flush();

            mAudioEffects.sendSamples(audioData, audioData.length);
            int ret = mAudioEffects.reveiveSamples(speechSamples, maxNbSamples);
            while (ret > 0) {
                byte[] effectData = Utils.getByteArrayInLittleOrder(speechSamples);
                mOsEffect.write(effectData, 0, ret << 1);
                mOsEffect.flush();
                ret = mAudioEffects.reveiveSamples(speechSamples, maxNbSamples);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
