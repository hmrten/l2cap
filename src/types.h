#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

typedef long long int64;
typedef unsigned long long uint64;

uchar parse_uchar(uchar **buf);
ushort parse_ushort(uchar **buf);
uint parse_uint(uchar **buf);
uint64 parse_uint64(uchar **buf);

wchar_t *parse_wstr(uchar **buf);

#endif
