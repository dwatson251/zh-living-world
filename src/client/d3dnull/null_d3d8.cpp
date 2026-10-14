// Living World: a Direct3D 8 that draws nothing. The dedicated server runs the retail W3D model, animation and scene code (the game logic asks the render
// objects for bone positions, so server and clients must run the same code), but has no GPU: every resource lives in memory and every draw call is ignored.
#include "gen_d3d8.h"
#include <string.h>
#include <stdlib.h>
#include <vector>

namespace {

int bytesPerPixel(D3DFORMAT f)
{
	switch (f)
	{
	case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_D32: case D3DFMT_D24S8: case D3DFMT_D24X8: case D3DFMT_Q8W8V8U8: case D3DFMT_V16U16: case D3DFMT_W11V11U10: return 4;
	case D3DFMT_R8G8B8: return 3;
	case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4: case D3DFMT_A8L8: case D3DFMT_V8U8: case D3DFMT_L6V5U5: case D3DFMT_D16: case D3DFMT_D16_LOCKABLE: case D3DFMT_D15S1: case D3DFMT_A8R3G3B2: case D3DFMT_X4R4G4B4: return 2;
	default: return 1;
	}
}
bool isDXT(D3DFORMAT f) { return f == D3DFMT_DXT1 || f == D3DFMT_DXT2 || f == D3DFMT_DXT3 || f == D3DFMT_DXT4 || f == D3DFMT_DXT5; }
void levelLayout(UINT w, UINT h, D3DFORMAT f, UINT *pitch, size_t *bytes)
{
	if (isDXT(f)) { UINT bb = f == D3DFMT_DXT1 ? 8 : 16, bw = (w + 3) / 4 ? (w + 3) / 4 : 1, bh = (h + 3) / 4 ? (h + 3) / 4 : 1; *pitch = bw * bb; *bytes = (size_t)*pitch * bh; }
	else { *pitch = w * bytesPerPixel(f); *bytes = (size_t)*pitch * h; }
}
UINT mipCount(UINT w, UINT h) { UINT n = 1; while (w > 1 || h > 1) { w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1; ++n; } return n; }

class RefCounted
{
public:
	RefCounted() : m_refs(1) {}
	virtual ~RefCounted() {}
protected:
	LONG m_refs;
};

#define UM_UNKNOWN(CLS, IID) \
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) { if (!ppv) return E_POINTER; *ppv = this; AddRef(); return S_OK; } \
	ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&m_refs); } \
	ULONG STDMETHODCALLTYPE Release() { LONG r = InterlockedDecrement(&m_refs); if (r == 0) delete this; return r; }

// ------------------------------------------------------------------------------------------------ surfaces
class NullSurface : public GenIDirect3DSurface8, public RefCounted
{
public:
	NullSurface(UINT w, UINT h, D3DFORMAT f, DWORD usage, D3DPOOL pool, bool owned = false) : m_owned(owned)
	{
		memset(&m_desc, 0, sizeof m_desc); m_desc.Format = f; m_desc.Type = D3DRTYPE_SURFACE; m_desc.Usage = usage; m_desc.Pool = pool; m_desc.Width = w; m_desc.Height = h;
		levelLayout(w, h, f, &m_pitch, &m_size); m_desc.Size = (UINT)m_size; m_mem = (unsigned char *)calloc(1, m_size ? m_size : 1);
	}
	~NullSurface() { free(m_mem); }
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **ppv) { if (!ppv) return E_POINTER; *ppv = this; AddRef(); return S_OK; }
	ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&m_refs); }
	ULONG STDMETHODCALLTYPE Release() { LONG r = InterlockedDecrement(&m_refs); if (r <= 0 && !m_owned) { delete this; return 0; } return r < 0 ? 0 : r; }
	HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *d) { *d = m_desc; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT *lr, const RECT *rect, DWORD) { lr->Pitch = m_pitch; lr->pBits = m_mem + (rect && !isDXT(m_desc.Format) ? (size_t)rect->top * m_pitch + (size_t)rect->left * bytesPerPixel(m_desc.Format) : 0); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE UnlockRect() { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetContainer(REFIID, void **ppContainer) { if (ppContainer) *ppContainer = NULL; return D3DERR_INVALIDCALL; }
	unsigned char *bits() { return m_mem; } UINT pitch() const { return m_pitch; } const D3DSURFACE_DESC &desc() const { return m_desc; } void forceRelease() { m_owned = false; }
private:
	D3DSURFACE_DESC m_desc; UINT m_pitch; size_t m_size; unsigned char *m_mem; bool m_owned;
};

// ------------------------------------------------------------------------------------------------ textures
class NullTexture : public GenIDirect3DTexture8, public RefCounted
{
public:
	NullTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT f, D3DPOOL pool) : m_lod(0), m_usage(usage), m_pool(pool)
	{
		UINT n = levels ? levels : mipCount(w, h);
		for (UINT i = 0; i < n; ++i) { m_levels.push_back(new NullSurface(w, h, f, usage, pool, true)); w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1; }
	}
	~NullTexture() { for (size_t i = 0; i < m_levels.size(); ++i) { m_levels[i]->forceRelease(); delete m_levels[i]; } }
	UM_UNKNOWN(NullTexture, 0)
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8 **ppDevice) { if (ppDevice) *ppDevice = NULL; return D3DERR_INVALIDCALL; }
	DWORD STDMETHODCALLTYPE SetPriority(DWORD p) { return 0; } DWORD STDMETHODCALLTYPE GetPriority() { return 0; }
	D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_TEXTURE; }
	DWORD STDMETHODCALLTYPE SetLOD(DWORD l) { DWORD o = m_lod; m_lod = l; return o; } DWORD STDMETHODCALLTYPE GetLOD() { return m_lod; }
	DWORD STDMETHODCALLTYPE GetLevelCount() { return (DWORD)m_levels.size(); }
	HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT l, D3DSURFACE_DESC *d) { if (l >= m_levels.size()) return D3DERR_INVALIDCALL; *d = m_levels[l]->desc(); d->Type = D3DRTYPE_TEXTURE; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT l, IDirect3DSurface8 **s) { if (l >= m_levels.size()) return D3DERR_INVALIDCALL; m_levels[l]->AddRef(); *s = m_levels[l]; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE LockRect(UINT l, D3DLOCKED_RECT *lr, const RECT *rect, DWORD f) { if (l >= m_levels.size()) return D3DERR_INVALIDCALL; return m_levels[l]->LockRect(lr, rect, f); }
	HRESULT STDMETHODCALLTYPE UnlockRect(UINT) { return D3D_OK; }
	NullSurface *level(UINT l) { return l < m_levels.size() ? m_levels[l] : NULL; }
private:
	std::vector<NullSurface *> m_levels; DWORD m_lod, m_usage; D3DPOOL m_pool;
};

class NullCubeTexture : public GenIDirect3DCubeTexture8, public RefCounted
{
public:
	NullCubeTexture(UINT size, UINT levels, DWORD usage, D3DFORMAT f, D3DPOOL pool) : m_lod(0)
	{
		UINT n = levels ? levels : mipCount(size, size);
		for (int face = 0; face < 6; ++face) { UINT s = size; for (UINT i = 0; i < n; ++i) { m_faces[face].push_back(new NullSurface(s, s, f, usage, pool, true)); s = s > 1 ? s / 2 : 1; } }
		m_n = n;
	}
	~NullCubeTexture() { for (int f = 0; f < 6; ++f) for (size_t i = 0; i < m_faces[f].size(); ++i) { m_faces[f][i]->forceRelease(); delete m_faces[f][i]; } }
	UM_UNKNOWN(NullCubeTexture, 0)
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8 **p) { if (p) *p = NULL; return D3DERR_INVALIDCALL; }
	D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_CUBETEXTURE; }
	DWORD STDMETHODCALLTYPE SetLOD(DWORD l) { DWORD o = m_lod; m_lod = l; return o; } DWORD STDMETHODCALLTYPE GetLOD() { return m_lod; }
	DWORD STDMETHODCALLTYPE GetLevelCount() { return m_n; }
	HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT l, D3DSURFACE_DESC *d) { if (l >= m_n) return D3DERR_INVALIDCALL; *d = m_faces[0][l]->desc(); d->Type = D3DRTYPE_CUBETEXTURE; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES face, UINT l, IDirect3DSurface8 **s) { if (l >= m_n || (UINT)face > 5) return D3DERR_INVALIDCALL; m_faces[face][l]->AddRef(); *s = m_faces[face][l]; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES face, UINT l, D3DLOCKED_RECT *lr, const RECT *rect, DWORD f) { if (l >= m_n || (UINT)face > 5) return D3DERR_INVALIDCALL; return m_faces[face][l]->LockRect(lr, rect, f); }
private:
	std::vector<NullSurface *> m_faces[6]; UINT m_n; DWORD m_lod;
};

// ------------------------------------------------------------------------------------------------ buffers
class NullVertexBuffer : public GenIDirect3DVertexBuffer8, public RefCounted
{
public:
	NullVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool) { memset(&m_desc, 0, sizeof m_desc); m_desc.Format = D3DFMT_VERTEXDATA; m_desc.Type = D3DRTYPE_VERTEXBUFFER; m_desc.Usage = usage; m_desc.Pool = pool; m_desc.Size = length; m_desc.FVF = fvf; m_mem = (BYTE *)calloc(1, length ? length : 1); }
	~NullVertexBuffer() { free(m_mem); }
	UM_UNKNOWN(NullVertexBuffer, 0)
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8 **p) { if (p) *p = NULL; return D3DERR_INVALIDCALL; }
	D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_VERTEXBUFFER; }
	HRESULT STDMETHODCALLTYPE Lock(UINT off, UINT size, BYTE **data, DWORD) { *data = m_mem + off; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE Unlock() { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC *d) { *d = m_desc; return D3D_OK; }
private:
	D3DVERTEXBUFFER_DESC m_desc; BYTE *m_mem;
};
class NullIndexBuffer : public GenIDirect3DIndexBuffer8, public RefCounted
{
public:
	NullIndexBuffer(UINT length, DWORD usage, D3DFORMAT f, D3DPOOL pool) { memset(&m_desc, 0, sizeof m_desc); m_desc.Format = f; m_desc.Type = D3DRTYPE_INDEXBUFFER; m_desc.Usage = usage; m_desc.Pool = pool; m_desc.Size = length; m_mem = (BYTE *)calloc(1, length ? length : 1); }
	~NullIndexBuffer() { free(m_mem); }
	UM_UNKNOWN(NullIndexBuffer, 0)
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8 **p) { if (p) *p = NULL; return D3DERR_INVALIDCALL; }
	D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_INDEXBUFFER; }
	HRESULT STDMETHODCALLTYPE Lock(UINT off, UINT size, BYTE **data, DWORD) { *data = m_mem + off; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE Unlock() { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC *d) { *d = m_desc; return D3D_OK; }
private:
	D3DINDEXBUFFER_DESC m_desc; BYTE *m_mem;
};

// ------------------------------------------------------------------------------------------------ the device
class NullD3D;
class NullDevice : public GenIDirect3DDevice8, public RefCounted
{
public:
	NullDevice(NullD3D *d3d, const D3DPRESENT_PARAMETERS &pp) : m_d3d(d3d), m_pp(pp), m_back(NULL), m_depth(NULL)
	{
		memset(m_rs, 0, sizeof m_rs); memset(m_tss, 0, sizeof m_tss); memset(m_tex, 0, sizeof m_tex);
		makeTargets();
	}
	~NullDevice() { if (m_back) m_back->forceRelease(), delete m_back; if (m_depth) m_depth->forceRelease(), delete m_depth; }
	UM_UNKNOWN(NullDevice, 0)
	HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8 **pp);
	HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8 *caps);
	HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *m) { m->Width = m_pp.BackBufferWidth ? m_pp.BackBufferWidth : 1024; m->Height = m_pp.BackBufferHeight ? m_pp.BackBufferHeight : 768; m->RefreshRate = 60; m->Format = D3DFMT_X8R8G8B8; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *pp) { m_pp = *pp; makeTargets(); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8 **s) { m_back->AddRef(); *s = m_back; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8 **s) { m_back->AddRef(); *s = m_back; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8 **s) { m_depth->AddRef(); *s = m_depth; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT f, D3DPOOL pool, IDirect3DTexture8 **out) { *out = new NullTexture(w, h, levels, usage, f, pool); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT size, UINT levels, DWORD usage, D3DFORMAT f, D3DPOOL pool, IDirect3DCubeTexture8 **out) { *out = new NullCubeTexture(size, levels, usage, f, pool); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture8 **out) { *out = NULL; return D3DERR_NOTAVAILABLE; }
	HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT len, DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer8 **out) { *out = new NullVertexBuffer(len, usage, fvf, pool); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT len, DWORD usage, D3DFORMAT f, D3DPOOL pool, IDirect3DIndexBuffer8 **out) { *out = new NullIndexBuffer(len, usage, f, pool); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT w, UINT h, D3DFORMAT f, D3DMULTISAMPLE_TYPE, WINBOOL, IDirect3DSurface8 **out) { *out = new NullSurface(w, h, f, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT f, D3DMULTISAMPLE_TYPE, IDirect3DSurface8 **out) { *out = new NullSurface(w, h, f, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT w, UINT h, D3DFORMAT f, IDirect3DSurface8 **out) { *out = new NullSurface(w, h, f, 0, D3DPOOL_SCRATCH); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8 *src, const RECT *srcRects, UINT n, IDirect3DSurface8 *dst, const POINT *dstPts)
	{
		NullSurface *s = (NullSurface *)src, *d = (NullSurface *)dst;
		if (!s || !d || n > 1 || s->desc().Format != d->desc().Format || isDXT(s->desc().Format)) return D3D_OK;
		UINT bpp = bytesPerPixel(s->desc().Format); RECT r = { 0, 0, (LONG)s->desc().Width, (LONG)s->desc().Height }; if (n == 1 && srcRects) r = srcRects[0];
		LONG dx = dstPts ? dstPts[0].x : r.left, dy = dstPts ? dstPts[0].y : r.top;
		for (LONG y = r.top; y < r.bottom; ++y) { LONG ty = y - r.top + dy; if (ty < 0 || ty >= (LONG)d->desc().Height) continue; LONG w = r.right - r.left; if (dx + w > (LONG)d->desc().Width) w = d->desc().Width - dx; if (w > 0) memcpy(d->bits() + (size_t)ty * d->pitch() + (size_t)dx * bpp, s->bits() + (size_t)y * s->pitch() + (size_t)r.left * bpp, (size_t)w * bpp); }
		return D3D_OK;
	}
	HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE s, DWORD v) { if ((UINT)s < 256) m_rs[s] = v; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE s, DWORD *v) { *v = (UINT)s < 256 ? m_rs[s] : 0; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD st, D3DTEXTURESTAGESTATETYPE t, DWORD v) { if (st < 8 && (UINT)t < 40) m_tss[st][t] = v; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD st, D3DTEXTURESTAGESTATETYPE t, DWORD *v) { *v = (st < 8 && (UINT)t < 40) ? m_tss[st][t] : 0; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE SetTexture(DWORD st, IDirect3DBaseTexture8 *t)
	{
		if (st >= 8) return D3D_OK; if (t) t->AddRef(); if (m_tex[st]) m_tex[st]->Release(); m_tex[st] = t; return D3D_OK;
	}
	HRESULT STDMETHODCALLTYPE GetTexture(DWORD st, IDirect3DBaseTexture8 **t) { *t = st < 8 ? m_tex[st] : NULL; if (*t) (*t)->AddRef(); return D3D_OK; }
	HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD *passes) { *passes = 1; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *, IDirect3DSwapChain8 **s) { *s = NULL; return D3DERR_NOTAVAILABLE; }
	UINT STDMETHODCALLTYPE GetAvailableTextureMem() { return 256u * 1024u * 1024u; }
	HRESULT STDMETHODCALLTYPE GetFrontBuffer(IDirect3DSurface8 *) { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD *, const DWORD *, DWORD *h, DWORD) { *h = 0; return D3DERR_NOTAVAILABLE; }
	HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD *, DWORD *h) { *h = 0; return D3DERR_NOTAVAILABLE; }
private:
	void makeTargets()
	{
		UINT w = m_pp.BackBufferWidth ? m_pp.BackBufferWidth : 1024, h = m_pp.BackBufferHeight ? m_pp.BackBufferHeight : 768;
		if (m_back) { m_back->forceRelease(); delete m_back; } if (m_depth) { m_depth->forceRelease(); delete m_depth; }
		m_back = new NullSurface(w, h, m_pp.BackBufferFormat == D3DFMT_UNKNOWN ? D3DFMT_X8R8G8B8 : m_pp.BackBufferFormat, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, true);
		m_depth = new NullSurface(w, h, m_pp.AutoDepthStencilFormat == D3DFMT_UNKNOWN ? D3DFMT_D24S8 : m_pp.AutoDepthStencilFormat, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, true);
	}
	NullD3D *m_d3d; D3DPRESENT_PARAMETERS m_pp; NullSurface *m_back, *m_depth; DWORD m_rs[256], m_tss[8][40]; IDirect3DBaseTexture8 *m_tex[8];
};

// ------------------------------------------------------------------------------------------------ the factory
class NullD3D : public GenIDirect3D8, public RefCounted
{
public:
	UM_UNKNOWN(NullD3D, 0)
	UINT STDMETHODCALLTYPE GetAdapterCount() { return 1; }
	HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT, DWORD, D3DADAPTER_IDENTIFIER8 *id)
	{
		memset(id, 0, sizeof *id); strcpy(id->Driver, "nullgpu.dll"); strcpy(id->Description, "Living World null display"); id->VendorId = 0x10DE; id->DeviceId = 0x0250; id->DriverVersion.QuadPart = 0x0006000E00000000ULL; return D3D_OK;
	}
	UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT) { return 2; }
	HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT, UINT i, D3DDISPLAYMODE *m) { m->Width = i ? 1280 : 1024; m->Height = i ? 720 : 768; m->RefreshRate = 60; m->Format = D3DFMT_X8R8G8B8; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT, D3DDISPLAYMODE *m) { m->Width = 1024; m->Height = 768; m->RefreshRate = 60; m->Format = D3DFMT_X8R8G8B8; return D3D_OK; }
	HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS8 *c);
	HRESULT STDMETHODCALLTYPE CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice8 **out) { *out = new NullDevice(this, *pp); return D3D_OK; }
};

HRESULT NullD3D::GetDeviceCaps(UINT, D3DDEVTYPE type, D3DCAPS8 *c)
{
	memset(c, 0, sizeof *c);
	c->DeviceType = type; c->AdapterOrdinal = 0;
	c->Caps = D3DCAPS_READ_SCANLINE; c->Caps2 = D3DCAPS2_CANRENDERWINDOWED | D3DCAPS2_FULLSCREENGAMMA; c->Caps3 = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD;
	c->PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;
	c->DevCaps = D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY | D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY | D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_CANRENDERAFTERFLIP | D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX | D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE | D3DDEVCAPS_HWRASTERIZATION;
	c->PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW | D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_CLIPPLANESCALEDPOINTS | D3DPMISCCAPS_CLIPTLVERTS | D3DPMISCCAPS_TSSARGTEMP | D3DPMISCCAPS_BLENDOP;
	c->RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZBIAS | D3DPRASTERCAPS_ANISOTROPY | D3DPRASTERCAPS_WFOG | D3DPRASTERCAPS_ZFOG;
	c->ZCmpCaps = 0xff; c->SrcBlendCaps = 0x3ff; c->DestBlendCaps = 0x3ff; c->AlphaCmpCaps = 0xff; c->ShadeCaps = 0xffffffff;
	c->TextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_PROJECTED | D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPMAP | D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_VOLUMEMAP | D3DPTEXTURECAPS_MIPVOLUMEMAP;
	c->TextureFilterCaps = 0x03030303; c->CubeTextureFilterCaps = 0x03030303; c->VolumeTextureFilterCaps = 0x03030303;
	c->TextureAddressCaps = 0x3f; c->VolumeTextureAddressCaps = 0x3f; c->LineCaps = 0x1f;
	c->MaxTextureWidth = 4096; c->MaxTextureHeight = 4096; c->MaxVolumeExtent = 256; c->MaxTextureRepeat = 8192; c->MaxTextureAspectRatio = 4096; c->MaxAnisotropy = 8;
	c->MaxVertexW = 10000000000.0f; c->GuardBandLeft = -32768; c->GuardBandTop = -32768; c->GuardBandRight = 32768; c->GuardBandBottom = 32768; c->ExtentsAdjust = 0;
	c->StencilCaps = 0xff; c->FVFCaps = 8 | D3DFVFCAPS_DONOTSTRIPELEMENTS; c->TextureOpCaps = 0x3ffffff;
	c->MaxTextureBlendStages = 8; c->MaxSimultaneousTextures = 4;
	c->VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 | D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS | D3DVTXPCAPS_LOCALVIEWER;
	c->MaxActiveLights = 8; c->MaxUserClipPlanes = 6; c->MaxVertexBlendMatrices = 4; c->MaxVertexBlendMatrixIndex = 0; c->MaxPointSize = 64.0f; c->MaxPrimitiveCount = 0xfffff; c->MaxVertexIndex = 0xffffff; c->MaxStreams = 16; c->MaxStreamStride = 255;
	c->VertexShaderVersion = D3DVS_VERSION(1, 1); c->MaxVertexShaderConst = 96; c->PixelShaderVersion = D3DPS_VERSION(1, 1); c->MaxPixelShaderValue = 1.0f;
	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE NullDevice::GetDirect3D(IDirect3D8 **pp) { *pp = m_d3d ? (IDirect3D8 *)m_d3d : NULL; if (*pp) (*pp)->AddRef(); return D3D_OK; }
HRESULT STDMETHODCALLTYPE NullDevice::GetDeviceCaps(D3DCAPS8 *caps) { return ((IDirect3D8 *)m_d3d)->GetDeviceCaps(0, D3DDEVTYPE_HAL, caps); }

}  // namespace

IDirect3D8 *WINAPI UmDirect3DCreate8(UINT) { return new NullD3D; }
