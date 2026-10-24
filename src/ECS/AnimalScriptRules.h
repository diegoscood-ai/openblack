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

#include "Enums.h"

/// The scripts' own animals and birds (CHL CREATE and CREATE_WITH_ANGLE_AND_SCALE, which POPULATE_CONTAINER also uses):
/// docs/wiki/scripts/map-script-reader.md#animals-and-birds
namespace openblack::ecs::animal_script_rules
{

/// The domain radius and the flock distance of the flock of its own that a script's animal is made with
constexpr uint16_t k_ScriptDomainRadius = 2;
constexpr uint16_t k_ScriptFlockDistance = 1;
/// How far, in metres, the town a script's animal belongs to may be: the nearest town of any tribe
constexpr float k_ScriptTownRadius = 500.0f;

/// A script makes an animal or a bird: it is created alone, its own flock is shrunk to a small domain, it belongs to
/// the nearest town within k_ScriptTownRadius (none when there is none), and then it is held by the script (IN_SCRIPT),
/// so it keeps still until the script moves it or lets it go. `create()` makes it (`none` when the species makes
/// nothing, and then nothing else is done), `setDomain(animal, radius, distance)`, `nearestTown(animal, radius)`,
/// `setTown(animal, town)` and `setScriptState(animal, state)` act on it. The result is the animal made
template <typename Thing, typename Create, typename SetDomain, typename NearestTown, typename SetTown, typename SetScriptState>
Thing CreateScriptAnimal(Thing none, Create&& create, SetDomain&& setDomain, NearestTown&& nearestTown, SetTown&& setTown,
                         SetScriptState&& setScriptState)
{
	const Thing animal = create();
	if (animal == none)
	{
		return none;
	}
	setDomain(animal, k_ScriptDomainRadius, k_ScriptFlockDistance);
	setTown(animal, nearestTown(animal, k_ScriptTownRadius));
	setScriptState(animal, LivingStates::LivingInScript);
	return animal;
}

} // namespace openblack::ecs::animal_script_rules
