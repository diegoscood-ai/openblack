/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The influence of towns and citadels: a radius each (Town +0x5C8, Citadel::GetInfluence).

#include <array>
#include <unordered_map>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "Influence.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// storyInfluence[land - 1] (exe +0xBC / +0x13C + 4 x land). Land 6 (one map sets it) reads the float after the array.
float StoryInfluence(const std::array<float, 5>& story, float after, int32_t land)
{
	if (land >= 1 && land <= static_cast<int32_t>(story.size()))
	{
		return story.at(static_cast<size_t>(land - 1));
	}
	return after; // land 6; nothing else is used by a map
}

/// Town::GetBaseInfluence 0x73FD40 (GTownInfo, the single info at 0xDA2780)
float BaseInfluence(const TownInfluence& town)
{
	if (town.noInfluence)
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value().town;
	const auto land = influence::LandNumber();
	return land != 0 ? StoryInfluence(info.storyInfluence, info.maxForTimeWillWorkUntil, land) : info.influence;
}

/// The abode's own info record: its abode number and mesh (GAbodeInfo::Find would take the tribeless ark and totem
/// records that come last), or the tribe's
const GAbodeInfo* AbodeInfoOf(const Abode& abode, entt::id_type mesh, Tribe tribe)
{
	const GAbodeInfo* byTribe = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != abode.type)
		{
			continue;
		}
		if (resources::HashIdentifier(info.meshId) == mesh)
		{
			return &info;
		}
		if (byTribe == nullptr && info.tribeType == tribe)
		{
			byTribe = &info;
		}
	}
	return byTribe;
}

/// Abode::GetInfluence 0x4072A0 = MultiMapFixed::GetInfluence 0x52ECA0 (percent built x scale x life x
/// info.influence) x (adults +0xB4 + children +0xB7 + 1). Fields, the town centre, the totem, the dispenser, the storage
/// pit... are all Abodes. openblack has no building sites: every abode is built (percent built +0x5C = 1).
float AbodeInfluence(entt::entity entity, const Abode& abode, Tribe tribe)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* info = AbodeInfoOf(abode, mesh != nullptr ? mesh->id : 0, tribe);
	if (info == nullptr)
	{
		return 0.0f;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	const float scale = transform != nullptr ? transform->scale.x : 1.0f;
	constexpr float k_PercentBuilt = 1.0f;
	const float fixedInfluence = k_PercentBuilt * scale * ecs::life::LifeOf(entity) * info->influence;
	uint32_t adults = 0;
	uint32_t children = 0;
	for (const auto villager : abode.inhabitants)
	{
		if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr)
		{
			(v->lifeStage == Villager::LifeStage::Child ? children : adults) += 1;
		}
	}
	return fixedInfluence * static_cast<float>(adults + children + 1);
}
} // namespace

namespace openblack::influence
{
void ProcessTowns()
{
	// The influence part of Town::Process 0x747380, for every town:
	//   +0x5C8 = GetBaseInfluence; fn_00747600: every processAbodeEvery turns (1: every turn) each abode of the town adds
	//   its GetInfluence (vt+0x868) unless +0x5F8; then, the town having a player, x townInfluenceMultiplier.
	auto& registry = Locator::entitiesRegistry::value();
	std::unordered_map<uint32_t, float> abodes;
	std::unordered_map<uint32_t, Tribe> tribes;
	registry.Each<const Town, const Tribe>(
	    [&](const Town& town, const Tribe& tribe) { tribes.emplace(town.id, tribe); });
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		const auto tribe = tribes.find(abode.townId);
		if (tribe != tribes.end())
		{
			abodes[abode.townId] += AbodeInfluence(entity, abode, tribe->second);
		}
	});
	const float multiplier = TownInfluenceMultiplier();
	registry.Each<const Town, TownInfluence>([&](const Town& town, TownInfluence& influence) {
		influence.radius = BaseInfluence(influence);
		if (!influence.noInfluence)
		{
			influence.radius += abodes[town.id];
		}
		influence.radius *= multiplier; // every town has a player (NEUTRAL included)
	});
}

float TownRadius(entt::entity town)
{
	const auto* influence = Locator::entitiesRegistry::value().TryGet<const TownInfluence>(town);
	return influence != nullptr ? influence->radius : 0.0f;
}

float CitadelRadius(entt::entity temple)
{
	// Citadel::GetInfluence 0x464090 = playerInfluenceMultiplier x Citadel+0x6C. +0x6C is set once, when the citadel's
	// first CitadelHeart is made (ctor 0x4649B0): GetInfluence() (0 then) + M2 x (land ? heart storyInfluence[land-1] :
	// heart influence), M2 = 1 from CREATE_CITADEL (Citadel::CreateCitadel 0x463240) and the planned citadel's scale
	// when it is built (PlannedTownCitadelHeart::CreatePlannedNoFixedCheck 0x467EF0). openblack makes the heart with
	// the temple, so the value is fixed the first time it is asked for (the map script sets the land number first).
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Temple>(temple))
	{
		return 0.0f;
	}
	const auto* stored = registry.TryGet<const CitadelInfluence>(temple);
	if (stored == nullptr)
	{
		const auto& info = Locator::infoConstants::value().citadelHeart;
		const auto land = LandNumber();
		const auto* transform = registry.TryGet<const Transform>(temple);
		// (CREATE_CITADEL passes 1 whatever its size; every map's citadel has size 1000, i.e. scale 1)
		const float scale = transform != nullptr ? transform->scale.x : 1.0f;
		const float story =
		    land != 0 ? StoryInfluence(info.storyInfluence, info.transferedDamageMultiplier, land) : info.influence;
		stored = &registry.Assign<CitadelInfluence>(temple, scale * story);
	}
	return PlayerInfluenceMultiplier() * stored->value;
}
} // namespace openblack::influence
