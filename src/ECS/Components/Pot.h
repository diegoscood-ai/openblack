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

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

namespace openblack::ecs::components
{

struct Pot
{
	uint32_t amount;
	uint32_t maxAmount;
	PotInfo type = PotInfo::_COUNT;
	bool poisoned = false;
	bool speedUp = false;
	entt::entity speedUpVisual = entt::null; ///< The PILEFOOD_SPEEDUP spot visual container
	/// The pile's own player: the player whose miracle or hand made a magic pile; a loaded food or wood pile that is
	/// part of no structure, the player its record names; else the neutral player. A pile of a storage pit or a worship
	/// site belongs to the pit's or the site's player instead (object_resources::PlayerOfPile)
	PlayerNames owner = PlayerNames::NEUTRAL;
	/// Its row's reaction was started (animal_ai::SetupPotReaction), until animal_ai::RemovePotReaction takes the
	/// pot's reactions away
	bool reactionStarted = false;
	/// The town its maker gave it: a field's or a fish farm's handful, a handful of a store's pile (the store's town),
	/// a creature's portion (the nearest town); none for the rest. Nothing in play reads it: a saved game writes it
	entt::entity town = entt::null;
	/// The player it answered with as it was made: its store's or its worship site's player then, a magic pile's maker,
	/// else the neutral player; a loaded pile's as the load set it (saved_world_load::Apply). Only a saved game reads it
	PlayerNames playerWhenMade = PlayerNames::NEUTRAL;
	/// Made as a building site's wood pile. Only a saved game reads it
	bool fromBuildingSite = false;
	/// Made to start its row's reaction as it is made (a script's pot, a pile a put-down or a script leaves). Only a
	/// saved game reads it once the pile is made
	bool reactsWhenMade = false;
};

// A pile's sink offset: piles rise out of / sink into the ground instead of scaling. A change of amount moves the
// offset to its new target in 1 s (the same quartic as the Zoomer).
struct PileSink
{
	float baseY;
	float height = 1.0f;
	openblack::Zoomer offset {};
};

// Texture offset of the object (u, v; graphics::frame_anim::UvOffset), added by the draw to the primitives whose
// material lacks flag bit 0x10. The food pile's draw scrolls the grain of the storage pit and magic food piles by
// v = 0.25 * sink / height, so that the grain stays put in the world and the pile seems to shrink; the one-off spell
// seeds step the orbs' 4x4 texture (u and v in quarters); the designed waterfall scrolls the water's v; the spell seed
// graphic steps the creature spell phials' 8 x 4 texture (u and v in eighths).
struct UvScroll
{
	float v = 0.0f;
	float u = 0.0f; ///< in 1/256 steps (the renderer packs it with v, frame_anim::PackUvOffset)
};

} // namespace openblack::ecs::components
