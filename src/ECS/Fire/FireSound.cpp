/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireSound.h"

#include <array>

#include <glm/geometric.hpp>

#include "Audio/AudioManagerInterface.h"
#include "Audio/Sound.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FireEffect.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::fire;

namespace
{
struct Slot
{
	FireEffect* fire {nullptr};
	float distance {0.0f};
	entt::entity emitter {entt::null};
};
std::array<Slot, 2> g_Slots {};
float g_MaxDistance = 0.0f; // 0xDA09C8

/// GUtils::GetDistanceInMetres(object, camera) (x, z)
float CameraDistance(const FireEffect& fire)
{
	if (!Locator::camera::has_value())
	{
		return 0.0f;
	}
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(fire.object);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	const auto camera = Locator::camera::value().GetOrigin();
	return glm::length(glm::vec2(transform->position.x - camera.x, transform->position.z - camera.z));
}

void RecomputeMax()
{
	g_MaxDistance = 0.0f;
	for (const auto& slot : g_Slots)
	{
		if (!(slot.distance < g_MaxDistance))
		{
			g_MaxDistance = slot.distance;
		}
	}
}

/// fn_0072EDC0: the fire's sound stops (GGlobal fn_0042A240(fire, 2, 1))
void Stop(Slot& slot)
{
	if (slot.fire != nullptr)
	{
		slot.fire->flags &= static_cast<uint8_t>(~FireEffect::SoundPlaying);
	}
	if (slot.emitter != entt::null && Locator::audio::has_value())
	{
		auto& manager = Locator::audio::value();
		if (manager.EmitterExists(slot.emitter))
		{
			manager.StopEmitter(slot.emitter);
			manager.DestroyEmitter(slot.emitter);
		}
	}
	slot.emitter = entt::null;
}
} // namespace

void sound::RefreshDistances()
{
	for (auto& slot : g_Slots)
	{
		if (slot.fire != nullptr)
		{
			slot.distance = CameraDistance(*slot.fire);
		}
	}
	RecomputeMax();
}

void sound::Consider(FireEffect& fire, bool loud)
{
	if (loud)
	{
		const float distance = CameraDistance(fire);
		if (!(distance < g_MaxDistance) && g_MaxDistance != 0.0f)
		{
			return;
		}
		for (auto& slot : g_Slots)
		{
			if (slot.fire != nullptr && slot.distance < g_MaxDistance)
			{
				continue;
			}
			if (slot.fire != nullptr && slot.fire != &fire)
			{
				Stop(slot);
			}
			slot.fire = &fire;
			slot.distance = distance;
			break;
		}
		RecomputeMax();
		return;
	}
	if ((fire.flags & FireEffect::SoundPlaying) == 0)
	{
		return;
	}
	for (auto& slot : g_Slots)
	{
		if (slot.fire == &fire)
		{
			Stop(slot);
			slot = Slot {};
			RecomputeMax();
			return;
		}
	}
}

void sound::StartSlots()
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& manager = Locator::audio::value();
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& slot : g_Slots)
	{
		if (slot.fire == nullptr)
		{
			continue;
		}
		const auto* transform = registry.TryGet<const ecs::components::Transform>(slot.fire->object);
		if (transform == nullptr)
		{
			continue;
		}
		// Get3DSoundPos 0x72EE70 (the object's position); the looped sample GGlobal +0x3AC (G_Fire, inf), bank 2
		if (slot.emitter == entt::null || !manager.EmitterExists(slot.emitter))
		{
			const auto id = static_cast<entt::id_type>(audio::SoundId::G_Fire_01);
			const auto& resource = manager.GetSound(id);
			slot.emitter = manager.CreateEmitter(id, audio::PlayType::Repeat, transform->position, glm::vec3(0.0f),
			                                   glm::vec2(0.0f), resource.volume, audio::AudioStatus::Playing, false);
			if (!manager.EmitterExists(slot.emitter))
			{
				slot.emitter = entt::null;
				continue;
			}
			manager.PlayEmitter(slot.emitter);
		}
		registry.Get<ecs::components::Transform>(slot.emitter).position = transform->position;
		slot.fire->flags |= FireEffect::SoundPlaying;
	}
}

void sound::Free(FireEffect& fire)
{
	for (auto& slot : g_Slots)
	{
		if (slot.fire == &fire)
		{
			Stop(slot);
			slot = Slot {};
			RecomputeMax();
			return;
		}
	}
}

void sound::Clear()
{
	for (auto& slot : g_Slots)
	{
		Stop(slot);
		slot = Slot {};
	}
	g_MaxDistance = 0.0f;
}
