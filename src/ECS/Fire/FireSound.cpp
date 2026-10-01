/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireSound.h"

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>

#include <glm/geometric.hpp>

#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FireEffect.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::fire;

namespace
{
/// LH_SAMPLE_G_FIRE_01: InGame sample 2 (the .sad gives it loops -1 and play mode 2, FLAGS 0x7E0)
constexpr int k_FireSample = 2;

struct Slot
{
	FireEffect* fire {nullptr};
	float distance {0.0f};
};
std::array<Slot, 2> g_Slots {};
float g_MaxDistance = 0.0f; // 0xDA09C8

/// The channels' owner of each fire that played (LH_SamplePlayOptions +0x20 = the FireEffect, 0x72EE3B): an
/// audio::Owner::Object number, registered for FireEffect::Get3DSoundPos 0x72EE70 until the fire goes
std::unordered_map<const FireEffect*, uint32_t> g_Owners;

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

/// FireEffect::Get3DSoundPos 0x72EE70: the object's (+0x1C) MapCoords as a point, 1; 0 without an object
std::optional<glm::vec3> Get3DSoundPos(const FireEffect& fire)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (fire.object == entt::null || !registry.Valid(fire.object))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(fire.object);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return transform->position;
}

audio::Owner OwnerOf(const FireEffect& fire)
{
	auto found = g_Owners.find(&fire);
	if (found == g_Owners.end())
	{
		found = g_Owners.emplace(&fire, audio::NewObjectId()).first;
		audio::RegisterObject(found->second, [&fire]() { return Get3DSoundPos(fire); });
	}
	return audio::Owner::Object(found->second);
}

void ForgetOwner(const FireEffect& fire)
{
	if (const auto found = g_Owners.find(&fire); found != g_Owners.end())
	{
		audio::UnregisterObject(found->second);
		g_Owners.erase(found);
	}
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

/// fn_0072EDC0: a fire whose sound plays (+0x38 & 0x20) stops it, GGlobal fn_0042A240(fire, 2, 1) ->
/// GAudio::StopPlayingSoundEffect (InGame, owner the fire, sample 2); then the bit goes
void Stop(FireEffect& fire)
{
	if ((fire.flags & FireEffect::SoundPlaying) != 0)
	{
		audio::StopSoundEffect(k_FireSample, OwnerOf(fire), audio::SfxBank::InGame);
	}
	fire.flags &= static_cast<uint8_t>(~FireEffect::SoundPlaying);
}

void Stop(Slot& slot)
{
	if (slot.fire != nullptr)
	{
		Stop(*slot.fire);
	}
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
	// fn_0072EFB0's end, 0x72F7C6..0x72F875: nearer than the farthest slot (or no slot yet), the fire takes the first
	// slot that is empty or not nearer than that one, then the farthest distance (0xDA09C8) is recomputed. As in the
	// original a fire already in a slot can take the other one too (no check there). (aproximado: the original also
	// stops the slot's sound when it is this same fire, fn_0072EDC0 at 0x72F82C; the port keeps it playing)
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
	// FireEffect::ProcessList 0x730823..0x73083E: fn_0072EDE0 for each slot's fire, every turn
	for (auto& slot : g_Slots)
	{
		if (slot.fire == nullptr)
		{
			continue;
		}
		// fn_0072EDE0: Get3DSoundPos 0x72EDF2 must answer 1
		const auto position = Get3DSoundPos(*slot.fire);
		if (!position)
		{
			continue;
		}
		// LH_SamplePlayOptions (ctor defaults): bank +0x04 GGlobal+0x3AC (InGame), owner +0x20 the fire, sample +0x24 2,
		// is3D +0x08 1, track +0x0C 1, the point +0x30; then GAudio::PlaySoundEffect 0x429E30 (0x72EE57). The .sad's
		// play mode 2 makes the later turns' calls do nothing while the loop plays.
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), k_FireSample};
		options.owner = OwnerOf(*slot.fire);
		options.is3D = true;
		options.track = true;
		options.position = *position;
		// 0x72EE1C: +0x38 |= 0x20 before the play
		slot.fire->flags |= FireEffect::SoundPlaying;
		audio::PlaySoundEffect(options);
	}
}

void sound::Free(FireEffect& fire)
{
	// FireEffect::ToBeDeleted 0x72EC79..0x72ECE4: the first slot of the fire: fn_0072EDC0, the slot emptied, the
	// farthest distance again; no slot, nothing. (openblack) a second slot holding the same fire is emptied too: the
	// original leaves it pointing at the deleted fire.
	auto found = std::ranges::find_if(g_Slots, [&fire](const Slot& slot) { return slot.fire == &fire; });
	if (found != g_Slots.end())
	{
		Stop(fire);
		for (auto& slot : g_Slots)
		{
			if (slot.fire == &fire)
			{
				slot = Slot {};
			}
		}
		RecomputeMax();
	}
	ForgetOwner(fire);
}

void sound::Clear()
{
	for (auto& slot : g_Slots)
	{
		Stop(slot);
		slot = Slot {};
	}
	for (const auto& [fire, id] : g_Owners)
	{
		audio::UnregisterObject(id);
	}
	g_Owners.clear();
	g_MaxDistance = 0.0f;
}
