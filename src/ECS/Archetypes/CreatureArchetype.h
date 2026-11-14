/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "ECS/Components/CreatureBodyMetrics.h"
#include "Enums.h"

namespace openblack::ecs::archetypes
{
class CreatureArchetype
{
public:
	/// What a creature has become: alignment from -1 (evil) to 1 (good), fatness and strength from 0 to 1. Its body is
	/// drawn from these, its species' own strength counting a little towards how strong it looks.
	struct Body
	{
		float alignment {0.0f};
		float fatness {0.5f};
		float strength {0.5f};
	};

	/// How a new creature of the species starts: neutral, as big, fat and strong as the species starts, before the
	/// draws that move its fatness and strength as it is made. The size is the species' as it is, whatever the limits
	/// its body is drawn within
	[[nodiscard]] static float StartScale(CreatureType species);
	[[nodiscard]] static Body StartBody(CreatureType species);
	/// The scale a creature of the species is drawn at for its size, by its base mesh's rest pose
	[[nodiscard]] static float DrawnScale(CreatureType species, float size);
	/// What the body of a creature of the species measures, from its base mesh and its stand animation: both 0 without
	/// the mesh, and the reach 0 without the animation, as the game measures nothing without bones or an animation
	[[nodiscard]] static components::CreatureBodyMetrics BodyMetrics(CreatureType species);
	/// The species' own strength, 0 to 1, which counts a little towards how strong its creatures look
	[[nodiscard]] static float SpeciesStrength(CreatureType species);

	/// The way a new creature faces when its maker gives no facing: the game makes its creatures at angle 0, and they
	/// turn only as they move
	static constexpr float k_MadeYAngle = 0.0f;

	/// A new creature of the species, of its player, at its place and facing. It keeps the size it is made with (scale)
	/// as its own; its body is drawn at that size kept within creature_morph::ClampScale's limits. With the game's
	/// tables it makes the draws of creature_creation::Make, whatever body it is given, and starts with the desires,
	/// personality and previous actions they make. Without a body it is neutral, with the strength and fatness drawn
	/// (the species' own without the tables); a body given replaces them
	static entt::entity Create(const glm::vec3& position, PlayerNames playerName, CreatureType creatureType,
	                           entt::id_type creatureMindId, float yAngleRadians, float scale,
	                           const std::optional<Body>& given = std::nullopt);
	CreatureArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
