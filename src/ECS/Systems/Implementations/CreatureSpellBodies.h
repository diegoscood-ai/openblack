/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/CreatureSpellBodiesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The creatures of the game's registry: the body where its Transform puts it, at the size it is drawn, its bones as
/// posed for the game turn, its eyes from its species' rig and the invisible spell's share from the spells on it
class CreatureSpellBodies final: public CreatureSpellBodiesInterface
{
public:
	[[nodiscard]] bool IsAvailable(entt::entity creature) const override;
	[[nodiscard]] std::optional<CreatureSpellBody> CreatureBody(entt::entity creature) const override;
};
} // namespace openblack::ecs::systems
