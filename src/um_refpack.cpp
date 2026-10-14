// RefPack (QFS) codec. Decoder implements the public format; encoder emits a valid literal-only stream.
// Replaces EA's REF_* routines, which are not part of the released source.
#include "EAC/refcodex.h"
#include <string.h>

typedef unsigned char u8;

bool REF_is(const void *p) { const u8 *s = (const u8 *)p; return (s[0] & 0x3e) == 0x10 && s[1] == 0xfb; }

static int hdrLen(const u8 *s) { return ((s[0] & 0x01) ? ((s[0] & 0x80) ? 4 : 3) : 0) + ((s[0] & 0x80) ? 4 : 3) + 2; }

int REF_size(const void *p)
{
	const u8 *s = (const u8 *)p;
	int big = s[0] & 0x80, skip = 2 + ((s[0] & 0x01) ? (big ? 4 : 3) : 0);
	const u8 *q = s + skip; unsigned v = 0;
	for (int i = 0; i < (big ? 4 : 3); ++i) v = (v << 8) | q[i];
	return (int)v;
}

int REF_decode(void *dest, const void *compresseddata, int *compressedsize)
{
	const u8 *s = (const u8 *)compresseddata; u8 *d = (u8 *)dest, *d0 = d;
	int total = REF_size(s);
	const u8 *in = s + hdrLen(s);
	for (;;)
	{
		unsigned b0 = *in++, plain, copy, off;
		if (!(b0 & 0x80))      { unsigned b1 = *in++; plain = b0 & 3; copy = ((b0 & 0x1c) >> 2) + 3; off = ((b0 & 0x60) << 3) + b1 + 1; }
		else if (!(b0 & 0x40)) { unsigned b1 = *in++, b2 = *in++; plain = (b1 >> 6) & 3; copy = (b0 & 0x3f) + 4; off = ((b1 & 0x3f) << 8) + b2 + 1; }
		else if (!(b0 & 0x20)) { unsigned b1 = *in++, b2 = *in++, b3 = *in++; plain = b0 & 3; copy = ((b0 & 0x0c) << 6) + b3 + 5; off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1; }
		else if (b0 < 0xfc)    { plain = ((b0 & 0x1f) << 2) + 4; copy = 0; off = 0; }
		else                   { plain = b0 & 3; copy = 0; off = 0; }
		while (plain--) *d++ = *in++;
		if (copy) { const u8 *r = d - off; while (copy--) *d++ = *r++; }
		if (b0 >= 0xfc) break;
	}
	if (compressedsize) *compressedsize = (int)(in - s);
	return (int)(d - d0) == total ? total : (int)(d - d0);
}

int REF_encode(void *compresseddata, const void *source, int sourcesize, int *opts)
{
	u8 *o = (u8 *)compresseddata; const u8 *src = (const u8 *)source;
	*o++ = 0x10; *o++ = 0xfb; *o++ = (u8)(sourcesize >> 16); *o++ = (u8)(sourcesize >> 8); *o++ = (u8)sourcesize;
	int left = sourcesize;
	while (left >= 4) { int run = left > 112 ? 112 : (left & ~3); *o++ = (u8)(0xe0 + ((run - 4) >> 2)); memcpy(o, src, run); o += run; src += run; left -= run; }
	*o++ = (u8)(0xfc + left); memcpy(o, src, left); o += left;
	return (int)(o - (u8 *)compresseddata);
}
