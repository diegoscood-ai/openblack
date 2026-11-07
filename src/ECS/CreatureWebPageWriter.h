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

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureWebPage.h"

namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack
namespace openblack::ecs
{
class Registry;
}

/// The creature's web page written for the player's own creature (creature_web_page): its values gathered from its
/// components, the template read through the resource cache and the page written into the profile's folder
namespace openblack::ecs::creature_web_page_writer
{

/// What the page tells of a creature, none for anything but a creature. The desires are named as the game's tables
/// name them when the tables are given
[[nodiscard]] std::optional<creature_web_page::Facts> FactsOf(const Registry& registry, const InfoConstants* info,
                                                              entt::entity creature);

/// Writes the creature's page, html/<its name>.html, into the folder of the profile played, the html folder made when
/// it is not there. Nothing without a player's folder (a run of the checks has none), without the template in the
/// game's Scripts folder, or for anything but a creature. True when the page was written
bool Write(entt::entity creature);

/// At the end of the creatures' turn: the local player's creature, at each turn its mind is saved at
/// (creature_web_page::IsSaveTurn of the turns it has lived), has its page written
void ProcessTurn();

} // namespace openblack::ecs::creature_web_page_writer
