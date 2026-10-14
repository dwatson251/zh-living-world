/* Living World: a small D3DX8 replacement. The DirectX 8 SDK is not available; the render stack only uses the math helpers
 * and a handful of texture/surface helpers, implemented in src/client/d3dx8_impl.cpp. Row-vector convention like D3DX. */
#ifndef UM_D3DX8_H
#define UM_D3DX8_H
#include <windows.h>
#include <d3d8.h>
#include <math.h>

#ifndef D3DSGR_NO_CALIBRATION
#define D3DSGR_NO_CALIBRATION 0
#define D3DSGR_CALIBRATE 1
#endif
#ifndef D3DCURSOR_IMMEDIATE_UPDATE
#define D3DCURSOR_IMMEDIATE_UPDATE 0x00000001L
#endif
#define D3DX_PI    3.14159265358979323846f
#define D3DXToRadian(d) ((d) * (D3DX_PI / 180.0f))
#define D3DXToDegree(r) ((r) * (180.0f / D3DX_PI))
#define D3DX_DEFAULT ((UINT)-1)
#define D3DX_FILTER_NONE 1
#define D3DX_FILTER_POINT 2
#define D3DX_FILTER_LINEAR 3
#define D3DX_FILTER_TRIANGLE 4
#define D3DX_FILTER_BOX 5
#define D3DX_FILTER_MIRROR_U 0x10000
#define D3DX_FILTER_MIRROR_V 0x20000
#define D3DX_FILTER_MIRROR_W 0x40000
#define D3DX_FILTER_MIRROR 0x70000
#define D3DX_FILTER_DITHER 0x80000

struct D3DXVECTOR2 { float x, y; D3DXVECTOR2() {} D3DXVECTOR2(float X, float Y) : x(X), y(Y) {} };
struct D3DXVECTOR3 : public D3DVECTOR
{
	D3DXVECTOR3() {} D3DXVECTOR3(const D3DVECTOR &v) { x = v.x; y = v.y; z = v.z; } D3DXVECTOR3(float X, float Y, float Z) { x = X; y = Y; z = Z; }
	operator float *() { return &x; } operator const float *() const { return &x; }
	D3DXVECTOR3 operator+(const D3DXVECTOR3 &o) const { return D3DXVECTOR3(x + o.x, y + o.y, z + o.z); }
	D3DXVECTOR3 operator-(const D3DXVECTOR3 &o) const { return D3DXVECTOR3(x - o.x, y - o.y, z - o.z); }
	D3DXVECTOR3 operator*(float f) const { return D3DXVECTOR3(x * f, y * f, z * f); }
};
struct D3DXVECTOR4 { float x, y, z, w; D3DXVECTOR4() {} D3DXVECTOR4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {} operator float *() { return &x; } operator const float *() const { return &x; } };
struct D3DXMATRIX : public D3DMATRIX
{
	D3DXMATRIX() {}
	D3DXMATRIX(const D3DMATRIX &m) { *(D3DMATRIX *)this = m; }
	D3DXMATRIX(float a11, float a12, float a13, float a14, float a21, float a22, float a23, float a24,
	           float a31, float a32, float a33, float a34, float a41, float a42, float a43, float a44)
	{ _11 = a11; _12 = a12; _13 = a13; _14 = a14; _21 = a21; _22 = a22; _23 = a23; _24 = a24; _31 = a31; _32 = a32; _33 = a33; _34 = a34; _41 = a41; _42 = a42; _43 = a43; _44 = a44; }
	float &operator()(UINT r, UINT c) { return m[r][c]; } float operator()(UINT r, UINT c) const { return m[r][c]; }
	operator float *() { return &_11; } operator const float *() const { return &_11; }
	D3DXMATRIX operator*(const D3DXMATRIX &o) const;
	D3DXMATRIX &operator*=(const D3DXMATRIX &o);
};

inline D3DXMATRIX *D3DXMatrixIdentity(D3DXMATRIX *o)
{ o->_11 = o->_22 = o->_33 = o->_44 = 1; o->_12 = o->_13 = o->_14 = o->_21 = o->_23 = o->_24 = o->_31 = o->_32 = o->_34 = o->_41 = o->_42 = o->_43 = 0; return o; }
inline D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *o, const D3DXMATRIX *a, const D3DXMATRIX *b)
{
	D3DXMATRIX r;
	for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) r.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j] + a->m[i][2] * b->m[2][j] + a->m[i][3] * b->m[3][j];
	*o = r; return o;
}
inline D3DXMATRIX D3DXMATRIX::operator*(const D3DXMATRIX &o) const { D3DXMATRIX r; D3DXMatrixMultiply(&r, this, &o); return r; }
inline D3DXMATRIX &D3DXMATRIX::operator*=(const D3DXMATRIX &o) { D3DXMatrixMultiply(this, this, &o); return *this; }
inline D3DXMATRIX *D3DXMatrixTranspose(D3DXMATRIX *o, const D3DXMATRIX *a) { D3DXMATRIX r; for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) r.m[i][j] = a->m[j][i]; *o = r; return o; }
inline D3DXMATRIX *D3DXMatrixScaling(D3DXMATRIX *o, float x, float y, float z) { D3DXMatrixIdentity(o); o->_11 = x; o->_22 = y; o->_33 = z; return o; }
inline D3DXMATRIX *D3DXMatrixTranslation(D3DXMATRIX *o, float x, float y, float z) { D3DXMatrixIdentity(o); o->_41 = x; o->_42 = y; o->_43 = z; return o; }
inline D3DXMATRIX *D3DXMatrixRotationX(D3DXMATRIX *o, float a) { D3DXMatrixIdentity(o); o->_22 = o->_33 = cosf(a); o->_23 = sinf(a); o->_32 = -sinf(a); return o; }
inline D3DXMATRIX *D3DXMatrixRotationY(D3DXMATRIX *o, float a) { D3DXMatrixIdentity(o); o->_11 = o->_33 = cosf(a); o->_13 = -sinf(a); o->_31 = sinf(a); return o; }
inline D3DXMATRIX *D3DXMatrixRotationZ(D3DXMATRIX *o, float a) { D3DXMatrixIdentity(o); o->_11 = o->_22 = cosf(a); o->_12 = sinf(a); o->_21 = -sinf(a); return o; }
D3DXMATRIX *D3DXMatrixInverse(D3DXMATRIX *out, float *det, const D3DXMATRIX *in);
inline D3DXVECTOR4 *D3DXVec4Transform(D3DXVECTOR4 *o, const D3DXVECTOR4 *v, const D3DXMATRIX *m)
{
	D3DXVECTOR4 r(v->x * m->_11 + v->y * m->_21 + v->z * m->_31 + v->w * m->_41, v->x * m->_12 + v->y * m->_22 + v->z * m->_32 + v->w * m->_42,
	              v->x * m->_13 + v->y * m->_23 + v->z * m->_33 + v->w * m->_43, v->x * m->_14 + v->y * m->_24 + v->z * m->_34 + v->w * m->_44);
	*o = r; return o;
}
inline float D3DXVec4Dot(const D3DXVECTOR4 *a, const D3DXVECTOR4 *b) { return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w; }
inline D3DXVECTOR4 *D3DXVec3Transform(D3DXVECTOR4 *o, const D3DXVECTOR3 *v, const D3DXMATRIX *m)
{ D3DXVECTOR4 t(v->x, v->y, v->z, 1.0f); return D3DXVec4Transform(o, &t, m); }
inline D3DXVECTOR3 *D3DXVec3TransformCoord(D3DXVECTOR3 *o, const D3DXVECTOR3 *v, const D3DXMATRIX *m)
{ D3DXVECTOR4 r; D3DXVec3Transform(&r, v, m); float iw = r.w != 0 ? 1.0f / r.w : 1.0f; o->x = r.x * iw; o->y = r.y * iw; o->z = r.z * iw; return o; }
inline float D3DXVec3Dot(const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) { return a->x * b->x + a->y * b->y + a->z * b->z; }
inline D3DXVECTOR3 *D3DXVec3Cross(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) { D3DXVECTOR3 r(a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x); *o = r; return o; }
inline float D3DXVec3Length(const D3DXVECTOR3 *a) { return sqrtf(a->x * a->x + a->y * a->y + a->z * a->z); }
inline D3DXVECTOR3 *D3DXVec3Normalize(D3DXVECTOR3 *o, const D3DXVECTOR3 *a) { float l = D3DXVec3Length(a); if (l > 0) { o->x = a->x / l; o->y = a->y / l; o->z = a->z / l; } else *o = *a; return o; }

/* ---- shaders / buffers ---- */
struct ID3DXBuffer : public IUnknown
{
	virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
	virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};
typedef ID3DXBuffer *LPD3DXBUFFER;
HRESULT WINAPI D3DXAssembleShader(LPCVOID pSrcData, UINT SrcDataLen, DWORD Flags, LPD3DXBUFFER *ppConstants, LPD3DXBUFFER *ppCompiledShader, LPD3DXBUFFER *ppCompilationErrors);
HRESULT WINAPI D3DXAssembleShaderFromFileA(const char *file, DWORD Flags, LPD3DXBUFFER *ppConstants, LPD3DXBUFFER *ppCompiledShader, LPD3DXBUFFER *ppCompilationErrors);
#define D3DXAssembleShaderFromFile D3DXAssembleShaderFromFileA
UINT WINAPI D3DXGetFVFVertexSize(DWORD FVF);
HRESULT WINAPI D3DXGetErrorStringA(HRESULT hr, char *pBuffer, UINT BufferLen);

/* ---- textures and surfaces ---- */
struct D3DXIMAGE_INFO { UINT Width, Height, Depth, MipLevels; D3DFORMAT Format; D3DRESOURCETYPE ResourceType; DWORD ImageFileFormat; };
HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE8 d, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DTEXTURE8 *out);
HRESULT WINAPI D3DXCreateCubeTexture(LPDIRECT3DDEVICE8 d, UINT size, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DCUBETEXTURE8 *out);
HRESULT WINAPI D3DXCreateVolumeTexture(LPDIRECT3DDEVICE8 d, UINT w, UINT h, UINT depth, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DVOLUMETEXTURE8 *out);
HRESULT WINAPI D3DXFilterTexture(LPDIRECT3DBASETEXTURE8 tex, const PALETTEENTRY *pal, UINT srcLevel, DWORD filter);
HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE8 dst, const PALETTEENTRY *dstPal, const RECT *dstRect, LPDIRECT3DSURFACE8 src, const PALETTEENTRY *srcPal, const RECT *srcRect, DWORD filter, D3DCOLOR colorKey);
HRESULT WINAPI D3DXLoadSurfaceFromFileA(LPDIRECT3DSURFACE8 dst, const PALETTEENTRY *dstPal, const RECT *dstRect, const char *file, const RECT *srcRect, DWORD filter, D3DCOLOR colorKey, D3DXIMAGE_INFO *info);
#define D3DXLoadSurfaceFromFile D3DXLoadSurfaceFromFileA
HRESULT WINAPI D3DXCreateSurfaceFromFileA(LPDIRECT3DDEVICE8 d, LPDIRECT3DSURFACE8 *out, const char *file);
HRESULT WINAPI D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE8 d, const char *file, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey, D3DXIMAGE_INFO *info, PALETTEENTRY *pal, LPDIRECT3DTEXTURE8 *out);
#endif
