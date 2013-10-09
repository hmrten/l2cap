#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include "types.h"

uchar parse_uchar(uchar **buf)
{
	uchar c = **buf;
	++(*buf);
	return c;
}

ushort parse_ushort(uchar **buf)
{
	ushort s = *(ushort *)(*buf);
	*buf += 2;
	return s;
}

uint parse_uint(uchar **buf)
{
	uint i = *(uint *)(*buf);
	*buf += 4;
	return i;
}

uint64 parse_uint64(uchar **buf)
{
	uint64 i = *(uint64 *)(*buf);
	*buf += 8;
	return i;
}

wchar_t *parse_wstr(uchar **buf)
{
	wchar_t *wstr;
	size_t len;

	len = wcslen((wchar_t *)*buf);
	wstr = malloc((len+1)*sizeof(wchar_t));
	memcpy(wstr, *buf, len*sizeof(wchar_t));
	wstr[len] = 0x0000;

	*buf += (len+1)*sizeof(wchar_t);

	return wstr;
}
