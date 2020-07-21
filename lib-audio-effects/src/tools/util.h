#ifndef AUDIO_EFFECT_UTIL_H
#define AUDIO_EFFECT_UTIL_H
#include <stddef.h>

#ifdef __linux__
#define HAVE_STRCASECMP 1
#endif

extern int ae_strcasecmp(const char *s1, const char *st);
extern int ae_strncasecmp(char const *s1, char const *s2, size_t n);

#ifndef HAVE_STRCASECMP
#define strcasecmp(s1, s2) ae_strcasecmp((s1), (s2))
#define strncasecmp(s1, s2, n) ae_strncasecmp((s1), (s2), (n))
#endif

#endif  // AUDIO_EFFECT_UTIL_H
