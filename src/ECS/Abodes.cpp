/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Abodes.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/Physics/CollisionSounds.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<AbodeType> abodes::TypeOf(entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const Abode>(abode);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	// Abode::GetAbodeType 0x4061F0 reads the info record the abode was made with. openblack keeps the abode number
	// (AbodeArchetype), so the record is looked up by it and by the mesh, as influence::AbodeInfoOf does; (inferido)
	// every tribe's record of one abode number carries the same ABODE_TYPE bits.
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const auto meshId = mesh != nullptr ? mesh->id : 0;
	std::optional<AbodeType> byNumber;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != component->type)
		{
			continue;
		}
		if (resources::HashIdentifier(info.meshId) == meshId)
		{
			return info.abodeType;
		}
		if (!byNumber.has_value())
		{
			byNumber = info.abodeType;
		}
	}
	return byNumber;
}

bool abodes::InterfaceValidToTap(entt::entity abode)
{
	// 0x406820: `mov eax, 1`
	return Locator::entitiesRegistry::value().AllOf<Abode>(abode);
}

void abodes::InterfaceTap(entt::entity abode, const glm::vec3& handPosition)
{
	// 0x406864..0x406870: only an abode whose ABODE_TYPE has the living-quarters bit (test al, 2) knocks; the houses A..F
	// and the windmill have it, the civic buildings (totem, storage pit, creche, workshop, wonder, graveyard, town
	// centre, football pitch, spell dispenser, field) do not.
	const auto type = TypeOf(abode);
	if (!type.has_value() || (static_cast<uint32_t>(*type) & static_cast<uint32_t>(AbodeType::LivingQuarters)) == 0)
	{
		return;
	}
	// 0x4068E2..0x40694A: GAudio::PlaySoundEffect 0x429E30 with bank InGame (GAudio+0x3AC), sample 110 G_KnockRoofMulti
	// + the counter [0xC4CC7C] (0..8 in turn, 0x4068F4..0x406919), owner the abode (+0x20), is3D 1 (+0x08), track 0
	// (+0x0C), at the interface status' +0xC8 (the hand's point); mode and loops stay the ctor's (3 and 0), and the .sad
	// gives the sample 5 % pitch spread and min / max 100 / 150.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 110 + audio::NextCounter(audio::Counter::KnockRoof)};
	options.owner = audio::Owner::Thing(abode);
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}

// ---- life and damage (moved unchanged from ECS/Physics/Buildings.cpp, session Edificios) ----------------------

void abodes::StopBeingFunctional(entt::entity building)
{
	// TODO: villagers leave, stores' piles come loose, the town's emergency, a repair site (Abode::ReduceLife 0x405D90)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} no longer works", static_cast<uint32_t>(building));
}

void abodes::DestroyedByEffect(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} destroyed", static_cast<uint32_t>(building));
	// Abode::RemoveAllVillagersFromAbode 0x404560: Villager::HomeDeleted 0x7611F0 of each (MakeHomeless: out of the
	// abode, the town's homeless list, 129 HOMELESS_START)
	if (registry.AllOf<Abode>(building))
	{
		ecs::abode_villagers::RemoveAllVillagersFromAbode(building);
	}
	if (const auto* pit = registry.TryGet<const StoragePit>(building))
	{
		for (const auto pile : pit->woodPiles)
		{
			if (registry.Valid(pile))
			{
				ecs::map_cells::RemoveMapObject(pile); // CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548
				registry.Destroy(pile);
			}
		}
		if (registry.Valid(pit->foodPile))
		{
			ecs::map_cells::RemoveMapObject(pit->foodPile); // CleanupWhenDeleted 0x6377F0, vt +0x548
			registry.Destroy(pit->foodPile);
		}
	}
	physics::Buildings::OnBuildingDeleted(building);
	// CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548 (MultiMapFixed 0x52E7B0), out of all its cells
	ecs::map_cells::RemoveMapObject(building);
	registry.Destroy(building);
	registry.SetDirty();
}

bool abodes::OnPhysicalDamage(entt::entity building, const PhysicalDamage& hit)
{
	auto& registry = Locator::entitiesRegistry::value();
	// 0x406640..0x40671D: SamplePlayAnimEffect(this, the camera's distance, {1, 0, 0x16, 9, 75}, 0, editor.sad, track 0)
	// (G_Crash_Abode_01..09 in editor.sad's table)
	physics::CollisionSounds::PlayAnimEffect({1, 0, 0x16, 9, 75}, building, registry.Get<const Transform>(building).position, false);
	auto& life = registry.AllOf<Life>(building) ? registry.Get<Life>(building) : registry.Assign<Life>(building);
	const float before = life.value;
	if (auto* damage = registry.TryGet<BuildingDamage>(building); damage != nullptr && hit.remaining)
	{
		life.value = std::min(life.value, *hit.remaining);
		// Abode::ReduceLife 0x405D90: a repair site whose baseline is 1.1 x life - 0.1
		damage->repairBase = 1.1f * life.value - 0.1f;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} hit, life {:.2f} -> {:.2f}", static_cast<uint32_t>(building), before,
	                   life.value);
	// TODO: GAlignment::Update (an evil act), Town::UpdateAggressor, GPlayer::DamageFromPlayer, creature mimic
	if (before >= 0.75f && life.value < 0.75f) // info.dat ThresholdForStopBeingFunctional
	{
		StopBeingFunctional(building);
	}
	if (life.value <= 0.0f)
	{
		DestroyedByEffect(building);
		return false;
	}
	return true;
}

float abodes::GetPercentForDrawBuilding(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	// GetPercentBuilt (vt +0x880, MultiMapFixed +0x5C): 1 until V6 (abode_queries::IsBuilt)
	const float built = 1.0f;
	// GetPercentRepairedFromWhenDamaged 0x52F010
	float repaired = 1.0f;
	if (abode_queries::IsBuilt(building))
	{
		const auto* life = registry.TryGet<const Life>(building);
		const float percentRepaired = life != nullptr ? life->value : 1.0f; // GetPercentRepaired = GetLife (0x401500)
		const auto* damage = registry.TryGet<const BuildingDamage>(building);
		if (damage != nullptr && damage->mesh)
		{
			const float a = 1.0f - damage->repairBase;
			const float b = percentRepaired - damage->repairBase;
			repaired = (a == 0.0f || b == 0.0f) ? 0.0f : b / a;
		}
		else
		{
			repaired = percentRepaired * 0.98f; // [0x8CF3FC]
		}
	}
	// 0x52EFD0: GetPercentBuilt <= repaired ? GetPercentBuilt : repaired
	return built <= repaired ? built : repaired;
}
