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

#include "Enums.h"

/// Whether a creature can pick a thing up, by the test of things in general: every animal but the birds, which have
/// their own (animals::birds::CreatureCanPickUp), and villagers, mobile objects and pots
namespace openblack::creature_pick_up
{

/// A creature lifts at most this share of its own weight
inline constexpr float k_LiftShare = 0.8f;

/// What the test reads of the creature and the thing
struct Check
{
	/// The creature's player is one the computer plays, the creature is not under a script's control, and the thing is
	/// a script's: such a creature leaves the scripts' things alone
	bool computerPlayer {false};
	bool creatureScripted {false};
	bool thingInScript {false};
	/// A villager worshipping at a site of the creature's own player
	bool ownWorshipper {false};
	/// The thing is on the map, or is the one this creature's hand has taken
	bool inMap {true};
	bool inThisHand {false};
	/// The thing the creature's leash is tied to
	bool leashTiedTo {false};
	/// Their radii across the land, and how far the creature reaches
	float thingRadius {0.0f};
	float creatureRadius {0.0f};
	float reach {0.0f};
	/// The thing's weight, and the creature's own
	float thingWeight {0.0f};
	float creatureWeight {0.0f};
	/// The thing's own say: not one that cannot be picked up, nor a villager inside its home
	bool thingAllows {true};
};

/// The test, in the game's order. Both measures pass when they are equal, and when either side is not a number
[[nodiscard]] bool CreatureMayPickUp(const Check& check);
/// A thing's weight: its scale cubed times its kind's weight
[[nodiscard]] float ThingWeight(float scale, float kindWeight);
/// A villager worshipping at its site (its final state), whose town's player is the creature's: a villager without a
/// town is no one's
[[nodiscard]] bool IsOwnWorshipper(bool worshippingAtSite, std::optional<PlayerNames> villagersPlayer,
                                   PlayerNames creaturesPlayer);
/// A villager's own say: not while it is inside its home, nor when it cannot be picked up
[[nodiscard]] bool VillagerAllows(bool insideItsHome, bool cannotBePickedUp);
/// A pile's whole test (of food, wood, magic food or wood, a store's): not when it cannot be picked up, and only when its
/// kind may become a thing that moves in the world; no weight, size or reach
[[nodiscard]] bool PileAllows(bool cannotBePickedUp, bool canBecomePhysicsObject);

} // namespace openblack::creature_pick_up
