/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// What the landscape vortices draw on and under the land (docs/bw1-notes/vortex.md, "Drawing").
//
// The depth box: every vortex has a box of four walls with no top or bottom (MeshId::SpellZCheatBox), standing at its
// x and z with its top at height 0 and scaled by its base scale. Its material is the depth-only one (mode 18): it
// writes depth and no colour. It is drawn every frame in the main view, after the sea and before the land, whatever
// the vortex's state. The vortex's own mesh (MeshId::SpellVortexCylinder) is never drawn.
//
// The decal: while its fade is not 0 (ecs::vortex::NextDecal), a vortex opens a hole in the one land block holding its
// centre and lays a ring over it, both centred on the vortex and 160 x its base scale wide, cut at the block's edge.
// In the main view the block is drawn in three passes: its depth only where the mask's alpha reaches the hole's
// reference (mode 18), the block itself with an equal depth test, so it shows only there, then the ring in mode 6
// with the same test, in the land's light. Where the mask stays below the reference the land is not drawn and what
// was drawn before it shows: the sky, the sea, the depth box, the ground effect. The ring does not fade. The textures
// are clamped.
//
// The effects: a vortex's ground effect is drawn right after the depth boxes and before the land, so its funnels
// show through the hole. Its surfaces of revolution are drawn there, at once; its sprites, if any, still go to the
// Z-sorter as a sorted effect's do. While a vortex does not show (fade 0) neither its ground effect nor its effect
// over the land is drawn: they keep what their last step left, unstepped.

#include <cstdint>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VortexDraw.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/Rules/SurfRevol.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The decal's texture stages, past every stage the land's program reads, so the land's bindings stay as they are
constexpr uint8_t k_DecalStage = 13;
constexpr uint8_t k_DecalAlphaStage = 14;
/// The vertex streams, the state and the transform go after each draw; the land's bindings stay
constexpr auto k_Discard = BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_TRANSFORM |
                           BGFX_DISCARD_VERTEX_STREAMS | BGFX_DISCARD_STATE;

/// A material without its tiling bit: the texture clamped
uint32_t Clamped(const Texture2D& texture)
{
	return (texture.GetSamplerFlags() & ~(BGFX_SAMPLER_U_MASK | BGFX_SAMPLER_V_MASK)) | BGFX_SAMPLER_U_CLAMP |
	       BGFX_SAMPLER_V_CLAMP;
}

/// x: the scale of the land's texture coordinates, yz: the offset, w: the hole's alpha reference (a byte)
glm::vec4 DecalUniform(const vortex_draw::DecalUv& uv, uint8_t alphaRef)
{
	return {uv.scale, uv.offset, static_cast<float>(alphaRef)};
}
} // namespace

void Renderer::DrawVortexBoxes(RenderPass viewId) const
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = resources::HashIdentifier(MeshId::SpellZCheatBox);
	if (!meshes.Contains(meshId))
	{
		return;
	}
	const auto mesh = meshes.Handle(meshId);
	if (mesh->GetSubMeshes().empty() || mesh->GetSubMeshes().front()->GetPrimitives().empty())
	{
		return;
	}
	// The box's one primitive is drawn in the depth-only mode, whose alpha reference (the material's, 0) lets every
	// pixel through; the material's own culling, flipped as every model's is in this view
	const bool twoSided = mesh->GetSubMeshes().front()->GetPrimitives().front().twoSided;
	const auto cull = sea_pass::ForPass(viewId).FaceCull(sea_pass::Surface::Model, twoSided, false);
	const uint64_t state = render_modes::State(render_modes::Mode::ChromaDepthOnly, {.cull = cull, .msaa = true});
	const auto& info = Locator::infoConstants::value().vortex;
	// the program of plain textured L3D geometry placed by one model matrix; no colour reaches the target
	const glm::vec4 white {1.0f};
	// every vortex has its box, made with it, whether it is available or not
	Locator::entitiesRegistry::value().Each<const ecs::components::LandscapeVortex>(
	    [&](const ecs::components::LandscapeVortex& vortex) {
		    const auto row = static_cast<size_t>(vortex.info);
		    if (row >= info.size())
		    {
			    return;
		    }
		    const auto model = vortex_draw::ZBoxModel(vortex.position, info.at(row).baseScale);
		    DrawCelestialMesh(viewId, *mesh, model, RevolvedSurfaceWhiteTexture(), white, state);
	    });
}

void Renderer::SplitVortexParticles() const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// a vortex shows while its fade is not 0, which is exactly while its decal is made (this frame's)
	std::vector<vortex_draw::VortexEffects> vortices;
	Locator::entitiesRegistry::value().Each<const ecs::components::LandscapeVortex>(
	    [&vortices](const ecs::components::LandscapeVortex& vortex) {
		    vortices.push_back(
		        {.preLandscape = vortex.preLandscape, .postLandscape = vortex.postLandscape, .shows = vortex.decal.made});
	    });
	if (vortices.empty())
	{
		return;
	}
	auto& frame = _particleFrame;
	const auto role = [&vortices](uint32_t effect) { return vortex_draw::RoleOf(effect, vortices); };
	// A shown ground effect's surfaces, drawn at once as a sorted effect's, move before the land; a hidden effect's
	// are dropped. Both leave the at-once list
	std::unordered_set<const psys::Atom*> leaving;
	std::vector<psys::surf_revol::Surface> kept;
	kept.reserve(frame.surfaces.size());
	for (auto& surface : frame.surfaces)
	{
		const auto drawn = role(surface.effect);
		if (drawn == vortex_draw::EffectRole::Hidden ||
		    (drawn == vortex_draw::EffectRole::Ground && surface.path == psys::DrawPath::Sorted))
		{
			leaving.insert(surface.atom);
			if (drawn == vortex_draw::EffectRole::Ground)
			{
				frame.groundSurfaces.push_back(std::move(surface));
			}
			continue;
		}
		kept.push_back(std::move(surface));
	}
	frame.surfaces = std::move(kept);
	std::erase_if(frame.sorted.atOnce, [&leaving](const psys::Atom* atom) { return leaving.contains(atom); });
	// and a hidden effect's sprites and chains leave the Z-sorter. (approximate) its mesh atoms and mists, drawn from
	// other lists, are not hidden: the vortices' effects have none
	const auto hidden = [&role](const auto& item) { return role(item.effect) == vortex_draw::EffectRole::Hidden; };
	std::erase_if(frame.sorted.sprites, hidden);
	std::erase_if(frame.sorted.chains, hidden);
}

void Renderer::DrawVortexGroundEffects(RenderPass viewId) const
{
	// in the effects' walk order, as the at-once surfaces are
	for (const auto& surface : _particleFrame.groundSurfaces)
	{
		DrawParticleSurface(viewId, surface);
	}
}

std::vector<Renderer::VortexDecalDraw> Renderer::CollectVortexDecals() const
{
	std::vector<VortexDecalDraw> decals;
	if (!Locator::infoConstants::has_value())
	{
		return decals;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto texture = [&textures](std::string_view name) -> const Texture2D* {
		const auto id = entt::hashed_string(("raw/" + std::string(name)).c_str()).value();
		return textures.Contains(id) ? &*textures.Handle(id) : nullptr;
	};
	const auto& info = Locator::infoConstants::value().vortex;
	Locator::entitiesRegistry::value().Each<const ecs::components::LandscapeVortex>(
	    [&](const ecs::components::LandscapeVortex& vortex) {
		    const auto row = static_cast<size_t>(vortex.info);
		    const auto block = vortex_draw::DecalBlock(vortex.position);
		    if (!vortex.decal.made || row >= info.size() || !block.has_value())
		    {
			    return;
		    }
		    // (approximate) one decal per block: the original keeps the last one made there
		    if (std::ranges::any_of(decals, [&block](const VortexDecalDraw& d) { return d.block == *block; }))
		    {
			    return;
		    }
		    const auto names = vortex_draw::DecalTexturesFor(vortex.type);
		    const auto* mask = texture(std::string(names.mask) + "a");
		    const auto* ring = texture(names.ring);
		    const auto* ringAlpha = texture(std::string(names.ring) + "a");
		    if (mask == nullptr || ring == nullptr || ringAlpha == nullptr)
		    {
			    return;
		    }
		    decals.push_back({.block = *block,
		                      .uv = vortex_draw::DecalUvTransform(vortex.position, *block, info.at(row).baseScale),
		                      .alphaRef = vortex.decal.alphaRef,
		                      .mask = mask,
		                      .ring = ring,
		                      .ringAlpha = ringAlpha});
	    });
	return decals;
}

void Renderer::DrawVortexDecalMask(RenderPass viewId, const VortexDecalDraw& decal, const LandBlock& block, uint64_t cull) const
{
	const auto* program = _shaderManager->GetShader("VortexDecalMask");
	if (program == nullptr)
	{
		return;
	}
	// the land's own vertex program and uniforms (set for this block just before), so the depth is the block's to the
	// bit and its own draw's equal test passes where this one wrote
	const auto u_vortexDecal = DecalUniform(decal.uv, decal.alphaRef);
	program->SetUniformValue("u_vortexDecal", &u_vortexDecal);
	program->SetTextureSampler("s13_vortexDecal", k_DecalStage, *decal.mask, Clamped(*decal.mask));
	block.GetMesh().GetVertexBuffer().Bind();
	bgfx::setState(render_modes::State(render_modes::Mode::ChromaDepthOnly, {.msaa = true}) | cull, 0);
	program->Submit(static_cast<bgfx::ViewId>(viewId), 0, k_Discard);
}

void Renderer::DrawVortexDecalRing(RenderPass viewId, const VortexDecalDraw& decal, const LandBlock& block, uint64_t cull) const
{
	const auto* program = _shaderManager->GetShader("VortexDecalRing");
	if (program == nullptr)
	{
		return;
	}
	// The ring's vertices take the cells' light and colour with no haze. The next block sets its own class again
	const glm::vec4 u_hazeBlock {0.0f};
	program->SetUniformValue("u_hazeBlock", &u_hazeBlock);
	const auto u_vortexDecal = DecalUniform(decal.uv, decal.alphaRef);
	program->SetUniformValue("u_vortexDecal", &u_vortexDecal);
	program->SetTextureSampler("s13_vortexDecal", k_DecalStage, *decal.ring, Clamped(*decal.ring));
	program->SetTextureSampler("s14_vortexDecalAlpha", k_DecalAlphaStage, *decal.ringAlpha, Clamped(*decal.ringAlpha));
	block.GetMesh().GetVertexBuffer().Bind();
	// mode 6 (SRCALPHA / INVSRCALPHA, no Z write) under ZFUNC EQUAL; the target's alpha is not written
	const auto state = render_modes::State(render_modes::Mode::AlphaTexturedAlphaNoZWrite,
	                                       {.zFunc = render_modes::ZFunc::Equal, .msaa = true});
	bgfx::setState(state | cull, 0);
	program->Submit(static_cast<bgfx::ViewId>(viewId), 0, k_Discard);
}
