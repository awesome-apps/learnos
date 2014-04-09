#include "stdio.h"
#include <stdarg.h>

static void _print_hex(uint64_t number, bool caps);
static void _print_dec(int64_t number);
static void _print_udec(uint64_t number);

int printf(const char * str, ...) {
  va_list list;
  va_start(list, str);
  int res = vprintf(str, list);
  va_end(list);
  return res;
}

int vprintf(const char * str, va_list list) {
  for (; *str; str++) {
    if (*str != '%') {
      char buf[2] = {*str, 0};
      sys_print(buf);
      continue;
    }
    str++;
    if (*str == '%') {
      sys_print("%");
    } else if (*str == 'c') {
      char buf[2] = {(char)va_arg(list, int), 0};
      sys_print(buf);
    } else if (*str == 'd' || *str == 'i') {
      _print_dec(va_arg(list, int64_t));
    } else if (*str == 'u') {
      _print_udec(va_arg(list, uint64_t));
    } else if (*str == 'x' || *str == 'X') {
      uint64_t num = va_arg(list, uint64_t);
      _print_hex(num, *str == 'X');
    } else {
      return -1;
    }
  }
  return 0;
}

static void _print_hex(uint64_t number, bool caps) {
  const char * chars = "0123456789ABCDEF";
  if (!caps) chars = "0123456789abcdef";
  unsigned char buf[32];
  unsigned char len = 0, i;
  do {
    unsigned char nextDig = (unsigned char)(number & 0xf);
    buf[len++] = chars[nextDig];
    number >>= 4;
  } while (number > 0);
  for (i = 0; i < len / 2; i++) {
    unsigned char a = buf[len - i - 1];
    buf[len - i - 1] = buf[i];
    buf[i] = a;
  }
  buf[len] = 0;
  sys_print((const char *)buf);
}

static void _print_dec(int64_t number) {
  if (number < 0) {
    sys_print("-");
    return _print_udec((uint64_t)-number);
  }
  _print_udec((uint64_t)number);
}

static void _print_udec(uint64_t number) {
  unsigned char buf[32];
  unsigned char len = 0, i;
  do {
    buf[len++] = (number % 10) + '0';
    number /= 10;
  } while (number > 0);
  for (i = 0; i < len / 2; i++) {
    unsigned char a = buf[len - i - 1];
    buf[len - i - 1] = buf[i];
    buf[i] = a;
  }
  buf[len] = 0;
  sys_print((const char *)buf);
}

