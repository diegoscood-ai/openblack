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
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/AnimalAI.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "FireEffect.h"
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
/// first of that number. (inferido: the original reads the info pointer Object +0x28; openblack's abode has none, so
/// the match by number and mesh, and the fallback, are the port's)
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

const GAbodeInfo* fire::traits::AbodeInfo(entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.TryGet<const Abode>(object);
	if (abode == nullptr)
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(object);
	return AbodeInfoOf(*abode, mesh != nullptr ? mesh->id : 0);
}

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
	// a field is an Abode with its GFieldTypeInfo; row 0 for all (the 6 rows are identical:
	// tmp_dis/field/field_notes.txt and miracles/infodump/info_dump.txt fieldType[0..5])
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
	if (registry.AllOf<WorshipSite>(object))
	{
		// WorshipSite::GetDefaultFireCentrePos 0x77DDE0 (vt +0x5F0) = CalculateCentrePos 0x77DD40, made a MapCoords by
		// 0x603160 -> Set 0x603340: the altitude is y - GetAltitude 0x803090 (0x603371..0x60337C)
		const glm::vec3 centre = object::WorshipSiteCentre(object);
		return {centre.x, centre.y - LandAt(centre.x, centre.z), centre.z};
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
	return object::GetDefaultFireRadius(object); // vt 0x5F4: Object 0x639AC0, DeadTree 0x510E10, WorshipSite 0x77DE10
}

float fire::traits::Height(entt::entity object)
{
	return object::GetHeight(object); // vt 0x42C, with MagicFireBall 0x682D30
}

float fire::traits::Radius(entt::entity object)
{
	return object::GetRadius(object); // vt 0x60 -> 0x64, with Field / FishFarm / PileFood / MagicFireBall 0x682D20
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

bool fire::traits::IsMultiMapFixed(entt::entity object)
{
	// the class ecs::map_cells inserts with MultiMapFixed::InsertMapObject 0x52E650 (or FishFarm's 0x52CA10): one
	// list of classes for both. bw1-decomp src/Black: WorshipSite and CitadelHeart (openblack's Temple entity) are
	// CitadelParts, AnimatedStatic is a Feature, DeadTree and Fragment are Rocks (: MobileStatic). GFootpath is a
	// GameThing, not a MultiMapFixed. (aproximado) The other CitadelParts, PFootball and PrayerSite have no component of
	// their own in openblack yet
	return ecs::map_cells::IsMultiMapFixedClass(object);
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
	// Villager 0x751D70 refuses only a heal when dead (inferido: dead villagers are gone from openblack's world, so that
	// branch is not ported)
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
		// Field 0x52A050: RemoveFood(damage x GFieldTypeInfo +0x130 = totalFoodInField), life stays 1 (row 0: the 6 rows
		// are identical, see InfoOf)
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
		ecs::animal_ai::DestroyedByEffect(object); // Animal 0x41B1B0 -> Living::SetDying (ECS/AnimalAI)
		return;
	}
	if (registry.AllOf<Field>(object))
	{
		// Field 0x52A010: fn_0052A030 sets crops, growth and food to 0 and Object::SetTemperature(0, null) (0x52A046 ->
		// FireEffect::SetTemperature 0x72EF10: the fire's T = 0); then the fire (+0x44) is deleted (vt 0xC, 0x52A024)
		auto& field = registry.Get<Field>(object);
		field.crops = 0;
		field.growth = 0.0f;
		field.food = 0.0f;
		if (auto* effect = fire::Find(object))
		{
			effect->temperature = 0.0f;
			fire::ToBeDeleted(*effect);
		}
		return;
	}
	if (registry.AllOf<Abode>(object))
	{
		// TODO(M5): Abode::DestroyedByEffect 0x403F80 (ghost, building site, SetLife(1)); the abode stays
		return;
	}
	// Object::DestroyedByEffect 0x6378E0 = ToBeDeleted (trees, dead trees, piles, mobile objects, features: Feature has
	// no override, vt_fire_overrides2.txt)
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
	// living ones don't override it). vt_fire_overrides2.txt: 0x52EC60 is also Rock, MobileStatic, AnimatedStatic,
	// Fragment, Field, FishFarm, BigForest, the citadel and worship classes, spell icons, etc.; 0x66D6C0 also the piles
	// and magic food/wood (openblack's Pot). The classes ported here: Abode (with Field), Feature, MobileStatic (rocks),
	// AnimatedStatic, Fragment, DeadTree, Pot; the rest (no portado)
	if (registry.AnyOf<Abode, Field, Feature, MobileStatic, AnimatedStatic, Fragment, DeadTree, Pot>(object))
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
	// DeadTree 0x510E60: while available (vt 0x2C), REACT_TO_WOOD (0xC) with its GetPlayer (vt 0x1C, 0x510E72) and 0.
	// (inferido: a dead tree has no owner in openblack: neutral)
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
