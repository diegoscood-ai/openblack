/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownCentreSpellIcon.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
#include "SpecialPoints.h"
#include "SpellSeedGraphic.h"
#include "TownMagic.h"
#include "WorshipSite.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

/// An icon a slot or a list can still be read through: a live entity with its SpellIcon
bool IsLiveIcon(entt::entity icon)
{
	return icon != entt::null && Registry().Valid(icon) && Registry().AllOf<SpellIcon>(icon);
}

/// entt's on_destroy<TownCentreSpellIcon>, whatever destroys the icon (Registry::Destroy on any path, Remove,
/// Reset): its centre's slot and its town's list let go of it, so no slot or list ever holds a destroyed icon. Only
/// those two lists are changed here, nothing is made or destroyed during the signal; the rest of its deletion
/// (worship site, seed graphic) is OnToBeDeleted's. The component is still there during the signal
void OnIconDestroyed(entt::registry& registry, entt::entity icon)
{
	const auto& townIcon = registry.get<TownCentreSpellIcon>(icon);
	if (townIcon.townCentre != entt::null && registry.valid(townIcon.townCentre))
	{
		if (auto* slots = registry.try_get<TownCentreIcons>(townIcon.townCentre); slots != nullptr)
		{
			std::ranges::replace(slots->icons, icon, entt::entity {entt::null});
		}
	}
	if (townIcon.town != entt::null && registry.valid(townIcon.town))
	{
		if (auto* magic = registry.try_get<TownMagic>(townIcon.town); magic != nullptr)
		{
			std::erase(magic->spellIcons, icon);
		}
	}
}

TownCentreIcons& IconsOf(entt::entity townCentre)
{
	auto& registry = Registry();
	if (auto* icons = registry.TryGet<TownCentreIcons>(townCentre); icons != nullptr)
	{
		return *icons;
	}
	return registry.Assign<TownCentreIcons>(townCentre);
}

entt::entity TownOf(entt::entity townCentre)
{
	const auto* abode = Registry().TryGet<const Abode>(townCentre);
	return abode != nullptr ? town::FromId(abode->townId) : entt::null;
}

/// The graphic shows the highest level the town holds
void UpdateTownCentrePowerUpGraphics(entt::entity icon)
{
	auto& registry = Registry();
	const auto& townIcon = registry.Get<const TownCentreSpellIcon>(icon);
	int level = -1;
	for (int pu = 0; pu < 3; ++pu)
	{
		if (townIcon.powerUps[static_cast<size_t>(pu)])
		{
			level = pu;
		}
	}
	seed_graphic::SetPowerUpType(registry.Get<const SpellIcon>(icon).graphic, level);
}

/// One power-up level on or off, then the graphic
void SetTownCentrePowerUpLevel(entt::entity icon, int powerUp, bool on)
{
	if (powerUp < 0 || powerUp > 2)
	{
		return;
	}
	Registry().Get<TownCentreSpellIcon>(icon).powerUps[static_cast<size_t>(powerUp)] = on;
	UpdateTownCentrePowerUpGraphics(icon);
}

/// A town spell icon in the slot: the entity, its seed graphic, and its place in the town's list
entt::entity CreateIcon(entt::entity townCentre, uint8_t slot, const SpecialPoint& point, SpellSeedType seed)
{
	auto& registry = Registry();
	const auto townEntity = TownOf(townCentre);
	const auto& centreTransform = registry.Get<const Transform>(townCentre);
	const auto& iconInfo = Locator::infoConstants::value().spellIcon.at(1); // "TownSpell Icon"

	// the town centre morphs with the land: the point is raised like its totem (VillageTotemArchetype::Create).
	// (inferred): an openblack adaptation, not in the original
	glm::vec3 position = point.position;
	position.y += GroundAt(position) - GroundAt(centreTransform.position);

	// its creation index: the next of those its town's spells took for their icons (ecs::object_index)
	const auto entity = registry.Create();
	if (const auto* town = townEntity != entt::null ? registry.TryGet<const Town>(townEntity) : nullptr; town != nullptr)
	{
		ecs::object_index::AssignTownIcon(entity, town->id);
	}
	registry.Assign<Transform>(entity, position, point.rotation, centreTransform.scale);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(iconInfo.meshId), static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& spellIcon = registry.Assign<SpellIcon>(entity);
	spellIcon.infoIndex = 1;
	spellIcon.seedType = seed;
	spellIcon.player = town::OwnerOf(townEntity); // the town's player
	registry.Assign<TownCentreSpellIcon>(entity, townEntity, townCentre, slot);
	town_centre::ConnectDeletionListener();

	// the seed graphic 1 m above the icon's top point
	const auto top = GetSpecialPoint(entity, 0);
	const auto graphicPosition = (top ? top->position : position) + glm::vec3(0.0f, 1.0f, 0.0f);
	const auto graphic = seed_graphic::Create(graphicPosition, seed, spellIcon.player, 1.0f, -1);
	seed_graphic::SetAutoUpdate(graphic, false);
	registry.Get<SpellIcon>(entity).graphic = graphic;
	UpdateTownCentrePowerUpGraphics(entity);

	// at the head of the town's list; the town's worship site gets an icon of the seed
	if (townEntity != entt::null)
	{
		if (auto* magic = registry.TryGet<TownMagic>(townEntity); magic != nullptr)
		{
			magic->spellIcons.insert(magic->spellIcons.begin(), entity);
			if (magic->worshipSite != entt::null)
			{
				site::AddSpellIconIfNecessary(magic->worshipSite, seed);
			}
		}
	}
	return entity;
}

/// The icon leaves the town and is destroyed
void DeleteIcon(entt::entity icon)
{
	auto& registry = Registry();
	town_centre::OnToBeDeleted(icon);
	// out of the map, then the entity
	ecs::map_cells::RemoveMapObject(icon);
	registry.Destroy(icon);
	registry.SetDirty();
}
} // namespace

void town_centre::ConnectDeletionListener()
{
	// entt's sink::connect disconnects the same listener first: connected once per registry
	Registry().OnDestroy<TownCentreSpellIcon>().connect<&OnIconDestroyed>();
}

void town_centre::OnToBeDeleted(entt::entity icon)
{
	auto& registry = Registry();
	auto* townIcon = icon != entt::null && registry.Valid(icon) ? registry.TryGet<TownCentreSpellIcon>(icon) : nullptr;
	if (townIcon == nullptr)
	{
		return;
	}
	auto* spellIcon = registry.TryGet<SpellIcon>(icon);
	// out of its town centre's six slots
	if (townIcon->townCentre != entt::null && registry.Valid(townIcon->townCentre))
	{
		if (auto* slots = registry.TryGet<TownCentreIcons>(townIcon->townCentre); slots != nullptr)
		{
			for (size_t slot = 0; slot < slots->icons.size(); ++slot)
			{
				if (slots->icons[slot] == icon)
				{
					slots->icons[slot] = entt::null;
					SPDLOG_LOGGER_INFO(spdlog::get("game"),
					                   "Worship: town centre {} spell icon {} leaves slot {} as it is deleted",
					                   static_cast<uint32_t>(townIcon->townCentre), static_cast<uint32_t>(icon), slot);
				}
			}
		}
	}
	townIcon->townCentre = entt::null;
	// out of its town's list; the town's worship site loses its icon of the seed when no town holds the seed any more
	if (townIcon->town != entt::null && registry.Valid(townIcon->town))
	{
		if (auto* magic = registry.TryGet<TownMagic>(townIcon->town); magic != nullptr)
		{
			std::erase(magic->spellIcons, icon);
			if (magic->worshipSite != entt::null && spellIcon != nullptr)
			{
				site::RemoveSpellIconIfUnheld(magic->worshipSite, spellIcon->seedType);
			}
		}
	}
	townIcon->town = entt::null;
	// then its seed graphic
	if (spellIcon != nullptr)
	{
		seed_graphic::Delete(spellIcon->graphic);
		spellIcon->graphic = entt::null;
	}
}

void town_centre::DeleteDependants(entt::entity townCentre)
{
	if (Registry().TryGet<TownCentreIcons>(townCentre) == nullptr)
	{
		return;
	}
	for (auto& icon : IconsOf(townCentre).icons)
	{
		if (icon == entt::null)
		{
			continue;
		}
		const auto removed = icon;
		icon = entt::null;
		if (auto* townIcon = Registry().TryGet<TownCentreSpellIcon>(removed); townIcon != nullptr)
		{
			townIcon->townCentre = entt::null;
			DeleteIcon(removed);
		}
	}
}

bool town_centre::AddSpell(entt::entity townCentre, SpellSeedType seed)
{
	auto& icons = IconsOf(townCentre).icons;
	if (FindSpellIcon(townCentre, seed) != entt::null)
	{
		return false;
	}
	for (size_t slot = 0; slot < icons.size(); ++slot)
	{
		if (IsLiveIcon(icons[slot]))
		{
			continue;
		}
		// the first free slot's special point and angle
		const auto point = GetSpecialPoint(townCentre, static_cast<int>(slot));
		if (!point)
		{
			if (trace::Enabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Worship: town centre {} has no special point {} (its mesh has {} extra metrics)",
				                   static_cast<uint32_t>(townCentre), slot, ExtraMetricCount(townCentre));
			}
			// the original's loop goes on, but it always tries the first free slot again, so every later try fails too
			// and AddSpell returns false
			return false;
		}
		const auto icon = CreateIcon(townCentre, static_cast<uint8_t>(slot), *point, seed);
		IconsOf(townCentre).icons[slot] = icon;
		return true;
	}
	return false;
}

void town_centre::RemoveSpell(entt::entity townCentre, SpellSeedType seed)
{
	auto& icons = IconsOf(townCentre).icons;
	for (auto& icon : icons)
	{
		if (IsLiveIcon(icon) && Registry().Get<const SpellIcon>(icon).seedType == seed)
		{
			const auto removed = icon;
			icon = entt::null;
			Registry().Get<TownCentreSpellIcon>(removed).townCentre = entt::null;
			DeleteIcon(removed);
		}
	}
}

void town_centre::AddPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp)
{
	if (const auto icon = FindSpellIcon(townCentre, seed); icon != entt::null)
	{
		SetTownCentrePowerUpLevel(icon, powerUp, true);
	}
}

void town_centre::ClearPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp)
{
	if (const auto icon = FindSpellIcon(townCentre, seed); icon != entt::null)
	{
		SetTownCentrePowerUpLevel(icon, powerUp, false);
	}
}

entt::entity town_centre::FindSpellIcon(entt::entity townCentre, SpellSeedType seed)
{
	for (const auto icon : IconsOf(townCentre).icons)
	{
		if (IsLiveIcon(icon) && Registry().Get<const SpellIcon>(icon).seedType == seed)
		{
			return icon;
		}
	}
	return entt::null;
}

int town_centre::SpellCount(entt::entity townCentre)
{
	const auto& icons = IconsOf(townCentre).icons;
	return static_cast<int>(std::ranges::count_if(icons, IsLiveIcon));
}

void town_centre::MakeFunctional(entt::entity townCentre)
{
	const auto townEntity = TownOf(townCentre);
	if (townEntity == entt::null)
	{
		return;
	}
	const auto& tables = Locator::infoConstants::value();
	for (size_t m = 0; m < magic::k_MagicTypeCount; ++m)
	{
		const auto type = static_cast<MagicType>(m);
		if (!town::IsMagicTypeHeld(townEntity, type))
		{
			continue;
		}
		const auto seed = magic::GetFirstSpellSeedForMagicType(tables, type);
		if (static_cast<int>(seed) < 0)
		{
			continue;
		}
		AddSpell(townCentre, seed);
		const int powerUp = magic::GetPowerUpFromMagicType(magic::GetSpellSeedInfo(tables, seed), type);
		if (powerUp != -1)
		{
			AddPowerUp(townCentre, seed, powerUp);
		}
	}
	if (const auto* magic = Registry().TryGet<const TownMagic>(townEntity);
	    magic != nullptr && magic->worshipSite != entt::null)
	{
		site::AddTownSpells(magic->worshipSite, townEntity);
	}
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: town centre {} functional, {} spell icons",
		                   static_cast<uint32_t>(townCentre), SpellCount(townCentre));
	}
}

void town_centre::SetPlayer(entt::entity townCentre, PlayerNames player)
{
	auto& registry = Registry();
	if (townCentre == entt::null || !registry.Valid(townCentre))
	{
		return;
	}
	const auto* slots = registry.TryGet<const TownCentreIcons>(townCentre);
	if (slots == nullptr)
	{
		return;
	}
	// in slot order; a copy, as the components below may move in their storage
	const auto icons = slots->icons;
	for (const auto icon : icons)
	{
		if (icon == entt::null || !registry.Valid(icon))
		{
			continue;
		}
		if (auto* spellIcon = registry.TryGet<SpellIcon>(icon); spellIcon != nullptr)
		{
			spellIcon->player = player;
			seed_graphic::SetPlayer(spellIcon->graphic, player);
		}
	}
}

std::vector<PlayerNames> town_centre::GraphicPlayers(entt::entity townCentre)
{
	std::vector<PlayerNames> players;
	auto& registry = Registry();
	if (townCentre == entt::null || !registry.Valid(townCentre))
	{
		return players;
	}
	const auto* slots = registry.TryGet<const TownCentreIcons>(townCentre);
	if (slots == nullptr)
	{
		return players;
	}
	for (const auto icon : slots->icons)
	{
		const auto* spellIcon = icon != entt::null && registry.Valid(icon) ? registry.TryGet<const SpellIcon>(icon) : nullptr;
		if (spellIcon == nullptr || spellIcon->graphic == entt::null || !registry.Valid(spellIcon->graphic))
		{
			continue;
		}
		if (const auto* graphic = registry.TryGet<const SpellSeedGraphic>(spellIcon->graphic); graphic != nullptr)
		{
			players.push_back(graphic->player);
		}
	}
	return players;
}
