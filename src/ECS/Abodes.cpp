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
#include "3D/L3DMesh.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Town.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PartialBuild.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownStores.h"
#include "Resources/ResourcesInterface.h"
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
	// every tribe's record of one abode number carries the same ABODE_TYPE bits. (V6) the record AbodeArchetype kept
	// (+0x28) first
	if (const auto i = static_cast<size_t>(static_cast<int32_t>(component->info));
	    component->info != AbodeInfo::None && i < Locator::infoConstants::value().abode.size())
	{
		return Locator::infoConstants::value().abode.at(i).abodeType;
	}
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
	// Graveyard::ToBeDeleted 0x595CB0 (vt +0x910) -> DeleteDependancys 0x595CE0: the town's +0x748 handed on.
	// (inferred) before the abode's own clean-up
	if (registry.AllOf<components::Graveyard>(building))
	{
		graveyard::DeleteDependancys(building);
	}
	// (inferred, repair_spec §9) MultiMapFixed::ToBeDeleted deletes its building site (+0x74) with it
	if (const auto site = GetBuildingSite(building); site != entt::null)
	{
		building_sites::ToBeDeleted(site);
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

// ---- construction (V6, session Edificios; spec dev\documentacion\edificios\V6_spec.md §3) -----------------------

namespace
{
Abode* AbodeOf(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	return building != entt::null && registry.Valid(building) ? registry.TryGet<Abode>(building) : nullptr;
}

/// [0x8CF3FC] = 0.98: GetPercentRepairedFromWhenDamaged 0x52F010 of a built building without a FragMesh
constexpr float k_RepairedDrawFactor = 0.98f;
/// MultiMapFixed::GetPercentRepairedForNonFunctional 0x52EFC0
constexpr float k_NonFunctionalDefault = 0.75f;

/// fn_404960 0x404960 (the tail of Abode::MakeFunctional; V6_pending §5): the town's first "a centre, a storage pit and
/// a house" (Town +0x5FC, set once)
void CheckTownHasCentrePitAndHouse(entt::entity building, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.TryGet<Town>(town);
	if (t == nullptr || t->hasCentrePitAndHouse)
	{
		return;
	}
	uint32_t pits = 0;
	uint32_t centres = 0;
	uint32_t houses = 0;
	const auto count = [&](entt::entity abode) {
		const auto type = abodes::TypeOf(abode); // vt +0x8C4
		if (type == AbodeType::StoragePit)
		{
			++pits;
		}
		else if (type == AbodeType::TownCentre) // IsTownCentre vt +0x1E0 (inferred: the TownCentre class is its type)
		{
			++centres;
		}
		else if (type == AbodeType::LivingQuarters) // == 2 exactly (a windmill 0xA is not a house here)
		{
			++houses;
		}
	};
	// 0x404990..0x404A0E: the other abodes (+0x754, next +0x9C) whose GetPercentBuilt (vt +0x880) is not below 1
	for (const auto abode : town_stats::AbodesOf(town))
	{
		if (abode != building && !(abodes::GetPercentBuilt(abode) < 1.0f))
		{
			count(abode);
		}
	}
	// 0x404A10: already complete before this one
	if (centres != 0 && pits != 0 && houses != 0)
	{
		return;
	}
	// 0x404A24..0x404A59: this one, with no %built test
	count(building);
	if (centres != 0 && pits != 0 && houses != 0)
	{
		// 0x404A71..0x404A88: +0x5FC = 1; GetPlayer()->FUN_0064DA80(12, 1) returns at once outside a multiplayer game
		// (0x64DA90): nothing to call. (not ported) the readers of +0x5FC are not searched
		t->hasCentrePitAndHouse = true;
	}
}

/// The DrawMesh's generated model, 0 without one
entt::id_type DrawMeshIdOf(entt::entity building)
{
	const auto* draw = Locator::entitiesRegistry::value().TryGet<const DrawMesh>(building);
	return draw != nullptr ? draw->id : 0;
}

/// entt's on_destroy<DrawMesh>: the generated partly built model out of the mesh cache, whatever takes the DrawMesh
/// away (RedrawConstruction's Remove, Registry::Destroy, ecs::ToBeDeleted, and Registry::Reset: entt::registry::clear
/// publishes it for every element, mixin.hpp pop_all). The component is still there during the signal
void OnDrawMeshDestroyed(entt::registry& registry, entt::entity entity)
{
	// (openblack, guard) a reset after the resources have gone (shutdown)
	if (Locator::resources::has_value())
	{
		physics::PartialBuild::EraseMesh(registry.get<DrawMesh>(entity).id);
	}
}
} // namespace

bool abodes::HasDestructionMesh(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* damage = registry.Valid(building) ? registry.TryGet<const BuildingDamage>(building) : nullptr;
	return damage != nullptr && damage->mesh;
}

bool abodes::IsBuilt(entt::entity building)
{
	if (const auto* a = AbodeOf(building); a != nullptr)
	{
		// 0x4016C0: !(+0x58 & 2) && GetPercentBuilt >= 1 (fcomp; test ah, 1)
		return (a->buildFlags & Abode::k_UnderConstruction) == 0 && !(a->percentBuilt < 1.0f);
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* feature = registry.Valid(building) ? registry.TryGet<const Feature>(building) : nullptr)
	{
		return !(feature->percentBuilt < 1.0f); // Feature 0x422110 (feature_build keeps no +0x58)
	}
	// TODO(H3): CitadelPart::IsBuilt 0x464AD0 for the temple
	return true; // MultiMapFixed 0x438D80
}

bool abodes::IsRepaired(entt::entity building)
{
	if (AbodeOf(building) != nullptr)
	{
		// 0x4016A0: GetPercentRepaired (vt +0x884 = GetLife) >= 1
		return !(GetPercentRepaired(building) < 1.0f);
	}
	// TODO(H3): CitadelPart::IsRepaired 0x464AB0
	return true; // MultiMapFixed 0x438D70
}

float abodes::GetPercentBuilt(entt::entity building)
{
	if (const auto* a = AbodeOf(building); a != nullptr)
	{
		return a->percentBuilt; // 0x4014F0: +0x5C
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* feature = registry.Valid(building) ? registry.TryGet<const Feature>(building) : nullptr)
	{
		return feature->percentBuilt;
	}
	return 1.0f; // (openblack) not a MultiMapFixed with a +0x5C kept
}

float abodes::GetPercentRepaired(entt::entity building)
{
	return life::LifeOf(building); // 0x401500: GetLife (vt +0x11C)
}

float abodes::GetPercentRepairedForNonFunctional(entt::entity building)
{
	const auto* info = InfoOf(building);
	return info != nullptr ? info->thresholdForStopBeingFunctional : k_NonFunctionalDefault; // 0x407290 / 0x52EFC0
}

entt::entity abodes::GetBuildingSite(entt::entity building)
{
	const auto* a = AbodeOf(building);
	return a != nullptr ? a->buildingSite : entt::null;
}

bool abodes::IsDrawBuilding(entt::entity building)
{
	return GetBuildingSite(building) != entt::null; // 0x52F0C0: +0x74 != 0
}

float abodes::GetPercentRepairedFromWhenDamaged(entt::entity building)
{
	// 0x52F010: not built -> 1
	if (!IsBuilt(building))
	{
		return 1.0f;
	}
	const float repaired = GetPercentRepaired(building);
	// GetDestructionMesh (vt +0x8B4, Abode 0x401700 = +0x90) && +0x74
	if (const auto site = GetBuildingSite(building); site != entt::null && HasDestructionMesh(building))
	{
		const float base = building_sites::GetRepairBase(site);
		const float a = 1.0f - base;
		const float b = repaired - base;
		return (a == 0.0f || b == 0.0f) ? 0.0f : b / a;
	}
	return repaired * k_RepairedDrawFactor;
}

float abodes::GetPercentForDrawBuilding(entt::entity building)
{
	// 0x52EFD0: GetPercentBuilt <= GetPercentRepairedFromWhenDamaged ? GetPercentBuilt : it
	const float built = GetPercentBuilt(building);
	const float repaired = GetPercentRepairedFromWhenDamaged(building);
	return built <= repaired ? built : repaired;
}

const GAbodeInfo* abodes::InfoOf(entt::entity building)
{
	const auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	if (const auto i = static_cast<size_t>(static_cast<int32_t>(a->info));
	    a->info != AbodeInfo::None && i < infos.size())
	{
		return &infos.at(i);
	}
	// (inferred) as TownDesire's GatherInputs: the town's tribe, CELTIC without one
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = abode_villagers::TownOf(building);
	const auto* tribe = town != entt::null ? registry.TryGet<const Tribe>(town) : nullptr;
	return town_stats::AbodeInfoOf(building, tribe != nullptr ? *tribe : Tribe::CELTIC);
}

bool abodes::CastsShadowOnTexture(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(building))
	{
		return false;
	}
	// only the creation (0x52EA40) and Built (0x52EC2C) change the bit, never the draw: a NotDrawn built abode keeps
	// it. +0x58 bit 8: off from the ctor (0x52E24A) until Built (0x52EC3A), the same lifetime as the 0x1000 bit
	const auto* a = AbodeOf(building);
	return a == nullptr || (a->buildFlags & Abode::k_Built) != 0;
}

void abodes::BuildBy(entt::entity building, float amount)
{
	auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return; // TODO(H3): the temple's MultiMapFixed::BuildBy (CitadelHeart vt +0x900)
	}
	if (IsBuilt(building)) // vt +0x890
	{
		if (!IsRepaired(building)) // vt +0x88C
		{
			IncreaseLife(building, amount); // vt +0x5BC
			if (!(life::LifeOf(building) < 1.0f))
			{
				Repaired(building); // vt +0x8AC
			}
		}
	}
	else
	{
		// +0x5C += x; below 0 -> 0; >= 1 -> Built (vt +0x8A8)
		a->percentBuilt = a->percentBuilt + amount;
		if (a->percentBuilt < 0.0f)
		{
			a->percentBuilt = 0.0f;
		}
		if (!(a->percentBuilt < 1.0f))
		{
			Built(building);
		}
	}
	RedrawConstruction(building);
}

void abodes::SetPercentBuilt(entt::entity building, float percent)
{
	auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return;
	}
	// fn_52EDD0: +0x5C = p; p < 0 -> 0; +0x5C >= 1 -> Built
	a->percentBuilt = percent;
	if (percent < 0.0f)
	{
		a->percentBuilt = 0.0f;
	}
	if (!(a->percentBuilt < 1.0f))
	{
		Built(building);
	}
	RedrawConstruction(building);
}

bool abodes::Built(entt::entity building)
{
	auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return false;
	}
	// MultiMapFixed::Built 0x52EBB0
	// 1. +0x74 -> its ToBeDeleted(0): the builders to 163, the pile released, +0x74 = 0
	if (a->buildingSite != entt::null)
	{
		building_sites::ToBeDeleted(a->buildingSite);
		a = AbodeOf(building);
	}
	// 2. a civic building (IsCivic vt +0x8C0) not 0x804 / 0x1004 with a town: Reaction::CreateReaction(this, 0xF,
	//    GetPlayer, 0) 0x6E3D70 ("new building"). (not ported) no such reaction in openblack yet
	const auto type = TypeOf(building);
	if (type.has_value() && town_stats::IsCivic(*type) && *type != AbodeType::Citadel &&
	    *type != AbodeType::FootballPitch && abode_villagers::TownOf(building) != entt::null)
	{
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "Buildings: {} built, reaction 15 not ported", static_cast<uint32_t>(building));
		}
	}
	// 3. +0x40 && !IsField (vt +0x210): SetShadowOnTexture(1) 0x52EC2C (the built bit below: CastsShadowOnTexture) and
	// RequestChangeTexture 0x5E2FF0 (the per-block re-bake: not needed, openblack redraws the static shadows each
	// frame) 4. +0x58 = (+0x58 & ~2) | 8; +0x5C = 1.0
	a->buildFlags = (a->buildFlags & ~Abode::k_UnderConstruction) | Abode::k_Built;
	a->percentBuilt = 1.0f;
	// Abode::Built 0x404720: with a town and a player, GameStats(player +0xA44)::IncrementAllBuildingsBuilt 0x56A3F0
	// (not ported) and FUN_0064da80(IsWonder ? 4 : 3, 1) (multiplayer only); then with a town MakeFunctional (vt
	// +0x914)
	if (abode_villagers::TownOf(building) != entt::null)
	{
		MakeFunctional(building);
	}
	RedrawConstruction(building);
	return true;
}

void abodes::MakeFunctional(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* a = AbodeOf(building);
	// 1. 0x4047EA: town = GetTown(); none -> return
	const auto town = abode_villagers::TownOf(building);
	if (a == nullptr || town == entt::null)
	{
		return;
	}
	// 2. 0x4047FA..0x404812: +0x58 bit 2 = !IsRepaired()
	a->buildFlags = IsRepaired(building) ? (a->buildFlags & ~Abode::k_NotRepaired)
	                                     : (a->buildFlags | Abode::k_NotRepaired);
	// 3. 0x404818..0x40483C: +0x7C bit 1 once, Town::AddAbodeToTownStats 0x739A20 = TownStats::Add(Abode) 0x7498C0
	//    (town_stats::Compute counts the abodes with the flag from the next town turn)
	a->addedToTownStats = true;
	// 4. GetRoomLeftForAdults 0x404660 != 0 -> Town::AllVillagersCheckNeedNewAbode 0x73D150, a bare `ret` in W120
	// 5. IsRepaired && IsBuilt -> Town::RemoveBuildingSite(this) 0x73BA20 (a repair site, if any)
	if (IsRepaired(building) && IsBuilt(building))
	{
		building_sites::RemoveBuildingSite(town, building);
	}
	// 6. a storage pit that is not this one and turn > 0: a GFootpath 0x534EB0 between the two GetArrivePos,
	//    AttemptRerenderFootpathWithCreatureRP 0x5387D0, AddFootpathLink on both. TODO(footpaths): not ported
	// 7. fn_404960
	CheckTownHasCentrePitAndHouse(building, town);
	// The class's own MakeFunctional (vt +0x914): StoragePit 0x732F30, Creche 0x50AB50, Graveyard 0x595E00 and
	// TownCentre 0x743E80 call Abode::MakeFunctional first, then their part
	auto& t = registry.Get<Town>(town);
	const auto type = TypeOf(building);
	if (type == AbodeType::StoragePit)
	{
		// StoragePit::MakeFunctional 0x732F30 -> Town::SetStoragePit 0x73EA60
		town_stores::SetStoragePit(town, building);
	}
	else if (type == AbodeType::Creche)
	{
		// Creche::MakeFunctional 0x50AB50: +0x744 = this when still null (0x50AB72)
		if (t.creche == entt::null)
		{
			t.creche = building;
		}
	}
	else if (type == AbodeType::Graveyard)
	{
		// Graveyard::MakeFunctional 0x595E00: +0x748 when null (fn_73D690), then fn_595E50 (one dead counted)
		graveyard::MakeFunctional(building);
	}
	else if (type == AbodeType::TownCentre)
	{
		// TownCentre::MakeFunctional 0x743E80: CreateTotemIfNecessary 0x743E9C, +0x9A4 when null 0x743EAB, the spell
		// icons (AbodeArchetype::MakeTownCentreFunctional, in that order)
		if (const auto* info = InfoOf(building); info != nullptr)
		{
			const auto& transform = registry.Get<const Transform>(building);
			const float yAngle = map_cells::detail::YAngleOf(transform.rotation);
			archetypes::AbodeArchetype::MakeTownCentreFunctional(building, *info, yAngle, transform.scale.x);
		}
	}
}

bool abodes::Repaired(entt::entity building)
{
	auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return false;
	}
	// MultiMapFixed::Repaired 0x52EC70: +0x74 -> its ToBeDeleted(0)
	if (a->buildingSite != entt::null)
	{
		building_sites::ToBeDeleted(a->buildingSite);
		a = AbodeOf(building);
	}
	// RemoveDamage (vt +0x8B8, Abode 0x403F40). TODO(Fisicas): the FragMesh (BuildingDamage) has no "remove" API yet
	// 0x52EC8D: +0x58 &= ~4
	a->buildFlags &= ~Abode::k_NotRepaired;
	// Abode::Repaired 0x4047B0: with a town MakeFunctional
	if (abode_villagers::TownOf(building) != entt::null)
	{
		MakeFunctional(building);
	}
	RedrawConstruction(building);
	return true;
}

float abodes::IncreaseLife(entt::entity building, float amount)
{
	// 0x405ED0: wasAbove = (vt +0x894 < life); Object::IncreaseLife 0x637870 (cap 1); !wasAbove && vt +0x894 < new ->
	// RestartBeingFunctional (vt +0x91C)
	const float threshold = GetPercentRepairedForNonFunctional(building);
	const bool wasAbove = threshold < life::LifeOf(building);
	const float now = life::IncreaseLife(building, amount);
	if (!wasAbove && threshold < now)
	{
		RestartBeingFunctional(building);
	}
	return now;
}

void abodes::RestartBeingFunctional(entt::entity building)
{
	// vt +0x91C 0x401680. (pending) not read
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Buildings: {} works again", static_cast<uint32_t>(building));
	}
}

bool abodes::CausesTownEmergencyIfDamaged(entt::entity building)
{
	// Abode 0x4016F0 = 0; StoragePit 0x55CCE0 = 1; TownCentre 0x55DB30 = 1
	const auto type = TypeOf(building);
	return type == AbodeType::StoragePit || type == AbodeType::TownCentre;
}

float abodes::ReduceLife(entt::entity building, float amount, [[maybe_unused]] std::optional<PlayerNames> player)
{
	if (AbodeOf(building) == nullptr)
	{
		return life::ReduceLife(building, amount);
	}
	// Abode::ReduceLife 0x405D90: old = GetLife; wasFunctional = (vt +0x894 < old) (0x405D98..0x405DBC)
	const float threshold = GetPercentRepairedForNonFunctional(building);
	const float old = life::LifeOf(building);
	const bool wasFunctional = threshold < old;
	// MultiMapFixed::ReduceLife 0x52F5E0 (0x405DCC)
	float l = 0.0f;
	if (IsBuilt(building))
	{
		l = life::ReduceLife(building, amount); // Object::ReduceLife 0x637810
	}
	else
	{
		// p = GetPercentBuilt - amount; p > 0 ? p : 0 (test ah, 0x41); SetPercentBuilt (fn_52EDD0); p == 0 ->
		// Object::ReduceLife(GetLife(), player) (0x52F659: life 0)
		float p = GetPercentBuilt(building) - amount;
		p = p > 0.0f ? p : 0.0f;
		SetPercentBuilt(building, p);
		if (p == 0.0f)
		{
			life::ReduceLife(building, life::LifeOf(building));
		}
		l = life::LifeOf(building);
	}
	// 0x405DD5..0x405DE0
	if (!(l < 1.0f))
	{
		return l;
	}
	// 0x405DE2..0x405E05: every inhabitant (+0xA0, next +0xE4) SetStateWhenTappedOnAbode 0x752B80. (pending) Personas' V11
	// (Hito 3): villager::SetStateWhenTappedOnAbode(entt::entity) in ECS/Villager/VillagerEmergency.h, state 197
	// 0x405E07..0x405E59
	if (wasFunctional && !(threshold < l))
	{
		StopBeingFunctional(building); // vt +0x918
		if (CausesTownEmergencyIfDamaged(building))
		{
			// GetTown()->SetInStateOfEmergency 0x7479A0 (no NULL check, literal). TODO(Milagros): town_emergency
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Buildings: {} damaged, town emergency not ported",
				                   static_cast<uint32_t>(building));
			}
		}
	}
	// else if (old >= 1.0) FUN_004073F0(player) 0x4073F0: a multiplayer statistic only (V6_pending §5)
	// 0x405E5E..0x405E7A: no site and a town -> Town::AddBuildingSite(this) 0x73B8E0
	const auto town = abode_villagers::TownOf(building);
	if (GetBuildingSite(building) == entt::null && town != entt::null)
	{
		building_sites::AddBuildingSite(town, building);
	}
	// 0x405E7F..0x405EA7: a site and built -> site +0x640 = 1.1 x l - 0.1 ([0x8AB230], [0x8AB22C])
	if (const auto site = GetBuildingSite(building); site != entt::null && IsBuilt(building))
	{
		building_sites::SetRepairBase(site, 1.1f * l - 0.1f);
	}
	// 0x405EBE: l == 0 -> FUN_00405D80, `mov eax, 1; ret` (no effect)
	RedrawConstruction(building);
	return l;
}

float abodes::GetDesireToBeRepaired(entt::entity building)
{
	const auto* a = AbodeOf(building);
	const auto* info = InfoOf(building);
	if (a == nullptr || info == nullptr)
	{
		return 0.0f;
	}
	// Abode::GetDesireToBeRepaired 0x406970 -> MultiMapFixed 0x52ECE0, through TownDesire's port of both
	town_desire::RepairInput input;
	input.life = GetPercentRepaired(building);
	constexpr auto k_LivingQuarters = static_cast<uint32_t>(AbodeType::LivingQuarters);
	input.livingQuarters = (static_cast<uint32_t>(info->abodeType) & k_LivingQuarters) != 0;
	input.inhabitants = static_cast<uint32_t>(a->inhabitants.size()); // +0xA4
	input.desireToBeRepaired = info->desireToBeRepaired;              // +0x118
	return town_desire::AbodeDesireToBeRepaired(input, Locator::infoConstants::value().town);
}

void abodes::RedrawConstruction(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (AbodeOf(building) == nullptr)
	{
		return;
	}
	const auto* mesh = registry.TryGet<const Mesh>(building);
	// with a FragMesh Abode::Draw 0x515F70 is the physics' (RedrawBuilding): no construction draw of ours either
	if (!IsDrawBuilding(building) || mesh == nullptr || HasDestructionMesh(building))
	{
		// MultiMapFixed::Draw 0x518090's normal draw: the whole model (the Mesh) again (the generated one is erased by
		// OnDrawMeshDestroyed)
		registry.Remove<AbodeConstructionDraw, DrawMesh, NotDrawn>(building);
		registry.SetDirty();
		return;
	}
	// DrawBuilding 0x517F90: pct = GetPercentForDrawBuilding (vt +0x898); rebuilt only when it changed (openblack)
	const float percent = GetPercentForDrawBuilding(building);
	if (const auto* state = registry.TryGet<const AbodeConstructionDraw>(building);
	    state != nullptr && state->percent == percent)
	{
		return;
	}
	registry.AssignOrReplace<AbodeConstructionDraw>(building, percent);
	const auto old = DrawMeshIdOf(building);
	// 0x517FE0: pct != 0 -> Game3DObject vt +0x110(pct) (fn_816AD0); at 0 nothing of the building is drawn
	const entt::id_type built = percent != 0.0f
	                                ? physics::PartialBuild::BuildMesh(building, mesh->id, percent, "abode-built")
	                                : entt::id_type {0};
	if (built == 0)
	{
		registry.Remove<DrawMesh>(building); // the old model erased by OnDrawMeshDestroyed
		registry.AssignOrReplace<NotDrawn>(building);
	}
	else
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		const auto submesh = meshes.Contains(built) && meshes.Handle(built)->GetNumSubMeshes() > 1
		                         ? static_cast<int8_t>(-1)
		                         : static_cast<int8_t>(0);
		// connected once per registry (entt's sink::connect disconnects the same listener first: idempotent)
		registry.OnDestroy<DrawMesh>().connect<&OnDrawMeshDestroyed>();
		// a replace publishes no destruction: the old model is erased here
		registry.AssignOrReplace<DrawMesh>(building, built, submesh, mesh->bbSubmeshId);
		registry.Remove<NotDrawn>(building);
		if (old != 0 && old != built)
		{
			physics::PartialBuild::EraseMesh(old);
		}
	}
	registry.SetDirty();
}

std::optional<float> abodes::GetBuiltPercentage(entt::entity entity)
{
	if (AbodeOf(entity) == nullptr)
	{
		return std::nullopt;
	}
	return GetPercentBuilt(entity); // 0x70E1A9: vt +0x880
}

bool abodes::SetBuiltPercentage(entt::entity entity, float value)
{
	if (AbodeOf(entity) == nullptr)
	{
		return false;
	}
	// 0x70EC69 -> fn_0052EDD0. (pending) 0x70EC9B..0x70ECD4, the town's building list part, not read
	SetPercentBuilt(entity, value);
	return true;
}
