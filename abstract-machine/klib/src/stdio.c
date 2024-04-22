#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

int printf(const char *fmt, ...) {
  char buf[1024];
  va_list args;
  int ret;

  va_start(args, fmt);

  ret = vsprintf(buf, fmt, args);

  va_end(args);

  for (int i = 0; i < ret; ++i) {
    putch(buf[i]);
  }
  return ret;

}

// 【TODO】用状态机更好
int vsprintf(char *out, const char *fmt, va_list ap) {
  char *p = out;
  char buf[20]; // 20 is enough for 64-bit integer
  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      int width = 0;
      // 对于%02d这种格式的拙劣实现,与标准库的功能定义并不完全相同,只能实现左边补0
      while (*fmt >= '0' && *fmt <= '9') {
          width = width * 10 + (*fmt - '0');
          fmt++;
      }
      switch (*fmt) {
        case 's': {
          char *str = va_arg(ap, char *);
          while (*str) {
            *(p++) = *(str++);
          }
          break;
        }
        case 'd': {
          int d = va_arg(ap, int);
          char *s = buf;
          if (d < 0) {
            *(p++) = '-';
            d = -d;
          }
          do {
            *(s++) = '0' + (d % 10);
            d /= 10;
          } while (d);
          int zeros = width - (s - buf);
          while (zeros-- > 0) {
            *(p++) = '0';
          }
          // 倒序写入到输出字符串
          while (s > buf) {
            *(p++) = *(--s);
          }
          break;
        }
        case 'u': {
          uint32_t u = va_arg(ap, uint32_t);
          char *s = buf;
          // 将无符号整数转换为字符串
          do {
            *(s++) = '0' + (u % 10);
            u /= 10;
          } while (u);
          // 补充零以满足指定宽度
          int zeros = width - (s - buf);
          while (zeros-- > 0) {
              *(p++) = '0';
          }
          // 倒序写入到输出字符串
          while (s > buf) {
              *(p++) = *(--s);
          }
          break;
        }
        case 'U': { // 临时用大U表示unsigned long long, printf接受输入时必须强制转换uint64_t
          uint64_t llu = va_arg(ap, uint64_t);
          char *s = buf;
          // 将无符号长长整数转换为字符串
          do {
            *(s++) = '0' + (llu % 10);
            llu /= 10;
          } while (llu);
          // 补充零以满足指定宽度
          int zeros = width - (s - buf);
          while (zeros-- > 0) {
            *(p++) = '0';
          }
          // 倒序写入到输出字符串
          while (s > buf) {
            *(p++) = *(--s);
          }
          break;
        }
        case 'x': { // 正确性不保证.无法直接输入uint64_t类型
            uint32_t d = va_arg(ap, uint32_t);
            char *s = buf;
            // 将整数转换为16进制字符串
            do {
              int digit = d % 16;
              *(s++) = digit < 10 ? '0' + digit : 'a' + digit - 10;
              d /= 16;
            } while (d);
            int zeros = width - (s - buf);
            while (zeros-- > 0) {
              *(p++) = '0';
            }
            // 倒序写入到输出字符串
            while (s > buf) {
              *(p++) = *(--s);
            }
            break;
        }
        case 'c': {
          char c = (char)va_arg(ap, int);
          *(p++) = c;
          break;
        }
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
