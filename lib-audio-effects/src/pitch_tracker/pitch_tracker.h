#ifndef PITCH_TRACKER_H_
#define PITCH_TRACKER_H_

typedef struct PitchTrackerT PitchTracker;

#ifdef __cplusplus
extern "C" {
#endif

PitchTracker* pitchtracker_create();
void pitchtracker_free(PitchTracker** pitch);
int pitchtracker_init(PitchTracker* const pitch, float max_freq, float min_freq,
                      float voice_max_power, float intens_min_power,
                      short candidate_max_num, float frame_peak_thrd);
float pitchtracker_get_pitchcand(PitchTracker* pitch, float* frame);

#ifdef __cplusplus
}
#endif
#endif  // PITCH_TRACKER_H_
