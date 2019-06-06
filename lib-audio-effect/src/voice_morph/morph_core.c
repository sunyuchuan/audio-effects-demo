#include "morph_core.h"
#include <assert.h>
#include <stdlib.h>
#include "error_def.h"
#include "logger.h"
#include "math/junior_func.h"
#include "pitch_tracker/pitch_macro.h"
#include "pitch_tracker/pitch_tracker.h"
#include "resample/resample.h"
#include "tools/conversion.h"
#include "tools/fifo.h"

#define DST_SAMPLE_RATE 44100
#define RESAMPLE_FRAME_LEN 512

struct MorphCoreT {
    PitchTracker* pitch;
    fifo* fifo_in;
    fifo* fifo_out;
    fifo* fifo_swr;
    MorphCore* morph;
    struct SwrContext* swr_ctx;
    uint8_t** src_samples;
    uint8_t** dst_samples;
    int max_dst_nb_samples;
    short is_morph_on;

    enum MorphType morph_type;
    float seg_pitch_transform;
    float pitch_ratio;
    float formant_ratio;
    float seg_pitch_primary[7];
    float seg_pitch_new[7];
    short pitch_peak[PITCH_FRAME_SHIFT];
    float morph_buf[MORPH_BUF_LEN];
    float last_joint_buf[PITCH_BUFFER_SIZE >> 1];
    float last_fall_buf[PITCH_BUFFER_SIZE];
    short last_joint_len;
    short last_fall_len;
    short tgt_acc_len;
    short prev_peak_pos;
    short src_acc_pos;
};

static void pitch_transform(MorphCore* morph, float pitch_cand) {
    if (pitch_cand > 0.0f) {
        float rised_pitch = pitch_cand * morph->pitch_ratio;
        float tmp1 = 4.405e-014f * rised_pitch - 1.215e-010f;
        float tmp2 = tmp1 * rised_pitch + 1.299e-007f;
        tmp1 = tmp2 * rised_pitch - 6.907e-005f;
        tmp2 = tmp1 * rised_pitch + 0.02078f;
        float log_pitch = tmp2 * rised_pitch + 3.064f;
        tmp1 = log_pitch - 4.7874917f;
        tmp1 = 3.15641287f + morph->formant_ratio * 17.31234f * tmp1;
        float f = tmp1 * 0.057762265f;
        tmp1 = 0.1017f * f + 0.1317f;
        tmp1 = tmp1 * f + 0.4056f;
        tmp1 = tmp1 * f + 1.042f;
        morph->seg_pitch_transform = 100.0f * (tmp1 * f + 1.021f);
    } else {
        morph->seg_pitch_transform = 0.0f;
    }
}

static void update_seg_pitch(MorphCore* morph, const float pitch_cand) {
    morph->seg_pitch_primary[0] = morph->seg_pitch_primary[1];
    morph->seg_pitch_primary[1] = morph->seg_pitch_primary[2];
    morph->seg_pitch_primary[2] = morph->seg_pitch_primary[3];
    morph->seg_pitch_primary[3] = morph->seg_pitch_primary[4];
    morph->seg_pitch_primary[4] = morph->seg_pitch_primary[5];
    morph->seg_pitch_primary[5] = morph->seg_pitch_primary[6];
    morph->seg_pitch_primary[6] = pitch_cand;
    morph->seg_pitch_new[0] = morph->seg_pitch_new[1];
    morph->seg_pitch_new[1] = morph->seg_pitch_new[2];
    morph->seg_pitch_new[2] = morph->seg_pitch_new[3];
    morph->seg_pitch_new[3] = morph->seg_pitch_new[4];
    morph->seg_pitch_new[4] = morph->seg_pitch_new[5];
    morph->seg_pitch_new[5] = morph->seg_pitch_new[6];
    morph->seg_pitch_new[6] = morph->seg_pitch_transform;
}

static short unvoiced_interval_process(MorphCore* morph, float* in_frame,
                                       float* out_frame,
                                       short* out_frame_size) {
    float stretched_period_float = floor(NOISE_PERIOD * morph->formant_ratio);
    short stretched_period_fixed = (short)stretched_period_float;
    short period_num = (IN_RANGE_LEN - morph->src_acc_pos) / FLOOR_NOISE_PERIOD;

    if (period_num == 0) return morph->prev_peak_pos;

    float delta_phase =
        PI * _reciprocal_sqrt(stretched_period_float * stretched_period_float);
    float incr_sin = sin(delta_phase);
    float tmp = 1.0f - incr_sin * incr_sin;
    float incr_cos = tmp * _reciprocal_sqrt(tmp);

    for (short i = 0; i < period_num; i++) {
        morph->src_acc_pos += FLOOR_NOISE_PERIOD;
        float cos_phase = 1.0f;
        float sin_phase = 0.0f;
        for (short p = morph->src_acc_pos,
                   q = morph->src_acc_pos - stretched_period_fixed,
                   k = *out_frame_size - stretched_period_fixed,
                   z = *out_frame_size;
             p < (morph->src_acc_pos + stretched_period_fixed);
             p++, q++, k++, z++) {
            out_frame[z] += in_frame[p] * (0.5f * cos_phase + 0.5f);
            out_frame[k] += in_frame[q] * (-0.5f * cos_phase + 0.5f);
            tmp = cos_phase * incr_cos - sin_phase * incr_sin;
            sin_phase = cos_phase * incr_sin + sin_phase * incr_cos;
            cos_phase = tmp;
        }
        *out_frame_size += stretched_period_fixed;
    }
    morph->last_fall_len = stretched_period_fixed;

    return morph->src_acc_pos;
}

static void update_buffers(MorphCore* morph, float* frame,
                           const short frame_size) {
    short dl = frame_size - LAST_JOINT_SIZE - morph->last_fall_len;
    if (dl < 0) dl = 0;
    fifo_write(morph->fifo_swr, frame, dl);
    memcpy(morph->last_joint_buf, &frame[dl], sizeof(float) * LAST_JOINT_SIZE);
    memcpy(morph->last_fall_buf, &frame[frame_size - morph->last_fall_len],
           sizeof(float) * morph->last_fall_len);
}

static void situation1(MorphCore* morph, float* in_frame) {
    if (morph->src_acc_pos < IN_RANGE_LEN) {
        float prc_buf[PITCH_BUFFER_SIZE / 2 + 6 * PITCH_FRAME_SHIFT] = {0.0f};
        memcpy(prc_buf, morph->last_joint_buf, sizeof(float) * LAST_JOINT_SIZE);
        memcpy(&prc_buf[LAST_JOINT_SIZE], morph->last_fall_buf,
               sizeof(float) * morph->last_fall_len);
        short prc_len = LAST_JOINT_SIZE + morph->last_fall_len;

        short pk_pos =
            unvoiced_interval_process(morph, in_frame, prc_buf, &prc_len);
        update_buffers(morph, prc_buf, prc_len);
        morph->prev_peak_pos = pk_pos;
    } else {
        morph->prev_peak_pos = morph->src_acc_pos;
    }
}

float SegFreq(short pos, float* freq_vec, short shift) {
    short seg = (pos + 1) / shift - 2;  // the first two segments are affiliate
    if (seg < 0) seg = 0;
    return freq_vec[seg];
}

short NextPitchPeak(float* data, short peak_pos, short period, short low_bound,
                    short high_bound, short* pitch_peak) {
    short start = peak_pos + low_bound - 1;
    short stop = FFMIN(peak_pos + high_bound - 1, MORPH_BUF_LEN);
    short pitch_peak_count = 0;

    if (peak_pos >= 0) {
        for (short i = start; i < stop; i++) {
            if ((data[i] >= 0.05f && data[i] >= data[i - 1] &&
                 data[i] >= data[i + 1]) ||
                (data[i] < -0.05f && data[i] <= data[i - 1] &&
                 data[i] <= data[i + 1])) {
                pitch_peak[pitch_peak_count] = i;
                pitch_peak_count++;
            }
        }
    }

    if (pitch_peak_count == 0) {
        if (fabs(data[start]) >= fabs(data[stop]))
            pitch_peak[0] = start;
        else
            pitch_peak[0] = stop;
        return pitch_peak[0];
    }

    float max_val = -1e10f;
    short index = -1;
    short half_period = period >> 1;
    short left_len1 = peak_pos - FFMAX(peak_pos - half_period, 0);
    short right_len1 = FFMIN(peak_pos + half_period, MORPH_BUF_LEN) - peak_pos;
    for (short q = 0; q < pitch_peak_count; q++) {
        short left_len2 = pitch_peak[q] - FFMAX(pitch_peak[q] - half_period, 0);
        short right_len2 =
            FFMIN(pitch_peak[q] + half_period, MORPH_BUF_LEN) - pitch_peak[q];
        short left_len = FFMIN(left_len1, left_len2);
        short right_len = FFMIN(right_len1, right_len2);
        float* vec1 = &data[peak_pos - left_len - 1];
        float* vec2 = &data[pitch_peak[q] - left_len - 1];
        float cross_term = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;
        for (short k = 0; k < (left_len + right_len + 1); k++) {
            cross_term += *vec1 * *vec2;
            norm1 += *vec1 * *vec1;
            norm2 += *vec2 * *vec2;
            vec1++;
            vec2++;
        }
        float corr_val = _reciprocal_sqrt(norm1 * norm2 + 1e-20f) * cross_term;
        if (corr_val > max_val) {
            max_val = corr_val;
            index = q;
        }
    }

    return pitch_peak[index];
}

static short voiced_interval_process(MorphCore* morph, float* in_frame,
                                     float last_pitch_trans, short* peak_vec,
                                     float* freq_vec, short peak_num,
                                     float* out_frame, short* out_frame_size) {
    float stretched_period_float, tmp;
    short period;

    if (last_pitch_trans == 0.0f) {
        stretched_period_float = floor(NOISE_PERIOD * morph->formant_ratio);
        period = FLOOR_NOISE_PERIOD;
    } else {
        tmp = _reciprocal_sqrt(last_pitch_trans * last_pitch_trans) *
              PITCH_SAMPLE_RATE;
        stretched_period_float = floor(tmp * morph->formant_ratio);
        period = (short)tmp;
    }
    short stretched_period_fixed = (short)stretched_period_float;
    short interval_peak = morph->src_acc_pos + period;
    if (interval_peak > peak_vec[peak_num - 1]) return morph->prev_peak_pos;

    short pos = morph->prev_peak_pos;
    for (short i = 0; i < peak_num; i++) {
        float delta_phase = PI * _reciprocal_sqrt(stretched_period_float *
                                                  stretched_period_float);
        float incr_sin = sin(delta_phase);
        tmp = 1.0f - incr_sin * incr_sin;
        float incr_cos = tmp * _reciprocal_sqrt(tmp);
        while (interval_peak <= peak_vec[i]) {
            float cos_phase = 1.0f;
            float sin_phase = 0.0f;
            for (short p = pos, q = pos - stretched_period_fixed,
                       k = *out_frame_size - stretched_period_fixed,
                       z = *out_frame_size;
                 p < (pos + stretched_period_fixed); p++, q++, k++, z++) {
                out_frame[k] += in_frame[q] * (-0.5f * cos_phase + 0.5f);
                out_frame[z] += in_frame[p] * (0.5f * cos_phase + 0.5f);
                tmp = cos_phase * incr_cos - sin_phase * incr_sin;
                sin_phase = cos_phase * incr_sin + sin_phase * incr_cos;
                cos_phase = tmp;
            }
            *out_frame_size += stretched_period_fixed;
            interval_peak += period;
        }
        morph->last_fall_len = stretched_period_fixed;
        pos = peak_vec[i];
        if (freq_vec[i] == 0.0f) {
            stretched_period_float = floor(NOISE_PERIOD * morph->formant_ratio);
            period = FLOOR_NOISE_PERIOD;
        } else {
            tmp =
                _reciprocal_sqrt(freq_vec[i] * freq_vec[i]) * PITCH_SAMPLE_RATE;
            stretched_period_float = floor(tmp * morph->formant_ratio);
            period = (short)tmp;
        }
        stretched_period_fixed = (short)stretched_period_float;
    }

    morph->src_acc_pos = interval_peak - period;
    return peak_vec[peak_num - 1];
}

static void morph_interval_process(MorphCore* morph, float* in_frame,
                                   short* peak_vec, float* freq_vec,
                                   short peak_count) {
    float prc_buf[PITCH_BUFFER_SIZE / 2 + 6 * PITCH_FRAME_SHIFT] = {0.0f};
    memcpy(prc_buf, morph->last_joint_buf, sizeof(float) * LAST_JOINT_SIZE);
    memcpy(&prc_buf[LAST_JOINT_SIZE], morph->last_fall_buf,
           sizeof(float) * morph->last_fall_len);
    short prc_len = LAST_JOINT_SIZE + morph->last_fall_len;

    float last_pitch =
        SegFreq(morph->prev_peak_pos, morph->seg_pitch_new, PITCH_FRAME_SHIFT);
    voiced_interval_process(morph, in_frame, last_pitch, peak_vec, freq_vec,
                            peak_count, prc_buf, &prc_len);

    update_buffers(morph, prc_buf, prc_len);
    morph->prev_peak_pos = peak_vec[peak_count - 1];
}

static void situation2(MorphCore* morph, float* in_frame) {
    float period = floor(_reciprocal_sqrt(morph->seg_pitch_primary[1] *
                                          morph->seg_pitch_primary[1]) *
                         PITCH_SAMPLE_RATE);
    short search_pos = morph->prev_peak_pos;
    short loop_region = -1;
    short peak_count = 0;
    short ret_peak;
    float freq_vec[12] = {0.0f};
    short peak_vec[12] = {0};

    if (morph->prev_peak_pos <= IN_RANGE_LEN) {
        short search_low = (short)(0.8f * period);  // should be 0.83333f
        short search_high = (short)(1.25f * period);
        if ((search_low + morph->prev_peak_pos) <= IN_RANGE_LEN) {
            while (loop_region <= IN_RANGE_LEN) {
                ret_peak =
                    NextPitchPeak(in_frame, search_pos, (short)period,
                                  search_low, search_high, morph->pitch_peak);
                peak_vec[peak_count] = ret_peak;
                loop_region = ret_peak;
                search_pos = ret_peak;
                freq_vec[peak_count] =
                    SegFreq(ret_peak, morph->seg_pitch_new, PITCH_FRAME_SHIFT);
                peak_count++;
            }
        } else {
            ret_peak =
                NextPitchPeak(in_frame, search_pos, (short)period, search_low,
                              search_high, morph->pitch_peak);
            peak_vec[peak_count] = ret_peak;
            freq_vec[peak_count] =
                SegFreq(ret_peak, morph->seg_pitch_new, PITCH_FRAME_SHIFT);
            peak_count++;
        }
    }
    if (peak_count >= 1)
        morph_interval_process(morph, in_frame, peak_vec, freq_vec, peak_count);
}

static short FindLocalPeak(float* buf, short start, short end) {
    float max_val = -1e10f, min_val = 1e10f;
    short pos_max = 0, pos_min = 0;
    for (short i = start; i <= end; i++) {
        if (buf[i] > max_val) {
            max_val = buf[i];
            pos_max = i;
        }
        if (buf[i] < min_val) {
            min_val = buf[i];
            pos_min = i;
        }
    }

    if (fabs(max_val) > fabs(min_val))
        return (pos_max + 1);
    else
        return (pos_min + 1);
}

static void unvoiced_to_voiced_interval_process(MorphCore* morph,
                                                float* in_frame,
                                                short search_from,
                                                float* out_frame,
                                                short* out_frame_size) {
    short period;
    float stretched_period_float;

    short dl = search_from - morph->src_acc_pos;
    if (dl <= FLOOR_NOISE_PERIOD) {
        period = dl;
        stretched_period_float = floor(((float)dl) * morph->formant_ratio);
    } else {
        period = FLOOR_NOISE_PERIOD;
        stretched_period_float = floor(NOISE_PERIOD * morph->formant_ratio);
    }
    short stretched_period_fixed = (short)stretched_period_float;
    short interval_peak = morph->src_acc_pos + period;
    float delta_phase =
        PI * _reciprocal_sqrt(stretched_period_float * stretched_period_float +
                              1e-20f);
    float incr_sin = sin(delta_phase);
    float tmp = 1.0f - incr_sin * incr_sin;
    float incr_cos = tmp * _reciprocal_sqrt(tmp);

    while (interval_peak <= search_from) {
        float cos_phase = 1.0f;
        float sin_phase = 0.0f;
        for (short p = morph->src_acc_pos,
                   q = morph->src_acc_pos - stretched_period_fixed,
                   k = *out_frame_size - stretched_period_fixed,
                   z = *out_frame_size;
             p < (morph->src_acc_pos + stretched_period_fixed);
             p++, q++, k++, z++) {
            out_frame[k] += in_frame[q] * (-0.5f * cos_phase + 0.5f);
            out_frame[z] += in_frame[p] * (0.5f * cos_phase + 0.5f);
            tmp = cos_phase * incr_cos - sin_phase * incr_sin;
            sin_phase = cos_phase * incr_sin + sin_phase * incr_cos;
            cos_phase = tmp;
        }
        *out_frame_size += stretched_period_fixed;
        interval_peak += period;
    }

    morph->src_acc_pos = search_from;
    morph->last_fall_len = stretched_period_fixed;
}

static void situation3(MorphCore* morph, float* in_frame) {
    short peak_count = 0;
    short ret_peak;
    float freq_vec[12] = {0.0f};
    short peak_vec[12] = {0};
    if (morph->prev_peak_pos <= IN_RANGE_LEN) {
        short search_pos =
            FindLocalPeak(in_frame, 3 * PITCH_FRAME_SHIFT, IN_RANGE_LEN - 1);
        if (search_pos <= morph->prev_peak_pos) {
            morph->prev_peak_pos = 3 * PITCH_FRAME_SHIFT;
            morph->src_acc_pos = 3 * PITCH_FRAME_SHIFT;
        }

        float prc_buf[PITCH_BUFFER_SIZE / 2 + 6 * PITCH_FRAME_SHIFT] = {0.0f};
        memcpy(prc_buf, morph->last_joint_buf, sizeof(float) * LAST_JOINT_SIZE);
        memcpy(&prc_buf[LAST_JOINT_SIZE], morph->last_fall_buf,
               sizeof(float) * morph->last_fall_len);
        short prc_len = LAST_JOINT_SIZE + morph->last_fall_len;

        unvoiced_to_voiced_interval_process(morph, in_frame, search_pos,
                                            prc_buf, &prc_len);
        morph->prev_peak_pos = search_pos;
        update_buffers(morph, prc_buf, prc_len);

        float period = _reciprocal_sqrt(morph->seg_pitch_primary[1] *
                                        morph->seg_pitch_primary[1]) *
                       PITCH_SAMPLE_RATE;
        short search_low = (short)(0.8f * period);
        short search_high = (short)(1.25f * period);
        short loop_region = -1;
        if ((search_low + search_pos) <= IN_RANGE_LEN) {
            loop_region = search_pos;
            while (loop_region <= IN_RANGE_LEN) {
                ret_peak =
                    NextPitchPeak(in_frame, search_pos, (short)period,
                                  search_low, search_high, morph->pitch_peak);
                peak_vec[peak_count] = ret_peak;
                loop_region = ret_peak;
                search_pos = ret_peak;
                freq_vec[peak_count] =
                    SegFreq(ret_peak, morph->seg_pitch_new, PITCH_FRAME_SHIFT);
                peak_count++;
            }
        } else {
            ret_peak =
                NextPitchPeak(in_frame, search_pos, (short)period, search_low,
                              search_high, morph->pitch_peak);
            peak_vec[peak_count] = ret_peak;
            freq_vec[peak_count] =
                SegFreq(ret_peak, morph->seg_pitch_new, PITCH_FRAME_SHIFT);
            peak_count++;
        }
    }
    if (peak_count >= 1)
        morph_interval_process(morph, in_frame, peak_vec, freq_vec, peak_count);
}

static void pitch_stretch(MorphCore* morph) {
    if (0.0f == morph->seg_pitch_new[1]) {
        situation1(morph, morph->morph_buf);
    } else if (0.0f != morph->seg_pitch_new[0] &&
               0.0f != morph->seg_pitch_new[1]) {
        situation2(morph, morph->morph_buf);
    } else if (0.0f == morph->seg_pitch_new[0] &&
               0.0f != morph->seg_pitch_new[1]) {
        situation3(morph, morph->morph_buf);
    }
}

static void voice_morph_process(MorphCore* morph, int16_t* in_frame_fixed) {
    float in_frame_flp[PITCH_FRAME_SHIFT];
    S16ToFloat(in_frame_fixed, in_frame_flp, PITCH_FRAME_SHIFT);
    memmove(morph->morph_buf, &morph->morph_buf[PITCH_FRAME_SHIFT],
            sizeof(float) * (MORPH_BUF_LEN - PITCH_FRAME_SHIFT));
    memcpy(&morph->morph_buf[MORPH_BUF_LEN - PITCH_FRAME_SHIFT], in_frame_flp,
           sizeof(float) * PITCH_FRAME_SHIFT);
    float pitch_cand = pitchtracker_get_pitchcand(morph->pitch, in_frame_flp);

    if (ROBOT == morph->morph_type) {
        if (0.0f != pitch_cand)
            morph->seg_pitch_transform = 175.0f;
        else
            morph->seg_pitch_transform = 0.0f;
    } else {
        pitch_transform(morph, pitch_cand);
    }

    update_seg_pitch(morph, pitch_cand);
    morph->prev_peak_pos -= PITCH_FRAME_SHIFT;
    morph->src_acc_pos -= PITCH_FRAME_SHIFT;
    pitch_stretch(morph);
}

MorphCore* morph_core_create() {
    MorphCore* morph = (MorphCore*)calloc(1, sizeof(MorphCore));
    if (NULL == morph) return NULL;
    int ret = 0;

    // 创建 PitchTracker
    morph->pitch = pitchtracker_create();
    if (NULL == morph->pitch) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    morph->fifo_in = fifo_create(sizeof(int16_t));
    if (NULL == morph->fifo_in) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    morph->fifo_out = fifo_create(sizeof(int16_t));
    if (NULL == morph->fifo_out) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    morph->fifo_swr = fifo_create(sizeof(float));
    if (NULL == morph->fifo_swr) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    morph->is_morph_on = 0;
    morph->max_dst_nb_samples = RESAMPLE_FRAME_LEN;
    ret = allocate_sample_buffer(&morph->src_samples, 1, RESAMPLE_FRAME_LEN,
                                 AV_SAMPLE_FMT_FLT);
    ret = allocate_sample_buffer(&morph->dst_samples, 1,
                                 morph->max_dst_nb_samples, AV_SAMPLE_FMT_S16);

end:
    if (ret < 0) morph_core_free(&morph);
    return morph;
}

void morph_core_free(MorphCore** morph) {
    if (NULL == morph || NULL == *morph) return;
    MorphCore* self = *morph;
    if (self->pitch) pitchtracker_free(&self->pitch);
    if (self->fifo_in) fifo_delete(&self->fifo_in);
    if (self->fifo_out) fifo_delete(&self->fifo_out);
    if (self->fifo_swr) fifo_delete(&self->fifo_swr);
    if (self->swr_ctx) swr_free(&self->swr_ctx);
    if (self->src_samples) {
        av_freep(&self->src_samples[0]);
        av_freep(&self->src_samples);
    }
    if (self->dst_samples) {
        av_freep(&self->dst_samples[0]);
        av_freep(&self->dst_samples);
    }
    free(*morph);
    *morph = NULL;
}

static float get_pitch_factor(float pitch_coeff) {
    if (pitch_coeff > 1.2f) {
        return 1.3f;
    } else if (pitch_coeff > 1.1f) {
        return 1.2f;
    } else if (pitch_coeff > 1.0f) {
        return 1.1f;
    } else if (pitch_coeff >= 0.9f) {
        return 0.9f;
    } else if (pitch_coeff >= 0.8f) {
        return 0.8f;
    } else {
        return 0.7f;
    }
}

static int morph_core_init_common(MorphCore* const morph) {
    morph->formant_ratio = get_pitch_factor(morph->pitch_ratio);
    morph->seg_pitch_transform = 0.0f;
    morph->prev_peak_pos = 4 * PITCH_FRAME_SHIFT;
    morph->src_acc_pos = 4 * PITCH_FRAME_SHIFT;
    morph->last_fall_len = PITCH_BUFFER_SIZE;
    memset(morph->seg_pitch_primary, 0, sizeof(float) * 7);
    memset(morph->seg_pitch_new, 0, sizeof(float) * 7);
    int src_sample_rate = (int)roundf(morph->formant_ratio * PITCH_SAMPLE_RATE);
    return resampler_init(1, 1, src_sample_rate, DST_SAMPLE_RATE,
                          AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_S16,
                          &morph->swr_ctx);
}

int morph_core_init(MorphCore* const morph) {
    morph_core_set_type(morph, ORIGINAL);
    return pitchtracker_init(morph->pitch, 450.0f, 75.0f, 0.5f, 0.1f,
                             MAX_CAND_NUM, 0.01f);
}

static void resample_rest_audio(MorphCore* morph) {
    size_t rest_nb_samples = fifo_occupancy(morph->fifo_swr);
    while (rest_nb_samples > 0) {
        rest_nb_samples = FFMIN(RESAMPLE_FRAME_LEN, rest_nb_samples);
        fifo_read(morph->fifo_swr, morph->src_samples[0], rest_nb_samples);
        int ret =
            resample_audio(morph->swr_ctx, morph->src_samples, rest_nb_samples,
                           &morph->dst_samples, &morph->max_dst_nb_samples, 1);
        if (ret > 0) fifo_write(morph->fifo_out, morph->dst_samples[0], ret);

        rest_nb_samples = fifo_occupancy(morph->fifo_swr);
    }
}

void morph_core_set_type(MorphCore* morph, enum MorphType type) {
    // 重采样缓存的音频
    resample_rest_audio(morph);
    // 更新变声类型
    morph->morph_type = type;
    morph->is_morph_on = 1;
    switch (morph->morph_type) {
        case ORIGINAL:
            morph->pitch_ratio = 1.0f;
            morph->is_morph_on = 0;
            break;
        case ROBOT:
            morph->pitch_ratio = 1.0f;
            break;
        case BRIGHT:
            morph->pitch_ratio = 1.0f;
            break;
        case MAN:
            morph->pitch_ratio = 0.5f;
            break;
        case WOMEN:
            morph->pitch_ratio = 1.5f;
            break;
        default:
            morph->pitch_ratio = 1.0f;
            break;
    }
    morph_core_init_common(morph);
}

void morph_core_morph_on(MorphCore* morph, const int is_morph_on) {
    morph->is_morph_on = is_morph_on;
}

int morph_core_send(MorphCore* morph, const int16_t* samples,
                    const size_t nb_samples) {
    assert(NULL != morph);
    return fifo_write(morph->fifo_in, samples, nb_samples);
}

int morph_core_receive(MorphCore* morph, int16_t* samples,
                       const size_t max_nb_samples) {
    assert(NULL != morph);

    if (morph->is_morph_on) {
        int16_t in_frame_fixed[PITCH_FRAME_SHIFT];
        while (fifo_occupancy(morph->fifo_in) >= PITCH_FRAME_SHIFT) {
            fifo_read(morph->fifo_in, in_frame_fixed, PITCH_FRAME_SHIFT);
            voice_morph_process(morph, in_frame_fixed);
        }
        while (fifo_occupancy(morph->fifo_swr) >= RESAMPLE_FRAME_LEN) {
            fifo_read(morph->fifo_swr, morph->src_samples[0],
                      RESAMPLE_FRAME_LEN);
            int ret = resample_audio(morph->swr_ctx, morph->src_samples,
                                     RESAMPLE_FRAME_LEN, &morph->dst_samples,
                                     &morph->max_dst_nb_samples, 1);
            if (ret > 0)
                fifo_write(morph->fifo_out, morph->dst_samples[0], ret);
        }
    } else {
        while (fifo_occupancy(morph->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(morph->fifo_in, samples, max_nb_samples);
            fifo_write(morph->fifo_swr, samples, nb_samples);
        }
        while (fifo_occupancy(morph->fifo_swr) > 0) {
            size_t nb_samples =
                fifo_read(morph->fifo_swr, samples, max_nb_samples);
            fifo_write(morph->fifo_out, samples, nb_samples);
        }
    }
    return fifo_read(morph->fifo_out, samples, max_nb_samples);
}
