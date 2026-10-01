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

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Alignment.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/AnimalAI.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Reactions.h"
#include "Resources/ResourcesInterface.h"

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

/// GetPlayer (vt 0x1C) of an effect receiver: a villager's town's owner (Town +0x2C); the others NEUTRAL (inf: not
/// ported per class yet)
PlayerNames PlayerOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const Villager>(object); villager != nullptr && registry.Valid(villager->town))
	{
		if (const auto* town = registry.TryGet<const Town>(villager->town); town != nullptr)
		{
			return town->owner;
		}
	}
	return PlayerNames::NEUTRAL;
}

/// Object::DestroyedByEffect (vt 0x5F8): Villager -> its death (no corpse or death states yet: life::Kill); Animal
/// 0x41B1B0 -> Living::SetDying (ECS/AnimalAI). TODO(M5/M6): Abode::DestroyedByEffect 0x403F80 and the other classes.
void DestroyedByEffect(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		life::Kill(object, "spell effect");
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

float effects::ObjectHeight(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	// 2 x the mesh's half height (+0x28) x the scale (inf: the bounding box height)
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.y;
}

float effects::Object2DRadius(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	// (inf) half the larger horizontal side of the bounding box, as the hand's hold radius does
	const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size();
	return 0.5f * std::max(size.x * transform->scale.x, size.z * transform->scale.z);
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

float effects::ApplyEffect(entt::entity object, EffectValues& values)
{
	const float life0 = life::LifeOf(object);
	const float damage = DamageEffect(object, values);
	const float heal = HealEffect(object, values);
	float result = 0.0f;
	if (heal > 0.0f)
	{
		result = (1.0f - life0) / heal;
		life::IncreaseLife(object, heal); // vt 0x5BC (Villager::IncreaseLife 0x753460)
	}
	if (damage > 0.0f)
	{
		result += life0 / damage;
		// vt 0x5B8. (aproximado) Object::ReduceLife 0x637810 for every class: MultiMapFixed 0x52F5E0 (the building-damage
		// path), Abode 0x405D90 (the repair site) and Creature 0x47DD00 are not ported (destructive.md 7.3)
		life::ReduceLife(object, damage);
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
	// TODO(belief): the town's aggressor (Town::UpdateAggressor 0x73C9B0) with ConvertTemperatureToDamage(burn) + damage
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
		DestroyedByEffect(object);
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
	if (!Locator::entitiesMap::has_value())
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto low = MapInterface::GetGridCell(glm::vec2(position.x - radius, position.z - radius));
	const auto high = MapInterface::GetGridCell(glm::vec2(position.x + radius, position.z + radius));
	const float altitude = LandAt(position.x, position.z) + position.y;
	entt::entity hit = entt::null;
	std::unordered_set<entt::entity> seen;
	for (uint32_t x = low.x; x <= high.x; ++x)
	{
		for (uint32_t z = low.y; z <= high.y; ++z)
		{
			const MapInterface::CellId cell(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			std::vector<entt::entity> objects(map.GetMobileInGridCell(cell).begin(), map.GetMobileInGridCell(cell).end());
			// the grid's fixed objects plus the small ones it leaves out (FixedObjectsInMapCell)
			const auto fixed = FixedObjectsInMapCell(static_cast<int>(x), static_cast<int>(z));
			objects.insert(objects.end(), fixed.begin(), fixed.end());
			for (const auto object : objects)
			{
				// (inferido) once per object: 0x525100 was not read for a per-object "done" flag; openblack's grid puts a
				// fixed object in every cell it touches, so without this it would be hit once per cell
				if (!seen.insert(object).second || !registry.Valid(object) || !IsEffectReceiver(object, *this))
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
				const float distance = glm::length(glm::vec2(centre.x - position.x, centre.z - position.z));
				if (fire::traits::DefaultFireRadius(object) + radius < distance)
				{
					continue;
				}
				if (ObjectHeight(object) + radius < std::abs(altitude - (LandAt(centre.x, centre.z) + centre.y)))
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
