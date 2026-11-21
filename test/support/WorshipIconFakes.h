/******************************************************************************
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

#include <array>
#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::test::worship_icons
{
/// A worship site and one miracle icon standing at it
struct SiteIcon
{
	entt::entity site {entt::null};
	entt::entity icon {entt::null};
};

/// The magic types of a seed's four levels (plain miracle, then power-ups one to three) and what each costs to create.
/// A level of MagicType::None has no cost.
struct SeedLevels
{
	std::array<MagicType, 4> types {MagicType::None, MagicType::None, MagicType::None, MagicType::None};
	std::array<float, 4> costs {0.0f, 0.0f, 0.0f, 0.0f};
};

/// Every magic record's own magic type set to where the record stands, as the game's file has them
inline void NumberMagicRecords(InfoConstants& info)
{
	uint32_t next = 0;
	const auto number = [&next](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(next++);
		}
	};
	number(info.magicGeneral);
	number(info.magicHeal);
	number(info.magicTeleport);
	number(info.magicForest);
	number(info.magicFood);
	number(info.magicStormAndTornado);
	number(info.magicShield);
	number(info.magicWood);
	number(info.magicWater);
	number(info.magicFlockFlying);
	number(info.magicFlockGround);
	number(info.magicCreatureSpell);
}

/// The tables in the locator (zeroed ones when there are none) with the magic records numbered and the seed's levels
/// and costs set; the caller keeps a RestoreService of Locator::infoConstants
inline void UseSeedLevels(SpellSeedType seed, const SeedLevels& levels)
{
	auto info = Locator::infoConstants::has_value() ? std::make_unique<InfoConstants>(Locator::infoConstants::value())
	                                                : std::make_unique<InfoConstants>();
	NumberMagicRecords(*info);
	info->spellSeed.at(static_cast<size_t>(seed)).magicTypes = levels.types;
	for (size_t level = 0; level < levels.types.size(); ++level)
	{
		if (levels.types.at(level) != MagicType::None)
		{
			info->magicEffect.at(static_cast<size_t>(levels.types.at(level))).costToCreate = levels.costs.at(level);
		}
	}
	Locator::infoConstants::reset(info.release());
}

/// A human player with every miracle enabled: the one a hand, its interface and its icons answer to
inline entt::entity AddHumanPlayer(ecs::Registry& registry, PlayerNames name)
{
	using namespace ecs::components;
	const auto player = registry.Create();
	registry.Assign<Player>(player, name);
	auto& magic = registry.Assign<PlayerMagic>(player);
	magic.playerType = 1;
	magic.allMagicCheat = true;
	return player;
}

/// The player's citadel, with its six worship site slots empty
inline entt::entity AddCitadel(ecs::Registry& registry, PlayerNames player)
{
	using namespace ecs::components;
	const auto citadel = registry.Create();
	registry.Assign<Temple>(citadel, player);
	registry.Assign<CitadelWorship>(citadel);
	return citadel;
}

/// A worship site in the citadel's slot, of the citadel's player, with `available` prayer power for its icons, and an
/// icon of the seed at `position` holding `stored` chants
inline SiteIcon AddSiteWithIcon(ecs::Registry& registry, entt::entity citadel, size_t slot, SpellSeedType seed, float available,
                                float stored = 0.0f, glm::vec3 position = glm::vec3(0.0f))
{
	using namespace ecs::components;
	const auto player = registry.Get<const Temple>(citadel).owner;
	const auto site = registry.Create();
	registry.Get<CitadelWorship>(citadel).sites.at(slot) = site;
	auto& siteComponent = registry.Assign<WorshipSite>(site);
	siteComponent.citadel = citadel;
	siteComponent.player = player;
	siteComponent.available = available;

	const auto icon = registry.Create();
	registry.Assign<Transform>(icon, Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
	registry.Assign<SpellIcon>(icon, SpellIcon {.seedType = seed, .player = player});
	registry.Assign<WorshipSpellIcon>(icon, WorshipSpellIcon {.site = site, .chantStore = stored});
	siteComponent.icons.push_back(icon);
	return {.site = site, .icon = icon};
}
} // namespace openblack::test::worship_icons
