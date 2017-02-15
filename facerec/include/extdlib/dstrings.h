#ifndef DM_DSTRINGS_H
#define DM_DSTRINGS_H

#include <stdio.h>

/**
 * Size-bounded string formating. Resulting string is guaranteed to be 0-terminated.
 * @param buffer Buffer to write to
 * @param count Size of the buffer
 * @param format String format
 * @return Size of the resulting string (excl terminating 0) if it fits, -1 otherwise
 */
int DM_SNPRINTF(char *buffer, size_t count, const char *format, ...);

/**
 * Tokenize strings. Equivalent to BSD strsep_r
 * @param string Pointer to string. For the first call string is the string to tokenize. Subsequent should pass NULL.
 * @param delim Delimiter string
 * @param lasts Internal state pointer
 * @return
 */
char* dmStrTok(char *string, const char *delim, char **lasts);

/**
 * Size-bounded string copying. Same as OpenBSD 2.4 strlcpy. (http://www.manpagez.com/man/3/strlcpy/)
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 */
size_t dmStrlCpy(char *dst, const char *src, size_t size);

/**
 * Size-bounded string concatenation. Same as OpenBSD 2.4 strlcat. (http://www.manpagez.com/man/3/strlcat)
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 */
size_t dmStrlCat(char *dst, const char *src, size_t size);

/**
 * Case-insensitive string comparison
 * @param s1 First string to compare
 * @param s2 Second string to compare
 * @return an integer greater than, equal to, or less than 0 after lexicographically comparison of s1 and s2
 */
int dmStrCaseCmp(const char *s1, const char *s2);

#endif //DM_DSTRINGS_H
