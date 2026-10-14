// Living World: the D3DX8 functions the render stack calls (see shim/d3dx8.h). Software implementations over Lock/Unlock.
#include "d3dx8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

D3DXMATRIX *D3DXMatrixInverse(D3DXMATRIX *out, float *det, const D3DXMATRIX *in)
{
	const float *m = &in->_11; float inv[16], d;
	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
	d = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if (det) *det = d;
	if (d == 0.0f) return NULL;
	d = 1.0f / d; for (int i = 0; i < 16; ++i) (&out->_11)[i] = inv[i] * d;
	return out;
}

HRESULT WINAPI D3DXAssembleShader(LPCVOID, UINT, DWORD, LPD3DXBUFFER *c, LPD3DXBUFFER *s, LPD3DXBUFFER *e)
{
	// Shader assembly is not available; the callers fall back to fixed-function rendering when this fails.
	if (c) *c = NULL; if (s) *s = NULL; if (e) *e = NULL;
	return E_FAIL;
}

HRESULT WINAPI D3DXAssembleShaderFromFileA(const char *, DWORD, LPD3DXBUFFER *c, LPD3DXBUFFER *s, LPD3DXBUFFER *e) { if (c) *c = NULL; if (s) *s = NULL; if (e) *e = NULL; return E_FAIL; }

UINT WINAPI D3DXGetFVFVertexSize(DWORD fvf)
{
	UINT s = 0;
	switch (fvf & D3DFVF_POSITION_MASK) { case D3DFVF_XYZ: s += 12; break; case D3DFVF_XYZRHW: s += 16; break; case D3DFVF_XYZB1: s += 16; break; case D3DFVF_XYZB2: s += 20; break; case D3DFVF_XYZB3: s += 24; break; case D3DFVF_XYZB4: s += 28; break; case D3DFVF_XYZB5: s += 32; break; }
	if (fvf & D3DFVF_NORMAL) s += 12;
	if (fvf & D3DFVF_PSIZE) s += 4;
	if (fvf & D3DFVF_DIFFUSE) s += 4;
	if (fvf & D3DFVF_SPECULAR) s += 4;
	UINT tc = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	for (UINT i = 0; i < tc; ++i) { switch ((fvf >> (16 + i * 2)) & 3) { case D3DFVF_TEXTUREFORMAT1: s += 4; break; case D3DFVF_TEXTUREFORMAT2: s += 8; break; case D3DFVF_TEXTUREFORMAT3: s += 12; break; case D3DFVF_TEXTUREFORMAT4: s += 16; break; } }
	return s;
}

HRESULT WINAPI D3DXGetErrorStringA(HRESULT hr, char *buf, UINT len) { _snprintf(buf, len, "D3D error 0x%08lx", (unsigned long)hr); return D3D_OK; }

// ---------------------------------------------------------------------------------------------- pixel access
namespace {
struct Px { float a, r, g, b; };
static bool bpp(D3DFORMAT f, int *bytes)
{
	switch (f) {
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: *bytes = 4; return true;
	case D3DFMT_R8G8B8: *bytes = 3; return true;
	case D3DFMT_R5G6B5: case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: case D3DFMT_A4R4G4B4: case D3DFMT_A8L8: case D3DFMT_V8U8: *bytes = 2; return true;
	case D3DFMT_L8: case D3DFMT_A8: case D3DFMT_P8: case D3DFMT_A4L4: case D3DFMT_R3G3B2: *bytes = 1; return true;
	default: return false;
	}
}
static bool isDXT(D3DFORMAT f) { return f == D3DFMT_DXT1 || f == D3DFMT_DXT2 || f == D3DFMT_DXT3 || f == D3DFMT_DXT4 || f == D3DFMT_DXT5; }
static Px get(const unsigned char *p, D3DFORMAT f)
{
	Px o = { 1, 0, 0, 0 };
	switch (f) {
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: o.b = p[0] / 255.f; o.g = p[1] / 255.f; o.r = p[2] / 255.f; o.a = f == D3DFMT_A8R8G8B8 ? p[3] / 255.f : 1; break;
	case D3DFMT_R8G8B8: o.b = p[0] / 255.f; o.g = p[1] / 255.f; o.r = p[2] / 255.f; break;
	case D3DFMT_R5G6B5: { unsigned v = p[0] | (p[1] << 8); o.r = ((v >> 11) & 31) / 31.f; o.g = ((v >> 5) & 63) / 63.f; o.b = (v & 31) / 31.f; break; }
	case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: { unsigned v = p[0] | (p[1] << 8); o.a = f == D3DFMT_A1R5G5B5 ? ((v >> 15) & 1) : 1; o.r = ((v >> 10) & 31) / 31.f; o.g = ((v >> 5) & 31) / 31.f; o.b = (v & 31) / 31.f; break; }
	case D3DFMT_A4R4G4B4: { unsigned v = p[0] | (p[1] << 8); o.a = ((v >> 12) & 15) / 15.f; o.r = ((v >> 8) & 15) / 15.f; o.g = ((v >> 4) & 15) / 15.f; o.b = (v & 15) / 15.f; break; }
	case D3DFMT_L8: o.r = o.g = o.b = p[0] / 255.f; break;
	case D3DFMT_A8: o.a = p[0] / 255.f; break;
	case D3DFMT_A8L8: o.r = o.g = o.b = p[0] / 255.f; o.a = p[1] / 255.f; break;
	case D3DFMT_A4L4: o.r = o.g = o.b = (p[0] & 15) / 15.f; o.a = (p[0] >> 4) / 15.f; break;
	default: break;
	}
	return o;
}
static unsigned char q8(float v) { if (v < 0) v = 0; if (v > 1) v = 1; return (unsigned char)(v * 255.f + 0.5f); }
static void put(unsigned char *p, D3DFORMAT f, const Px &c)
{
	switch (f) {
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: p[0] = q8(c.b); p[1] = q8(c.g); p[2] = q8(c.r); p[3] = f == D3DFMT_A8R8G8B8 ? q8(c.a) : 255; break;
	case D3DFMT_R8G8B8: p[0] = q8(c.b); p[1] = q8(c.g); p[2] = q8(c.r); break;
	case D3DFMT_R5G6B5: { unsigned v = (unsigned)(q8(c.r) >> 3) << 11 | (unsigned)(q8(c.g) >> 2) << 5 | (q8(c.b) >> 3); p[0] = v & 255; p[1] = v >> 8; break; }
	case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: { unsigned v = (f == D3DFMT_A1R5G5B5 ? (c.a >= 0.5f ? 1u : 0u) : 1u) << 15 | (unsigned)(q8(c.r) >> 3) << 10 | (unsigned)(q8(c.g) >> 3) << 5 | (q8(c.b) >> 3); p[0] = v & 255; p[1] = v >> 8; break; }
	case D3DFMT_A4R4G4B4: { unsigned v = (unsigned)(q8(c.a) >> 4) << 12 | (unsigned)(q8(c.r) >> 4) << 8 | (unsigned)(q8(c.g) >> 4) << 4 | (q8(c.b) >> 4); p[0] = v & 255; p[1] = v >> 8; break; }
	case D3DFMT_L8: p[0] = q8((c.r + c.g + c.b) / 3); break;
	case D3DFMT_A8: p[0] = q8(c.a); break;
	case D3DFMT_A8L8: p[0] = q8((c.r + c.g + c.b) / 3); p[1] = q8(c.a); break;
	case D3DFMT_A4L4: p[0] = (q8(c.a) & 0xf0) | (q8((c.r + c.g + c.b) / 3) >> 4); break;
	default: break;
	}
}
static Px rgb565(unsigned v) { Px o = { 1, ((v >> 11) & 31) / 31.f, ((v >> 5) & 63) / 63.f, (v & 31) / 31.f }; return o; }
// Decode one pixel of a DXT block-compressed image.
static Px dxtPixel(const unsigned char *bits, int pitch, D3DFORMAT f, int x, int y)
{
	int blockBytes = f == D3DFMT_DXT1 ? 8 : 16;
	const unsigned char *b = bits + (y / 4) * pitch + (x / 4) * blockBytes;
	const unsigned char *cb = f == D3DFMT_DXT1 ? b : b + 8;
	unsigned c0 = cb[0] | (cb[1] << 8), c1 = cb[2] | (cb[3] << 8);
	Px p[4]; p[0] = rgb565(c0); p[1] = rgb565(c1);
	if (c0 > c1 || f != D3DFMT_DXT1) { for (int i = 0; i < 3; ++i) { float* a = &p[0].a; (void)a; } p[2].a = p[3].a = 1; p[2].r = (2 * p[0].r + p[1].r) / 3; p[2].g = (2 * p[0].g + p[1].g) / 3; p[2].b = (2 * p[0].b + p[1].b) / 3; p[3].r = (p[0].r + 2 * p[1].r) / 3; p[3].g = (p[0].g + 2 * p[1].g) / 3; p[3].b = (p[0].b + 2 * p[1].b) / 3; }
	else { p[2].a = 1; p[2].r = (p[0].r + p[1].r) / 2; p[2].g = (p[0].g + p[1].g) / 2; p[2].b = (p[0].b + p[1].b) / 2; p[3].a = 0; p[3].r = p[3].g = p[3].b = 0; }
	int idx = (cb[4 + (y & 3)] >> (2 * (x & 3))) & 3;
	Px o = p[idx];
	if (f == D3DFMT_DXT2 || f == D3DFMT_DXT3) { int n = (b[(y & 3) * 2 + ((x & 3) >> 1)] >> (4 * (x & 1))) & 15; o.a = n / 15.f; }
	else if (f == D3DFMT_DXT4 || f == D3DFMT_DXT5)
	{
		float a0 = b[0], a1 = b[1], t[8]; t[0] = a0; t[1] = a1;
		if (a0 > a1) for (int i = 1; i < 7; ++i) t[i + 1] = ((7 - i) * a0 + i * a1) / 7; else { for (int i = 1; i < 5; ++i) t[i + 1] = ((5 - i) * a0 + i * a1) / 5; t[6] = 0; t[7] = 255; }
		unsigned long long bitsA = 0; for (int i = 0; i < 6; ++i) bitsA |= (unsigned long long)b[2 + i] << (8 * i);
		o.a = t[(bitsA >> (3 * ((y & 3) * 4 + (x & 3)))) & 7] / 255.f;
	}
	return o;
}
struct SurfView
{
	D3DLOCKED_RECT lr; D3DSURFACE_DESC d; LPDIRECT3DSURFACE8 s; bool ok;
	SurfView(LPDIRECT3DSURFACE8 surf, bool readOnly) : s(surf), ok(false) { memset(&lr, 0, sizeof lr); if (surf && SUCCEEDED(surf->GetDesc(&d)) && SUCCEEDED(surf->LockRect(&lr, NULL, readOnly ? D3DLOCK_READONLY : 0))) ok = true; }
	~SurfView() { if (ok) s->UnlockRect(); }
	Px read(int x, int y) const
	{
		if (isDXT(d.Format)) return dxtPixel((const unsigned char *)lr.pBits, lr.Pitch, d.Format, x, y);
		int b = 4; bpp(d.Format, &b); return get((const unsigned char *)lr.pBits + y * lr.Pitch + x * b, d.Format);
	}
};
}

HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE8 dst, const PALETTEENTRY *, const RECT *dstRect, LPDIRECT3DSURFACE8 src, const PALETTEENTRY *, const RECT *srcRect, DWORD filter, D3DCOLOR)
{
	SurfView S(src, true), D(dst, false);
	if (!S.ok || !D.ok) return D3DERR_INVALIDCALL;
	int db = 0; if (!bpp(D.d.Format, &db)) return D3DERR_INVALIDCALL;     // block-compressed destinations are not supported
	RECT sr = { 0, 0, (LONG)S.d.Width, (LONG)S.d.Height }, dr = { 0, 0, (LONG)D.d.Width, (LONG)D.d.Height };
	if (srcRect) sr = *srcRect; if (dstRect) dr = *dstRect;
	int sw = sr.right - sr.left, sh = sr.bottom - sr.top, dw = dr.right - dr.left, dh = dr.bottom - dr.top;
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return D3DERR_INVALIDCALL;
	bool box = (filter & 7) == D3DX_FILTER_BOX || (filter & 7) == D3DX_FILTER_TRIANGLE || (filter & 7) == D3DX_FILTER_LINEAR;
	for (int y = 0; y < dh; ++y) for (int x = 0; x < dw; ++x)
	{
		Px c = { 0, 0, 0, 0 };
		if (box && (sw > dw || sh > dh))
		{
			int x0 = x * sw / dw, x1 = (x + 1) * sw / dw; if (x1 <= x0) x1 = x0 + 1; int y0 = y * sh / dh, y1 = (y + 1) * sh / dh; if (y1 <= y0) y1 = y0 + 1; int n = 0;
			for (int yy = y0; yy < y1; ++yy) for (int xx = x0; xx < x1; ++xx) { Px p = S.read(sr.left + xx, sr.top + yy); c.a += p.a; c.r += p.r; c.g += p.g; c.b += p.b; ++n; }
			c.a /= n; c.r /= n; c.g /= n; c.b /= n;
		}
		else c = S.read(sr.left + x * sw / dw, sr.top + y * sh / dh);
		put((unsigned char *)D.lr.pBits + (dr.top + y) * D.lr.Pitch + (dr.left + x) * db, D.d.Format, c);
	}
	return D3D_OK;
}

HRESULT WINAPI D3DXFilterTexture(LPDIRECT3DBASETEXTURE8 base, const PALETTEENTRY *pal, UINT srcLevel, DWORD filter)
{
	if (!base || base->GetType() != D3DRTYPE_TEXTURE) return D3D_OK;      // cube/volume mip chains come from the files
	LPDIRECT3DTEXTURE8 t = (LPDIRECT3DTEXTURE8)base; DWORD n = t->GetLevelCount();
	for (DWORD l = srcLevel + 1; l < n; ++l)
	{
		LPDIRECT3DSURFACE8 a = NULL, b = NULL;
		if (FAILED(t->GetSurfaceLevel(l - 1, &a))) break;
		if (FAILED(t->GetSurfaceLevel(l, &b))) { a->Release(); break; }
		D3DXLoadSurfaceFromSurface(b, pal, NULL, a, pal, NULL, D3DX_FILTER_BOX, 0);
		a->Release(); b->Release();
	}
	return D3D_OK;
}

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE8 d, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DTEXTURE8 *out)
{
	if (w == D3DX_DEFAULT || w == 0) w = 256; if (h == D3DX_DEFAULT || h == 0) h = 256;
	if (levels == D3DX_DEFAULT) { levels = 1; for (UINT m = w > h ? w : h; m > 1; m >>= 1) ++levels; }
	if (fmt == D3DFMT_UNKNOWN) fmt = D3DFMT_A8R8G8B8;
	return d->CreateTexture(w, h, levels, usage, fmt, pool, out);
}
HRESULT WINAPI D3DXCreateCubeTexture(LPDIRECT3DDEVICE8 d, UINT size, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DCUBETEXTURE8 *out)
{
	if (levels == D3DX_DEFAULT) { levels = 1; for (UINT m = size; m > 1; m >>= 1) ++levels; }
	return d->CreateCubeTexture(size, levels, usage, fmt, pool, out);
}
HRESULT WINAPI D3DXCreateVolumeTexture(LPDIRECT3DDEVICE8 d, UINT w, UINT h, UINT depth, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DVOLUMETEXTURE8 *out)
{
	if (levels == D3DX_DEFAULT) levels = 1;
	return d->CreateVolumeTexture(w, h, depth, levels, usage, fmt, pool, out);
}

// ---------------------------------------------------------------------------------------------- file loading (TGA and BMP)
namespace {
struct Img { int w, h; std::vector<unsigned char> bgra; };
static bool loadImage(const char *file, Img &im)
{
	FILE *f = fopen(file, "rb"); if (!f) return false;
	std::vector<unsigned char> d; unsigned char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n); fclose(f);
	if (d.size() > 18 && (d[2] == 2 || d[2] == 10) && (d[16] == 24 || d[16] == 32))
	{
		int w = d[12] | (d[13] << 8), h = d[14] | (d[15] << 8), bytes = d[16] / 8; bool top = (d[17] & 0x20) != 0; im.w = w; im.h = h; im.bgra.assign(w * h * 4, 255);
		size_t p = 18 + d[0]; std::vector<unsigned char> px((size_t)w * h * bytes);
		if (d[2] == 2) { if (p + px.size() > d.size()) return false; memcpy(&px[0], &d[p], px.size()); }
		else { size_t o = 0; while (o < px.size() && p < d.size()) { unsigned char c = d[p++]; int cnt = (c & 127) + 1; if (c & 128) { for (int i = 0; i < cnt && o < px.size(); ++i) for (int k = 0; k < bytes; ++k) px[o++] = d[p + k]; p += bytes; } else for (int i = 0; i < cnt * bytes && o < px.size() && p < d.size(); ++i) px[o++] = d[p++]; } }
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { const unsigned char *s = &px[((size_t)(top ? y : h - 1 - y) * w + x) * bytes]; unsigned char *o = &im.bgra[((size_t)y * w + x) * 4]; o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = bytes == 4 ? s[3] : 255; }
		return true;
	}
	if (d.size() > 54 && d[0] == 'B' && d[1] == 'M')
	{
		int off = *(int *)&d[10], w = *(int *)&d[18], h = *(int *)&d[22], bits = *(short *)&d[28]; bool flip = h > 0; if (h < 0) h = -h; if (bits != 24 && bits != 32) return false;
		int stride = ((w * bits / 8) + 3) & ~3; im.w = w; im.h = h; im.bgra.assign(w * h * 4, 255);
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { const unsigned char *s = &d[off + (size_t)(flip ? h - 1 - y : y) * stride + x * bits / 8]; unsigned char *o = &im.bgra[((size_t)y * w + x) * 4]; o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = bits == 32 ? s[3] : 255; }
		return true;
	}
	return false;
}
static void fill(LPDIRECT3DSURFACE8 dst, const Img &im)
{
	SurfView D(dst, false); if (!D.ok) return; int db = 0; if (!bpp(D.d.Format, &db)) return;
	for (UINT y = 0; y < D.d.Height; ++y) for (UINT x = 0; x < D.d.Width; ++x)
	{
		const unsigned char *s = &im.bgra[((size_t)(y * im.h / D.d.Height) * im.w + (x * im.w / D.d.Width)) * 4];
		Px c = { s[3] / 255.f, s[2] / 255.f, s[1] / 255.f, s[0] / 255.f }; put((unsigned char *)D.lr.pBits + y * D.lr.Pitch + x * db, D.d.Format, c);
	}
}
}
HRESULT WINAPI D3DXLoadSurfaceFromFileA(LPDIRECT3DSURFACE8 dst, const PALETTEENTRY *, const RECT *, const char *file, const RECT *, DWORD, D3DCOLOR, D3DXIMAGE_INFO *info)
{
	Img im; if (!loadImage(file, im)) return D3DERR_NOTFOUND; fill(dst, im);
	if (info) { memset(info, 0, sizeof *info); info->Width = im.w; info->Height = im.h; info->Depth = 1; info->MipLevels = 1; info->Format = D3DFMT_A8R8G8B8; }
	return D3D_OK;
}
HRESULT WINAPI D3DXCreateSurfaceFromFileA(LPDIRECT3DDEVICE8 d, LPDIRECT3DSURFACE8 *out, const char *file)
{
	Img im; if (!loadImage(file, im)) return D3DERR_NOTFOUND;
	HRESULT hr = d->CreateImageSurface(im.w, im.h, D3DFMT_A8R8G8B8, out); if (FAILED(hr)) return hr; fill(*out, im); return D3D_OK;
}
HRESULT WINAPI D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE8 d, const char *file, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, DWORD, DWORD, D3DCOLOR, D3DXIMAGE_INFO *info, PALETTEENTRY *, LPDIRECT3DTEXTURE8 *out)
{
	Img im; if (!loadImage(file, im)) return D3DERR_NOTFOUND;
	if (w == D3DX_DEFAULT || !w) w = im.w; if (h == D3DX_DEFAULT || !h) h = im.h;
	HRESULT hr = D3DXCreateTexture(d, w, h, levels, usage, fmt, pool, out); if (FAILED(hr)) return hr;
	LPDIRECT3DSURFACE8 s0 = NULL; if (SUCCEEDED((*out)->GetSurfaceLevel(0, &s0))) { fill(s0, im); s0->Release(); }
	D3DXFilterTexture(*out, NULL, 0, D3DX_FILTER_BOX);
	if (info) { memset(info, 0, sizeof *info); info->Width = im.w; info->Height = im.h; info->Depth = 1; info->MipLevels = 1; info->Format = D3DFMT_A8R8G8B8; }
	return D3D_OK;
}
