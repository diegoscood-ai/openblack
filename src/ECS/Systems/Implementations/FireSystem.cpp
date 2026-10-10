/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FireSystem.h"

#include <algorithm>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::fire::FireEffect;

void FireSystem::ProcessTurn()
{
	ecs::fire::ProcessList();
}

void FireSystem::Update(float seconds)
{
	ecs::fire::graphic::Update(seconds);
}

void FireSystem::Reset()
{
	// The fires go first (each takes its graphic and its crackle with it), then whatever graphics are left
	ecs::fire::Clear();
	ecs::fire::graphic::Clear();
}

void FireSystem::SetTemperature(entt::entity object, float temperature, entt::entity source)
{
	ecs::fire::SetTemperature(object, temperature, source);
}

void FireSystem::SetOnFire(entt::entity object, float speed)
{
	ecs::fire::SetOnFire(object, speed);
}

void FireSystem::PutOut(entt::entity object)
{
	const auto& transform = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object);
	ecs::fire::SetTemperature(object, ecs::fire::AmbientTemperature(transform.position), entt::null);
}

void FireSystem::HeatHeldObject(entt::entity object)
{
	ecs::fire::CheckToSeeIfObjectIsNearOnFireObject(object);
}

void FireSystem::StartedMoving(entt::entity object, bool inHand)
{
	ecs::fire::StartedMoving(object, inHand);
}

void FireSystem::SetCanBeSetOnFire(entt::entity object, bool can)
{
	ecs::fire::traits::SetCannotBeSetOnFire(object, !can);
}

void FireSystem::SetHurtByFire(entt::entity object, bool hurt)
{
	ecs::fire::traits::SetNotHurtByFire(object, !hurt);
}

float FireSystem::GetTemperature(entt::entity object) const
{
	return ecs::fire::GetTemperature(object);
}

bool FireSystem::IsOnFire(entt::entity object) const
{
	return ecs::fire::IsOnFire(object);
}

bool FireSystem::IsFireNear(const glm::vec3& point, float radius) const
{
	// The map cells of the square round the point, fixed objects then mobile ones: one on fire whose distance in metres
	// from the point is within the radius (a worship site's is its totem's). A fireball is not in the map cells.
	const auto at = map_coords::FromWorld(point);
	const auto onFireNear = [&at, radius](entt::entity object) {
		if (!ecs::fire::IsOnFire(object))
		{
			return false;
		}
		auto position = ecs::object::MapCoordsOf(object);
		if (const auto* site = Locator::entitiesRegistry::value().TryGet<const ecs::components::WorshipSite>(object);
		    site != nullptr && site->totem != entt::null && Locator::entitiesRegistry::value().Valid(site->totem))
		{
			position = ecs::object::MapCoordsOf(site->totem);
		}
		return gutils::GetDistanceInMetres(at, position) <= radius;
	};
	return ecs::map_cells::FindNearForScript(at, onFireNear, radius) != entt::null;
}

float FireSystem::GetCharring(entt::entity object) const
{
	const auto it = _byObject.find(object);
	return it != _byObject.end() ? it->second->charring : 0.0f;
}

FireEffect* FireSystem::FindByObject(entt::entity object)
{
	const auto it = _byObject.find(object);
	return it != _byObject.end() ? it->second : nullptr;
}

FireEffect* FireSystem::FindById(uint32_t id)
{
	const auto it = _byId.find(id);
	return it != _byId.end() ? it->second : nullptr;
}

uint32_t FireSystem::TakeId()
{
	return _nextId++;
}

uint8_t FireSystem::TakeCreateTag()
{
	const auto tag = _createTag;
	_createTag = 0;
	return tag;
}

uint8_t FireSystem::ProcessTag() const
{
	return _processTag;
}

void FireSystem::SetProcessTag(uint8_t tag)
{
	_processTag = tag;
}

FireEffect& FireSystem::Insert(std::unique_ptr<FireEffect> fire)
{
	auto* raw = fire.get();
	_byObject[raw->object] = raw;
	_byId[raw->id] = raw;
	_list.insert(_list.begin(), raw);
	_pool.push_back(std::move(fire));
	return *raw;
}

void FireSystem::Unlist(const FireEffect& fire)
{
	std::erase(_list, &fire);
}

void FireSystem::ForgetObject(entt::entity object)
{
	_byObject.erase(object);
}

void FireSystem::ForgetId(uint32_t id)
{
	_byId.erase(id);
}

void FireSystem::MoveObject(entt::entity from, entt::entity to, FireEffect& fire)
{
	_byObject.erase(from);
	_byObject[to] = &fire;
}

const std::vector<FireEffect*>& FireSystem::List() const
{
	return _list;
}

void FireSystem::FreeDeleted()
{
	std::erase_if(_pool, [](const std::unique_ptr<FireEffect>& fire) { return (fire->flags & FireEffect::k_Deleted) != 0; });
}

void FireSystem::ClearFires()
{
	_list.clear();
	_byObject.clear();
	_byId.clear();
	_pool.clear();
	_processTag = 0;
	_createTag = 0;
}

FireSystem::Graphics& FireSystem::AllGraphics()
{
	return _graphics;
}

void FireSystem::SetGraphic(uint32_t fire, std::unique_ptr<ecs::fire::graphic::Graphic> graphic)
{
	_graphics[fire] = std::move(graphic);
}

void FireSystem::EraseGraphic(uint32_t fire)
{
	_graphics.erase(fire);
}

void FireSystem::ClearGraphics()
{
	_graphics.clear();
}

bool FireSystem::GraphicSourceAdded() const
{
	return _graphicSourceAdded;
}

void FireSystem::SetGraphicSourceAdded()
{
	_graphicSourceAdded = true;
}

FireSystem::Slots& FireSystem::GetSlots()
{
	return _slots;
}

float FireSystem::MaxDistance() const
{
	return _maxDistance;
}

void FireSystem::SetMaxDistance(float distance)
{
	_maxDistance = distance;
}

FireSystem::Owners& FireSystem::GetOwners()
{
	return _owners;
}
