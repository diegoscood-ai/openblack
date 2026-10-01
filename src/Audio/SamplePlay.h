/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio::sample_play
{

/// LHaudiodllR's sample channels (LHSamplePlay 0x100113B0: channel allocation 0x10011020 + start 0x10011420) and
/// GAudio's filters in front of it (PlaySoundEffect 0x429E30, SamplePlayAnimEffect 0x42A4B0), with QMixer's volume and
/// distance laws. Research: dev\tmp_dis\agua\re\NOTES.md.
///
/// The audio system has 16 channels (LH_AudioSystem+0xCC, ctor 0x1001535E; the "MaxSamp" override of
/// HKCU\Software\Lionhead Studios Ltd\Audio\Override is not set by the game). A channel remembers its bank (+4), its
/// owner (+0x18, the options' attached object), its sample (+0x1C), its clone group (+0x28, .sad +0x118) and the
/// priority it started with (+0x70, .sad +0x240). The play mode (options +0x50, 3 by default, the .sad's +0x274 with
/// flag 0x400) picks the channel:
///  - 1: a free channel;
///  - 2: nothing at all (not even a new position) while a channel of the same bank and owner plays the same sample, or
///       one of the same clone group (> 0); else a free channel;
///  - 3: the channel of the same bank, owner and sample (restarted), else one of the same clone group (> 0) (restarted),
///       else a free channel;
///  - no free channel (and any other mode): the channel with the lowest priority, if it is lower than the sample's
///    (restarted); else the sample does not play.
/// Only the samples started through this module count towards the 16 channels [openblack: music and the few 2D
/// sounds of AudioManager::PlaySound have their own sources].

/// LH_SamplePlayOptions+0x20: the attached object. 0 = none (a tracked 3D channel then follows the camera), -1 = the
/// atmos mixer's (a tracked channel is stopped), otherwise a game thing, or a SoundTag (fn_0071E680 passes the tag
/// itself as the Base* of GAudio::PlaySoundEffect 0x42A100, which stores it at +0x20 = GAudio+0x260).
struct Owner
{
	enum class Kind : uint8_t
	{
		None,
		Atmos,
		Thing,
		SoundTag
	};
	Kind kind {Kind::None};
	entt::entity thing {entt::null};
	uint32_t tag {0};

	[[nodiscard]] static Owner Of(entt::entity e) { return {e == entt::null ? Kind::None : Kind::Thing, e, 0}; }
	[[nodiscard]] static Owner AtmosMixer() { return {Kind::Atmos, entt::null, 0}; }
	[[nodiscard]] static Owner Tag(uint32_t id) { return {Kind::SoundTag, entt::null, id}; }
	bool operator==(const Owner& other) const { return kind == other.kind && thing == other.thing && tag == other.tag; }
};

/// LH_SamplePlayOptions (0x168 bytes, ctor 0x10010E90), the fields openblack uses
struct Options
{
	/// the bank's sample: openblack's "<bank>.sad/<id>" sound (the bank +4 and the sample number +0x24)
	entt::id_type sound {0};
	/// +0x00 AtmosInfo: set for the atmos mixer's channels (LHAtmosProcess), which LHSampleUpdate3DChannels skips
	bool atmos {false};
	/// +0x08
	bool is3D {false};
	/// +0x0C (default 1): LHSampleUpdate3DChannels moves a 3D channel with its owner every turn
	bool track {true};
	/// +0x14: the position is relative to the listener (LHSamplePlay 0x10012269 turns it into polar coordinates)
	bool relative {false};
	Owner owner {};
	/// +0x28 (default 127), 0..127
	int volume {127};
	/// +0x30 (+ the offset +0x3C, always 0 here)
	glm::vec3 position {0.0f};
	/// +0x4C: 0 once, -1 for ever
	int loops {0};
	/// +0x50 (default 3)
	int mode {3};
	/// +0x1C: the fields the caller set, which the .sad does not override: 0x20 volume, 0x40 loops, 0x400 mode
	/// (0x10011420 @0x100119D4..0x10011B84). The pitch (+0x48) is always the default 100 here, so the .sad's (flag 0x1)
	/// applies.
	uint32_t callerMask {0};
};

/// LHSamplePlay 0x100113B0. The channel's emitter, entt::null when nothing plays. In mode 2 with the sample already
/// playing, the playing channel's emitter (untouched).
entt::entity Play(const Options& options);

/// GAudio::PlaySoundEffect 0x429E30: a 3D sample is not started when the camera is farther than the sample's max
/// distance (.sad +0x26C raw, LHSampleGetMaxDistance; the options' 9999 when that is 0). Then, by the sample's user
/// parameter (.sad +0x25C >> 16): none of kind 1 while a script holds the widescreen (HelpSystem +0x45E8 and the task
/// +0x45EC), only kind 2 inside the citadel (g_game+0x205A28 == 1), none of kind 4 in the interface states 0x10, 0x16,
/// 0x17 [not in openblack]; only the dialogue banks HelpSprites / Villagers after SET_GAME_SOUND false (GScript+0x90);
/// not a tracked 3D sample of a thing that is not available inside the citadel.
entt::entity PlaySoundEffect(const Options& options);

/// GAudio::SamplePlayAnimEffect 0x42A4B0 -> LHSamplePlayAnimEffect 0x10014A20 with a sample number: the same filters
/// (not the camera test), then not started farther than the sample's max distance (.sad +0x26C raw) or the global 800
/// (LHSampleRegister3DObjectFunction, 0x426E6B); options: is3D 1, track = `track`, owner = `thing`, at the thing's
/// Get3DSoundPos (0x427200: its position, the camera for no thing).
entt::entity PlayAnimEffect(entt::id_type sound, entt::entity thing, glm::vec3 position, bool track);

/// LHSampleSetPitch 0x10013520: the channel of (bank, owner, sample) gets rate * percent / 100 (integers, no deviation);
/// nothing when its pitch is already that
void SetPitch(entt::id_type sound, Owner owner, int percent);

/// LHSampleSetVolume 0x10013400 on a channel's emitter (0..127, QMixer's law)
void SetVolume(entt::entity emitter, int volume);

/// LHSampleStop 0x10012C50: the channel of (bank, owner, sample)
void Stop(entt::id_type sound, Owner owner);

/// LHSampleIsPlaying (GAudio fn_0042A2D0): a channel of (bank, owner, sample) is in use
[[nodiscard]] bool IsPlaying(entt::id_type sound, Owner owner);

/// LHSampleReleaseLoop (GAudio 0x42A310): the looping channel of (bank, owner, sample) ends with its current pass
void ReleaseLoop(entt::id_type sound, Owner owner);

/// fn_004270D0, once per turn from GAudio::ProcessAudioGameTurn: LHSampleUpdate3DChannels 0x10014310 (every playing 3D
/// channel with +0x0C set and no AtmosInfo goes to its owner's Get3DSoundPos + offset, the camera for no owner; a gone
/// owner or the atmos one stops it, and so does a camera farther than its max distance, +0x6C = options +0x58) and then
/// LHListenerUpdate (QMixer's listener = the camera: position, forward and up, once a turn).
void ProcessTurn();

/// SET_GAME_SOUND 0x7100B0: false stops every sample (fn_004287D0 = LHSampleStopAll) and from then on only the dialogue
/// banks play (GScript+0x90 = 1); true lets every bank play again
void SetGameSound(bool enabled);

/// HelpSystem::SetWideScreen 0x5C6AD0 from a script (GScript::SetWideScreen 0x6F7BF0 passes the task number, +0x45EC):
/// while on, PlaySoundEffect skips the samples of user parameter 1
void SetScriptWideScreen(bool on);

/// g_game+0x205A28 == 1 (GoInsideCitadel 0x554004; the symbol HelpSystem::GetWideScreenControl 0x4282F0 is this test):
/// openblack has no citadel interior yet, so false
[[nodiscard]] bool IsInsideCitadel();

/// g_game+0x250188 (the video object of fn_0054AB20 / DeleteVideo): openblack plays no in-game video, so false
[[nodiscard]] bool IsVideoPlaying();

/// QMixer's gain of an LHaudio volume v (0..127): LHSampleSetVolume sends floor(master * v / 127) * 258 to
/// QSWaveMixSetVolume (0..32766, master = LHSampleSetMasterVolume, BWSetup AudioSampleMasterVolume = 127), which keeps
/// it as vol / 32767 (0x18007AE5) and multiplies it with the distance gain (0x1800AE20)
[[nodiscard]] float QMixerGain(int volume);

/// QMixer's distance gain (0x1800ACDF..0x1800AE1A, channel flags 0x103 / 0x111 of 0x10012065: neither 0x800 "clamp
/// at max" nor 0x1000 "linear"; fn 0x1802CE50): 1 up to min or with scale 0, min / (min + scale (d - min)) up to max,
/// 0 beyond max
[[nodiscard]] float DistanceGain(float minDistance, float maxDistance, float scale, float distance);

/// The listener-space point QMixer hears a relative LHaudio position at (0x10012269: LHaudio's (x, y, z) -> azimuth
/// atan2(x, y) and elevation atan(z / |(x, y)|) in degrees with pi taken as 1 / 0.318471, range |(x, y, z)|;
/// QSWaveMixSetPolarPosition -> QMixer 0x1800AA85: right = r cos(el) sin(az), up = r sin(el), ahead = r cos(el) cos(az)).
/// So LHaudio's relative x is right, y ahead and z up. Returns (right, up, ahead).
[[nodiscard]] glm::vec3 PolarRelative(glm::vec3 position);

/// Every channel forgotten (a new map: the registry reset destroys the emitters)
void Clear();

} // namespace openblack::audio::sample_play
