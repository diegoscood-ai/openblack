/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireObjectTraits.h"

#include <algorithm>
#include <unordered_set>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/Objects/MagicTree.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
std::unordered_set<entt::entity> g_CannotBeSetOnFire;
std::unordered_set<entt::entity> g_NotHurtByFire;

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The abode's own GAbodeInfo: its abode number and mesh (GAbodeInfo::Find would give the tribeless records), else the
/// first of that number
const GAbodeInfo* AbodeInfoOf(const Abode& abode, entt::id_type mesh)
{
	const GAbodeInfo* first = nullptr;
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
		if (first == nullptr)
		{
			first = &info;
		}
	}
	return first;
}
} // namespace

const GObjectInfo* fire::traits::InfoOf(entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return nullptr;
	}
	const auto& constants = Locator::infoConstants::value();
	if (const auto* fireBall = registry.TryGet<const MagicFireBall>(object))
	{
		return &constants.magicFireBall.at(static_cast<size_t>(fireBall->infoRow));
	}
	if (const auto* info = physics::PhysicsObjects::ObjectInfo(object))
	{
		return info;
	}
	// a field is an Abode with its GFieldTypeInfo (the 6 types have the same values)
	if (registry.AllOf<Field>(object))
	{
		return &constants.fieldType.at(0);
	}
	if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		const auto* mesh = registry.TryGet<const Mesh>(object);
		return AbodeInfoOf(*abode, mesh != nullptr ? mesh->id : 0);
	}
	if (const auto* feature = registry.TryGet<const Feature>(object))
	{
		return &constants.feature.at(static_cast<size_t>(feature->type));
	}
	return nullptr;
}

float fire::traits::CombustionTemperature(entt::entity object)
{
	const auto* info = InfoOf(object);
	return info != nullptr ? info->combustionTemperature : 0.0f;
}

float fire::traits::HeatCapacity(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* info = InfoOf(object);
	if (info == nullptr)
	{
		return 0.0f;
	}
	if (const auto* fireBall = registry.TryGet<const MagicFireBall>(object))
	{
		// MagicFireBall::GetHeatCapacity 0x682D40: r^2 x 0.0625 x info.heatCapacity x the effect's strength (mgr +0x54)
		const float r = Radius(object);
		static_cast<void>(fireBall);
		return r * r * 0.0625f * info->heatCapacity * magic::fireball::Strength(object);
	}
	return info->heatCapacity;
}

float fire::traits::DefenceMultiplierBurn(entt::entity object)
{
	const auto* info = InfoOf(object);
	return info != nullptr ? info->defenceMultiplierBurn : 0.0f;
}

float fire::traits::BurningPriority(entt::entity object)
{
	const auto* info = InfoOf(object);
	return info != nullptr ? info->burningPriority : 0.0f;
}

glm::vec3 fire::traits::FireCentre(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	// MapCoords(pos): x, z and the height above the land (Object +0x1C)
	const float height = transform->position.y - LandAt(transform->position.x, transform->position.z);
	if (registry.AllOf<DeadTree>(object) && Locator::resources::has_value())
	{
		// DeadTree 0x510CE0: the world matrix x the mesh centre (L3D +0x18) for x, z; the altitude it computes is then
		// overwritten by the object's own height (+0x1C)
		if (const auto* mesh = registry.TryGet<const Mesh>(object))
		{
			auto& meshes = Locator::resources::value().GetMeshes();
			if (meshes.Contains(mesh->id))
			{
				const glm::vec3 centre = meshes.Handle(mesh->id)->GetBoundingBox().Center();
				const glm::vec3 world = transform->position + transform->rotation * (centre * transform->scale);
				return {world.x, height, world.z};
			}
		}
	}
	return {transform->position.x, height, transform->position.z};
}

float fire::traits::DefaultFireRadius(entt::entity object)
{
	if (Locator::entitiesRegistry::value().AllOf<DeadTree>(object))
	{
		return 0.35f * Height(object); // DeadTree::GetDefaultFireRadius 0x510E10
	}
	return Radius(object);
}

float fire::traits::Height(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MagicFireBall>(object))
	{
		// MagicFireBall::GetHeight: GetScale() x 1.0
		const auto* transform = registry.TryGet<const Transform>(object);
		return transform != nullptr ? transform->scale.x : 0.0f;
	}
	return effects::ObjectHeight(object);
}

float fire::traits::Radius(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MagicFireBall>(object))
	{
		// MagicFireBall::Get2DRadius: GetScale() x 1.0
		const auto* transform = registry.TryGet<const Transform>(object);
		return transform != nullptr ? transform->scale.x : 0.0f;
	}
	return effects::Object2DRadius(object);
}

float fire::traits::RainCoolingMultiplier(entt::entity object)
{
	if (const auto* fireBall = Locator::entitiesRegistry::value().TryGet<const MagicFireBall>(object))
	{
		return fireBall->affectedByRain ? 0.01f : 0.0f; // 0x682DB0: +0x58 ? 0.01 : 0
	}
	return 0.01f; // Object 0x639AB0
}

bool fire::traits::IsAvailable(entt::entity object)
{
	return Locator::entitiesRegistry::value().Valid(object);
}

bool fire::traits::IsObjectInMap(entt::entity object)
{
	return !Locator::entitiesRegistry::value().AllOf<MagicFireBall>(object);
}

bool fire::traits::IsVillager(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Villager>(object);
}

bool fire::traits::IsCreature([[maybe_unused]] entt::entity object)
{
	return false; // no creature yet (M8)
}

bool fire::traits::InHand(entt::entity object)
{
	if (!Locator::handSystem::has_value())
	{
		return false;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	return held.has_value() && *held == object;
}

bool fire::traits::IsBurnReceiver(entt::entity object, float burn)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	// Pot::IsEffectReceiver 0x66D650: a pot with something in it (+0x70)
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->amount != 0;
	}
	// Field::IsEffectReceiver 0x528900: a burn only while ValidForPlaceInHand (vt 0x6FC; inf: while it has food to give)
	if (const auto* field = registry.TryGet<const Field>(object))
	{
		return burn <= 0.0f || field->food > 0.0f;
	}
	// Villager 0x751D70 refuses only a heal when dead (dead villagers are gone from openblack's world)
	return true;
}

bool fire::traits::CannotBeSetOnFire(entt::entity object)
{
	return g_CannotBeSetOnFire.contains(object);
}

void fire::traits::SetCannotBeSetOnFire(entt::entity object, bool value)
{
	if (value)
	{
		g_CannotBeSetOnFire.insert(object);
	}
	else
	{
		g_CannotBeSetOnFire.erase(object);
	}
}

bool fire::traits::NotHurtByFire(entt::entity object)
{
	return g_NotHurtByFire.contains(object);
}

void fire::traits::SetNotHurtByFire(entt::entity object, bool value)
{
	if (value)
	{
		g_NotHurtByFire.insert(object);
	}
	else
	{
		g_NotHurtByFire.erase(object);
	}
}

float fire::traits::ReduceLifeDueToBurning(entt::entity object, float damage, [[maybe_unused]] bool hasPlayer,
                                           [[maybe_unused]] PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Field>(object))
	{
		// Field 0x52A050: RemoveFood(damage x GFieldTypeInfo +0x130 = totalFoodInField), life stays 1
		RemoveFieldFood(object, damage * Locator::infoConstants::value().fieldType.at(0).totalFoodInField);
		return 1.0f;
	}
	// Object 0x637C20: not with +0x0A bit 2. TODO(belief): the town's aggressor (Town::UpdateAggressor with
	// EffectValues(BURN, T, 0, 1, player) and GetAggressorValueFromDamage(damage))
	if (NotHurtByFire(object))
	{
		return life::LifeOf(object);
	}
	return life::ReduceLife(object, damage);
}

void fire::traits::DestroyedByEffect(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (registry.AllOf<Villager>(object))
	{
		life::Kill(object, "burnt"); // Villager 0x7502D0 -> VillagerDead (reason 2); no corpse yet
		return;
	}
	if (registry.AllOf<Animal>(object))
	{
		life::Kill(object, "burnt"); // Animal 0x41B1B0 (the dying states belong to the animal AI)
		return;
	}
	if (registry.AllOf<Field>(object))
	{
		// Field 0x52A010 (fn_0052A030): crops, growth and food to 0, and the temperature back to ambient
		auto& field = registry.Get<Field>(object);
		field.crops = 0;
		field.growth = 0.0f;
		field.food = 0.0f;
		return;
	}
	if (registry.AnyOf<Abode, Feature>(object))
	{
		// TODO(M5): Abode::DestroyedByEffect 0x403F80 (ghost, building site, SetLife(1)); the abode stays
		return;
	}
	// Object::DestroyedByEffect 0x6378E0 = ToBeDeleted (trees, dead trees, piles, mobile objects)
	physics::PhysicsObjects::RemoveObject(object);
	g_CannotBeSetOnFire.erase(object);
	g_NotHurtByFire.erase(object);
	registry.Destroy(object);
	registry.SetDirty();
}

void fire::traits::StartOnFire(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// MultiMapFixed 0x52EC60, DeadTree 0x510E20, Pot 0x66D6C0 (RemoveReaction): the object's reactions go (Tree and the
	// living ones don't override it)
	if (registry.AnyOf<Abode, Feature, DeadTree, Pot>(object))
	{
		effects::reactions::RemoveAllReactionsInitiatedByObject(object);
	}
	// MagicTree 0x5FD0D0: its REACT_TO_MAGIC_TREE goes (Magic/Objects/MagicTree)
	if (registry.AllOf<MagicTree>(object))
	{
		magic::magic_tree::StartOnFire(object);
	}
}

void fire::traits::EndOnFire(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// DeadTree 0x510E60: while available, REACT_TO_WOOD (0xC) with its player
	if (registry.Valid(object) && registry.AllOf<DeadTree>(object))
	{
		effects::reactions::CreateReaction(object, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	}
	// TODO(M3): Pot::EndOnFire 0x66D6D0 re-creates the pot's reaction (fn_0066D660, info +0x128)
	// MagicTree 0x5FD0E0: REACT_TO_MAGIC_TREE again
	if (registry.Valid(object) && registry.AllOf<MagicTree>(object))
	{
		magic::magic_tree::EndOnFire(object);
	}
}

void fire::traits::Clear()
{
	g_CannotBeSetOnFire.clear();
	g_NotHurtByFire.clear();
}
