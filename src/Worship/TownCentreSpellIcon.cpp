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
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
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

/// TownCentreSpellIcon::UpdateGraphicWithPULevels 0x748ED0: the graphic shows the highest level the town holds
void UpdateGraphicWithPULevels(entt::entity icon)
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

/// TownCentreSpellIcon::SetPULevel 0x748EB0
void SetPULevel(entt::entity icon, int powerUp, bool on)
{
	if (powerUp < 0 || powerUp > 2)
	{
		return;
	}
	Registry().Get<TownCentreSpellIcon>(icon).powerUps[static_cast<size_t>(powerUp)] = on;
	UpdateGraphicWithPULevels(icon);
}

/// fn_00748CB0 -> fn_00748BF0 -> fn_00748A70 (TownSpellIcon ctor, fn_0073D1C0) and
/// TownCentreSpellIcon::CallVirtualFunctionsForCreation 0x748D20
entt::entity CreateIcon(entt::entity townCentre, uint8_t slot, const SpecialPoint& point, SpellSeedType seed)
{
	auto& registry = Registry();
	const auto townEntity = TownOf(townCentre);
	const auto& centreTransform = registry.Get<const Transform>(townCentre);
	const auto& iconInfo = Locator::infoConstants::value().spellIcon.at(1); // 0xD9D514 "TownSpell Icon"

	// the town centre morphs with the land: the point is raised like its totem (AbodeArchetype CreateTotemStatue).
	// (inferido): an openblack adaptation, not in 0x744050 / 0x748CB0
	glm::vec3 position = point.position;
	position.y += GroundAt(position) - GroundAt(centreTransform.position);

	// its creation index is taken by ecs::object_index (AddTownSpell / OnTownCentre)
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, point.rotation, centreTransform.scale);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(iconInfo.meshId), static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& spellIcon = registry.Assign<SpellIcon>(entity);
	spellIcon.infoIndex = 1;
	spellIcon.seedType = seed;
	spellIcon.player = town::OwnerOf(townEntity); // GameThing::GetPlayer: the town's
	registry.Assign<TownCentreSpellIcon>(entity, townEntity, townCentre, slot);

	// SpellIcon::Create3DSpellObject 0x726210
	const auto top = GetSpecialPoint(entity, 0);
	const auto graphicPosition = (top ? top->position : position) + glm::vec3(0.0f, 1.0f, 0.0f);
	const auto graphic = seed_graphic::Create(graphicPosition, seed, spellIcon.player, 1.0f, -1);
	seed_graphic::SetAutoUpdate(graphic, false);
	registry.Get<SpellIcon>(entity).graphic = graphic;
	UpdateGraphicWithPULevels(entity);

	// fn_0073D1C0: at the head of the town's list; the town's worship site gets an icon of the seed
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

/// TownSpellIcon::ToBeDeleted 0x748AE0
void DeleteIcon(entt::entity icon)
{
	auto& registry = Registry();
	const auto& townIcon = registry.Get<const TownCentreSpellIcon>(icon);
	const auto seed = registry.Get<const SpellIcon>(icon).seedType;
	if (townIcon.town != entt::null && registry.Valid(townIcon.town))
	{
		// Town::RemoveSpellIcon 0x73D220
		if (auto* magic = registry.TryGet<TownMagic>(townIcon.town); magic != nullptr)
		{
			auto& icons = magic->spellIcons;
			icons.erase(std::remove(icons.begin(), icons.end(), icon), icons.end());
			if (magic->worshipSite != entt::null)
			{
				site::RemoveSpellIconIfUnheld(magic->worshipSite, seed);
			}
		}
	}
	seed_graphic::Delete(registry.Get<const SpellIcon>(icon).graphic);
	registry.Destroy(icon);
	registry.SetDirty();
}
} // namespace

bool town_centre::AddSpell(entt::entity townCentre, SpellSeedType seed)
{
	auto& icons = IconsOf(townCentre).icons;
	if (FindSpellIcon(townCentre, seed) != entt::null)
	{
		return false;
	}
	for (size_t slot = 0; slot < icons.size(); ++slot)
	{
		if (icons[slot] != entt::null)
		{
			continue;
		}
		// fn_00743F60: the first free slot, Game3DObject::GetSpecialPos(slot, &pos, &angle) 0x63B0B0
		const auto point = GetSpecialPoint(townCentre, static_cast<int>(slot));
		if (!point)
		{
			if (trace::Enabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Worship: town centre {} has no special point {} (its mesh has {} extra metrics)",
				                   static_cast<uint32_t>(townCentre), slot, ExtraMetricCount(townCentre));
			}
			// the loop goes on (0x7440A5), but fn_00743F60 always tries the first free slot again (0x743F68), so every
			// later try fails too and AddSpell returns 0 (0x7440AE)
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
		if (icon != entt::null && Registry().Get<const SpellIcon>(icon).seedType == seed)
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
		SetPULevel(icon, powerUp, true);
	}
}

void town_centre::ClearPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp)
{
	if (const auto icon = FindSpellIcon(townCentre, seed); icon != entt::null)
	{
		SetPULevel(icon, powerUp, false);
	}
}

entt::entity town_centre::FindSpellIcon(entt::entity townCentre, SpellSeedType seed)
{
	for (const auto icon : IconsOf(townCentre).icons)
	{
		if (icon != entt::null && Registry().Get<const SpellIcon>(icon).seedType == seed)
		{
			return icon;
		}
	}
	return entt::null;
}

int town_centre::SpellCount(entt::entity townCentre)
{
	const auto& icons = IconsOf(townCentre).icons;
	return static_cast<int>(std::ranges::count_if(icons, [](entt::entity icon) { return icon != entt::null; }));
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
	if (const auto* magic = Registry().TryGet<const TownMagic>(townEntity); magic != nullptr && magic->worshipSite != entt::null)
	{
		site::AddTownSpells(magic->worshipSite, townEntity);
	}
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: town centre {} functional, {} spell icons",
		                   static_cast<uint32_t>(townCentre), SpellCount(townCentre));
	}
}
