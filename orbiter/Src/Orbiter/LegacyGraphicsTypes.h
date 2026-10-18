// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#pragma once

// The mesh and camera algorithms still use these historic names. They are
// data layouts, not a Direct3D backend; the native renderer consumes the same
// mesh data through GraphicsClient.
#ifndef ORBITER_NATIVE_CORE
#include <d3d.h>
#include <ddraw.h>
#else
#include "OrbiterAPI.h"
using D3DVALUE = float;
using D3DCOLOR = DWORD;
inline D3DCOLOR D3DRGBA(float r, float g, float b, float a) {
    return (static_cast<DWORD>(a * 255.0f) << 24) |
           (static_cast<DWORD>(r * 255.0f) << 16) |
           (static_cast<DWORD>(g * 255.0f) << 8) | static_cast<DWORD>(b * 255.0f);
}
struct D3DVECTOR { float x, y, z; };
using D3DVERTEX = NTVERTEX;
struct D3DMATERIAL7 {
    COLOUR4 diffuse, ambient, specular, emissive;
    float power;
};
struct D3DMATRIX {
    union {
        struct { float _11, _12, _13, _14, _21, _22, _23, _24,
                       _31, _32, _33, _34, _41, _42, _43, _44; };
        float m[4][4];
    };
};
struct IDirect3D7;
struct IDirect3DDevice7;
using LPDIRECT3D7 = IDirect3D7 *;
using LPDIRECT3DDEVICE7 = IDirect3DDevice7 *;
using LPDIRECTDRAWSURFACE7 = SURFHANDLE;
constexpr DWORD D3DFVF_XYZ = 0x002;
constexpr DWORD D3DFVF_NORMAL = 0x010;
constexpr DWORD D3DFVF_DIFFUSE = 0x040;
constexpr DWORD D3DFVF_TEX1 = 0x100;
constexpr DWORD D3DFVF_VERTEX = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
#define D3DFVF_TEXCOORDSIZE2(index) 0
#define D3DVAL(value) static_cast<float>(value)
#endif
