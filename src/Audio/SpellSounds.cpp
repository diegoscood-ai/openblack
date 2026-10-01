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
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "ECS/SeaCells.h"
#include "Locator.h"
#include "PSys/PSys.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// lbl_0093A460: a live looping sound is only kept going within this distance of the camera (0x6D131E)
constexpr float k_CullDistance = 1200.0f;
/// lbl_009354C4: the speed of sound for the thunder delay (units per second, 0x6747AE)
constexpr float k_SoundSpeed = 347.0f;

/// The global list 0xD4EE70
std::vector<std::shared_ptr<PSysSound>> g_Sounds;

bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_PSYS_SOUND_TRACE") != nullptr;
	return trace;
}

/// GGlobal+0x3B4: the spells bank (Audio\Sfx\Game\spells.sad, AUDIO_SFX_BANK_TYPE 3 of 0x9CB3F8)
BankId SpellsBank()
{
	return Bank(SfxBank::Spells);
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

/// The attribute array handed to the bank: {size +0x24, alignment +0x28, 1, surface +0x20, action +0x1C} (the key of
/// 0x674758..0x674783, 0x6D124A..0x6D127E and 0x6D1337..0x6D135F)
AnimKey Attributes(const psys::SoundAction& action)
{
	return {action.size, action.alignment, 1, action.surface, action.action};
}

Owner OwnerOf(const PSysSound& sound)
{
	return Owner::Object(sound.owner);
}

/// PSysSound::Get3DSoundPos 0x6D1000: with an atom, its position (+0xF4) with the land's height under it when
/// SnapToGround (+0x30 & 0x20: LH3DIsland::GetAltitude 0x6D1068); it answers 1 even without an atom, leaving the point
/// as it was (0x6D107E), so the channel keeps the last one
/// (openblack, Milagros') the original reads +0xF4 without asking whether the atom was drawn: that field is only
/// written when it is (Atom::Draw), so a never drawn atom would give it a stale point; `drawn` stands for that.
glm::vec3 PSysSoundPosition(PSysSound& sound)
{
	if (sound.atom != nullptr && sound.atom->drawn)
	{
		sound.position = sound.atom->current.position;
		if ((sound.action.flags & psys::SoundAction::SnapToGround) != 0)
		{
			sound.position.y = LandAltitude(sound.position);
		}
	}
	return sound.position;
}

/// GAudio::SamplePlayAnimEffect 0x42A4B0 with action 0 (0x674799, 0x6D137A): the owner this PSysSound, the camera
/// distance the caller measured, the key, the spells bank, track 1, min / max 0. GAudio's filters, the row's sample at
/// random, the 800 / max distance gates and the sample's play mode are audio::SamplePlayAnimEffect's.
void Play(const PSysSound& sound, float distance)
{
	const auto channel = SamplePlayAnimEffect(OwnerOf(sound), distance, Attributes(sound.action), AnimAction::Play,
	                                          SpellsBank(), true, 0.0f, 0.0f);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: start {} ({}) size {} surface {} at {:.1f} -> {}",
		                   psys::SoundActionName(sound.action.action), sound.action.action, sound.action.size,
		                   sound.action.surface, distance,
		                   channel != k_NoChannel ? fmt::format("channel {}", channel) : std::string("nothing"));
	}
}

/// The release 0x6D1262..0x6D129C: SamplePlayAnimEffect(this, 0, key, 1 + (SoftRelease ? 1 : 0), bank, 1, 0, 0), so 1
/// = LHSampleStop and 2 = LHSampleReleaseLoop of every sample of the row's list playing for this sound (0x100146F0)
void Release(const PSysSound& sound, bool soft)
{
	SamplePlayAnimEffect(OwnerOf(sound), 0.0f, Attributes(sound.action), soft ? AnimAction::Release : AnimAction::Stop,
	                     SpellsBank(), true, 0.0f, 0.0f);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} {}, atom gone", soft ? "release loop" : "stop",
		                   psys::SoundActionName(sound.action.action));
	}
}

/// The PSysSound's destructor 0x6D0FC0: out of the global list (fn_006D1110); and its owner number forgotten
void Forget(const PSysSound& sound)
{
	UnregisterObject(sound.owner);
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
	// 0x6745D8: nothing for NO_SOUND
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
	// AtomCore::CalculateGlobalPos 0x6745FA, the land's height with SnapToGround (0x6745FF..0x674648)
	glm::vec3 p = effect.GlobalPosition(atom);
	if ((action.flags & psys::SoundAction::SnapToGround) != 0)
	{
		p.y = LandAltitude(p);
	}
	// USESURFACE: GSoundMap::GetSurfaceType 0x674661 (ecs::sea_cells, the one reading of the map's cells)
	if ((action.flags & psys::SoundAction::UseSurface) != 0)
	{
		sound->action.surface = ecs::sea_cells::GetSurfaceType(p);
	}
	// the owner's alignment (GAlignment::GetDiscreteAlignmentValue 0..6 -> 1,1,2,2,2,3,3, 0x674678..0x6746B4) needs the
	// player link: the action's slot stays (no spells.sad row reads it)
	sound->atom = &atom;
	sound->position = p;
	sound->owner = NewObjectId();
	// the channels' 3D function asks the PSysSound for its point (fn_00427200 -> Get3DSoundPos 0x6D1000)
	RegisterObject(sound->owner, [raw = sound.get()]() -> std::optional<glm::vec3> { return PSysSoundPosition(*raw); });
	atom.sounds.insert(atom.sounds.begin(), sound);
	g_Sounds.push_back(sound);
	// fn_00442D50: the camera's distance to the point
	const float distance = CameraDistance(p);
	// 0x674752: the Delayed flag waits distance / 347 s (0x6747AA)
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
	// fn_006D11A0, each PSysSound of the global list
	for (auto it = g_Sounds.begin(); it != g_Sounds.end();)
	{
		auto& sound = **it;
		PSysSoundPosition(sound);
		if (sound.atom == nullptr)
		{
			// 0x6D120A: LHSampleIsPlaying(spells bank, this, &info), straight to the DLL; none -> the sound is deleted
			// (vtable +4, 0x6D12B5)
			const auto channel = PlayingChannel(OwnerOf(sound), SpellsBank());
			if (channel == k_NoChannel)
			{
				if (Trace())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} deleted", psys::SoundActionName(sound.action.action));
				}
				Forget(sound);
				it = g_Sounds.erase(it);
				continue;
			}
			// 0x6D121C..0x6D1239: with a FadeStep (+0x2C), LHSampleSetVolume(info, max(info +0x38 - FadeStep, 0)) on that
			// first channel of the owner
			if (sound.action.fadeStep != 0)
			{
				SetVolume(channel, std::max(Volume(channel) - sound.action.fadeStep, 0));
			}
			// 0x6D123F..0x6D129C: the release once (+0x3C)
			if (!sound.released)
			{
				Release(sound, (sound.action.flags & psys::SoundAction::SoftRelease) != 0);
				sound.released = true;
			}
			++it;
			continue;
		}
		// 0x6D12BD: Looping (+0x30 & 1) plays again; Delayed (& 2) with a delay left counts it down and plays once it
		// goes below 0 (0x6D12CC..0x6D12F5)
		bool play = (sound.action.flags & psys::SoundAction::Looping) != 0;
		if (!play && (sound.action.flags & psys::SoundAction::Delayed) != 0 && sound.delay > 0.0f)
		{
			sound.delay -= turnSeconds;
			play = sound.delay < 0.0f;
		}
		if (play)
		{
			// 0x6D1304..0x6D1333: Get3DSoundPos, GCamera::GetDistanceSq < 1200 * 1200, then the square root
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
	// (openblack) a new map: GAudio::Reset 0x426CA0 stops every channel already; the sounds are forgotten
	for (const auto& sound : g_Sounds)
	{
		sound->atom = nullptr;
		StopSoundEffect(0, OwnerOf(*sound), SpellsBank());
		Forget(*sound);
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
