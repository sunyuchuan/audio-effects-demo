#include "util.h"
#include <ctype.h>
#include <string.h>

int ae_strcasecmp(const char* s1, const char* s2) {
#if defined(HAVE_STRCASECMP)
    return strcasecmp(s1, s2);
#elif defined(_MSC_VER)
    return _stricmp(s1, s2);
#else
    while (*s1 && (toupper(*s1) == toupper(*s2))) s1++, s2++;
    return toupper(*s1) - toupper(*s2);
#endif
}

int ae_strncasecmp(char const* s1, char const* s2, size_t n) {
#if defined(HAVE_STRCASECMP)
    return strncasecmp(s1, s2, n);
#elif defined(_MSC_VER)
    return _strnicmp(s1, s2, n);
#else
    while (--n && *s1 && (toupper(*s1) == toupper(*s2))) s1++, s2++;
    return toupper(*s1) - toupper(*s2);
#endif
}