/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CitadelLanterns.h"

#include <cstdint>

#include <vector>

#include <glm/mat3x3.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "Resources/ResourceManager.h"
#include "Temple/TempleDestruction.h"

using namespace openblack;
using namespace openblack::ecs::components;

glm::vec3 worship::citadel_lanterns::Position(const Transform& heart, size_t lamp)
{
	const auto offset = k_Offsets.at(lamp);
	const auto& right = heart.rotation[0];
	const auto& forward = heart.rotation[2];
	glm::vec3 position;
	for (int c = 0; c < 3; ++c)
	{
		const float along = offset.y * forward[c];
		const float across = offset.x * right[c];
		const float sum = along + across;
		position[c] = sum + heart.position[c];
	}
	return position;
}

namespace
{
/// The town lamp, at the point with no turn and scale 1, a model only, with its town light made with it
entt::entity CreateLamp(ecs::Registry& registry, entt::entity heart, const glm::vec3& position,
                        ecs::systems::VillageLightSystemInterface* lights)
{
	const auto lamp = registry.Create();
	registry.Assign<Transform>(lamp, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(lamp, resources::HashIdentifier(MeshId::ObjectTownLight), static_cast<int8_t>(0),
	                      static_cast<int8_t>(1));
	registry.Assign<LanternLight>(lamp, static_cast<uint8_t>(0));
	registry.Assign<CitadelLamp>(lamp, heart);
	if (lights != nullptr)
	{
		lights->AddLight(lamp, position, 0);
	}
	return lamp;
}

void SetDrawn(ecs::Registry& registry, entt::entity lamp, bool drawn)
{
	if (lamp == entt::null || !registry.Valid(lamp))
	{
		return;
	}
	if (registry.AllOf<NotDrawn>(lamp) == drawn)
	{
		if (drawn)
		{
			registry.Remove<NotDrawn>(lamp);
		}
		else
		{
			registry.Assign<NotDrawn>(lamp);
		}
	}
}
} // namespace

void worship::citadel_lanterns::Draw(ecs::Registry& registry, entt::entity heart,
                                     ecs::systems::VillageLightSystemInterface* lights)
{
	const auto entrance = registry.Get<const CitadelHeart>(heart).entrance;
	if (entrance == entt::null || !registry.Valid(entrance))
	{
		return;
	}
	// A little after the explosion of a temple being destroyed, nothing of its heart is drawn: its lamps are neither
	// made nor drawn
	if (const auto* temple = registry.TryGet<const Temple>(heart);
	    temple != nullptr && !temple_destruction::HeartDrawn(temple->destroying, temple->destructionClock))
	{
		for (const auto lamp : registry.Get<const CitadelEntrance>(entrance).lamps)
		{
			SetDrawn(registry, lamp, false);
		}
		return;
	}
	const bool lit = Lit(registry.Get<const CitadelHeart>(heart).drawPercent);
	if (lit && registry.Get<const CitadelEntrance>(entrance).lamps[0] == entt::null)
	{
		// each lamp with its light before the next is made; the heart by value, as making a lamp can move the
		// Transform storage
		const auto transform = registry.Get<const Transform>(heart);
		for (size_t i = 0; i < k_Count; ++i)
		{
			const auto lamp = CreateLamp(registry, heart, Position(transform, i), lights);
			registry.Get<CitadelEntrance>(entrance).lamps.at(i) = lamp;
		}
	}
	for (const auto lamp : registry.Get<const CitadelEntrance>(entrance).lamps)
	{
		SetDrawn(registry, lamp, lit);
	}
}

void worship::citadel_lanterns::DrawAll(ecs::Registry& registry, ecs::systems::VillageLightSystemInterface* lights)
{
	// The hearts in the order the storage gives them, newest first while none has been deleted
	std::vector<entt::entity> hearts;
	registry.Each<const CitadelHeart>([&hearts](entt::entity heart, const CitadelHeart&) { hearts.push_back(heart); });
	for (const auto heart : hearts)
	{
		Draw(registry, heart, lights);
	}
}

void worship::citadel_lanterns::Delete(ecs::Registry& registry, entt::entity heart,
                                       ecs::systems::VillageLightSystemInterface* lights)
{
	const auto entrance = registry.Get<const CitadelHeart>(heart).entrance;
	if (entrance == entt::null || !registry.Valid(entrance))
	{
		return;
	}
	for (auto& lamp : registry.Get<CitadelEntrance>(entrance).lamps)
	{
		if (lamp != entt::null && registry.Valid(lamp))
		{
			if (lights != nullptr)
			{
				lights->RemoveLight(lamp);
			}
			registry.Destroy(lamp);
		}
		lamp = entt::null;
	}
}
