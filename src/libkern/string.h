#ifndef __LIBKERN_STRING_H__
#define __LIBKERN_STRING_H__

#include <stdint.h>

uint64_t strlen(const char * str);
bool strequal(const char * str1, const char * str2);
bool memequal(const void * b1, const void * b2, uint64_t len);

#endif