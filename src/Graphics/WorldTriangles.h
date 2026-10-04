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

#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/RenderModes.h"
#include "Graphics/RenderPass.h"

namespace bgfx
{
struct VertexLayout;
}

// Triangles already in the world, already lit on the CPU, drawn at once in an L3D primitive's material: what
// LH3DTech::Draw3DWorldTriangle 0x81C090 does for RenderParticleGJMesh::DrawAt 0x67C150 (the exploded pieces of
// PSys/Rules/ExplodeObject.h). Draw3DWorldTriangle does a software T&L into LH3DP3::Table1: its last argument 0 takes
// g_world_to_clipping as it is (0x81C09F..0x81C128; DrawAt has put the identity world matrix, 0x67C8ED..0x67C979), no
// specular (0x81C2BF), the culling of the material (g_NoBackfaceCull = material +5 bit 0, 0x81C30C..0x81C319; CULLMODE
// ((~m[5]) & 1) * 2 + 1, 0x81C556..0x81C58F), SetMaterial through the current mode table (0x81C48E..0x81C4A0) and ONE
// LH3DRender::DrawTriangle 0x82F810 for the whole primitive (0x81C5B1), at once: no Z object.
// Its indexed sibling fn_0081C780 (FragMesh fn_007F7ED0 0x7F86D8 / 0x7F8730, RenderParticleGJMesh::DrawAt 0x67CAEE)
// is the same T&L with g_world_to_clipping (0x81C783..0x81C80A), the same culling (0x81CA0A..0x81CA1C, 0x81CC57..
// 0x81CC90; without the two-sided bit a backface test on the screen points, 0x81CBC7..0x81CC11, which keeps what that
// CULLMODE keeps) and the same SetMaterial (0x81CB79..0x81CBA1), but it also copies each vertex's specular (the 8-byte
// colour + specular pairs, 0x81C9B9..0x81C9D0): Vertex::specular.
//
// A frame's batches go up in one transient vertex buffer (no bgfx handle per piece: the 4096 handles were the cause
// of the old crash, tmp_dis\miracles\polish\PENDIENTE.md) and are submitted in the order they were appended. The
// program is "WorldTriangles" = vs_world_triangles + fs_object, so the alpha test and stage 0 alpha are the models'.
// Plan: tmp_dis\miracles\polish\pieces_shadows_PLAN.md §1.3 (b).
namespace openblack::graphics
{
class L3DMesh;
class ShaderManager;
class Texture2D;
} // namespace openblack::graphics

namespace openblack::graphics::world_triangles
{

/// One vertex of Draw3DWorldTriangle 0x81C090: in the world, with its colour already lit (D3DCOLOR 0xAARRGGBB turned to
/// bgfx's Color0 ABGR), no specular (0x81C2BF); fn_0081C780's has one (`specular`, Color1 ABGR, 0 for the pieces)
struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	uint32_t abgr;
	uint32_t specular {0}; ///< ABGR, added after the texture (fs_object), its alpha unused
};
/// Position 3 floats, TexCoord0 2 floats, Color0 and Color1 4 normalised bytes each
[[nodiscard]] const bgfx::VertexLayout& Layout();

/// 0xAARRGGBB (LH3DColor) to the ABGR bytes bgfx reads for Color0
[[nodiscard]] constexpr uint32_t ToAbgr(uint32_t argb)
{
	return (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
}

/// The material a primitive of triangles is drawn with: an L3D mesh's primitive (the GJMesh +0 of a piece is its
/// source primitive, 0x680C49): its texture (skin), mode (+0), +5 bits (two-sided, tiling) and ALPHAREF (+4)
struct MaterialRef
{
	entt::id_type meshId {0};
	uint16_t subMesh {0};
	uint16_t primitive {0};

	[[nodiscard]] bool operator==(const MaterialRef&) const = default;
};

/// A run of triangles (a list without indices: three vertices each) in one material and mode table
struct Batch
{
	MaterialRef material;
	render_modes::Table table {render_modes::Table::Normal}; ///< g_set_render_mode_data [0xECA618] while it is drawn
	/// The draw's alpha byte, for render_modes::PrimitiveAlpha (ALPHAREF of modes 9 / 15 with the table 0xC387C8)
	uint8_t globalAlpha {255};
	uint32_t firstVertex {0};
	uint32_t vertexCount {0};
	/// Who appended it (a Queued / Immediate atom drawn on its own: Submit's `only`), nullptr for the Sorted ones
	const void* tag {nullptr};
};

/// One frame's triangles, refilled where they are drawn
struct Frame
{
	std::vector<Vertex> vertices;
	std::vector<Batch> batches;

	void Clear();
	/// The vertices of one primitive (three per triangle) at the end; joined to the last batch when its material,
	/// table, alpha and tag are the same (the pieces of one source primitive come one after the other from ExplodeMesh,
	/// so the order of the draws is kept with few calls)
	void Append(const MaterialRef& material, render_modes::Table table, uint8_t globalAlpha, std::span<const Vertex> v,
	            const void* tag = nullptr);
};

/// Uploads the vertices of the batches drawn in ONE transient vertex buffer and submits one draw per batch, in order,
/// to `view`: the primitive's texture (s_diffuse, clamped without its tiling bit as Renderer::DrawSubMesh does), its
/// alpha test and stage 0 alpha (u_skyAlphaThreshold = render_modes::PrimitiveAlpha(Select(mode, table), table,
/// ALPHAREF, globalAlpha)), the material colour of an untextured primitive (u_materialColour), and the state
/// render_modes::PrimitiveState of the selected mode with the culling of the material (+5 bit 0, 0x81C556..0x81C58F).
/// `only`: just the batches with that tag (nullptr: all). (openblack guard) when the transient buffer cannot take them
/// all, the first ones that fit are drawn and a warning is given once. Returns the batches drawn
uint32_t Submit(RenderPass view, const Frame& frame, const ShaderManager& shaders, const void* only = nullptr);

/// Draw3DWorldTriangle 0x81C090 in a material made by LH3DRender::CreateMaterial 0x82FD30 on a .raw texture (the
/// influence border's [0xEB9A18], the ripples' smoke [0xEA1ABC]) instead of an L3D primitive's: indexed triangles
/// (three indices each) drawn at once with the program "WorldQuad" (vs_blob + fs_world_quad: colour = texture x
/// diffuse, alpha = the alpha file x diffuse alpha, the stage 0 of mode 6; no fog and no specular, 0x81C2BF), `diffuse`
/// and `alpha` the X.raw / Xa.raw textures with their own samplers (the .raw loader's Repeat, which the tiling bit +5
/// bit 2 of the materials drawn this way asks for), and render_modes::State of the material (its mode, the culling of
/// +5 bit 0). Only for the modes whose stage 0 is that one (mode 6; mode 13, the intro light's SRCALPHA / ONE, has the
/// same stage 0). `options`: the draw's own render states (the ZFUNC of the intro light's 0x828483 / 0x828673; the
/// default is the plain SetMaterial). (openblack guard) nothing is drawn when the transient buffers cannot take it all.
/// Returns true when it was drawn
bool SubmitRaw(RenderPass view, std::span<const Vertex> vertices, std::span<const uint16_t> indices, const Texture2D& diffuse,
               const Texture2D& alpha, const render_modes::Material& material, const ShaderManager& shaders,
               const render_modes::StateOptions& options = {});

/// The texture of an L3D primitive: the mesh's skins (or its SetSkinSource's), else the texture manager; nullptr for
/// an untextured one. The models' lookup (Renderer::DrawSubMesh and the shadow casters use it too)
[[nodiscard]] const Texture2D* PrimitiveTexture(const L3DMesh& mesh, uint32_t skinId);

} // namespace openblack::graphics::world_triangles
