/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashMarkerArchetype.h"

#include <entt/core/hashed_string.hpp>
#include <glm/vec4.hpp>

#include "3D/CreatureBody.h"
#include "3D/FrameAnim.h"
#include "Creature/LeashMarker.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The sheet of markers and footprints, and their shapes in the alpha beside it
constexpr auto k_Texture = entt::hashed_string("raw/misc0");
constexpr auto k_Alpha = entt::hashed_string("raw/misc0a");
/// Its pictures are eight to a row
constexpr float k_CellExtent = 1.0f / 8.0f;
/// Both are drawn in the yellow of the camera bookmarks' markers
constexpr glm::vec4 k_Yellow {1.0f, 1.0f, 0.0f, 1.0f};

Sprite CellSprite(uint8_t cell, float angle)
{
	return {
	    .texture = k_Texture.value(),
	    .uvMin = graphics::frame_anim::SpriteCellUv(cell)[0],
	    .uvExtent = glm::vec2 {k_CellExtent, k_CellExtent},
	    .tint = k_Yellow,
	    .additive = false,
	    .alpha = k_Alpha.value(),
	    .angle = angle,
	};
}
} // namespace

Sprite LeashMarkerArchetype::RingSprite()
{
	return CellSprite(leash_marker::k_RingCell, 0.0f);
}

Sprite LeashMarkerArchetype::FootprintSprite(CreatureType species)
{
	return CellSprite(leash_marker::FootprintCell(creature::InfoRow(species)), leash_marker::k_FootprintAngle);
}

std::optional<std::array<LeashMarkerArchetype::Placed, 2>>
LeashMarkerArchetype::Drawn(const Registry& registry, entt::entity creature, const bookmark_pulse::Pulse& pulse)
{
	if (!registry.Valid(creature) || pulse.width == 0.0f || pulse.height == 0.0f)
	{
		return std::nullopt;
	}
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (leashes == nullptr || body == nullptr || !leashes->hasTarget)
	{
		return std::nullopt;
	}
	// over the thing it was sent to as the thing stands now, else at the point kept
	std::optional<leash_marker::Thing> thing;
	if (leashes->targetThing != entt::null && registry.Valid(leashes->targetThing))
	{
		if (const auto* at = registry.TryGet<const Transform>(leashes->targetThing))
		{
			thing = leash_marker::Thing {.place = at->position, .height = object::GetHeight(leashes->targetThing)};
		}
	}
	const auto position = leash_marker::Position(thing, leashes->targetPoint);
	return std::array<Placed, 2> {{
	    {.sprite = RingSprite(), .transform = {position, glm::mat3(1.0f), glm::vec3(leash_marker::RingHalfSize(pulse), 1.0f)}},
	    {.sprite = FootprintSprite(body->species),
	     .transform = {position, glm::mat3(1.0f), glm::vec3(leash_marker::FootprintHalfSize(pulse), 1.0f)}},
	}};
}
