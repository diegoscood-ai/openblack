/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/DrawList/ObjectList.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::draw_list
{

/// The game's objects as the object draw list reads them, from the registry it is given (never the locator):
/// - available while the entity exists and is not components::Unavailable (to be deleted, waiting on the dead list);
/// - DontDraw from components::DontDraw;
/// - a human 3D object is a villager's (components::Villager, the special villagers included); a complex one is the
///   creature's (components::Creature). The advisor spirits' complex objects are not in the registry;
/// - the listed mark is components::DrawListed on the entity, so it goes when the entity is destroyed. An unavailable
///   object keeps it, and a rebuild clearing its old entry still clears it.
class RegistryObjects final: public EntityProbe
{
public:
	explicit RegistryObjects(Registry& registry);

	[[nodiscard]] bool Available(entt::entity entity) const override;
	[[nodiscard]] bool DontDraw(entt::entity entity) const override;
	[[nodiscard]] bool IsHuman(entt::entity entity) const override;
	[[nodiscard]] bool IsComplex(entt::entity entity) const override;
	[[nodiscard]] bool Listed(entt::entity entity) const override;
	void SetListed(entt::entity entity, bool listed) override;

private:
	[[nodiscard]] bool Exists(entt::entity entity) const;

	Registry& _registry;
};

} // namespace openblack::ecs::draw_list
