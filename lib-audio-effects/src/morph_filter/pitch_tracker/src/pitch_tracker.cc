#ifdef __cplusplus
extern "C" {
#endif

#include "pitch_tracker.h"
#include "low_pass.h"
#include "pitch_core.h"
#include "pitch_macro.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace xmly_audio_recorder_android {
PitchTracker::PitchTracker()
    : shift_buf_pos(0),
      min_freq(0.0f),
      max_freq(0.0f),
      min_lag(0),
      max_lag(0),
      sample_rate(44100.0f),
      voice_thrd(0.0f),
      intens_thrd(0.0f),
      cand_num(0),
      pitch_cand_buf_len(0),
      local_peak_thrd(0.0f),
      prev_pitch(0.0f),
      successive_confidence(0),
      last_pitch_flag(0),
      long_term_pitch(0.0f),
      long_term_pitch_count(0),
      long_term_pitch_ready(0),
      pitch_counting_flag(0),
      successive_pitch_seg_count(0),
      pitch_inbuf_count(0),
      pitch_buf(NULL),
      frame_shift_buf(NULL),
      pitch_cand_buf(NULL),
      long_term_pitch_record(NULL),
      autocorr_buf(NULL),
      freq_cand_seq(NULL),
      intensity_cand_seq(NULL),
      win_pitch_frame(NULL) {}

int PitchTracker::PitchTracker_Create() {
    short i;

    pitch_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (pitch_buf == NULL) {
        return -1;
    }

    frame_shift_buf = (float *)malloc(sizeof(float) * PITCH_FRAME_SHIFT);
    if (frame_shift_buf == NULL) {
        return -1;
    }

    pitch_cand_buf =
        (float *)malloc(sizeof(float) * 10);  // 15680 Bytes input maximum
    if (pitch_cand_buf == NULL) {
        return -1;
    }

    long_term_pitch_record =
        (float *)malloc(sizeof(float) * LONG_TERM_PITCH_REFERENCE_LEN);
    if (long_term_pitch_record == NULL) {
        return -1;
    }

    autocorr_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (autocorr_buf == NULL) {
        return -1;
    }

    freq_cand_seq = (float *)malloc(sizeof(float) * MAX_CAND_NUM);
    if (freq_cand_seq == NULL) {
        return -1;
    }

    intensity_cand_seq = (float *)malloc(sizeof(float) * MAX_CAND_NUM);
    if (intensity_cand_seq == NULL) {
        return -1;
    }

    win_pitch_frame = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (win_pitch_frame == NULL) {
        return -1;
    }

    return 0;
}

int PitchTracker::PitchTracker_Init(
    float freq_upper_bound, float freq_lower_bound, float voice_max_power,
    float intens_min_power, short candidate_max_num, float frame_peak_thrd) {
    short i, j;

    if (freq_upper_bound >= FREQUENCY_FLOOR &&
        freq_upper_bound <= FREQUENCY_CEIL) {
        max_freq = freq_upper_bound;
    } else {
        return -1;
    }
    if (freq_lower_bound >= FREQUENCY_FLOOR &&
        freq_lower_bound <= FREQUENCY_CEIL) {
        min_freq = freq_lower_bound;
    } else {
        return -1;
    }

    if (voice_max_power > 0.0f) {
        voice_thrd = voice_max_power;
    } else {
        return -1;
    }

    if (intens_min_power > 0.0f) {
        intens_thrd = intens_min_power;
    } else {
        return -1;
    }

    if (candidate_max_num <= MAX_CAND_NUM && candidate_max_num > 0) {
        cand_num = candidate_max_num;
    } else {
        return -1;
    }

    if (frame_peak_thrd > 0.0f) {
        local_peak_thrd = frame_peak_thrd;
    } else {
        return -1;
    }

    memset(pitch_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH);
    memset(frame_shift_buf, 0, sizeof(float) * PITCH_FRAME_SHIFT);
    memset(long_term_pitch_record, 0,
           sizeof(float) * LONG_TERM_PITCH_REFERENCE_LEN);
    memset(autocorr_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH);
    memset(freq_cand_seq, 0, sizeof(float) * MAX_CAND_NUM);
    memset(intensity_cand_seq, 0, sizeof(float) * MAX_CAND_NUM);
    memset(pitch_cand_buf, 0, sizeof(float) * 10);
    memset(win_pitch_frame, 0, sizeof(float) * PITCH_BUFFER_LENGTH);

    max_lag = ceil(sample_rate / min_freq);
    min_lag = floor(sample_rate / max_lag);

    long_term_pitch = 0.0f;
    long_term_pitch_count = 0;
    long_term_pitch_ready = 0;
    pitch_counting_flag = 0;
    successive_pitch_seg_count = 0;
    pitch_inbuf_count = 0;
    pitch_cand_buf_len = 0;


	memset(peak_record, 0, sizeof(float) * 10);
	peak_sum=0;
	peak_count=0;
	peak_avg = 0;
    return 0;
}

int PitchTracker::PitchTracker_Process(short *in, short in_len,
                                       float *float_buf,float *seg_pitch_primary, float *seg_pitch_new) {
	short test_low[PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT] = { 0 };
    short shift_count = 0, i, n, k, in_pos = 0, lowpass_len, win_frame_len,
          total_len, buf_init_pos, float_buf_pos = 0;
    float local_peak, best_cand, d;
	short local_peak_pos;
    int to_send_size;
	int zero_pass_count = 0;
	static int mute_count = 0;

	short low_pass_buf[PITCH_FRAME_SHIFT] = { 0 };
	static int count = 0;
    if (in_len < 0 || in == NULL || in_len > 3920) {
        return -1;
    }
    total_len = in_len + shift_buf_pos;
    buf_init_pos = shift_buf_pos;
    if (in_len < PITCH_FRAME_SHIFT) {
        if (total_len < PITCH_FRAME_SHIFT) {
            // fewer than a shift-length, just copy
            for (n = 0; n < in_len; n++) {
                d = ((float)in[n]) * 0.000030517578f;
                frame_shift_buf[shift_buf_pos + n] = d;
                float_buf[n] = d;
            }
            shift_buf_pos += in_len;
            return 0;
        } else {
            shift_count = 1;
        }
    } else {
        shift_count = total_len / PITCH_FRAME_SHIFT;
    }

    // process data
    for (i = 0; i < shift_count; i++) {
		count++;
        cand_num = 0;
        for (n = 0; n < (PITCH_FRAME_SHIFT - shift_buf_pos); n++) {			//进来1024点被分为若干个帧，有多余部分留到下一次使用
            d = ((float)in[n + in_pos]) * 0.000030517578f;				//short转归一化float
            frame_shift_buf[shift_buf_pos + n] = d;
            float_buf[float_buf_pos + n] = d;
        }
        float_buf_pos += (PITCH_FRAME_SHIFT - shift_buf_pos);
        in_pos = (PITCH_FRAME_SHIFT - buf_init_pos) + i * PITCH_FRAME_SHIFT;
        shift_buf_pos = 0;

        memmove(pitch_buf, &pitch_buf[PITCH_FRAME_SHIFT],
                (PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT) * sizeof(float));	//将前两帧的滤波结果向前挪392点，为新的数据腾出空间
        LowPassIIR(frame_shift_buf, PITCH_FRAME_SHIFT,
                   &pitch_buf[PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT],		//对刚进来的392点进行低通滤波，并把结果
                   &lowpass_len);											//存储在pitch_buf中的最后392的位置上
		/*for (int k = 0; k < PITCH_FRAME_SHIFT;k++)
		{
			low_pass_buf[k] = (short)(32767.0*pitch_buf[k]);
		}

		fwrite(low_pass_buf, sizeof(short), PITCH_FRAME_SHIFT, test_fp);*/

		zero_pass_count=zero_pass(frame_shift_buf, PITCH_FRAME_SHIFT);
		local_peak = FindLocalPitchPeak(pitch_buf, PITCH_FRAME_SHIFT);	//寻找波峰
		mute_count++;
        if ((local_peak > local_peak_thrd) && zero_pass_count<200) {
			local_peak_thrd = (local_peak_thrd*2*0.95+ local_peak * 0.05)*0.5;
			mute_count = 0;
            DedirectAndWindow(pitch_buf, PITCH_BUFFER_LENGTH, win_pitch_frame,	//将输入数据的直流分量去除，数据存入win_pitch_frame
                              &win_frame_len);
            AutoCorrelation(win_pitch_frame, autocorr_buf);					//自相关
            FindPitchCand(autocorr_buf, freq_cand_seq, intensity_cand_seq,	//自相关函数值、可能的基频序列、
                          max_lag, min_lag, voice_thrd, intens_thrd,
                          sample_rate, MAX_CAND_NUM, &cand_num, win_pitch_frame);
        }
		if (mute_count == 100)
		{
			local_peak_thrd = local_peak_thrd * 0.5;
			mute_count = 0;
		}
        best_cand = SelectBestPitchCand(									//选择本次基频的最优解
            intensity_cand_seq, freq_cand_seq, cand_num, &prev_pitch,
            &successive_confidence, &last_pitch_flag, min_freq, max_freq,
            long_term_pitch_ready, long_term_pitch, seg_pitch_primary,seg_pitch_new, pitch_cand_buf);
		//peak_avg = Peak_avg_update(best_cand, &peak_count, peak_record, local_peak, &peak_sum);
        pitch_cand_buf[pitch_cand_buf_len] = best_cand;
        pitch_cand_buf_len++;
        LongTermPitchEsitmate(best_cand, &pitch_counting_flag,					//判断是否是一段语音的结束，如果是的话就计算包括这段语音和之前
                              &successive_pitch_seg_count, &pitch_inbuf_count,	//的语音的基频均值，为之后的基频计算限定范围
                              &long_term_pitch_ready, long_term_pitch_record,	//如果不是一段语音的结束，那么就记录这一次的基频
                              &long_term_pitch, &long_term_pitch_count);
    }
    shift_buf_pos = total_len - shift_count * PITCH_FRAME_SHIFT;
    for (k = 0; k < shift_buf_pos; k++) {
        d = ((float)in[in_pos + k]) * 0.000030517578f;
        frame_shift_buf[k] = d;
        float_buf[float_buf_pos + k] = d;
    }
    return 0;
}

int PitchTracker::PitchTracker_Release() {
    short i;

    if (pitch_buf != NULL) {
        free(pitch_buf);
        pitch_buf = NULL;
    }

    if (frame_shift_buf != NULL) {
        free(frame_shift_buf);
        frame_shift_buf = NULL;
    }

    if (long_term_pitch_record != NULL) {
        free(long_term_pitch_record);
        long_term_pitch_record = NULL;
    }

    if (autocorr_buf != NULL) {
        free(autocorr_buf);
        autocorr_buf = NULL;
    }

    if (freq_cand_seq != NULL) {
        free(freq_cand_seq);
        freq_cand_seq = NULL;
    }

    if (intensity_cand_seq != NULL) {
        free(intensity_cand_seq);
        intensity_cand_seq = NULL;
    }

    if (win_pitch_frame != NULL) {
        free(win_pitch_frame);
        win_pitch_frame = NULL;
    }

    if (pitch_cand_buf != NULL) {
        free(pitch_cand_buf);
        pitch_cand_buf = NULL;
    }
    delete this;

    return 0;
}

}  // namespace xmly_audio_recorder_android

#ifdef __cplusplus
}
#endif
