/* Replacement for EA's refcodex.h (RefPack codec; original source not in the released drop). Implemented in src/um_refpack.cpp. */
#pragma once
int REF_encode(void *compresseddata, const void *source, int sourcesize, int *opts = 0);
int REF_decode(void *dest, const void *compresseddata, int *compressedsize = 0);
bool REF_is(const void *compresseddata);
int REF_size(const void *compresseddata);
