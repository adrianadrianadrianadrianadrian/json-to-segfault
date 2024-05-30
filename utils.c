#include <stdlib.h>

int strcmp_s(const char *s1, const char *s2, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (s1[i] != s2[i])
      return 0;
  }

  return 1;
}

void memcpy_ranged(void *dest, const void *src, size_t from, size_t to) {
  char *d = dest;
  const char *s = src;

  for (size_t i = from; i < to; i++)
    d[i] = s[i];
}
