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

/// Whether a thing still works, by the rule of its kind. A sound tag on a thing ends the turn the thing stops working
/// (Audio's thingFunctional query). Buildings with an abode keep their own rule (abode_queries::IsFunctional). Wiki:
/// docs/wiki/audio/world-sounds.md#soundtag.
namespace openblack::ecs::thing_functional
{

/// Which rule a thing follows
enum class Kind : uint8_t
{
	/// Any thing: it works while it is available
	Plain,
	/// A creature or an animal: available, with a life that is not 0
	Living,
	/// A villager: available, and not in the two top states of its death
	Villager,
	/// A fixed object made of several map cells (a village totem): available, built, and above the life it stops
	/// working at
	MultiMapFixed,
};

/// What the rule reads of the thing
struct Facts
{
	Kind kind {Kind::Plain};
	bool available {false};
	float life {1.0f};
	bool built {true};
	/// The life at or below which a fixed object stops working
	float nonFunctionalBelow {0.0f};
	/// The villager's top state number
	uint32_t villagerTop {0};
};

/// The two top states in which a villager no longer works
constexpr uint32_t k_FirstDeadTopState = 13;
constexpr uint32_t k_LastDeadTopState = 14;

/// The thing's rule on its facts. A life that is not a number counts as 0 for a living thing and as working for a fixed
/// one, as the original's comparisons fall
[[nodiscard]] constexpr bool IsFunctional(const Facts& facts)
{
	if (!facts.available)
	{
		return false;
	}
	switch (facts.kind)
	{
	case Kind::Living:
		return facts.life < 0.0f || facts.life > 0.0f;
	case Kind::Villager:
		return facts.villagerTop < k_FirstDeadTopState || facts.villagerTop > k_LastDeadTopState;
	case Kind::MultiMapFixed:
		return facts.built && !(facts.life <= facts.nonFunctionalBelow);
	case Kind::Plain:
	default:
		return true;
	}
}

/// The rule of the game's thing: a building's own (an abode), a village totem's as a fixed object of several cells, a
/// creature's or an animal's while it lives, a villager's until its death, anything else's while it is available. A
/// thing no longer in the registry does not work
[[nodiscard]] bool OfThing(entt::entity thing);

} // namespace openblack::ecs::thing_functional
