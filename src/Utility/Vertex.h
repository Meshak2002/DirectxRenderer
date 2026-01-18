//***************************************************************************************
// Vertex.h
//
// Common vertex and geometry structures used throughout the renderer
//***************************************************************************************

#pragma once

#include <cstdint>
#include <DirectXMath.h>

// Common vertex structure used by both GeometryGenerator and ModelImporter
struct Vertex
{
	Vertex() {}
	Vertex(
		const DirectX::XMFLOAT3& p,
		const DirectX::XMFLOAT2& uv,
		const DirectX::XMFLOAT3& n,
		const DirectX::XMFLOAT3& t) :
		Position(p),
		TexCoord(uv),
		Normal(n),
		Tangent(t) {}
	Vertex(
		float px, float py, float pz,
		float u, float v,
		float nx, float ny, float nz,
		float tx, float ty, float tz) :
		Position(px, py, pz),
		TexCoord(u, v),
		Normal(nx, ny, nz),
		Tangent(tx, ty, tz) {}

	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 TexCoord;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
};

// Index type aliases
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
