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

namespace openblack::ecs::systems
{

/// A temple whose heart has lost all its life, or which a script deletes with its explosion, is destroyed over 22
/// seconds: the heart's sounds, its beams and glow, the explosion and smoke, then the heart goes. The local player's
/// losing their temple ends the game. The heart passes the blows it takes on to its player's buildings and people.
class TempleDestructionSystemInterface
{
public:
	virtual ~TempleDestructionSystemInterface() = default;

	/// The heart has lost the last of its life: its destruction starts, or starts again from nothing, and the
	/// temple's other parts go at once
	virtual void Start(entt::entity heart) = 0;
	/// A script deletes the heart with the temple's explosion: a heart already being destroyed has its model broken up,
	/// any other starts its destruction
	virtual void DeleteByScript(entt::entity heart) = 0;
	/// The heart's own turn, as its temple takes its turn: a heart being destroyed moves on a turn, and a hurt heart
	/// beams across itself
	virtual void ProcessHeart(entt::entity heart) = 0;
	/// What the heart passes a blow or an effect on to, over its player's towns: a building, else a homeless villager,
	/// else the heart itself. While what strikes it breaks buildings, it beams at what it chose
	virtual entt::entity ActualObjectToEffect(entt::entity heart, bool breaksBuildings) = 0;
	/// At the end of each game turn: the game ends for the local player whose temple is being destroyed
	virtual void EndTurn() = 0;
};

} // namespace openblack::ecs::systems
