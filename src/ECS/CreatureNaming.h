/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <string_view>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureNames.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct CreatureMindState;
}

/// Where the game finds what a creature is named with: who it belongs to, the help texts' words and the profile being
/// played. The name itself is kept in the creature's mind (the name its file keeps, or a script's), and until the mind
/// has one, the name it was given as it was made
namespace openblack::ecs::creature_naming
{

/// Who a creature of an owner belongs to, as far as its name goes. The scripts' creatures belong to no one (the neutral
/// player)
[[nodiscard]] creature_names::Owner OwnerOf(PlayerNames owner);

/// The words of a help text, from the interface's text database; empty without one (before the interface, and in the
/// unit tests). Number 0 and numbers past the last give the first text's
[[nodiscard]] std::u16string HelpWords(uint32_t text);

/// The creature name of the profile being played, none without a profile
[[nodiscard]] std::optional<std::u16string> ProfileCreatureName();

/// A new creature is given its name as it is made, with the creatures there are counting it: the player's creature
/// the profile's creature name, any other one of the four help texts' names
void NameNewCreature(components::CreatureMindState& mind, PlayerNames owner, size_t creatures);

/// The name a creature is shown by: a script's, else the one its mind keeps, else the one it was given as it was made;
/// empty without a mind
[[nodiscard]] std::u16string NameOf(const Registry& registry, entt::entity creature);

} // namespace openblack::ecs::creature_naming
