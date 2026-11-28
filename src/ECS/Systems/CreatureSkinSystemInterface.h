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

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureTattoo.h"

namespace openblack
{
enum class PlayerNames : uint8_t;
}

namespace openblack::ecs::systems
{

/// Paints the creatures' skins (see components::CreatureSkin): blended towards evil or good as each body is drawn,
/// with its tattoos, wounds and blood over them, painted again whenever one of those changes. Heals the marks as the
/// game's turns go by.
class CreatureSkinSystemInterface
{
public:
	virtual ~CreatureSkinSystemInterface() = default;

	/// Once a frame, after the bodies are brought up to date: paints the skins that have changed, a new creature's
	/// straight away
	virtual void Update() = 0;
	/// Once a game turn: the marks heal a little
	virtual void ProcessTurn() = 0;
	/// The creature turn one creature at a time, as the original's creature turn (issue #6): its shared start, before
	/// the first creature; one creature's part; its shared end, after the last. ProcessTurn is the three over every
	/// creature in its own order
	virtual void BeginCreatureTurns() {}
	virtual void ProcessCreatureTurn(entt::entity /*creature*/) {}
	virtual void EndCreatureTurns() {}

	/// A tattoo put in one of a creature's slots, or an empty slot to take one off
	virtual void SetTattoo(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) = 0;
	/// The masks of the creature's player (the local player's for a creature of no player) made again from its tattoos
	/// and its block of pictures, when it has a block (creature_tattoo::RemakeMasks), then the creature painted again.
	/// The other creatures keep their skins until they are painted again.
	virtual void RemakeTattooMasks(entt::entity /*creature*/, creature_tattoo::Sheet /*sheet*/) {}
	/// Tattoos put on as the end of a drag in the tattoo editor puts them: the creature takes the slots and a new block
	/// of the player's own pictures they show, and its player's masks are made again from the sheet with those pictures
	virtual void PutOnTattoos(entt::entity /*creature*/, const creature_tattoo::Slots& /*slots*/) {}
	/// A player's masks, which the tattoos of its creatures are painted with
	[[nodiscard]] virtual const creature_tattoo::Masks* TattooMasks(PlayerNames /*player*/) const { return nullptr; }
	/// A wound or burn, or a drop of blood, on a creature's skin
	virtual void AddWound(entt::entity creature, const creature_marks::Mark& wound) = 0;
	virtual void AddBlood(entt::entity creature, const creature_marks::Mark& blood) = 0;
	/// A creature's marks healed by some counts at once, as a heal effect does
	virtual void Heal(entt::entity creature, uint32_t counts) = 0;
	/// A trail of blood started from a bleeding wound's cut (creature_marks::StartTrail)
	virtual void Bleed(entt::entity /*creature*/, const creature_marks::MeshCut& /*cut*/) {}
	/// Once a game turn, before the bodies take the turn's new pose: each running trail of blood takes its step
	virtual void BleedTurn() {}
	/// One creature's part of that, in its own body's update
	virtual void BleedTurnOf(entt::entity /*creature*/) {}
};

} // namespace openblack::ecs::systems
