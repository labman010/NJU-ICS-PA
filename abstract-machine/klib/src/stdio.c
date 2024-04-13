#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

int printf(const char *fmt, ...) {
  panic("Not implemented");
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  char buf[20];
  char *str, *s;
  int d;

  char *p = out;

  while (*fmt) {
    if (*fmt == '%') {
      switch (*(++fmt)) {
        case 's':
          str = va_arg(ap, char *);
          while (*str) {
            *(p++) = *(str++);
          }
          break;
        case 'd':
          d = va_arg(ap, int);
          s = buf;
          if (d < 0) {
            *(p++) = '-';
            d = -d;
          }
          do {
            *(s++) = '0' + (d % 10);
            d /= 10;
          } while (d);
          // 倒序写入到输出字符串
          while (s > buf) {
            *(p++) = *(--s);
          }
          break;
        default:
          panic("Other format in vsprintf not implemented");
          *(p++) = *fmt;
          break;
      }
    } else {
      *(p++) = *fmt;
    }
    ++fmt;
  }
  *p = '\0';
  return p - out;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list args;
  int ret;

  va_start(args, fmt);

  ret = vsprintf(out, fmt, args);

  va_end(args);

  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
