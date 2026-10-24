/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include <entt/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Creature
{
	/// The player the creature belongs to
	PlayerNames owner;
	/// Whether it is the one creature its owner can put a leash on. Each player has at most one; the leash system keeps
	/// it so, making a player's first creature theirs to lead.
	bool leashable {false};
	CreatureType species;
	entt::id_type mind;
	/// What the creature has become, which its body shows: alignment from -1 (evil) to 1 (good), fatness and strength
	/// from 0 to 1
	float alignment {0.0f};
	/// The alignment change still to come this turn: what the creature did adds to it, and the end of its turn folds it
	/// in, by at most its species' change per turn
	float pendingAlignment {0.0f};
	float fatness {0.5f};
	float strength {0.5f};
	/// How big the creature is: 1 is about 15 units tall, whatever its species' mesh
	float size {1.0f};
	/// Whether it is in its growing-up scripts, as CREATURE_IN_DEV_SCRIPT sets it
	bool inDevScript {false};
	/// Whether a miracle that takes the last of its life knocks it out; a script can make it unable to die, when such a
	/// miracle gives it back all its life instead
	bool canDie {true};
	/// Whether a script lets it go into a vortex that leads away from the land: while it is on the leash it may then be
	/// taken by one. No creature may until a script says so (the scripts' property 36)
	bool mayGoThroughVortex {false};
	/// The angle about the upright, in radians, that the scripts read and set (their property ANGLE). The game keeps it
	/// apart from the way the creature faces: it is 0 when the creature is made and only a script's SET changes it;
	/// the creature's turns and walks neither follow nor change it
	float scriptYAngle {0.0f};
};
} // namespace openblack::ecs::components
