/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalArchetype.h"

#include <algorithm>

#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/DetailMeshes.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
enum class AnimalClass
{
	None,
	Ground,
	/// made by the grazing animals' constructor 0x41D0B0: IsOkToBeShepherd (vtable +0xBA4) is 1 (0x41D0E0)
	Grazing,
	/// the Dove constructor 0x41DCF0 (doves, crows, swallows, pigeons, seagulls, bats and the spell ones)
	Flying,
};

/// fn_00419E00: the jump table on info.animalInfo (+0x1F4, 27 cases) picks each species' class factory. 5 (goat),
/// 7 (zebra), 17-19 and anything above 26 (the puzzle horse, cow, tortoise and pig) make nothing.
AnimalClass ClassOf(const GAnimalInfo& info)
{
	switch (static_cast<int>(info.animalInfo))
	{
	case 4:  // Sheep
	case 6:  // Tortoise
	case 8:  // Cow
	case 9:  // Horse
	case 10: // Pig
	case 24: // PieceSheep
		return AnimalClass::Grazing;
	case 11: // Crow
	case 12: // Dove
	case 13: // Swallow
	case 14: // Pigeon
	case 15: // Seagull
	case 16: // Bat
	case 20: // SpellDove
	case 21: // SpellDove (the spell bat)
		return AnimalClass::Flying;
	case 0:  // Lion
	case 1:  // Tiger
	case 2:  // Wolf
	case 3:  // Leopard
	case 22: // SpellWolf
	case 23: // PieceLion
	case 25: // PieceWolf
	case 26: // PieceVillager
		return AnimalClass::Ground;
	default:
		return AnimalClass::None;
	}
}

/// GameRand(range) + 5
uint32_t RandomAge(uint32_t range)
{
	return Locator::rng::value().NextValue<uint32_t>(0, range - 1) + 5;
}

/// The class factory and CallVirtualFunctionsForCreation of a ground animal
entt::entity MakeAnimal(const glm::vec3& position, AnimalInfo type, const GAnimalInfo& info, uint32_t age)
{
	auto& rng = Locator::rng::value();
	// InitialiseScale 0x417B20 / SetScaleForAge 0x417A40 (K = 0.75): young ones take their age's scale and a random part
	// of the step to the next; adults 1.05 - FloatRand(0.1)
	float scale;
	const auto& ageToScale = info.ageToScale.values;
	if (age < info.grownUpAge && age >= 1 && age + 1 < ageToScale.size())
	{
		scale = ageToScale[age - 1];
		const float step = 0.75f * (ageToScale[age + 1] - scale);
		scale += step > 0.0f ? rng.NextValue<float>(0.0f, step) : 0.0f;
	}
	else
	{
		scale = 1.05f - rng.NextValue<float>(0.0f, 0.1f);
	}

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// no start angle in the Object / Living constructors; standing on the land
	glm::vec3 ground = position;
	if (Locator::terrainSystem::has_value())
	{
		ground.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	registry.Assign<Transform>(entity, ground, glm::mat3(1.0f), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<Animal>(entity, type, age, entt::null, true);
	// Object::CallVirtualFunctionsForCreation 0x636BE0 gives the LH3DObject GetDetailMesh(2, 1, 0) (info +0x1FC + 4k:
	// high, std, low), and the LevelOfDetail loads are NOPed (always LOD 1): the std mesh, which is also GetMesh (or the
	// high one, mod graphics.hd-tweaks)
	registry.Assign<Mesh>(entity, resources::HashIdentifier(ecs::detail_meshes::Animal(info)), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	// fn_0041FD30: a predator's flee-from-predator reaction, spread once to the animals already around it
	ecs::animal_ai::SpreadPredatorReaction(entity);
	return entity;
}

/// fn_0052FA50 (without the +0xD4 ordering) and Living::SetFlock
void JoinFlock(entt::entity flockEntity, Flock& flock, entt::entity animal)
{
	flock.members.push_back(animal);
	Locator::entitiesRegistry::value().Get<Animal>(animal).flock = flockEntity;
}

/// fn_00419C20: no flock given
entt::entity CreateAlone(const glm::vec3& position, AnimalInfo type, const GAnimalInfo& info, entt::entity town,
                         uint32_t age)
{
	if (age == 0)
	{
		age = RandomAge(40);
	}
	const auto animalClass = ClassOf(info);
	if (animalClass == AnimalClass::None)
	{
		return entt::null;
	}
	if (animalClass == AnimalClass::Flying)
	{
		// The Dove constructor 0x41DCF0 sets its MapCoords altitude (+0x1C) to info.altitudeNormal, so it starts at
		// GetAltitude + altitudeNormal (Game3DObject::SetPosition 0x63B680), but its flight (the living states and the
		// flap/glide clips) isn't decoded yet: not made. It is still an Object of the creation counter.
		ecs::object_index::Skip(1);
		return entt::null;
	}
	const auto entity = MakeAnimal(position, type, info, age);
	auto& registry = Locator::entitiesRegistry::value();
	// Flock::Flock(Living*) 0x52F950 at the animal's position, with the species' domain radius and flock distance
	const auto flockEntity = registry.Create();
	auto& flock = registry.Assign<Flock>(flockEntity);
	flock.domainCentre = registry.Get<Transform>(entity).position;
	flock.savedDomainCentre = flock.domainCentre;
	flock.domainRadius = static_cast<uint16_t>(info.domainRadius);
	flock.flockDistance = static_cast<uint16_t>(static_cast<int32_t>(info.flockDistance));
	JoinFlock(flockEntity, flock, entity);
	flock.maxMembers = std::max(flock.maxMembers, static_cast<uint32_t>(flock.members.size()));
	if (animalClass == AnimalClass::Grazing)
	{
		registry.Get<Animal>(entity).town = town;
	}
	return entity;
}
} // namespace

entt::entity AnimalArchetype::Create(const glm::vec3& position, AnimalInfo type, entt::entity town, entt::entity flock,
                                     uint32_t age)
{
	if (type == AnimalInfo::None || type >= AnimalInfo::_COUNT)
	{
		return entt::null;
	}
	const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
	auto& registry = Locator::entitiesRegistry::value();
	if (flock == entt::null || !registry.Valid(flock) || !registry.AllOf<Flock>(flock))
	{
		return CreateAlone(position, type, info, town, age);
	}

	if (age == 0)
	{
		age = RandomAge(20);
	}
	const auto animalClass = ClassOf(info);
	if (animalClass == AnimalClass::None)
	{
		// (the original doesn't check the class factory's result here)
		return entt::null;
	}
	auto& flockData = registry.Get<Flock>(flock);
	const bool grazing = animalClass == AnimalClass::Grazing;
	// an animal that can't be shepherded takes the flock off its town's list
	if (!grazing && flockData.town != entt::null)
	{
		if (auto* flockTown = registry.TryGet<Town>(flockData.town))
		{
			std::erase(flockTown->flocks, flock);
		}
		flockData.town = entt::null;
	}
	if (animalClass == AnimalClass::Flying)
	{
		// see CreateAlone: the flyers are not made yet
		ecs::object_index::Skip(1);
		return entt::null;
	}
	const auto entity = MakeAnimal(position, type, info, age);
	JoinFlock(flock, flockData, entity);
	flockData.maxMembers = std::max(flockData.maxMembers, static_cast<uint32_t>(flockData.members.size()));
	if (grazing)
	{
		registry.Get<Animal>(entity).town = town;
	}
	return entity;
}

entt::entity AnimalArchetype::Create(const glm::vec3& position, AnimalInfo type, int32_t, uint32_t age)
{
	return Create(position, type, entt::null, entt::null, age);
}
