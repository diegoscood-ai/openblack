/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanternSounds.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::ecs::components;

namespace
{
/// The SoundTag a lantern keeps in GStreetLantern +0x60. openblack only has an emitter while the sample plays, so
/// `emitter` is null between passes; `released` is a tag whose lantern is gone (LHSampleReleaseLoop was called and it
/// is finishing the pass it was in).
struct LanternSound
{
	entt::entity lantern {entt::null};
	entt::entity emitter {entt::null};
	bool released {false};
};

/// [0xDA0A10]: dark enough for the lanterns to be heard
bool g_On = false;
std::vector<LanternSound> g_Sounds;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_LANTERN_SOUND_TRACE") != nullptr;
	return k_Trace;
}

/// GAudio::StopPlayingSoundEffect (LHSampleStop): the sample stops where it is
void HardStop(AudioManagerInterface& audio, LanternSound& tag)
{
	if (tag.emitter == entt::null)
	{
		return;
	}
	if (audio.EmitterExists(tag.emitter))
	{
		audio.StopEmitter(tag.emitter);
		audio.DestroyEmitter(tag.emitter);
	}
	tag.emitter = entt::null;
}
} // namespace

void lantern_sounds::SetOn(bool on)
{
	// fn_007349E0: only when the flag changes does it walk the lanterns
	if (on == g_On)
	{
		return;
	}
	g_On = on;
	if (on || !Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	for (auto& tag : g_Sounds)
	{
		// a released tag is no longer in the lantern list: SetActive does not reach it
		if (tag.released)
		{
			continue;
		}
		if (tag.emitter != entt::null && Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: stop (light) of lantern {}",
			                   static_cast<uint32_t>(tag.lantern));
		}
		HardStop(audio, tag);
	}
}

void lantern_sounds::ProcessTurn()
{
	if (!Locator::audio::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::camera::has_value() ||
	    !Locator::resources::has_value())
	{
		return;
	}
	const auto id = static_cast<entt::id_type>(SoundId::G_Lantern_01);
	if (!Locator::resources::value().GetSounds().Contains(id))
	{
		return;
	}
	auto& audio = Locator::audio::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& sound = audio.GetSound(id);

	// the emitters the audio manager has already destroyed, and the tags whose lantern is gone
	// (GameThing::IsAvailable false -> ToBeDeleted -> CreateSoundTagForDeadObject)
	std::erase_if(g_Sounds, [&](LanternSound& tag) {
		if (tag.emitter != entt::null && !audio.EmitterExists(tag.emitter))
		{
			tag.emitter = entt::null; // the pass ended; a later turn may start it again
		}
		if (registry.Valid(tag.lantern) && registry.AllOf<StreetLantern>(tag.lantern))
		{
			return false;
		}
		if (tag.emitter == entt::null)
		{
			return true;
		}
		if (!tag.released)
		{
			// LHSampleReleaseLoop: it stops looping and ends with this pass
			registry.Get<AudioEmitter>(tag.emitter).loop = PlayType::Once;
			tag.released = true;
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: release of the gone lantern {}",
				                   static_cast<uint32_t>(tag.lantern));
			}
		}
		return false;
	});

	// CreateEmitter makes entities, so the lanterns are read first
	std::vector<std::pair<entt::entity, glm::vec3>> lanterns;
	registry.Each<const StreetLantern, const Transform>(
	    [&lanterns](entt::entity entity, const StreetLantern& /*unused*/, const Transform& transform) {
		    // the tag's offset (0, Object::GetHeight 0x638120, 0): the top of the lantern
		    lanterns.emplace_back(entity, transform.position + glm::vec3(0.0f, ecs::Rocks::Height(entity), 0.0f));
	    });

	const auto cameraOrigin = Locator::camera::value().GetOrigin();
	float nearest = std::numeric_limits<float>::max();
	for (const auto& [entity, at] : lanterns)
	{
		auto found = std::ranges::find_if(
		    g_Sounds, [entity](const LanternSound& tag) { return tag.lantern == entity && !tag.released; });
		if (found == g_Sounds.end())
		{
			g_Sounds.push_back({entity, entt::null, false});
			found = std::prev(g_Sounds.end());
		}
		// LHSamplePlay (0x429E30): not started beyond the sample's max distance from the camera
		const float distance = glm::distance(at, cameraOrigin);
		nearest = std::min(nearest, distance);
		// SetActive(0) or play mode 2 (already playing: nothing happens)
		if (!g_On || found->emitter != entt::null)
		{
			continue;
		}
		if (distance > sound.maxDistance)
		{
			continue;
		}
		found->emitter = audio.CreateEmitter(id, PlayType::Repeat, at, glm::vec3(0.0f), glm::vec2(0.0f), sound.volume,
		                                     AudioStatus::Playing, false);
		if (found->emitter == entt::null)
		{
			continue;
		}
		registry.Get<Transform>(found->emitter).position = at;
		audio.PlayEmitter(found->emitter);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: start of lantern {} at {:.1f} (max {:.1f})",
			                   static_cast<uint32_t>(entity), distance, sound.maxDistance);
		}
	}

	static uint32_t s_Turn = 0;
	if (Trace() && !lanterns.empty() && s_Turn++ % 50 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: {} lanterns, dark {}, nearest at {:.1f} (max {:.1f})",
		                   lanterns.size(), g_On, nearest, sound.maxDistance);
	}
}

void lantern_sounds::Clear()
{
	if (Locator::audio::has_value())
	{
		auto& audio = Locator::audio::value();
		for (auto& tag : g_Sounds)
		{
			HardStop(audio, tag);
		}
	}
	g_Sounds.clear();
	g_On = false;
}
