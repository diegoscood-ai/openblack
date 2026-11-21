/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// How the miracles look outside their effects, as the renderer draws them: the one-shot globes' draws, each queued in
// the frame's single queue keyed where it is sorted, and the glow over a hand holding a miracle, inside the hand's own Z
// object. The list of globe draws stays empty, the bubbles, the seeds inside them and the bands being entities queued
// with the other models.

#include <cstdint>
#include <cstring>

#include <array>
#include <exception>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Globes.h"
#include "Graphics/HandGlow.h"
#include "Graphics/SeaPass.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ZObject.h"
#include "Graphics/ZSort.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Particles/Rules/SurfRevol.h"
#include "Renderer.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/RenderModes.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The flow's sheet, S_Hand_Flow.raw with the alpha of S_Hand_Flowa.raw
constexpr entt::id_type k_HandFlow = entt::hashed_string("raw/S_Hand_Flow").value();
} // namespace

void Renderer::DrawGlobes(const DrawSceneDesc& desc, zsort::Queue<ZObject>& queue) const
{
	// the world view only: the reflection draws no globe of this list
	if (desc.viewId != RenderPass::Main || desc.camera == nullptr)
	{
		return;
	}
	globes::Submit(std::span<const globes::GlobeDraw>(_frameGlobes), queue, desc.camera->GetOrigin(),
	               [](int i) { return ZObject {.globe = i}; });
}

bool Renderer::LoadHandFlow() const
{
	auto& textures = Locator::resources::value().GetTextures();
	if (textures.Contains(k_HandFlow))
	{
		return true;
	}
	// the game makes the flow one texture of its colour file and its alpha file, cut to four bits a channel; its material
	// has no wrapping bit, so it is clamped
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto directory = fileSystem.GetPath<filesystem::Path::Textures>();
		textures.Load(k_HandFlow, resources::Texture2DLoader::FromColourAlphaTag {}, "raw/S_Hand_Flow",
		              resources::Texture2DLoader::ColourAlphaDesc {
		                  .colour = fileSystem.FindPath(directory / "S_Hand_Flow.raw"),
		                  .alpha = directory / "S_Hand_Flowa.raw",
		                  .packing = resources::Texture2DLoader::ColourAlpha::Argb4444,
		              });
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "The hand's glow: cannot load Data/Textures/S_Hand_Flow.raw: {}", e.what());
		return false;
	}
	return textures.Contains(k_HandFlow);
}

void Renderer::NoteHandGlow(std::string_view outcome, float alpha) const
{
	if (_handGlowOutcome == outcome)
	{
		return;
	}
	_handGlowOutcome = outcome;
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "The hand's glow: {} (strength {})", outcome, alpha);
}

void Renderer::DrawHandGlow(const L3DMesh& mesh, L3DMeshSubmitDesc& desc, const ecs::systems::RenderContext& context,
                            uint32_t row) const
{
	if (!Locator::config::value().drawHandGlow)
	{
		NoteHandGlow("switched off", 0.0f);
		return;
	}
	if (!Locator::miracleFxSystem::has_value())
	{
		NoteHandGlow("no miracle effects service", 0.0f);
		return;
	}
	if (row >= context.instanceUniforms.size() || row >= context.instanceColours.size())
	{
		NoteHandGlow("no instance row", 0.0f);
		return;
	}
	const auto strength = Locator::miracleFxSystem::value().GetHandGlow();
	const auto glow = HandGlowOf(strength, particles::surf_revol::PlayerColour(static_cast<int>(creature::LocalPlayer())));
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr auto k_Stride = static_cast<uint16_t>(sizeof(std::array<glm::vec4, 5>));
	if (!glow.has_value())
	{
		NoteHandGlow("nothing in the hand", strength.alpha);
		return;
	}
	if (!textures.Contains(k_HandFlow))
	{
		NoteHandGlow("no flow texture", strength.alpha);
		return;
	}
	if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
	{
		NoteHandGlow("no instance buffer", strength.alpha);
		return;
	}
	NoteHandGlow("drawn", strength.alpha);
	// The hand's own object drawn again: the same pose, shape and program, from a row of its own in the player's colour
	// at the glow's strength
	const auto glowRow = HandGlowRow(context.instanceUniforms[row], context.instanceColours[row], *glow);
	bgfx::InstanceDataBuffer instance {};
	bgfx::allocInstanceDataBuffer(&instance, 1, k_Stride);
	std::memcpy(instance.data, glowRow.data(), sizeof(glowRow));

	const auto* transientInstance = desc.transientInstance;
	const auto* textureOverride = desc.textureOverride;
	const auto mode = desc.mode;
	const auto table = desc.table;
	const auto globalAlpha = desc.globalAlpha;
	const auto forcedAlphaRef = desc.forcedAlphaRef;
	const auto blendFilter = desc.blendFilter;
	const auto options = desc.options;
	// every primitive in the flow's material, whatever its own: mode 13 (added by its alpha, no depth written), the flow's
	// sheet in place of its skin, one-sided (CCW), through the normal table
	desc.transientInstance = &instance;
	desc.textureOverride = textures.Handle(k_HandFlow).operator->();
	desc.mode = render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite;
	desc.table = render_modes::Table::Normal;
	desc.globalAlpha = std::numeric_limits<uint8_t>::max();
	desc.forcedAlphaRef = std::nullopt;
	desc.blendFilter = 0;
	desc.options.cull = sea_pass::ForPass(desc.viewId).FaceCull(sea_pass::Surface::Model, false, desc.sea.unmirror);
	// the frame's LESSEQUAL itself: the redraw lies at exactly the hand's depth, which the strict test of every other
	// model would drop
	desc.options.zFunc = render_modes::ZFunc::LessEqualInclusive;
	desc.handGlowCell = glow->cell;
	DrawMesh(mesh, desc, std::numeric_limits<uint8_t>::max());
	desc.handGlowCell = std::nullopt;
	desc.options = options;
	desc.blendFilter = blendFilter;
	desc.forcedAlphaRef = forcedAlphaRef;
	desc.globalAlpha = globalAlpha;
	desc.table = table;
	desc.mode = mode;
	desc.textureOverride = textureOverride;
	desc.transientInstance = transientInstance;
}
