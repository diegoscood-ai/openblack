/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectGhosts.h"

#include <algorithm>
#include <utility>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>

#include "3D/FrameAnim.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "Graphics/RenderModes.h"
#include "Locator.h"

namespace openblack::ecs::object_ghosts
{
namespace
{
/// The second pass's forced ALPHAREF: the model's own materials, cut off at 10
constexpr uint8_t k_SecondPassAlphaRef = 10;

struct Ghost
{
	entt::id_type meshId;
	glm::mat4 model;
	float remainingMs;
};

/// What this module keeps between calls (Locator::worldEffects)
struct ObjectGhostsState
{
	std::vector<Ghost> ghosts {};
	/// The object a KeepDrawnPose holds, and the matrix it was drawn with then
	std::optional<std::pair<entt::entity, glm::mat4>> kept {};
};

ObjectGhostsState& ObjectGhostsData()
{
	return openblack::Locator::worldEffects::value().Get<ObjectGhostsState>();
}
} // namespace

void Add(entt::entity object)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::worldEffects::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto* mesh = registry.TryGet<const components::Mesh>(object);
	const auto* transform = registry.TryGet<const components::Transform>(object);
	if (mesh == nullptr || transform == nullptr)
	{
		return;
	}
	// the matrix it is drawn with now, as the original copies its model's: in the hand, in flight between two physics
	// turns, or on the ground
	auto& data = ObjectGhostsData();
	const auto model =
	    data.kept.has_value() && data.kept->first == object ? data.kept->second : DrawnBodyModel(registry, object);
	data.ghosts.push_back({mesh->id, model, graphics::frame_anim::k_GhostMs});
}

KeepDrawnPose::KeepDrawnPose(entt::entity object)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::worldEffects::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(object) && registry.AllOf<components::Transform>(object))
	{
		ObjectGhostsData().kept = std::pair(object, DrawnBodyModel(registry, object));
	}
}

KeepDrawnPose::~KeepDrawnPose()
{
	if (Locator::worldEffects::has_value())
	{
		ObjectGhostsData().kept.reset();
	}
}

void Update(float frameMs)
{
	if (!Locator::worldEffects::has_value())
	{
		return;
	}
	auto& ghosts = ObjectGhostsData().ghosts;
	for (auto& ghost : ghosts)
	{
		const auto step = graphics::frame_anim::GhostStep(ghost.remainingMs, frameMs);
		ghost.remainingMs = step.expired ? -1.0f : step.remainingMs;
	}
	std::erase_if(ghosts, [](const Ghost& ghost) { return ghost.remainingMs < 0.0f; });
}

size_t Count()
{
	return Locator::worldEffects::has_value() ? ObjectGhostsData().ghosts.size() : 0;
}

std::array<Pass, 2> Passes(float t) noexcept
{
	const auto frame = graphics::frame_anim::GoolooFrame(t);
	return {{{.uv = frame.uv,
	          .mode = graphics::render_modes::Mode::ChromaDepthOnly,
	          .mask = true,
	          .alphaRef = frame.materialByte,
	          .depthEqual = false},
	         {.uv = {0.0f, 0.0f}, .mode = std::nullopt, .mask = false, .alphaRef = k_SecondPassAlphaRef, .depthEqual = true}}};
}

entt::id_type MaskTexture() noexcept
{
	return entt::hashed_string("raw/cool_effect").value();
}

std::vector<psys::mesh_atoms::Instance> Instances()
{
	std::vector<psys::mesh_atoms::Instance> result;
	if (!Locator::worldEffects::has_value())
	{
		return result;
	}
	for (const auto& ghost : ObjectGhostsData().ghosts)
	{
		for (const auto& pass : Passes(ghost.remainingMs))
		{
			result.push_back({
			    .meshId = ghost.meshId,
			    .model = ghost.model,
			    .alpha = 1.0f,
			    .uv = {pass.uv.x, pass.uv.y},
			    .translucent = false,
			    .additive = false,
			    .colour = {0xFF, 0xFF, 0xFF},
			    .landscapeColour = true,
			    .mode = pass.mode,
			    .forcedAlphaRef = pass.alphaRef,
			    .depthEqual = pass.depthEqual,
			    .texture = pass.mask ? std::optional(MaskTexture()) : std::nullopt,
			});
		}
	}
	return result;
}

void Clear()
{
	if (Locator::worldEffects::has_value())
	{
		ObjectGhostsData().ghosts.clear();
	}
}
} // namespace openblack::ecs::object_ghosts
