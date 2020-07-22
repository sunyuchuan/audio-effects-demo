#ifndef _PITCH_MACRO_H_
#define _PITCH_MACRO_H_

#define LOCAL_SAMPLE_RATE_FIXED 44100
#define LOCAL_SAMPLE_RATE_FLOAT 44100.0f

#define PITCH_FRAME_SHIFT 588  // 44100Hz
#define PITCH_BUFFER_LENGTH (PITCH_FRAME_SHIFT * 3)
#define INV_PITCH_BUFFER_LENGTH 8.503401e-4f

#define MAX_CAND_NUM 10
#define FREQUENCY_CEIL 600.0f
#define FREQUENCY_FLOOR 75.0f

#define AUTO_CORR_LEN PITCH_BUFFER_LENGTH

#define PITCH_CONFIDENCE_THRD 4

#define LONG_TERM_PITCH_REFERENCE_LEN 500
#define PITCH_VARIANCE_FACTOR 0.35f

#define NOISE_PERIOD (0.004f * 44100.0f)
#define FLOOR_NOISE_PERIOD 176
#define FLOOR_NOISE_PERIOD_FLOAT 176.0f
#define IN_RANGE_LEN (4 * PITCH_FRAME_SHIFT)

#define PITCH_SEG_COUNT_THRD 8
#define PITCH_AVERAGE_UPDATE_THRD 50

#define PITCH_MINIMUM_SEND_THRD 80

#define RESAMPLE_FRAME_INPUT_LEN (PITCH_FRAME_SHIFT * 3)

#define PI 3.1415926535f

#define MORPH_BUF_LEN (9 * PITCH_FRAME_SHIFT)

#define LAST_JOINT_LEN (PITCH_BUFFER_LENGTH >> 1)

#define RESET_NUM 2000000000
#endif
