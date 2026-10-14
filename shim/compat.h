/* Force-included compat layer for building the VC6/STLport-era sources with clang + libc++ (mingw). */
#pragma once
#ifdef __cplusplus
/* VC6 treated wchar_t as unsigned short (/Zc:wchar_t-); the engine mixes WideChar with UnsignedShort. */
typedef unsigned short wchar_t;
#endif
#ifdef __cplusplus
#include <ext/hash_map>
namespace std { using __gnu_cxx::hash_map; using __gnu_cxx::hash_multimap; }
#endif
#ifdef __cplusplus
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define AnimateWindow RTS_AnimateWindow /* after windows.h: the engine has a class AnimateWindow */
#endif
#define TRUE_FALSE_DEFINED /* skip WWLib/bool.h pre-C++98 bool shim */
#define UM_LW 1 /* Living World rules and networking are compiled in (server and client) */
#ifndef UM_CLIENT
#define UM_HEADLESS 1 /* Living World: dedicated-server build, no rendering */
#else
#include <mmsystem.h>
#define D3DCURSOR_IMMEDIATE_UPDATE 0x00000001L
#endif
