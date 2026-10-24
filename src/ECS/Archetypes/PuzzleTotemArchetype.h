/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
/// A puzzle totem (components::PuzzleTotem): a totem building of no town, as the script's CREATE of a totem makes it
class PuzzleTotemArchetype
{
public:
	/// The abode of the info on the land at the position, with its Y angle and scale, of no town, whole and
	/// indestructible, with a highest step of puzzle_totem::k_DefaultMaxHeight and no height: it stands sunk by its
	/// whole travel until a script, the creature or the hand raises it. Null when the abode cannot be made
	static entt::entity Create(const glm::vec3& ground, AbodeInfo info, float yAngleRadians, float scale);
	/// How far the totem travels between its lowest and its highest (puzzle_totem::Travel of its mesh and scale); 0
	/// without a loaded mesh
	[[nodiscard]] static float TravelOf(entt::entity totem);
	/// Puts the totem at the altitude its share gives (the share is read as the drawing reads it, so a totem puzzle
	/// game's blend counts one read off)
	static void Place(entt::entity totem);
	/// Puts the totem at an altitude above the land, as the hand holds it
	static void PlaceAt(entt::entity totem, float altitude);
	PuzzleTotemArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
