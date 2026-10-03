/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Citadel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "TownMagic.h"
#include "WorshipSite.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

bool IsCitadel(entt::entity citadel)
{
	return citadel != entt::null && Registry().Valid(citadel) && Registry().AllOf<CitadelWorship, Temple, Transform>(citadel);
}

} // namespace

entt::entity citadel::Of(PlayerNames player)
{
	entt::entity found = entt::null;
	Registry().Each<const Temple, const CitadelWorship>([&](entt::entity entity, const Temple& temple, const CitadelWorship&) {
		if (found == entt::null && temple.owner == player)
		{
			found = entity;
		}
	});
	return found;
}

std::array<entt::entity, 6> citadel::WorshipSitesOf(PlayerNames player)
{
	std::array<entt::entity, 6> sites {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	// 0x71B2A5..0x71B2AD: GPlayer +0xA48, none -> nothing
	const auto citadelEntity = Of(player);
	if (!IsCitadel(citadelEntity))
	{
		return sites;
	}
	// 0x71B2B3..0x71B301: +0x34 + 4 i, i = 0..5 (CitadelWorship::sites is indexed by the slot, WorshipSite +0x110)
	const auto& worship = Registry().Get<const CitadelWorship>(citadelEntity);
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto site = worship.sites.at(i);
		sites.at(i) = site != entt::null && Registry().Valid(site) && Registry().AllOf<WorshipSite>(site) ? site : entt::null;
	}
	return sites;
}

float citadel::StrainSoundFraction(entt::entity citadelEntity)
{
	return IsCitadel(citadelEntity) ? Registry().Get<const CitadelWorship>(citadelEntity).strainSoundFraction : 0.0f;
}

float citadel::StrainSoundFractionAtMostOne(entt::entity citadelEntity)
{
	const float fraction = StrainSoundFraction(citadelEntity);
	// 0x71B319..0x71B332: fld +0x70; fcomp 1.0 (0x8AA390); C0 (below or unordered) -> +0x70, else 1.0 (0x3F800000)
	return fraction < 1.0f || std::isnan(fraction) ? fraction : 1.0f;
}

void citadel::Initialise(entt::entity temple, float heartYAngle)
{
	auto& worship = Registry().AssignOrReplace<CitadelWorship>(temple);
	worship.heartYAngle = heartYAngle;
}

entt::entity citadel::AddTown(entt::entity citadelEntity, entt::entity town)
{
	const auto site = FindOrCreateWorshipSite(citadelEntity, town);
	if (site == entt::null)
	{
		return entt::null;
	}
	const auto& towns = Registry().Get<const WorshipSite>(site).towns;
	if (std::ranges::find(towns, town) == towns.end())
	{
		site::AddTown(site, town);
	}
	// the heart's vt 0x5B0(1.0) (UNVERIFIED: a refresh of the heart; nothing to do here)
	return site;
}

entt::entity citadel::FindOrCreateWorshipSite(entt::entity citadelEntity, entt::entity town)
{
	if (!IsCitadel(citadelEntity) || town == entt::null)
	{
		return entt::null;
	}
	if (Registry().Get<const CitadelWorship>(citadelEntity).cannotCreateSites || !town::IsAllowedToCreateWorshipSite(town))
	{
		return entt::null;
	}
	const auto* tribe = Registry().TryGet<const Tribe>(town);
	return tribe != nullptr ? FindOrCreateWorshipSite(citadelEntity, *tribe) : entt::null;
}

entt::entity citadel::FindOrCreateWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	if (const auto site = FindTribeWorshipSite(citadelEntity, tribe); site != entt::null)
	{
		return site;
	}
	// Citadel::RequestANewWorshipSite 0x4633F0: Town::GetNearestTownToPos 0x73B170 (the citadel's MapCoords +0x14, the
	// tribe, 0x7FFF = any abode type, FLT_MAX [0x8C7E1C]) at 0x46345C; that town's MapCoords, else the citadel's
	auto near = Registry().Get<const Transform>(citadelEntity).position;
	if (const auto town = ecs::map_cells::GetNearestTownToPos(ecs::map_coords::FromWorld(near), tribe,
	                                                          ecs::map_cells::k_AnyAbodeType,
	                                                          std::numeric_limits<float>::max());
	    town != entt::null)
	{
		near = Registry().Get<const Transform>(town).position;
	}
	return site::Create(citadelEntity, tribe, near);
}

entt::entity citadel::FindTribeWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	for (const auto site : Registry().Get<const CitadelWorship>(citadelEntity).sites)
	{
		if (site != entt::null && Registry().Valid(site) && Registry().Get<const WorshipSite>(site).tribe == tribe)
		{
			return site;
		}
	}
	return entt::null;
}

void citadel::ProcessSpellIcons(entt::entity citadelEntity)
{
	if (!IsCitadel(citadelEntity))
	{
		return;
	}
	float strain = 0.0f;
	for (const auto site : std::array(Registry().Get<const CitadelWorship>(citadelEntity).sites))
	{
		if (site == entt::null || !Registry().Valid(site))
		{
			continue;
		}
		site::ProcessSpellIcons(site);
		const float siteStrain = Registry().Get<const WorshipSite>(site).strain;
		if (siteStrain > strain)
		{
			strain = siteStrain;
		}
	}
	// Citadel::SetWorshipStrainSoundFrac 0x463850 (the local player's citadel only)
	auto& worship = Registry().Get<CitadelWorship>(citadelEntity);
	if (!magic::players::IsHuman(Registry().Get<const Temple>(citadelEntity).owner) || worship.strainSoundFraction == strain)
	{
		return;
	}
	// 0x4638A4..0x4638B9: mov eax, [0xD01A38]; fild qword (zero high half); fmul [0x8C7E30] = 0.001f, read every turn
	const float step = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	if (worship.strainSoundFraction > strain)
	{
		worship.strainSoundFraction -= step;
		if (worship.strainSoundFraction <= strain)
		{
			worship.strainSoundFraction = strain;
		}
	}
	else
	{
		worship.strainSoundFraction += step;
		if (!(worship.strainSoundFraction < strain))
		{
			worship.strainSoundFraction = strain;
		}
	}
}

entt::entity citadel::CreateBuiltWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	const auto site = FindOrCreateWorshipSite(citadelEntity, tribe);
	if (site == entt::null)
	{
		return entt::null;
	}
	const auto player = Registry().Get<const Temple>(citadelEntity).owner;
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		const auto* townTribe = Registry().TryGet<const Tribe>(town);
		if (townTribe == nullptr || *townTribe != tribe) // Town +0x5B8: the tribe index
		{
			continue;
		}
		const auto& towns = Registry().Get<const WorshipSite>(site).towns;
		if (std::ranges::find(towns, town) == towns.end())
		{
			site::AddTown(site, town);
		}
		// Town::GetBuildingSiteInList / RemoveBuildingSite: openblack's towns have no building sites
		return site;
	}
	return site; // vt 0x900(1.0): built
}

void citadel::OpenWorshipSites(entt::entity citadelEntity)
{
	if (!IsCitadel(citadelEntity))
	{
		return;
	}
	for (const auto town : ecs::map_cells::TownsOf(Registry().Get<const Temple>(citadelEntity).owner))
	{
		AddTown(citadelEntity, town);
	}
}

entt::entity citadel::GetSpellIcon(entt::entity citadelEntity, MagicType type)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	for (const auto site : Registry().Get<const CitadelWorship>(citadelEntity).sites)
	{
		if (site != entt::null && Registry().Valid(site))
		{
			if (const auto icon = site::GetSpellIconFromMagicType(site, type); icon != entt::null)
			{
				return icon;
			}
		}
	}
	return entt::null;
}

void citadel::PostLoadCleanup()
{
	std::vector<std::pair<entt::entity, PlayerNames>> citadels;
	Registry().Each<const Temple, const CitadelWorship>([&](entt::entity entity, const Temple& temple, const CitadelWorship&) {
		citadels.emplace_back(entity, temple.owner);
	});
	for (const auto& [citadelEntity, player] : citadels)
	{
		for (const auto town : ecs::map_cells::TownsOf(player))
		{
			const auto* magic = Registry().TryGet<const TownMagic>(town);
			if (magic != nullptr && magic->worshipSite == entt::null) // vt 0x30C GetWorshipSite
			{
				AddTown(citadelEntity, town);
			}
		}
	}
}
