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
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
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

/// Town::GetNearestTownToPos 0x73B170 (pos, tribe, 0x7FFF, FLT_MAX): the nearest town of that tribe.
/// (inferido): 0x7FFF is taken as "any player" and the distance as x/z; 0x73B170 is not read
entt::entity NearestTownOfTribe(const glm::vec3& position, Tribe tribe)
{
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max();
	Registry().Each<const Town, const Tribe, const Transform>(
	    [&](entt::entity town, const Town&, const Tribe& t, const Transform& transform) {
		    if (t != tribe)
		    {
			    return;
		    }
		    const float distance = glm::distance(glm::vec2(position.x, position.z),
		                                         glm::vec2(transform.position.x, transform.position.z));
		    if (distance < bestDistance)
		    {
			    bestDistance = distance;
			    best = town;
		    }
	    });
	return best;
}

std::vector<entt::entity> TownsOf(PlayerNames player)
{
	std::vector<entt::entity> towns;
	Registry().Each<const Town>([&](entt::entity town, const Town& data) {
		if (data.owner == player)
		{
			towns.push_back(town);
		}
	});
	return towns;
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
	// Citadel::RequestANewWorshipSite 0x4633F0
	auto near = Registry().Get<const Transform>(citadelEntity).position;
	if (const auto town = NearestTownOfTribe(near, tribe); town != entt::null)
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
	constexpr float k_Step = static_cast<float>(magic::k_TurnMs) * 0.001f;
	if (worship.strainSoundFraction > strain)
	{
		worship.strainSoundFraction -= k_Step;
		if (worship.strainSoundFraction <= strain)
		{
			worship.strainSoundFraction = strain;
		}
	}
	else
	{
		worship.strainSoundFraction += k_Step;
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
	for (const auto town : TownsOf(player))
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
	for (const auto town : TownsOf(Registry().Get<const Temple>(citadelEntity).owner))
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
		for (const auto town : TownsOf(player))
		{
			const auto* magic = Registry().TryGet<const TownMagic>(town);
			if (magic != nullptr && magic->worshipSite == entt::null) // vt 0x30C GetWorshipSite
			{
				AddTown(citadelEntity, town);
			}
		}
	}
}
