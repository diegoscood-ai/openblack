/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EffectValues.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Alignment.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Map.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/AnimalAI.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Reactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::effects;
using namespace openblack::ecs::components;

namespace
{
float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// Object::FillInEffectDefenceMultiplier (vt 0x5C8): the info's defenceMultiplier* (+0x90..; creatures 0x478C00 are
/// not ported). Without an info: 1 (inf)
std::array<float, EffectValues::_COUNT> DefenceMultipliers(entt::entity object)
{
	const auto* info = fire::traits::InfoOf(object); // the physics' infos plus abodes, fields, features, fireballs
	if (info == nullptr)
	{
		std::array<float, EffectValues::_COUNT> ones {};
		ones.fill(1.0f);
		return ones;
	}
	return {info->defenceMultiplierBurn,
	        info->defenceMultiplierCrush,
	        info->defenceMultiplierHit,
	        info->defenceMultiplierHeal,
	        info->defenceMultiplierFlyAway,
	        info->defenceMultiplierAlignmentModification,
	        info->defenceMultiplierBeliefModification};
}

/// Object::GetDamageEffect 0x637D00: the burn goes to the fire first (FireEffect::ApplyEffectToFireEffectIfNecessary
/// 0x730670, ECS/Fire), then the positive crush and hit x their multipliers
float DamageEffect(entt::entity object, const EffectValues& values)
{
	fire::ApplyEffectToFireEffectIfNecessary(object, values);
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		return 0.0f;
	}
	const auto multipliers = DefenceMultipliers(object);
	float damage = 0.0f;
	for (const size_t i : {size_t {EffectValues::Crush}, size_t {EffectValues::Hit}})
	{
		const float amount = values.numbers[i] * multipliers[i];
		if (amount > 0.0f)
		{
			damage += amount;
		}
	}
	return damage;
}

/// Object::GetHealEffect 0x637D80
float HealEffect(entt::entity object, const EffectValues& values)
{
	const float amount = values.numbers[EffectValues::Heal] * DefenceMultipliers(object)[EffectValues::Heal];
	return amount > 0.0f ? amount : 0.0f;
}

/// GetTown (vt +0x48) of an effect receiver: an abode's (abode_villagers::TownOf) and a villager's (+0x48 town).
/// (approximate) none for the other classes
entt::entity TownOfObject(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Abode>(object))
	{
		return abode_villagers::TownOf(object);
	}
	if (const auto* villager = registry.TryGet<const Villager>(object); villager != nullptr)
	{
		return registry.Valid(villager->town) ? villager->town : entt::null;
	}
	return entt::null;
}

/// GetPlayer (vt 0x1C) of an effect receiver: a villager's town's owner (Villager::GetPlayer 0x7502F0,
/// villager::GetPlayerOf); the others NEUTRAL (inf: not ported per class yet)
PlayerNames PlayerOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		return villager::GetPlayerOf(object).value_or(PlayerNames::NEUTRAL);
	}
	return PlayerNames::NEUTRAL;
}

/// Object::DestroyedByEffect (vt 0x5F8) with Object::ApplyEffect 0x637A79's player (EffectValues::GetPlayer 0x5254C0)
/// and damage ([esp + 0xC]): Villager 0x7502D0 -> VillagerDead(2 SPELL) (ECS/Villager/VillagerDeath.h); Animal
/// 0x41B1B0 -> Living::SetDying (ECS/AnimalAI). TODO(M5/M6): Abode::DestroyedByEffect 0x403F80 and the other classes.
void DestroyedByEffect(entt::entity object, const EffectValues& values, float damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		villager::DestroyedByEffect(object, values.hasPlayer ? std::optional<PlayerNames>(values.player) : std::nullopt,
		                            damage);
	}
	else if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::DestroyedByEffect(object);
	}
}
} // namespace

EffectValues EffectValues::FromEffectInfo(const GEffectInfo& info)
{
	EffectValues values;
	values.numbers = {info.effectBurn,    info.effectCrush,
	                  info.effectHit,     info.effectHeal,
	                  info.effectFlyAway, info.effectAlignmentModification,
	                  info.effectBeliefModification};
	values.radius = info.radius;
	return values;
}

void EffectValues::Scale(float factor)
{
	if (factor == 1.0f)
	{
		return;
	}
	for (auto& number : numbers)
	{
		number *= factor;
	}
}

bool EffectValues::IsDestructive() const
{
	return numbers[Burn] > 0.0f || numbers[Crush] > 0.0f || numbers[Hit] > 0.0f || numbers[FlyAway] > 0.0f;
}

bool effects::IsEffectReceiver(entt::entity object, const EffectValues& /*values*/)
{
	// Villager 0x751D70: a heal only for the living (dead villagers are gone from openblack's world); otherwise vt
	// 0x530 (Villager::IsReachable 0x756460, UNVERIFIED: taken as 1). Object 0x4029E0: 1.
	return Locator::entitiesRegistry::value().Valid(object);
}

float effects::ConvertTemperatureToDamage(entt::entity object, float temperature)
{
	const auto* info = fire::traits::InfoOf(object);
	if (info == nullptr)
	{
		return 0.0f;
	}
	// Object::GetCombustionTemperature 0x639A30
	const float combustion = info->combustionTemperature;
	if (temperature < combustion)
	{
		return 0.0f;
	}
	return (temperature - combustion) / combustion * info->defenceMultiplierBurn * 0.1f;
}

std::array<float, EffectValues::_COUNT> effects::GetDefenseMultiplier(entt::entity object)
{
	return DefenceMultipliers(object); // 0x637930: info +0x90.. (rep movsd of 7 dwords)
}

float effects::ApplyEffect(entt::entity object, EffectValues& values)
{
	const float life0 = life::LifeOf(object);
	// the class of vt 0x5BC / 0x5B8: every Abode class (houses, storage pit, town centre, creche, workshop ...)
	const bool isAbode = Locator::entitiesRegistry::value().AllOf<Abode>(object);
	const float damage = DamageEffect(object, values);
	const float heal = HealEffect(object, values);
	float result = 0.0f;
	if (heal > 0.0f)
	{
		result = (1.0f - life0) / heal;
		// vt 0x5BC: Abode::IncreaseLife 0x405ED0 (ecs::abodes: RestartBeingFunctional when it crosses the threshold);
		// Villager::IncreaseLife 0x753460 / Object 0x637870 for the rest
		if (isAbode)
		{
			abodes::IncreaseLife(object, heal);
		}
		else
		{
			life::IncreaseLife(object, heal);
		}
	}
	if (damage > 0.0f)
	{
		result += life0 / damage;
		// vt 0x5B8 with the EffectValues' player (GetPlayer 0x5254C0): Abode::ReduceLife 0x405D90 (ecs::abodes: the
		// MultiMapFixed part 0x52F5E0, the building site, StopBeingFunctional, the town's emergency) for the abodes,
		// Field::ReduceLife 0x52A0A0 (no change, no site) for the fields, which carry an Abode too: abodes::ReduceLife
		// dispatches both; Object::ReduceLife 0x637810 for the rest. (aproximado) Creature 0x47DD00 is not ported
		// (destructive.md 7.3)
		if (isAbode)
		{
			abodes::ReduceLife(object, damage, values.hasPlayer ? std::optional(values.player) : std::nullopt);
		}
		else
		{
			life::ReduceLife(object, damage);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	const bool killed = registry.Valid(object) && life::LifeOf(object) == 0.0f && life0 != 0.0f;
	// Crushed: REACT_TO_OBJECT_CRUSHED from the applier (or the object), if the object started none (Living::CanBeCrushed
	// vt 0x62C; inf: villagers)
	if (values.numbers[EffectValues::Crush] > 0.01f && registry.AllOf<Villager>(object) &&
	    reactions::GetReactionInitiatedBy(object) == 0)
	{
		const auto initiator = values.appliedBy != entt::null ? values.appliedBy : object;
		// CreateReaction(initiator, 0x12, object->GetPlayer(), 1)
		reactions::CreateReaction(initiator, Reaction::ReactToObjectCrushed, PlayerOf(object), true);
	}
	// 0x637AFE..0x637B7D: IsDestructive 0x5258C0, a town (GetTown vt +0x48), AppliedBy (+0x28, a GameThingWithPos) and
	// ConvertTemperatureToDamage(burn) + damage != 0 -> Town::UpdateAggressor(e, vt +0x588(it)) 0x73C9B0 with
	// EffectValues::GetCausedPlayer 0x525910. Only its record is ported (town_emergency::UpdateAggressor)
	if (values.IsDestructive() && registry.Valid(object) && values.appliedBy != entt::null &&
	    registry.Valid(values.appliedBy))
	{
		const float aggression = ConvertTemperatureToDamage(object, values.numbers[EffectValues::Burn]) + damage;
		if (const auto town = TownOfObject(object); town != entt::null && aggression != 0.0f)
		{
			const auto caused = values.causedPlayer.has_value()
			                        ? values.causedPlayer
			                        : (values.hasPlayer ? std::optional(values.player) : std::nullopt);
			town_emergency::UpdateAggressor(town, caused);
		}
	}
	// Whose alignment moves: the creature's (creature +0x168, M8) or the caster player's (+0x60). The per-player damage
	// statistic (+0x94[caster]) is not kept.
	// TODO(M8): Object::ApplyEffect 0x637980's creature(AppliedBy) +0x11C0 kill counter on a kill, and the creature's own
	// GAlignment (+0x168) when a creature applies the effect: today a creature-applied effect moves no alignment
	// (openblack deletes a dead villager at once, so the alignment is read before DestroyedByEffect)
	if (!values.appliedByCreature && values.hasPlayer)
	{
		alignment::Update(magic::players::AlignmentOf(values.player), object, values, life0);
	}
	if (killed)
	{
		DestroyedByEffect(object, values, damage);
	}
	return result;
}

std::vector<entt::entity> effects::FixedObjectsInMapCell(int cellX, int cellZ)
{
	std::vector<entt::entity> result;
	if (!Locator::entitiesMap::has_value() || cellX < 0 || cellZ < 0 || cellX >= MapInterface::k_GridSize.x ||
	    cellZ >= MapInterface::k_GridSize.y)
	{
		return result;
	}
	const MapInterface::CellId id(static_cast<uint16_t>(cellX), static_cast<uint16_t>(cellZ));
	const auto& grid = Locator::entitiesMap::value().GetFixedInGridCell(id);
	result.assign(grid.begin(), grid.end());
	// (aproximado, see the header) the fixed objects whose position is in the cell, which the grid may have left out
	Locator::entitiesRegistry::value().Each<const Fixed, const Transform>(
	    [&](entt::entity entity, const Fixed& /*fixed*/, const Transform& transform) {
		    if (MapInterface::GetGridCell(transform.position) == id && grid.find(entity) == grid.end())
		    {
			    result.push_back(entity);
		    }
	    });
	std::sort(result.begin(), result.end());
	return result;
}

std::vector<entt::entity> effects::ObjectsInMapCell(int cellX, int cellZ)
{
	auto result = FixedObjectsInMapCell(cellX, cellZ);
	if (!Locator::entitiesMap::has_value() || cellX < 0 || cellZ < 0 || cellX >= MapInterface::k_GridSize.x ||
	    cellZ >= MapInterface::k_GridSize.y)
	{
		return result;
	}
	const MapInterface::CellId id(static_cast<uint16_t>(cellX), static_cast<uint16_t>(cellZ));
	const auto& grid = Locator::entitiesMap::value().GetMobileInGridCell(id);
	std::vector<entt::entity> mobile(grid.begin(), grid.end());
	std::sort(mobile.begin(), mobile.end());
	result.insert(result.end(), mobile.begin(), mobile.end());
	return result;
}

entt::entity EffectValues::ApplyEffectToMapPos(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the corners: MapCoords x * 10 * (1 / 65536) -/+ radius, back with GUtils' * 65536 / 10 and __ftol
	// (0x52514F..0x5251F6); the cells are their signed high words (movsx, 0x525212) and each one is tested with
	// MapCoords::InBounds (0x525259), so a corner off the map does not wrap
	const auto corner = [](float metres, float offset) {
		return static_cast<int32_t>(map_coords::SignedCellOf(map_coords::ToFixedGUtils(map_coords::Quantise(metres) + offset)));
	};
	const glm::ivec2 low(corner(position.x, -radius), corner(position.z, -radius));
	const glm::ivec2 high(corner(position.x, radius), corner(position.z, radius));
	const float altitude = LandAt(position.x, position.z) + position.y;
	entt::entity hit = entt::null;
	for (int32_t x = low.x; x <= high.x; ++x) // cmp ax, cx; jg (0x525209)
	{
		for (int32_t z = low.y; z <= high.y; ++z)
		{
			if (!map_coords::InBounds(glm::ivec2(x, z)))
			{
				continue;
			}
			// GetFirstIterator 0x52526F: the cell's fixed list, then its mobile one (ecs::map_cells). 0x525274..0x5253BF keep
			// no "done" set and no own-cell test (fn_00604F40): a multi-cell object is offered the effect once per cell of
			// the square it is in (inferido: unless ApplyEffect vt +0x5CC limits it itself)
			for (const auto object : map_cells::ObjectsInCell(glm::ivec2(x, z)))
			{
				// 0x5252BD..0x5252C3: IsAvailable (vt +0x2C) == 1 (GameThing 0x401810: not being deleted, here a
				// valid entity; Villager 0x751D50: its final state is not DYING), then IsEffectReceiver (vt +0x774,
				// 0x5252D0); both skip to 0x5253BF
				if (!registry.Valid(object) ||
				    (registry.AllOf<Villager>(object) && !villager::IsAvailable(object)) ||
				    !IsEffectReceiver(object, *this))
				{
					continue;
				}
				const auto* transform = registry.TryGet<const Transform>(object);
				if (transform == nullptr)
				{
					continue;
				}
				// GetDefaultFireCentrePos (vt 0x5F0: the position; DeadTree its mesh centre) and GetDefaultFireRadius (vt
				// 0x5F4: Get2DRadius; DeadTree 0.35 x its height), ECS/Fire/FireObjectTraits
				const glm::vec3 centre = fire::traits::FireCentre(object);
				// 0x525307: GUtils::GetDistanceInMetres 0x74CD70 of the position and that centre, then the radius sum is
				// compared with it (fcomp; test ah, 1 at 0x525320)
				const float distance = gutils::GetDistanceInMetres(position, centre);
				if (fire::traits::DefaultFireRadius(object) + radius < distance)
				{
					continue;
				}
				// GetHeight vt +0x42C (0x52536E) + the radius against the altitude difference (0x525377)
				if (object::GetHeight(object) + radius < std::abs(altitude - (LandAt(centre.x, centre.z) + centre.y)))
				{
					continue;
				}
				// GetActualObjectToEffect (vt 0x5D8) is the object itself for Object
				ApplyEffect(object, *this);
				hit = object;
			}
		}
	}
	return hit;
}
