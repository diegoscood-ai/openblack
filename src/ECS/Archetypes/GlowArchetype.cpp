/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GlowArchetype.h"

#include <entt/core/hashed_string.hpp>
#include <glm/ext/matrix_float3x3.hpp>

#include "3D/FrameAnim.h"
#include "3D/Light.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
constexpr int k_GlowCell = 22;
} // namespace

std::array<entt::entity, 2> GlowArchetype::Create(const LightEmitter& emitter, components::TempleRoom room)
{
	auto texture = Locator::resources::value().GetTextures().Handle(entt::hashed_string("raw/ATMOS"));
	auto& registry = Locator::entitiesRegistry::value();
	const auto extent = glm::vec2 {1.0f / 8.0f, 1.0f / 8.0f};
	// (openblack) cell 22 of ATMOS (column 6, row 2), openblack's own choice: no original address
	const auto cell = graphics::frame_anim::SpriteCellUv(k_GlowCell)[0];

	// Softer glow that uses a larger sprite
	auto glowEntity = registry.Create();
	{
		registry.Assign<ecs::components::TempleInteriorPart>(glowEntity, room);
		registry.Assign<Sprite>(glowEntity, texture->GetNativeHandle(), cell, extent, emitter.glow.backgroundColour);
		registry.Assign<ecs::components::Transform>(glowEntity, emitter.glow.position, glm::mat3(1.0f),
		                                            glm::vec3(emitter.glow.backgroundScale));
	}
	// A small bright shine at the center
	auto shineEntity = registry.Create();
	{
		registry.Assign<ecs::components::TempleInteriorPart>(shineEntity, room);
		registry.Assign<Sprite>(shineEntity, texture->GetNativeHandle(), cell, extent, emitter.glow.brightSpotColour);
		registry.Assign<ecs::components::Transform>(shineEntity, emitter.glow.position, glm::mat3(1.0f),
		                                            glm::vec3(emitter.glow.brightSpotScale));
	}
	return {glowEntity, shineEntity};
}
