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

#include <entt/fwd.hpp>

#include "Creature/TattooMenu.h"

namespace openblack::ecs::archetypes
{
class TattooMenuCreatureArchetype
{
public:
	/// The body the tattoo editor of the options' Players page shows (components::TattooMenuCreature): a creature's
	/// shape, skin, tattoos, pose, eyes and hair for the body given, of no player, at the size of 1, with no place in
	/// the world. None without the registry
	static std::optional<entt::entity> Create(const tattoo_menu::Shown& shown);
	TattooMenuCreatureArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
