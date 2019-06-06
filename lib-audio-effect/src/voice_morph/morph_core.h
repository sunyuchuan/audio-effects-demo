#ifndef MORPH_CORE_H_
#define MORPH_CORE_H_

#include <stddef.h>
#include <stdint.h>
typedef struct MorphCoreT MorphCore;
enum MorphType { ORIGINAL = 0, ROBOT, BRIGHT, MAN, WOMEN };

#ifdef __cplusplus
extern "C" {
#endif

MorphCore* morph_core_create();
void morph_core_free(MorphCore** morph);
int morph_core_init(MorphCore* morph);
void morph_core_set_type(MorphCore* morph, enum MorphType type);
int morph_core_send(MorphCore* morph, const int16_t* samples,
                    const size_t nb_samples);
int morph_core_receive(MorphCore* morph, int16_t* samples,
                       const size_t max_nb_samples);

#ifdef __cplusplus
}
#endif
#endif  // MORPH_CORE_H_
