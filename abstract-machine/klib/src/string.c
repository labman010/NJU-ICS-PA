#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

// 部分参考Linux标准库glibc源码

size_t strlen(const char *s) {
  size_t length = 0;
  while (*s != '\0') {
    length++;
    s++;
  }
  return length;
}

char *strcpy(char *dst, const char *src) {
  // strcpy本身就不是一个很安全的函数
  char *ptr = dst;
  while (*src != '\0') {
    *dst = *src;
    src++;
    dst++;
  }
  *dst = '\0';
  return ptr;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *dst, const char *src) {
  size_t dst_len = strlen(dst);
  size_t i;
  for (i = 0; src[i] != '\0'; i++) {
    dst[dst_len + i] = src[i];
  }
  dst[dst_len + i] = '\0';

  return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && (*s1 == *s2)) {
      s1++;
      s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  size_t i = 0;
  while (i < n && s1[i] != '\0' && s2[i] != '\0' && s1[i] == s2[i]) {
    i++;
  }
  if (i == n) {
    return 0;
  }
  return ((unsigned char)s1[i] - (unsigned char)s2[i]);
}

void *memset(void *s, int c, size_t n) {
  unsigned char *ptr = s;
  unsigned char value = c;
  for (size_t i = 0; i < n; i++) {
    ptr[i] = value;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *pdst = dst;
  const unsigned char *psrc = src;
  if (pdst < psrc) {
    for (size_t i = 0; i < n; i++) {
      pdst[i] = psrc[i];
    }
  }
  else {
    for (size_t i = n; i > 0; i--) {
      pdst[i - 1] = psrc[i - 1];
    }
  }

  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *d = out;
  const unsigned char *s = in;
  while (n--) {
    *d++ = *s++;
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
        return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

#endif
