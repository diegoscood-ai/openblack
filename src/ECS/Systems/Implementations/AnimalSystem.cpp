/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AnimalSystem.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Flocks.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::Animal;
using openblack::ecs::components::AnimalBrain;
using openblack::ecs::components::Flock;
using openblack::ecs::components::Transform;

namespace
{
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

float AnimalSystem::VisualTime() const
{
	return _visualTime;
}

void AnimalSystem::SetVisualTime(float hours)
{
	_visualTime = hours;
}

uint32_t AnimalSystem::AddDeathListener(DeathCallback callback)
{
	const uint32_t id = _nextListenerId++;
	_deathListeners.emplace_back(id, std::move(callback));
	return id;
}

void AnimalSystem::RemoveDeathListener(uint32_t id)
{
	std::erase_if(_deathListeners, [id](const auto& listener) { return listener.first == id; });
}

const AnimalSystem::DeathListeners& AnimalSystem::GetDeathListeners() const
{
	return _deathListeners;
}

uint32_t AnimalSystem::SingleSlotId() const
{
	return _singleSlotId;
}

void AnimalSystem::SetSingleSlotId(uint32_t id)
{
	_singleSlotId = id;
}

void AnimalSystem::SetSpeciesDying(std::size_t species, DeathCallback dying)
{
	if (species >= _speciesDying.size())
	{
		_speciesDying.resize(species + 1);
	}
	_speciesDying[species] = std::move(dying);
}

const AnimalSystem::DeathCallback* AnimalSystem::SpeciesDying(std::size_t species) const
{
	return species < _speciesDying.size() && _speciesDying[species] ? &_speciesDying[species] : nullptr;
}

void AnimalSystem::SetScale(entt::entity animal, float scale)
{
	if (auto* transform = EntityRegistry().TryGet<Transform>(animal))
	{
		transform->scale = glm::vec3(scale);
	}
}

float AnimalSystem::RadiusOf(entt::entity animal) const
{
	// Animals have no radius of their own: the mesh's footprint, scaled
	return ecs::object::Get2DRadius(animal);
}

entt::entity AnimalSystem::LeaderOf(entt::entity flock) const
{
	return ecs::flocks::Leader(flock);
}

std::vector<entt::entity> AnimalSystem::MembersOf(entt::entity flock) const
{
	const auto& registry = EntityRegistry();
	const auto* data = registry.Valid(flock) ? registry.TryGet<const Flock>(flock) : nullptr;
	return data != nullptr ? data->members : std::vector<entt::entity> {};
}

glm::vec2 AnimalSystem::GoalOf(entt::entity animal) const
{
	// Read only: an animal that has not had its first turn has no brain yet, and asking must not make one
	const auto* brain = EntityRegistry().TryGet<const AnimalBrain>(animal);
	return brain != nullptr ? brain->goal : glm::vec2(0.0f);
}

float AnimalSystem::GoalHeightOf(entt::entity animal) const
{
	const auto* brain = EntityRegistry().TryGet<const AnimalBrain>(animal);
	return brain != nullptr ? brain->goalAltitude : 0.0f;
}

bool AnimalSystem::IsFrighteningToCreature(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	if (data == nullptr)
	{
		return false;
	}
	// Bats, the evil flock's bats and vultures frighten creatures, and so do the big cats and the wolves, which share the
	// lion's answer. The puzzle lion and wolf do not
	switch (data->type)
	{
	case AnimalInfo::Bat:
	case AnimalInfo::SpellBat:
	case AnimalInfo::Vulture:
	case AnimalInfo::Lion:
	case AnimalInfo::Tiger:
	case AnimalInfo::Leopard:
	case AnimalInfo::Wolf:
	case AnimalInfo::SpellWolf:
		return true;
	default:
		return false;
	}
}

bool AnimalSystem::CanPlayerPickUp(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	return data != nullptr && Locator::infoConstants::value().animal.at(static_cast<size_t>(data->type)).playerCanPickUp != 0;
}
