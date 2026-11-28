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

namespace openblack::creature_mimic_type
{

/// The kinds of thing a creature copying the player tells apart, in the game's numbers: a thing like the one the
/// player did a deed to is a thing of the same kind
enum class MimicType : uint32_t
{
	MaleVillager = 0,
	FemaleVillager = 1,
	Child = 2,
	NiceAnimal = 3,
	NastyAnimal = 4,
	Abode = 5,
	Tree = 6,
	Rock = 7,
	MobileObject = 8,
	Creature = 9,
	/// Anything else
	Other = 10,
};

/// What tells a thing's kind, as its kind of object answers it
struct Facts
{
	bool creature {false};
	bool villager {false};
	bool child {false};
	/// A villager other than a man
	bool female {false};
	bool animal {false};
	/// An animal whose info is of the nasty animals' alignment
	bool nastyAnimal {false};
	/// A rock: a rock itself, the bonfire, a piece knocked off a building, or a dead or felled tree
	bool rock {false};
	bool tree {false};
	/// A building or a field, a football pitch, a totem or a spell dispenser
	bool abode {false};
	/// Any other thing that moves or can be moved: a mobile object or a mobile static
	bool mobile {false};
};

/// The kind of a thing: a creature, a villager (a child, else a woman, else a man), an animal (nasty or nice), a rock
/// (before the other mobile statics it is one of), a tree, an abode, another mobile thing, else none of these
[[nodiscard]] MimicType Of(const Facts& facts);

} // namespace openblack::creature_mimic_type
