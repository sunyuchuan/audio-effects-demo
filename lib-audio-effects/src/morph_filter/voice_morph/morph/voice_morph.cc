
#ifdef __cplusplus
extern "C" {
#endif

#include "voice_morph.h"
#include <stdlib.h>
#include <string.h>
#include "../../pitch_tracker/src/pitch_macro.h"
#include "../../utility/log.h"
#include "morph_core.h"
#include "../resample/resample.h"
namespace xmly_audio_recorder_android {
VoiceMorph::VoiceMorph()
    : pitch(NULL),
      morph_inbuf_float(NULL),
      morph_buf_pos(0),
      morph_buf(NULL),
      prev_peak_pos(0),
      src_acc_pos(0),
      seg_pitch_primary(NULL),
      seg_pitch_new(NULL),
      seg_pitch_transform(0.0f),
      formant_ratio(1.0f),
      pitch_ratio(1.0f),
      pitch_range_factor(1.0f),
      pitch_peak(NULL),
      last_joint_buf(NULL),
      last_fall_buf(NULL),
      last_fall_len(0),
      out_buf(NULL),
      out_len(0),
      swr_ctx(NULL),
      rsmp_in_buf(NULL),
      rsmp_out_buf(NULL),
      rsmp_in_len_count(0),
      rsmp_out_len(0),
      rsmp_out_linesize(0),
      rate_supervisor(0),
      resample_initialized(0)
	  //rebuild_factor(NULL)
{}

int VoiceMorph::VoiceMorph_Create(char *file_name) {
    LogInfo("%s\n", __func__);
    pitch = new PitchTracker;
    if (pitch == NULL) {
        return -1;
    }

    if (pitch->PitchTracker_Create() == -1) {
        return -1;
    }

    if (VoiceMorph_AudioResample_Create(&swr_ctx) == -1) {
        return -1;
    }

    if (VoiceMorph_AudioResample_Init(
            swr_ctx, RESAMPLE_FRAME_INPUT_LEN, LOCAL_SAMPLE_RATE_FIXED,
            &rsmp_in_buf, &rsmp_out_buf, &rsmp_out_len, &rsmp_out_linesize,
            &resample_initialized) == -1) {
        return -1;
    }

    morph_inbuf_float = (float *)malloc(sizeof(float) * PITCH_FRAME_SHIFT * 10);
    if (morph_inbuf_float == NULL) {
        return -1;
    }

    morph_buf = (float *)malloc(sizeof(float) * PITCH_FRAME_SHIFT * 9);
    if (morph_buf == NULL) {
        return -1;
    }

    seg_pitch_primary = (float *)malloc(sizeof(float) * 7);
    if (seg_pitch_primary == NULL) {
        return -1;
    }

    seg_pitch_new = (float *)malloc(sizeof(float) * 7);
    if (seg_pitch_new == NULL) {
        return -1;
    }

    pitch_peak = (short *)malloc(sizeof(short) *
                                 PITCH_FRAME_SHIFT);  // Fs/Fmin*(1.25-0.8)
    if (pitch_peak == NULL) {
        return -1;
    }

    last_joint_buf =
        (float *)malloc(sizeof(float) * LAST_JOINT_LEN);  // Fs/Fmin
    if (last_joint_buf == NULL) {
        return -1;
    }

    last_fall_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (last_fall_buf == NULL) {
        return -1;
    }

    out_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH * 3);
    if (out_buf == NULL) {
        return -1;
    }
	//rebuild_factor = (refactor*)malloc(sizeof(refactor));
    return 0;
}

int VoiceMorph::VoiceMorph_Init() {
    LogInfo("%s\n", __func__);
    if (pitch->PitchTracker_Init(450.0f, 75.0f, 0.5f, 0.1f, MAX_CAND_NUM,
                                 0.02f) == -1) {
        return -1;
    }

    memset(morph_inbuf_float, 0, sizeof(float) * PITCH_FRAME_SHIFT * 10);
    memset(morph_buf, 0, sizeof(float) * PITCH_FRAME_SHIFT * 9);
    memset(seg_pitch_primary, 0, sizeof(float) * 7);
    memset(seg_pitch_new, 0, sizeof(float) * 7);
    memset(pitch_peak, 0, sizeof(short) * PITCH_FRAME_SHIFT);
    memset(last_joint_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH / 2);
    memset(last_fall_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH);
    memset(out_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH * 2);
	memset(&rebuild_factor, 0x0, sizeof(refactor));
	rebuild_factor.fake_time_num = -1;
    morph_buf_pos = 8 * PITCH_FRAME_SHIFT;
    prev_peak_pos = 4 * PITCH_FRAME_SHIFT;
    src_acc_pos = 4 * PITCH_FRAME_SHIFT;
    seg_pitch_transform = 0.0f;
    formant_ratio = 1.0f;
    pitch_ratio = 1.0f;
    pitch_range_factor = 1.0f;
    last_fall_len = PITCH_BUFFER_LENGTH;
    out_len = 0;

    return 0;
}

int VoiceMorph::VoiceMorph_SetConfig(float pitch_coeff) {
    int src_rate;
    LogInfo("%s pitch_coeff %f.\n", __func__, pitch_coeff);
    if (pitch_coeff > 2.0f || pitch_coeff < 0.5f) {
        return -1;
    }

    pitch_ratio = pitch_coeff;
    formant_ratio = VoiceMorphGetPitchFactor(pitch_coeff);
    memset(seg_pitch_primary, 0, sizeof(float) * 7);
    memset(seg_pitch_new, 0, sizeof(float) * 7);
    pitch_range_factor = formant_ratio;
    /*reset morph related parameters*/
    prev_peak_pos = 4 * PITCH_FRAME_SHIFT;
    src_acc_pos = 4 * PITCH_FRAME_SHIFT;
    last_fall_len = PITCH_BUFFER_LENGTH;

    src_rate = (int)roundf(formant_ratio * LOCAL_SAMPLE_RATE_FLOAT);
    VoiceMorph_AudioResample_Init(swr_ctx, RESAMPLE_FRAME_INPUT_LEN, src_rate,
                                  &rsmp_in_buf, &rsmp_out_buf, &rsmp_out_len,
                                  &rsmp_out_linesize, &resample_initialized);

    return 0;
}

int VoiceMorph::VoiceMorph_Process(void *raw, short in_size, char *morph_out,
                                   int * morph_out_size, bool robot) {		//�������ݡ����볤�ȡ�������ݡ�������ȡ�����ģʽ
	refactor *re_factor = &rebuild_factor;
	short i, k, j, total_len,
        shift_count = 0, in_pos = 0, res_len, swr_total_len, swr_res_len,
        swr_outsize, loop_count, buf_pos = 0, in_len, *in = (short *)raw;
	static float morph_factor = 0;
	static bool robot_status = 0;
	int output_size_rsmp = 0;
	char morph_out_rsmp[10000] = { 0 };
	if ((morph_factor != 0 && morph_factor != formant_ratio) || (morph_factor != 0 && robot_status != robot))
	{
		int link_out_size = 0;
		link_calculate(re_factor,&link_out_size, formant_ratio, &rsmp_in_len_count);
		*morph_out_size += link_out_size*2;
		memset(morph_out, 0,*morph_out_size);
	}
	else if (re_factor->total_out_len>= RESET_NUM)
	{
		int link_out_size = 0;
		link_calculate(re_factor, &link_out_size, formant_ratio, &rsmp_in_len_count);
		*morph_out_size += link_out_size * 2;
		memset(morph_out, 0, *morph_out_size);
	}
	morph_factor = formant_ratio;
	robot_status = robot;
    int src_rate = (int)(LOCAL_SAMPLE_RATE_FLOAT * formant_ratio);
    if (in_size < 0) {
        return -1;
    }
	static long test_count = 0;
    in_len = in_size >> 1;

    if (pitch->PitchTracker_Process(in, in_len, morph_inbuf_float, seg_pitch_primary, seg_pitch_new) == -1) {		//׷�ٻ�Ƶ
        return -1;
    }

    total_len = morph_buf_pos - 8 * PITCH_FRAME_SHIFT + in_len;	//�ܳ�������һ֡ʣ�µĺ���һ֡���������
    if (in_len < PITCH_FRAME_SHIFT) {
        if (total_len <
            PITCH_FRAME_SHIFT)  // fewer than a shift-length, just copy
        {
            memcpy(&morph_buf[morph_buf_pos], morph_inbuf_float, in_len);
            morph_buf_pos += in_len;
            return 0;
        } else {
            shift_count = 1;
        }
    } else {
        shift_count = total_len / PITCH_FRAME_SHIFT;
    }
	//���˵õ��˵�ǰ֡�Ļ�Ƶ��
    for (i = 0; i < shift_count; i++) {											//��ÿһ֡���м���
        out_len = 0;
        memcpy(&morph_buf[morph_buf_pos], &morph_inbuf_float[in_pos],			//���µ�һ֡���ݴ���morph_buf��
               (MORPH_BUF_LEN - morph_buf_pos) * sizeof(float));
        in_pos += (MORPH_BUF_LEN - morph_buf_pos);
        morph_buf_pos = MORPH_BUF_LEN - PITCH_FRAME_SHIFT;
		
        if (robot) {
            if (pitch->pitch_cand_buf[i] != 0.0f) {
                seg_pitch_transform = 175.0f;
            } else {
                seg_pitch_transform = 0.0f;
            }
        } else {
            VoiceMorphPitchTransform(pitch->pitch_cand_buf[i], pitch_ratio,
                                     pitch_range_factor, &seg_pitch_transform);
        }
        seg_pitch_primary[0] = seg_pitch_primary[1];
        seg_pitch_primary[1] = seg_pitch_primary[2];
        seg_pitch_primary[2] = seg_pitch_primary[3];
        seg_pitch_primary[3] = seg_pitch_primary[4];
		seg_pitch_primary[4] = pitch->pitch_cand_buf[i];
		seg_pitch_new[0] = seg_pitch_new[1];
		seg_pitch_new[1] = seg_pitch_new[2];
		seg_pitch_new[2] = seg_pitch_new[3];
		seg_pitch_new[3] = seg_pitch_new[4];
		seg_pitch_new[4] = seg_pitch_transform;		
        prev_peak_pos -= PITCH_FRAME_SHIFT;		//��ǰ�����������λ��
        src_acc_pos -= PITCH_FRAME_SHIFT;		//��ǰ���벨�����ڵ�λ��

        VoiceMorphPitchStretch(re_factor,								//��ȡ��һ֡�Ĳ���ľ��Բ�����͸���
            morph_buf, seg_pitch_new, seg_pitch_primary, &prev_peak_pos,	
            formant_ratio, 
            pitch->sample_rate, &src_acc_pos, out_buf, &out_len, pitch_peak);

		
        memmove(morph_buf, &morph_buf[PITCH_FRAME_SHIFT],
                (MORPH_BUF_LEN - PITCH_FRAME_SHIFT) * sizeof(float));
		
        swr_total_len = rsmp_in_len_count + out_len;
        if (swr_total_len >= RESAMPLE_FRAME_INPUT_LEN) {
            loop_count = swr_total_len / RESAMPLE_FRAME_INPUT_LEN;
            buf_pos = 0;
            for (k = 0; k < loop_count; k++) {
                memcpy(&rsmp_in_buf[0][rsmp_in_len_count << 2],
                       &out_buf[buf_pos],
                       sizeof(float) *
                           (RESAMPLE_FRAME_INPUT_LEN - rsmp_in_len_count));
                swr_outsize = VoiceMorph_AudioResample_Process(
                    swr_ctx, rsmp_in_buf, RESAMPLE_FRAME_INPUT_LEN,
                    rsmp_out_buf, &rsmp_out_len, src_rate, &rsmp_out_linesize);
                if (swr_outsize < 0) {
                    return -1;
                }
                memcpy(&morph_out_rsmp[output_size_rsmp], rsmp_out_buf[0],
                       swr_outsize);
				output_size_rsmp += swr_outsize;
                buf_pos += (RESAMPLE_FRAME_INPUT_LEN - rsmp_in_len_count);
                rsmp_in_len_count = 0;
            }
            rsmp_in_len_count =
                swr_total_len - loop_count * RESAMPLE_FRAME_INPUT_LEN;
            memcpy(rsmp_in_buf[0], &out_buf[buf_pos],
                   sizeof(float) * rsmp_in_len_count);
        } else {
            memcpy(&rsmp_in_buf[0][rsmp_in_len_count << 2], out_buf,
                   sizeof(float) * out_len);
            rsmp_in_len_count = swr_total_len;
        }
    }
	re_factor->real_out_count += output_size_rsmp /2;
    // residual data copy
    res_len = total_len - shift_count * PITCH_FRAME_SHIFT;
    memcpy(&morph_buf[morph_buf_pos], &morph_inbuf_float[in_pos],
           res_len * sizeof(float));
    morph_buf_pos += res_len;

    pitch->pitch_cand_buf_len = 0;
	memcpy(&morph_out[*morph_out_size], &morph_out_rsmp[0],
		output_size_rsmp);
	*morph_out_size += output_size_rsmp;

    return 0;
}

int VoiceMorph::VoiceMorph_Release() {
    if (pitch != NULL) {
        if (pitch->PitchTracker_Release() == -1) {
            return -1;
        }
    }

    VoiceMorph_AudioResample_Release(swr_ctx, rsmp_in_buf, rsmp_out_buf);

    if (morph_inbuf_float != NULL) {
        free(morph_inbuf_float);
    }

    if (morph_buf != NULL) {
        free(morph_buf);
    }

    if (seg_pitch_primary != NULL) {
        free(seg_pitch_primary);
    }

    if (seg_pitch_new != NULL) {
        free(seg_pitch_new);
    }

    if (pitch_peak != NULL) {
        free(pitch_peak);
    }

    if (last_joint_buf != NULL) {
        free(last_joint_buf);
    }

    if (last_fall_buf != NULL) {
        free(last_fall_buf);
    }

    if (out_buf != NULL) {
        free(out_buf);
    }

    delete this;

    return 0;
}
}  // namespace xmly_audio_recorder_android

#ifdef __cplusplus
}
#endif
