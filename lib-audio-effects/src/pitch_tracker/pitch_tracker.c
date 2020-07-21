#include "pitch_tracker.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "low_pass.h"
#include "math/junior_func.h"
#include "math/spl_math.h"
#include "pitch_macro.h"

struct PitchTrackerT {
    float pitch_buf[PITCH_BUFFER_SIZE];
    float win_pitch_frame[PITCH_BUFFER_SIZE];
    float autocorr_buf[PITCH_BUFFER_SIZE];
    float intensity_cand[MAX_CAND_NUM];
    float freq_cand[MAX_CAND_NUM];
    float long_term_pitch_record[LONG_TERM_PITCH_REFERENCE_LEN];
    float pitch_cand;
    short nb_candidate;
    short max_nb_candidate;
    float min_frequency;
    float max_frequency;
    short max_delay_pts;
    short min_delay_pts;
    float voice_max_power;
    float intens_min_power;
    float frame_peak_thrd;
    float last_pitch;
    short last_pitch_flag;
    short successive_confidence;
    float long_term_pitch;
    short long_term_pitch_count;
    short long_term_pitch_ready;
    short pitch_counting_flag;
    short successive_pitch_seg_count;
    short pitch_inbuf_count;
};

static float find_local_pitch_peak(const float* frame, const short frame_size) {
    float max_val = 0.0f, tmp;
    for (short i = 0; i < frame_size; ++i) {
        tmp = frame[i] < 0.0f ? -frame[i] : frame[i];
        if (tmp > max_val) max_val = tmp;
    }
    return max_val;
}

static void dedirect_and_window(const float* in, float* out) {
    float sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f, sum4 = 0.0f;
    for (short i = 0; i < PITCH_BUFFER_SIZE; i += 4) {
        sum1 += in[i];
        sum2 += in[i + 1];
        sum3 += in[i + 2];
        sum4 += in[i + 3];
    }
    float mean = (sum1 + sum2 + sum3 + sum4) * INV_PITCH_BUFFER_SIZE;
    for (short i = 0; i < PITCH_BUFFER_SIZE; i += 4) {
        out[i] = in[i] - mean;
        out[i + 1] = in[i + 1] - mean;
        out[i + 2] = in[i + 2] - mean;
        out[i + 3] = in[i + 3] - mean;
    }
}

static void auto_correlation(float* in, float* corr_buf) {
    float tmp32no1 = 0.0f, tmp32no2 = 0.0f;
    for (short i = 0; i < PITCH_BUFFER_SIZE; i += 8) {
        tmp32no1 += (in[i] * in[i]);
        tmp32no2 += (in[i + 4] * in[i + 4]);
    }

    float norm2 = tmp32no1 + tmp32no2;
    float inv_norm = _reciprocal_sqrt(norm2 * norm2);
    for (short i = 0; i < (PITCH_BUFFER_SIZE >> 2); i++) {
        float tmp = 0;
        for (short j = 0; j < (PITCH_BUFFER_SIZE - 4 * i); j += 4) {
            tmp += in[j] * in[4 * i + j];  // q20
        }
        corr_buf[i] = tmp * inv_norm;
    }
    corr_buf[0] = 1.0f;
}

static void find_pitch_cand(PitchTracker* pitch) {
    float half_voice_thrd = 0.1f * pitch->voice_max_power;
    short start = (pitch->min_delay_pts >> 2) - 2;
    short end = (pitch->max_delay_pts >> 2) - 1;

    for (short i = start; i < end; ++i) {
        if (pitch->autocorr_buf[i] > half_voice_thrd &&
            pitch->autocorr_buf[i] > pitch->autocorr_buf[i - 1] &&
            pitch->autocorr_buf[i] > pitch->autocorr_buf[i + 1]) {
            float dr = 0.5f * (pitch->autocorr_buf[i + 1] -
                               pitch->autocorr_buf[i - 1]);
            float d2r = 2.0f * pitch->autocorr_buf[i] -
                        pitch->autocorr_buf[i + 1] - pitch->autocorr_buf[i - 1];
            float tmp1 = ((float)i) * d2r + dr;
            float tmp2 = _reciprocal_sqrt(tmp1 * tmp1);
            float freq_est = tmp2 * d2r * PITCH_SAMPLE_RATE * 0.25f;
            if (pitch->autocorr_buf[i] > pitch->intens_min_power &&
                pitch->nb_candidate < MAX_CAND_NUM) {
                pitch->intensity_cand[pitch->nb_candidate] =
                    pitch->autocorr_buf[i];
                pitch->freq_cand[pitch->nb_candidate] = freq_est;
                pitch->nb_candidate++;
            }
        }
    }
}

static float SelectPitch_Cand1_ConfGtThrd(short pitch_ready, float freq_min,
                                          float freq_max, float* freq_seq,
                                          float last_pitch) {
    float inv, mul;

    if (pitch_ready == 0) {
        return freq_seq[0];
    } else {
        if (freq_seq[0] >= freq_min && freq_seq[0] <= freq_max) {
            return freq_seq[0];
        } else if (freq_seq[0] > 1.75f * last_pitch) {
            inv = _reciprocal_sqrt(last_pitch * last_pitch);
            mul = inv * freq_seq[0];
            mul = round_float(mul);
            inv = _reciprocal_sqrt(mul * mul);
            return freq_seq[0] * inv;
        } else if (freq_seq[0] > freq_max &&
                   freq_seq[0] <= 1.75f * last_pitch) {
            return freq_max;
        } else if (freq_seq[0] < freq_min && freq_seq[0] < 0.7f * last_pitch) {
            inv = _reciprocal_sqrt(freq_seq[0] * freq_seq[0]);
            mul = inv * last_pitch;
            mul = round_float(mul);
            return freq_seq[0] * mul;
        } else if (freq_seq[0] < freq_min && freq_seq[0] >= 0.7f * last_pitch) {
            return freq_min;
        }
    }
    return 0.0f;
}

float SelectPitch_Cand1_ConfLtThrd(short pitch_ready, float pitch_mean,
                                   float fraction, float* freq_seq) {
    float inv, mul;
    float upper_bound = (1.0f + fraction) * pitch_mean,
          lower_bound = (1.0f - fraction) * pitch_mean;

    if (pitch_ready == 0) {
        return freq_seq[0];
    } else {
        if (freq_seq[0] <= upper_bound && freq_seq[0] >= lower_bound) {
            return freq_seq[0];
        } else if (freq_seq[0] > upper_bound) {
            inv = _reciprocal_sqrt(pitch_mean * pitch_mean);
            mul = freq_seq[0] * inv;
            mul = round_float(mul);
            inv = _reciprocal_sqrt(mul * mul);
            return freq_seq[0] * inv;
        } else if (freq_seq[0] < lower_bound) {
            inv = _reciprocal_sqrt(freq_seq[0] * freq_seq[0]);
            mul = pitch_mean * inv;
            mul = round_float(mul);
            return freq_seq[0] * mul;
        }
    }
    return 0.0f;
}

float SelectPitch_CandGt1_ConfGtThrd(short pitch_ready, float* freq_seq,
                                     float* inens_seq, short cand_num,
                                     float pitch_average, float prev_pitch) {
    float inv, max_val = 0.0f, delta1, delta2, part1, part2, score;
    short possible_pos = -1, i;

    if (pitch_ready == 0) {
        for (i = 0; i < cand_num; i++) {
            delta1 = fabs(freq_seq[i] - prev_pitch);
            if (delta1 > prev_pitch) {
                part1 = 0.0f;
            } else {
                inv = _reciprocal_sqrt(prev_pitch * prev_pitch);
                part1 = 1.0f - inv * delta1;
            }
            score = 0.5f * part1 + 0.5f * inens_seq[i];
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    } else {
        for (i = 0; i < cand_num; i++) {
            delta1 = fabs(freq_seq[i] - prev_pitch);
            if (delta1 > prev_pitch) {
                part1 = 0.0f;
            } else {
                inv = _reciprocal_sqrt(prev_pitch * prev_pitch);
                part1 = 1.0f - inv * delta1;
            }

            delta2 = fabs(freq_seq[i] - pitch_average);
            if (delta2 > prev_pitch) {
                part2 = 0.0f;
            } else {
                inv = _reciprocal_sqrt(pitch_average * pitch_average);
                part2 = 1.0f - inv * delta2;
            }
            score = 0.1f * part1 + 0.1f * part2 + 0.8f * inens_seq[i];
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    }
    return 0.0f;
}

float SelectPitch_CandGt1_ConfLtThrd(short pitch_ready, float* freq_seq,
                                     float* intens_seq, short cand_num,
                                     float pitch_average, float last_pitch,
                                     short pos) {
    float max_val = 0.0f, delta1, part1, delta2, part2, inv, score;
    short possible_pos = -1, i;

    if (pitch_ready == 0) {
        return freq_seq[pos];
    } else {
        for (i = 0; i < cand_num; i++) {
            inv = _reciprocal_sqrt(pitch_average * pitch_average);
            delta1 = fabs(pitch_average - freq_seq[i]);
            if (delta1 < pitch_average) {
                part1 = 1.0f - delta1 * inv;
            } else {
                part1 = 0.0f;
            }

            delta2 = fabs(last_pitch - freq_seq[i]);
            inv = _reciprocal_sqrt(last_pitch * last_pitch);
            if (delta2 < last_pitch) {
                part2 = 1.0f - delta2 * inv;
            } else {
                part2 = 0.0f;
            }
            score = part1 * 0.2f + part2 * 0.2f + intens_seq[i] * 0.6f;
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    }
    return 0.0f;
}

float SelectPitch_CandGt1_LastPitch0(short pitch_ready, float* freq_seq,
                                     float* intens_seq, short cand_num,
                                     float pitch_average, short pos) {
    float max_val = 0.0f, inv, div_val, ratio, score;
    short i, possible_pos = -1;

    if (pitch_ready == 0) {
        return freq_seq[pos];
    } else {
        for (i = 0; i < cand_num; i++) {
            inv = _reciprocal_sqrt(pitch_average * pitch_average);
            div_val = inv * freq_seq[i];
            ratio = 1.0f - fabs(1.0f - div_val);
            if (ratio < 0.0f) {
                ratio = 0.0f;
            }
            score = 0.3f * ratio + intens_seq[i] * 0.7f;
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    }
    return 0.0f;
}

static float select_best_pitch_cand(PitchTracker* pitch) {
    float upper_freq = FFMIN(pitch->last_pitch * 1.2f, pitch->max_frequency);
    float lower_freq = FFMAX(pitch->last_pitch * 0.8f, pitch->min_frequency);
    float best_freq = 0.0f;
    float max_val;
    short pos;

    if (pitch->nb_candidate == 0 || pitch->nb_candidate >= MAX_CAND_NUM) {
        if (pitch->last_pitch != 0.0f && pitch->last_pitch_flag == 0 &&
            pitch->successive_confidence > PITCH_CONFIDENCE_THRD) {
            best_freq = pitch->last_pitch;
            pitch->successive_confidence++;
            pitch->last_pitch_flag = 1;
        } else if (pitch->last_pitch != 0.0f && pitch->last_pitch_flag == 0 &&
                   pitch->successive_confidence <= PITCH_CONFIDENCE_THRD) {
            best_freq = 0.0f;
            pitch->successive_confidence = 0;
        } else if (pitch->last_pitch != 0.0f && pitch->last_pitch_flag == 1) {
            best_freq = 0.0f;
            pitch->last_pitch_flag = 0;
            pitch->successive_confidence = 0;
        } else if (pitch->last_pitch == 0.0f) {
            best_freq = 0.0f;
            pitch->last_pitch_flag = 0;
            pitch->successive_confidence = 0;
        }
    } else if (pitch->nb_candidate == 1) {
        if (pitch->last_pitch != 0.0f &&
            pitch->successive_confidence > PITCH_CONFIDENCE_THRD) {
            best_freq = SelectPitch_Cand1_ConfGtThrd(
                pitch->long_term_pitch_ready, lower_freq, upper_freq,
                pitch->freq_cand, pitch->last_pitch);
            pitch->successive_confidence++;
        } else if (pitch->last_pitch != 0.0f &&
                   pitch->successive_confidence <= PITCH_CONFIDENCE_THRD) {
            best_freq = pitch->freq_cand[0];
            pitch->successive_confidence++;
        } else if (pitch->last_pitch == 0.0f) {
            best_freq = SelectPitch_Cand1_ConfLtThrd(
                pitch->long_term_pitch_ready, pitch->long_term_pitch,
                PITCH_VARIANCE_FACTOR, pitch->freq_cand);
            pitch->successive_confidence = 1;
        }
        pitch->last_pitch_flag = 0;
    } else if (pitch->nb_candidate < MAX_CAND_NUM) {
        max_val = pitch->intensity_cand[0];
        pos = 0;
        for (short i = 1; i < pitch->nb_candidate; i++) {
            if (pitch->intensity_cand[i] > max_val) {
                max_val = pitch->intensity_cand[i];
                pos = i;
            }
        }
        best_freq = pitch->freq_cand[pos];
        if (pitch->last_pitch != 0.0f &&
            pitch->successive_confidence > PITCH_CONFIDENCE_THRD) {
            best_freq = SelectPitch_CandGt1_ConfGtThrd(
                pitch->long_term_pitch_ready, pitch->freq_cand,
                pitch->intensity_cand, pitch->nb_candidate,
                pitch->long_term_pitch, pitch->last_pitch);
            pitch->successive_confidence++;
        } else if (pitch->last_pitch != 0.0f &&
                   pitch->successive_confidence <= PITCH_CONFIDENCE_THRD) {
            best_freq = SelectPitch_CandGt1_ConfLtThrd(
                pitch->long_term_pitch_ready, pitch->freq_cand,
                pitch->intensity_cand, pitch->nb_candidate,
                pitch->long_term_pitch, pitch->last_pitch, pos);
            pitch->successive_confidence++;
        } else if (pitch->last_pitch == 0.0f) {
            best_freq = SelectPitch_CandGt1_LastPitch0(
                pitch->long_term_pitch_ready, pitch->freq_cand,
                pitch->intensity_cand, pitch->nb_candidate,
                pitch->long_term_pitch, pos);
            pitch->successive_confidence = 1;
        }
        pitch->last_pitch_flag = 0;
    }
    pitch->last_pitch = best_freq;
    return best_freq;
}

static short PitchAverage(float* pitch_mean, short pitch_valid_len,
                          float* pitch_record, short pitch_avail_inbuf_count) {
    short i, invalid_pitch_count = 0;
    float invalid_pitch_sum = 0.0f, pitch_sum = 0.0f, mean_val, inv, tmp,
          upper_mean, lower_mean, valid_count;

    if (*pitch_mean == 0.0f) {
        for (i = 0; i < pitch_avail_inbuf_count; i++) {
            pitch_sum += pitch_record[i];
        }
        tmp = (float)pitch_avail_inbuf_count;
        inv = _reciprocal_sqrt(tmp * tmp);
        mean_val = inv * pitch_sum;
        upper_mean = 1.75f * mean_val;
        lower_mean = 0.25f * mean_val;
        for (i = 0; i < pitch_avail_inbuf_count; i++) {
            if (pitch_record[i] > upper_mean || pitch_record[i] < lower_mean) {
                invalid_pitch_count += 1;
                invalid_pitch_sum += pitch_record[i];
            }
        }
        valid_count = (float)(pitch_avail_inbuf_count - invalid_pitch_count);
        pitch_sum -= invalid_pitch_sum;
        inv = _reciprocal_sqrt(valid_count * valid_count);
        *pitch_mean = inv * pitch_sum;
    } else {
        upper_mean = 1.35f * *pitch_mean;
        lower_mean = 0.65f * *pitch_mean;
        for (i = 0; i < pitch_avail_inbuf_count; i++) {
            pitch_sum += pitch_record[i];
            if (pitch_record[i] > upper_mean || pitch_record[i] < lower_mean) {
                invalid_pitch_count += 1;
                invalid_pitch_sum += pitch_record[i];
            }
        }
        valid_count = (float)(pitch_valid_len - invalid_pitch_count +
                              pitch_avail_inbuf_count);
        pitch_sum += (pitch_valid_len * *pitch_mean - invalid_pitch_sum);
        inv = _reciprocal_sqrt(valid_count * valid_count);
        *pitch_mean = inv * pitch_sum;
    }
    return (pitch_avail_inbuf_count - invalid_pitch_count);
}

static void LongTermPitchEsitmate(float best_cand, short* seg_counting_flag,
                                  short* seg_pitch_count,
                                  short* pitch_avail_inbuf_count,
                                  short* pitch_mean_ready, float* pitch_record,
                                  float* pitch_mean, short* pitch_valid_len) {
    short i, valid_increment, index;

    if (best_cand != 0.0f) {
        if (*seg_counting_flag == 0) {
            *seg_counting_flag = 1;
        }

        if (*pitch_avail_inbuf_count >= LONG_TERM_PITCH_REFERENCE_LEN) {
            if (*seg_counting_flag == 1 &&
                *seg_pitch_count <= PITCH_SEG_COUNT_THRD) {
                for (i = (*pitch_avail_inbuf_count - *seg_pitch_count);
                     i < *pitch_avail_inbuf_count; i++) {
                    pitch_record[i] = 0.0f;
                }
                *pitch_avail_inbuf_count -= *seg_pitch_count;
            }
            *seg_pitch_count = 0;
            *seg_counting_flag = 0;
        } else {
            index = *pitch_avail_inbuf_count;
            pitch_record[index] = best_cand;
            *seg_pitch_count += 1;
            *pitch_avail_inbuf_count += 1;
        }
    } else {
        if (*seg_counting_flag == 1 &&
            *seg_pitch_count <= PITCH_SEG_COUNT_THRD) {
            for (i = (*pitch_avail_inbuf_count - *seg_pitch_count);
                 i < *pitch_avail_inbuf_count; i++) {
                pitch_record[i] = 0.0f;
            }
            *pitch_avail_inbuf_count -= *seg_pitch_count;
        }
        *seg_pitch_count = 0;
        *seg_counting_flag = 0;
    }

    if (*seg_counting_flag == 0 &&
        *pitch_avail_inbuf_count >= PITCH_AVERAGE_UPDATE_THRD) {
        valid_increment = PitchAverage(pitch_mean, *pitch_valid_len,
                                       pitch_record, *pitch_avail_inbuf_count);
        *pitch_valid_len += valid_increment;
        for (i = 0; i < *pitch_avail_inbuf_count; i++) {
            pitch_record[i] = 0.0f;
        }
        *pitch_avail_inbuf_count = 0;
        if (*pitch_mean_ready == 0) {
            *pitch_mean_ready = 1;
        }
    }
}

PitchTracker* pitchtracker_create() {
    PitchTracker* pitch = (PitchTracker*)calloc(1, sizeof(PitchTracker));
    return pitch;
}

void pitchtracker_free(PitchTracker** pitch) {
    if (NULL == pitch || NULL == *pitch) return;
    free(*pitch);
    *pitch = NULL;
}

int pitchtracker_init(PitchTracker* const pitch, float max_frequency,
                      float min_frequency, float voice_max_power,
                      float intens_min_power, short max_nb_candidate,
                      float frame_peak_thrd) {
    assert(NULL != pitch);
    assert(max_frequency >= FREQUENCY_FLOOR);
    assert(max_frequency <= FREQUENCY_CEIL);
    assert(min_frequency >= FREQUENCY_FLOOR);
    assert(min_frequency <= FREQUENCY_CEIL);
    assert(voice_max_power > 0.0f);
    assert(intens_min_power > 0.0f);
    assert(max_nb_candidate <= MAX_CAND_NUM);
    assert(max_nb_candidate > 0);
    assert(frame_peak_thrd > 0.0f);

    if (max_nb_candidate > MAX_CAND_NUM) return -1;

    pitch->max_frequency = max_frequency;
    pitch->min_frequency = min_frequency;
    pitch->max_delay_pts = ceil(PITCH_SAMPLE_RATE / min_frequency);
    pitch->min_delay_pts = floor(PITCH_SAMPLE_RATE / pitch->max_delay_pts);

    pitch->voice_max_power = voice_max_power;
    pitch->intens_min_power = intens_min_power;
    pitch->frame_peak_thrd = frame_peak_thrd;

    memset(pitch->pitch_buf, 0, sizeof(float) * PITCH_BUFFER_SIZE);
    memset(pitch->long_term_pitch_record, 0,
           sizeof(float) * LONG_TERM_PITCH_REFERENCE_LEN);
    memset(pitch->autocorr_buf, 0, sizeof(float) * PITCH_BUFFER_SIZE);
    memset(pitch->freq_cand, 0, sizeof(float) * MAX_CAND_NUM);
    memset(pitch->intensity_cand, 0, sizeof(float) * MAX_CAND_NUM);
    memset(pitch->win_pitch_frame, 0, sizeof(float) * PITCH_BUFFER_SIZE);

    pitch->long_term_pitch = 0.0f;
    pitch->long_term_pitch_count = 0;
    pitch->long_term_pitch_ready = 0;
    pitch->pitch_counting_flag = 0;
    pitch->successive_pitch_seg_count = 0;
    pitch->pitch_inbuf_count = 0;
    return 0;
}

float pitchtracker_get_pitchcand(PitchTracker* pitch, float* frame) {
    memmove(pitch->pitch_buf, &pitch->pitch_buf[PITCH_FRAME_SHIFT],
            (PITCH_BUFFER_SIZE - PITCH_FRAME_SHIFT) * sizeof(float));
    LowPassIIR(frame, PITCH_FRAME_SHIFT,
               &pitch->pitch_buf[PITCH_BUFFER_SIZE - PITCH_FRAME_SHIFT]);

    float local_peak =
        find_local_pitch_peak(pitch->pitch_buf, PITCH_BUFFER_SIZE);

    pitch->nb_candidate = 0;
    if (local_peak > pitch->frame_peak_thrd) {
        dedirect_and_window(pitch->pitch_buf, pitch->win_pitch_frame);
        auto_correlation(pitch->win_pitch_frame, pitch->autocorr_buf);
        find_pitch_cand(pitch);
    }
    float best_cand = select_best_pitch_cand(pitch);
    LongTermPitchEsitmate(
        best_cand, &pitch->pitch_counting_flag,
        &pitch->successive_pitch_seg_count, &pitch->pitch_inbuf_count,
        &pitch->long_term_pitch_ready, pitch->long_term_pitch_record,
        &pitch->long_term_pitch, &pitch->long_term_pitch_count);
    return best_cand;
}