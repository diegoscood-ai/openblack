/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSounds.h"

#include <cstdlib>

#include <algorithm>
#include <array>
#include <memory>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/AnimEffectBank.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/SoundMap.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// lbl_0093A460: a live looping sound is only kept going within this distance of the camera
constexpr float k_CullDistance = 1200.0f;
/// lbl_009354C4: the speed of sound for the thunder delay (units per second)
constexpr float k_SoundSpeed = 347.0f;

/// The global list 0xD4EE70
std::vector<std::shared_ptr<PSysSound>> g_Sounds;

bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_PSYS_SOUND_TRACE") != nullptr;
	return trace;
}

/// GGlobal+0x3B4: the spells bank (Audio\Sfx\Game\spells.sad)
const AnimEffectBank& Bank()
{
	static AnimEffectBank bank {"spells.sad", {}, {}, {}};
	static const bool loaded = [] {
		if (Locator::filesystem::has_value())
		{
			try
			{
				auto& fileSystem = Locator::filesystem::value();
				bank.Load(fileSystem.GetPath<filesystem::Path::Audio>() / "Sfx" / "Game" / "spells.sad");
			}
			catch (const std::exception& e)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("audio"), "PSys sound: cannot read spells.sad: {}", e.what());
			}
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "PSys sound: spells.sad {} effect rows", bank.rows.size());
		return true;
	}();
	static_cast<void>(loaded);
	return bank;
}

float CameraDistance(glm::vec3 position)
{
	return Locator::camera::has_value() ? glm::distance(Locator::camera::value().GetOrigin(), position) : 1e9f;
}

float LandAltitude(glm::vec3 position)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                                           : position.y;
}

/// The attribute array handed to the bank: {size, alignment, 1, surface, action}
std::array<int32_t, 5> Attributes(const psys::SoundAction& action)
{
	return {action.size, action.alignment, 1, action.surface, action.action};
}

bool EmitterPlaying(entt::entity emitter)
{
	if (!Locator::audio::has_value())
	{
		return false;
	}
	auto& audio = Locator::audio::value();
	return audio.EmitterExists(emitter) && audio.GetStatus(emitter) != AudioStatus::Stopped;
}

/// LHSampleIsPlaying(bank, obj): any of its channels still playing
bool IsPlaying(PSysSound& sound)
{
	std::erase_if(sound.emitters, [](entt::entity emitter) { return !EmitterPlaying(emitter); });
	return !sound.emitters.empty();
}

/// GAudio::SamplePlayAnimEffect 0x42A4B0 with mode 0 -> LHSamplePlayAnimEffect (LHaudiodllR 0x100146F0): the row's list,
/// one sample at random, not beyond the sample's max distance; the sample's play mode 2 does nothing if it already plays
/// for this sound. The game-state filters of 0x42A4B0 (help and cinema modes, the citadel interior) are not ported.
void Play(PSysSound& sound, float distance)
{
	if (!Locator::audio::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& bank = Bank();
	const auto list = bank.FindList(Attributes(sound.action));
	if (list.empty())
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} ({}) size {} surface {}: no spells.sad row",
			                   psys::SoundActionName(sound.action.action), sound.action.action, sound.action.size,
			                   sound.action.surface);
		}
		return;
	}
	const auto sample = list.size() == 1 ? list[0] : list[Locator::rng::value().NextValue<size_t>(0, list.size() - 1)];
	const auto* info = bank.FindSample(sample);
	const auto id = bank.SoundId(sample);
	if (info == nullptr || !Locator::resources::value().GetSounds().Contains(id))
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} -> spells.sad/{} not loaded",
			                   psys::SoundActionName(sound.action.action), sample);
		}
		return;
	}
	if (distance > info->maxDistance)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} -> spells.sad/{} too far ({:.1f} > {:.0f})",
			                   psys::SoundActionName(sound.action.action), sample, distance, info->maxDistance);
		}
		return;
	}
	auto& audio = Locator::audio::value();
	auto& registry = Locator::entitiesRegistry::value();
	const bool same = std::ranges::any_of(sound.emitters, [&](entt::entity emitter) {
		return EmitterPlaying(emitter) && registry.Get<const ecs::components::AudioEmitter>(emitter).soundId == id;
	});
	if (same && info->playMode == 2)
	{
		return;
	}
	if (same && info->playMode == 3)
	{
		for (const auto emitter : sound.emitters)
		{
			if (EmitterPlaying(emitter) && registry.Get<const ecs::components::AudioEmitter>(emitter).soundId == id)
			{
				audio.StopEmitter(emitter);
			}
		}
	}
	const auto& resource = audio.GetSound(id);
	const auto type = info->loops == -1 ? PlayType::Repeat : PlayType::Once;
	const auto emitter = audio.CreateEmitter(id, type, sound.position, glm::vec3(0.0f), glm::vec2(0.0f), resource.volume,
	                                         AudioStatus::Playing, false);
	if (!audio.EmitterExists(emitter))
	{
		return;
	}
	registry.Get<ecs::components::Transform>(emitter).position = sound.position;
	audio.PlayEmitter(emitter);
	sound.emitters.push_back(emitter);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: start {} ({}) size {} surface {} at {:.1f} -> spells.sad/{} ({}){}",
		                   psys::SoundActionName(sound.action.action), sound.action.action, sound.action.size,
		                   sound.action.surface, distance, sample, resource.name, type == PlayType::Repeat ? " looping" : "");
	}
}

/// The same call with mode 1 (LHSampleStop) or 2 (LHSampleReleaseLoop): every sample of the row's list playing for
/// this sound
void Release(PSysSound& sound, bool soft)
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& bank = Bank();
	const auto list = bank.FindList(Attributes(sound.action));
	for (const auto emitter : sound.emitters)
	{
		if (!EmitterPlaying(emitter))
		{
			continue;
		}
		auto& component = registry.Get<ecs::components::AudioEmitter>(emitter);
		if (std::ranges::none_of(list, [&](int32_t sample) { return bank.SoundId(sample) == component.soundId; }))
		{
			continue;
		}
		if (soft)
		{
			component.loop = PlayType::Once; // the loop plays to its end
		}
		else
		{
			audio.StopEmitter(emitter);
		}
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} {} ({}), atom gone",
			                   soft ? "release loop" : "stop", psys::SoundActionName(sound.action.action),
			                   audio.GetSound(component.soundId).name);
		}
	}
}

void FollowAtom(PSysSound& sound)
{
	if (sound.atom != nullptr && sound.atom->drawn)
	{
		sound.position = sound.atom->current.position;
		if ((sound.action.flags & psys::SoundAction::SnapToGround) != 0)
		{
			sound.position.y = LandAltitude(sound.position);
		}
	}
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto emitter : sound.emitters)
	{
		if (registry.Valid(emitter))
		{
			if (auto* transform = registry.TryGet<ecs::components::Transform>(emitter); transform != nullptr)
			{
				transform->position = sound.position;
			}
		}
	}
}
} // namespace

openblack::psys::Atom::~Atom()
{
	for (const auto& sound : sounds)
	{
		sound->atom = nullptr;
	}
}

void spell_sounds::StartSound(const psys::Effect& effect, psys::Atom& atom, const psys::SoundAction& action)
{
	if (action.action == -1)
	{
		return;
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} StartSound {} ({}) flags {:#x}", effect.GetFile().name,
		                   psys::SoundActionName(action.action), action.action, action.flags);
	}
	auto sound = std::make_shared<PSysSound>();
	sound->action = action;
	glm::vec3 p = effect.GlobalPosition(atom);
	if ((action.flags & psys::SoundAction::SnapToGround) != 0)
	{
		p.y = LandAltitude(p);
	}
	if ((action.flags & psys::SoundAction::UseSurface) != 0)
	{
		sound->action.surface = GetSurfaceType(p);
	}
	// the owner's alignment (GAlignment::GetDiscreteAlignmentValue 0..6 -> 1,1,2,2,2,3,3) needs the player link: the
	// action's slot stays (no spells.sad row reads it)
	sound->atom = &atom;
	sound->position = p;
	atom.sounds.insert(atom.sounds.begin(), sound);
	g_Sounds.push_back(sound);
	const float distance = CameraDistance(p);
	if ((action.flags & psys::SoundAction::Delayed) != 0)
	{
		sound->delay = distance / k_SoundSpeed;
		return;
	}
	Play(*sound, distance);
}

void spell_sounds::StopSound(psys::Atom& atom, PSysSound& sound)
{
	sound.atom = nullptr;
	std::erase_if(atom.sounds, [&sound](const std::shared_ptr<PSysSound>& s) { return s.get() == &sound; });
}

void spell_sounds::StopAllSounds(psys::Atom& atom)
{
	for (const auto& sound : atom.sounds)
	{
		sound->atom = nullptr;
	}
	atom.sounds.clear();
}

PSysSound* spell_sounds::GetSoundOfAction(const psys::Atom& atom, int32_t action)
{
	for (const auto& sound : atom.sounds)
	{
		if (sound->action.action == action)
		{
			return sound.get();
		}
	}
	return nullptr;
}

void spell_sounds::ProcessTurn(float turnSeconds)
{
	for (auto it = g_Sounds.begin(); it != g_Sounds.end();)
	{
		auto& sound = **it;
		FollowAtom(sound);
		if (sound.atom == nullptr)
		{
			if (!IsPlaying(sound))
			{
				if (Trace())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} deleted", psys::SoundActionName(sound.action.action));
				}
				it = g_Sounds.erase(it);
				continue;
			}
			if (sound.action.fadeStep != 0)
			{
				// LHSampleSetVolume(vol - FadeStep, not below 0): the volume in 0..127 units. (inferido) the LH volume
				// 0..127 mapped onto the emitter's 0..1
				auto& registry = Locator::entitiesRegistry::value();
				for (const auto emitter : sound.emitters)
				{
					auto& component = registry.Get<ecs::components::AudioEmitter>(emitter);
					component.volume = std::max(component.volume - static_cast<float>(sound.action.fadeStep) / 127.0f, 0.0f);
				}
			}
			if (!sound.released)
			{
				Release(sound, (sound.action.flags & psys::SoundAction::SoftRelease) != 0);
				sound.released = true;
			}
			++it;
			continue;
		}
		bool play = (sound.action.flags & psys::SoundAction::Looping) != 0;
		if (!play && (sound.action.flags & psys::SoundAction::Delayed) != 0 && sound.delay > 0.0f)
		{
			sound.delay -= turnSeconds;
			play = sound.delay < 0.0f;
		}
		if (play)
		{
			const float distance = CameraDistance(sound.position);
			if (distance < k_CullDistance)
			{
				Play(sound, distance);
			}
		}
		++it;
	}
}

void spell_sounds::Clear()
{
	for (const auto& sound : g_Sounds)
	{
		sound->atom = nullptr;
		if (Locator::audio::has_value())
		{
			for (const auto emitter : sound->emitters)
			{
				if (EmitterPlaying(emitter))
				{
					Locator::audio::value().StopEmitter(emitter);
				}
			}
		}
	}
	g_Sounds.clear();
}

size_t spell_sounds::Count()
{
	return g_Sounds.size();
}

// (inferido) SizeFromRadius / SizeFromImpactSpeed: strict "<" at the thresholds; the compares of 0x69F410 and
// fn_006A1630 were not noted
int32_t spell_sounds::SizeFromRadius(float radius, float small, float medium)
{
	if (radius < small)
	{
		return 3;
	}
	return radius < medium ? 2 : 1;
}

int32_t spell_sounds::SizeFromThrow(float fraction)
{
	// doubles 0x8CF7D8 = 0.6 and 0x9375E8 = 0.3
	if (fraction > 0.6f)
	{
		return 1;
	}
	return fraction > 0.3f ? 2 : 3;
}

int32_t spell_sounds::SizeFromImpactSpeed(float speed, float medium, float large)
{
	if (speed < medium)
	{
		return 3;
	}
	return speed < large ? 2 : 1;
}
