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

#include <chrono>
#include <span>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/VirtualInfluence.h"
#include "ECS/Influence/Influence.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The players' influence: how far their citadels, towns and influence rings reach, and the border drawn round it
class InfluenceSystemInterface
{
public:
	virtual ~InfluenceSystemInterface() = default;

	/// Once a game turn, at its start: the influence rings follow their objects, then the towns' and the citadels'
	/// reach is worked out again
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// Later in the game turn: the border is drawn again, on every tenth turn, once a reach has moved
	virtual void UpdateBorders() = 0;
	/// Once a frame: while the game runs, the hand crossing a border sends out a ripple and a sound, and then the
	/// ripples grow and fade by the game time
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;

	/// How much a place is in a player's influence, from -1 to 1, allies counted: the reach of their citadel, of each of
	/// their towns and of their influence rings that reach it
	[[nodiscard]] virtual float PlayerInfluence(PlayerNames player, const glm::vec3& position) const = 0;
	/// The same, measured the way a caller asks for, with or without allies
	[[nodiscard]] virtual float PlayerInfluence(PlayerNames player, const glm::vec3& position, influence::CalcType type,
	                                            bool includeAllies) const = 0;
	/// Whether a place is inside one of the player's anti-influence rings
	[[nodiscard]] virtual bool IsInAntiInfluence(PlayerNames player, const glm::vec3& position) const = 0;

	/// The border's circles, newest first
	[[nodiscard]] virtual std::span<const influence::Circle> GetCircles() const = 0;
	/// Whether a player's border shows yet: once their citadel has faded in
	[[nodiscard]] virtual bool IsBorderShown(PlayerNames player) const = 0;
	/// The hand's ripples, newest first
	[[nodiscard]] virtual std::span<const influence::Ripple> GetRipples() const = 0;

	/// SET_VIRTUAL_INFLUENCE: what the player's hand keeps of their influence past the border is switched on or off;
	/// off, it loses all its strength. Only a player with an interface, the local one, has it
	virtual void SetVirtualInfluence(PlayerNames player, bool enabled) = 0;
	/// The local interface's turn, right after the hand's movement, and again when the hand uses what it holds on the
	/// land: what the hand keeps past the border follows the place the hand was synced to
	virtual void ProcessVirtualInfluence(const map_coords::MapCoords& handPlace) = 0;
	/// The same turn once more in the map's clear's pass, with every thing of the last land deleted (a new game's made
	/// afresh first): no citadel, town or ring is left, so the hand is out of its player's influence and under no
	/// anti-influence ring, unless the influence is everywhere
	virtual void ProcessVirtualInfluenceAtClearMap([[maybe_unused]] const map_coords::MapCoords& handPlace,
	                                               [[maybe_unused]] bool newGame)
	{
	}
	/// What the player's hand keeps of their influence past the border; none for a player without an interface
	[[nodiscard]] virtual const virtual_influence::State* GetVirtualInfluence(PlayerNames player) const = 0;
};

} // namespace openblack::ecs::systems
