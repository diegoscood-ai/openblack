/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "PSys/PSys.h"

// ZR_SurfRevol (PSysGeom.cpp; ModifyAtomCollection 0x686370, DefineProperties 0x6B2E80): a create rule that makes one
// atom per collection carrying a textured surface of revolution (RenderParticleGJMeshRotatingUV, ctor 0x6C8B60): the
// teleport's vortex pool (SF_TeleportVortex, S_TileLandscape.raw) and the spell dispensers' discs. The renderer draws it
// with Graphics/RendererSurfRevol.cpp. Wiki: docs/bw1-notes/miracles.md, "Teletransporte" (ZR_SurfRevol).

namespace openblack::psys
{

/// The profile objects of FunctionIndex (jump table 0x6868CC): Eval(t) -> (radius, height) for t in 0..1
enum class SurfProfile : int
{
	Disk = 0,         ///< TestDisk 0x685860: r = t, y = 0 (also any other index)
	Funnel = 1,       ///< TestFunnel 0x6868E0: r = t, y = 3 (sqrt t - 1)
	FunnelSpout = 2,  ///< TestFunnelSpout 0x686940: r = 1.5 t, y = 3 (sqrt 2t - 1)
	FunnelParab = 3,  ///< TestFunnelParab 0x686910: r = t, y = 3 (t^2 - 1)
};

/// The mesh the rule builds (RenderParticleGJMesh's GJ mesh): NumU x NumV vertices, row by row
struct SurfMesh
{
	int numU {0};
	int numV {0};
	std::vector<glm::vec3> positions; ///< local, radius 1 at t = 1
	std::vector<glm::vec2> uvs;
	std::vector<uint32_t> colours;    ///< ARGB
	std::vector<uint32_t> speculars;  ///< ARGB
	std::vector<uint16_t> indices;    ///< triangles
};

namespace surf_revol
{
/// Eval of the profile object (radius, height)
[[nodiscard]] glm::vec2 Profile(int functionIndex, float t);

/// fn_006858F0: vertex (i, j) at u = i / (NumU - 1), t = j / (NumV - 1): (r(t) cos 2 pi u, y(t), r(t) sin 2 pi u), uv (u, t).
/// With FadeAlphas: t < fadeIn -> RGB 255 t / fadeIn (alpha 255); t > 1 - fadeOut -> alpha 255 (1 - (t - (1 - fadeOut)) /
/// fadeOut) (RGB 255); the specular is the player colour x (1 - the RGB factor) with ChangeSpecColor. Without FadeAlphas
/// the colour is white and the specular 0. Triangles per row j and column i < NumU - 1: (b+U+i, b+i, b+U+1+i) and
/// (b+U+1+i, b+i, b+i+1), b = j U.
[[nodiscard]] SurfMesh Build(int numU, int numV, int functionIndex, bool fadeAlphas, float fadeIn, float fadeOut,
                             bool changeSpecColour, uint32_t playerColour);
/// fn_00685F00: the UVs x (TextureWidth / 256, TextureHeight / 256)
void ScaleUVs(SurfMesh& mesh, float u, float v);
/// fn_00685F40: u += (1 - t_j)^2 x MaxUVChange x amount (from the saved UVs)
void TwistUVs(SurfMesh& mesh, const std::vector<glm::vec2>& original, float maxUVChange, float amount);
/// fn_00685FC0: each row turned about Y by t_j x MaxVertexChange x amount (from the saved positions)
void TwistVertices(SurfMesh& mesh, const std::vector<glm::vec3>& original, float maxVertexChange, float amount);
/// GPlayer::GetPlayer3DColor 0x64B590 (GetPlayerColour 0x64D800: 0xBFF0B8[GetRemapedPlayer], alpha 0xFF); the neutral
/// player's entry is 0xFF000000
[[nodiscard]] uint32_t PlayerColour(int player);

/// One surface to draw this frame, in world space (draped over the land when the rule asks for it)
struct Surface
{
	std::string texture;      ///< the .raw base name ("S_TileLandscape")
	bool additive {false};    ///< UseAdditiveAlpha
	bool writeDepth {false};  ///< MaterialUpdateZBuffer
	bool doubleSided {false}; ///< MaterialSetDoubleSided
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t abgr;     ///< the diffuse (vertex colour x the atom's colour and alpha)
		uint32_t specular; ///< ABGR, alpha = the diffuse alpha
	};
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
	/// The effect's origin: the key of a Queued effect's single Z object (PSysManager::AddDrawing 0x6797D0 -> fn_00679860
	/// -> fn_006798B0 -> fn_00679920 -> vt+0xFC DrawAt 0x67CBA0), which the surface is drawn inside
	glm::vec3 origin {0.0f};
	/// Its effect's draw path. DrawAt 0x67CBA0 never reads [0xC0215D] (RenderParticleGJMesh::DrawAt 0x67C150 ->
	/// Draw3DWorldTriangle 0x81C090, 0x67C9F2): Sorted, drawn at once when its effect is drawn, unsorted; Queued /
	/// Immediate, at its place in its effect's items (manager::OrderedEffect, matched by `atom`)
	DrawPath path {DrawPath::Sorted};
	uint32_t effect {0};        ///< the effect's id
	const Atom* atom {nullptr}; ///< the ZR_SurfRevol atom
};
/// Every ZR_SurfRevol atom of the running effects
[[nodiscard]] std::vector<Surface> Collect();
} // namespace surf_revol

} // namespace openblack::psys
