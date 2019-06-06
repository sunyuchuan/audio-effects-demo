package com.layne.audioeffect;

import android.Manifest;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.RadioGroup;

import com.layne.audioeffect.audio.AudioCapturer;
import com.layne.audioeffect.audio.AudioPlayer;
import com.layne.libeffect.AudioEffect;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends AppCompatActivity implements AudioCapturer.OnAudioFrameCapturedListener, View.OnClickListener, RadioGroup.OnCheckedChangeListener {
    private final static String TAG = MainActivity.class.getName();
    private static final String speech = "/sdcard/audio_effect_test/speech.pcm";
    private static final String effect = "/sdcard/audio_effect_test/effect.pcm";
    private static final int maxNbSamples = 1024;
    private OutputStream mOsSpeech;
    private OutputStream mOsEffect;
    private AudioPlayer mPlayer;
    private AudioCapturer mCapturer;
    private Button mBtnRecord;
    private Button mBtnPlay;
    private Button mBtnNsSwitch;
    private Thread mThread;
    private AudioEffect mEffect;
    private short[] speechSamples = new short[maxNbSamples];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getSystemPermission();
        mEffect = new AudioEffect();

        mBtnRecord = findViewById(R.id.btn_record);
        mBtnRecord.setOnClickListener(this);

        mBtnPlay = findViewById(R.id.btn_play);
        mBtnPlay.setOnClickListener(this);

        mBtnNsSwitch = findViewById(R.id.btn_ns);
        mBtnNsSwitch.setOnClickListener(this);

        ((RadioGroup) findViewById(R.id.voice_group)).setOnCheckedChangeListener(this);
        ((RadioGroup) findViewById(R.id.echo_group)).setOnCheckedChangeListener(this);
        ((RadioGroup) findViewById(R.id.eq_group)).setOnCheckedChangeListener(this);

        mPlayer = new AudioPlayer();
        mCapturer = new AudioCapturer();
        mCapturer.setOnAudioFrameCapturedListener(MainActivity.this);
    }

    @Override
    protected void onStop() {
        stopPlay();
        stopRecord();
        super.onStop();
    }

    private void getReadExternalStoragePermission() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.READ_EXTERNAL_STORAGE)) {
                // TODO: show explanation
            } else {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
            }
        }
    }

    private void getWriteExternalStoragePermission() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                // TODO: show explanation
            } else {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
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

    private void getSystemPermission() {
        getReadExternalStoragePermission();
        getWriteExternalStoragePermission();
        getRecordAudioPermission();
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

    private class WorkRunnable implements Runnable {

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
                while (true) {
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
        mBtnRecord.setText("录制");
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

    private void startPlay() {
        mBtnPlay.setText("停止");
        mThread = new Thread(new WorkRunnable());
        mThread.setPriority(Thread.MAX_PRIORITY);
        mThread.start();
    }

    private void stopPlay() {
        mBtnPlay.setText("试听");
        if (null != mThread && mThread.isAlive()) {
            mThread.interrupt();
        }
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.btn_record) {
            if (mBtnRecord.getText().toString().contentEquals("录制")) {
                stopPlay();
                startRecord();
            } else if (mBtnRecord.getText().toString().contentEquals("停止")) {
                stopRecord();
            }
        } else if (id == R.id.btn_play) {
            if (mBtnPlay.getText().toString().contentEquals("试听")) {
                stopRecord();
                startPlay();
            } else if (mBtnPlay.getText().toString().contentEquals("停止")) {
                stopPlay();
            }
        } else if (id == R.id.btn_ns) {
            if (mBtnNsSwitch.getText().toString().contentEquals("打开降噪")) {
                mEffect.set_xmly_effect("NoiseSuppression", "On", 0);
                mBtnNsSwitch.setText("关闭降噪");
            } else if (mBtnNsSwitch.getText().toString().contentEquals("关闭降噪")) {
                mEffect.set_xmly_effect("NoiseSuppression", "Off", 0);
                mBtnNsSwitch.setText("打开降噪");
            }
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (checkedId == R.id.rb_voice_original) {
            mEffect.set_xmly_effect("Morph", "Original", 0);
        } else if (checkedId == R.id.rb_voice_robot) {
            mEffect.set_xmly_effect("Morph", "Robot", 0);
        } else if (checkedId == R.id.rb_voice_minions) {
            mEffect.set_xmly_effect("Morph", "Mimions", 0);
        } else if (checkedId == R.id.rb_voice_bright) {
            mEffect.set_xmly_effect("Morph", "Bright", 0);
        } else if (checkedId == R.id.rb_voice_man) {
            mEffect.set_xmly_effect("Morph", "Man", 0);
        } else if (checkedId == R.id.rb_voice_women) {
            mEffect.set_xmly_effect("Morph", "Women", 0);
        } else if (checkedId == R.id.rb_echo_none) {
            mEffect.set_xmly_effect("Environment", "None", 0);
        } else if (checkedId == R.id.rb_echo_volley) {
            mEffect.set_xmly_effect("Environment", "Valley", 0);
        } else if (checkedId == R.id.rb_echo_church) {
            mEffect.set_xmly_effect("Environment", "Church", 0);
        } else if (checkedId == R.id.rb_echo_classroom) {
            mEffect.set_xmly_effect("Environment", "Classroom", 0);
        } else if (checkedId == R.id.rb_echo_live) {
            mEffect.set_xmly_effect("Environment", "Live", 0);
        } else if (checkedId == R.id.rb_eq_none) {
            mEffect.set_xmly_effect("Equalizer", "None", 0);
        } else if (checkedId == R.id.rb_eq_clean_voice) {
            mEffect.set_xmly_effect("Equalizer", "CleanVoice", 0);
        } else if (checkedId == R.id.rb_eq_bass) {
            mEffect.set_xmly_effect("Equalizer", "Bass", 0);
        } else if (checkedId == R.id.rb_eq_low_voice) {
            mEffect.set_xmly_effect("Equalizer", "LowVoice", 0);
        } else if (checkedId == R.id.rb_eq_penetrating) {
            mEffect.set_xmly_effect("Equalizer", "Penetrating", 0);
        } else if (checkedId == R.id.rb_eq_magnetic) {
            mEffect.set_xmly_effect("Equalizer", "Magnetic", 0);
        } else if (checkedId == R.id.rb_eq_soft_pitch) {
            mEffect.set_xmly_effect("Equalizer", "SoftPitch", 0);
        } else if (checkedId == R.id.rb_eq_old_radio) {
            mEffect.set_xmly_effect("Equalizer", "OldRadio", 0);
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

            mEffect.xmly_send_samples(audioData, audioData.length);
            int ret = mEffect.xmly_receive_samples(speechSamples, maxNbSamples);
            while (ret > 0) {
                byte[] effectData = Utils.getByteArrayInLittleOrder(speechSamples);
                mOsEffect.write(effectData, 0, ret << 1);
                mOsEffect.flush();
                ret = mEffect.xmly_receive_samples(speechSamples, maxNbSamples);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
