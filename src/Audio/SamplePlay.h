/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio
{
class Sound;
class SampleOutput;

/// A playing sample channel as the callers keep it: 0 = nothing plays. (GAudio::PlaySoundEffect returns nothing,
/// 0x429FE8; LHSamplePlay returns its LH_SampleInfo*, which LHAtmos, HelpDude and PSysSound keep.) A handle names one
/// start of one channel, so a handle of a channel restarted since by another sample is no longer playing.
using Channel = uint32_t;
inline constexpr Channel k_NoChannel = 0;

/// LH_SamplePlayOptions+0x20, the channel's owner (+0x18): a pointer in the original, compared as a number. 0 = none (a
/// tracked 3D channel then follows the camera), -1 = the atmos mixer's (a tracked channel is stopped), a GameThing, a
/// SoundTag (fn_0071E680 passes the tag itself to GAudio::PlaySoundEffect 0x42A100), any other Base (PSysSound, the
/// hand, the fire: their Get3DSoundPos, vtable +0x10, through fn_00427200), or a plain number used as a key (the CHL
/// sample number of PLAY_SOUND_EFFECT, 0x270C..0x270F of the voices: never tracked).
struct Owner
{
	enum class Kind : uint8_t
	{
		None,
		Atmos,
		Thing,
		SoundTag,
		Key,
		Object
	};
	Kind kind {Kind::None};
	entt::entity thing {entt::null};
	uint32_t id {0};

	[[nodiscard]] static Owner None() { return {}; }
	/// A game thing: its position through GameQueries::thingPosition, gone = GameThing::IsAvailable() == 0
	[[nodiscard]] static Owner Thing(entt::entity e) { return {e == entt::null ? Kind::None : Kind::Thing, e, 0}; }
	/// The same as Thing (the name agua's callers use)
	[[nodiscard]] static Owner Of(entt::entity e) { return Thing(e); }
	[[nodiscard]] static Owner AtmosMixer() { return {Kind::Atmos, entt::null, 0}; }
	[[nodiscard]] static Owner Tag(uint32_t tag) { return {Kind::SoundTag, entt::null, tag}; }
	[[nodiscard]] static Owner Key(uint32_t key) { return {Kind::Key, entt::null, key}; }
	/// A non-thing Base with a position: audio::RegisterObject gives it
	[[nodiscard]] static Owner Object(uint32_t object) { return {Kind::Object, entt::null, object}; }
	bool operator==(const Owner& other) const { return kind == other.kind && thing == other.thing && id == other.id; }
};

/// The owners of the voices (dev\tmp_dis\audio\script.md §3.3): HelpDude's 0x270C (fn_005C5F90), SAY_SOUND's 0x270D /
/// 0x270F (0x70F931..0x70F98F), STOP_SOUND_EFFECT's 0x270E (0x70FA50), the 2D text voice 0x270F
inline constexpr uint32_t k_OwnerAdvisor = 0x270C;
inline constexpr uint32_t k_OwnerVoiceAlt = 0x270D;
inline constexpr uint32_t k_OwnerVoiceStop = 0x270E;
inline constexpr uint32_t k_OwnerVoice = 0x270F;

} // namespace openblack::audio

namespace openblack::audio::sample_play
{

/// LHaudiodllR's sample channels (LHSamplePlay 0x100113B0: channel allocation 0x10011020 + start 0x10011420), the
/// LHaudio layer of dev\tmp_dis\audio\PLAN.md §2.1. GAudio's filters in front of it are in AudioSystem / GameSfx.
/// Research: dev\tmp_dis\agua\re\NOTES.md, dev\tmp_dis\audio\engine.md §1.6-1.10.
///
/// The audio system has 16 channels (LH_AudioSystem+0xCC, set by the GAudio ctor 0x426DF5; the "MaxSamp" override of
/// HKCU\Software\Lionhead Studios Ltd\Black & White\Audio\Override is not set by the game). A channel remembers its bank
/// (+4), its owner (+0x18), its sample (+0x1C), its clone group (+0x28, .sad +0x118) and the priority it started with
/// (+0x70, .sad +0x240). The play mode (options +0x50, 3 by default, the .sad's +0x274 with flag 0x400) picks the
/// channel:
///  - 1: a free channel;
///  - 2: nothing at all (not even a new position) while a channel of the same bank and owner plays the same sample, or
///       one of the same clone group (> 0); else a free channel;
///  - 3: the channel of the same bank, owner and sample (restarted), else one of the same clone group (> 0) (restarted),
///       else a free channel;
///  - no free channel (and any other mode): the channel with the lowest priority, if it is lower than the sample's
///    (restarted); else the sample does not play.
/// Each channel owns one OpenAL source (SampleOutput), out of the ECS registry.
using Owner = audio::Owner;

inline constexpr size_t k_Channels = 16;

/// LH_SamplePlayOptions (0x168 bytes, ctor 0x10010E90: the defaults below)
struct Options
{
	/// the bank's sample: openblack's "<bank>.sad/<id>" sound (the bank +4 and the sample number +0x24)
	entt::id_type sound {0};
	/// +0x00 AtmosInfo: set for the atmos mixer's channels (LHAtmosProcess), which LHSampleUpdate3DChannels skips and
	/// LHSampleStopAll leaves alone (0x10012C13)
	bool atmos {false};
	/// +0x08
	bool is3D {false};
	/// +0x0C (default 1): LHSampleUpdate3DChannels moves a 3D channel with its owner every turn
	bool track {true};
	/// +0x10: GAudio's variants copy their 6th argument here (0x42A06E, 0x42A121); LHSamplePlay picks the 3D flags 3
	/// instead of 0x11 with 1 (0x10012050) (unknown meaning, no effect in openblack)
	bool flag10 {false};
	/// +0x14: the position is relative to the listener (LHSamplePlay 0x10012269 turns it into polar coordinates)
	bool relative {false};
	Owner owner {};
	/// +0x28 (default 127), 0..127
	int volume {127};
	/// +0x30 and the offset +0x3C added to it (0x10012159; the 3D cull of 0x429E6F measures pos + offset too)
	glm::vec3 position {0.0f};
	glm::vec3 offset {0.0f};
	/// +0x48 (default 100): percent of the wave's rate
	int pitch {100};
	/// +0x4C: 0 once, -1 for ever, N > 0 N more passes
	int loops {0};
	/// +0x50 (default 3)
	int mode {3};
	/// +0x54 / +0x58 / +0x5C (defaults 1, 9999, 0.3): QSWaveMixSetDistanceMapping; +0x58 is also the 3D cull of
	/// GAudio::PlaySoundEffect when the .sad's max distance is 0 (0x429F0B)
	float minDistance {1.0f};
	float maxDistance {9999.0f};
	float scale {0.3f};
	/// +0x1C: the fields the caller set, which the .sad does not override (0x10011420 @0x100119D4..0x10011B84): 0x1 pitch,
	/// 0x20 volume, 0x40 loops, 0x80 min, 0x100 max, 0x200 scale, 0x400 mode
	uint32_t callerMask {0};
	/// +0x164: the DLL converts the wave to PCM and keeps it in the channel (+0x80/+0x84) for HelpDude's lip-sync
	/// (fn_10010910, 0x10011CB3 / 0x10011E25). Only HelpDude::PlaySample reads that PCM (0x5BB57B..0x5BB5C9): the advisor
	/// decodes its own copy (audio::advisor), so the flag has no effect on the channels
	bool keepPcm {false};
};

/// What the channels need from the rest of openblack. Unset members: the device output of Locator::audio, the sounds
/// of Locator::resources, Locator::rng, the camera of Locator::camera, and owners that are not moved.
struct Backend
{
	SampleOutput* output {nullptr};
	std::function<Sound*(entt::id_type)> sound;
	/// MSVC rand(), 0..32767 (the pitch deviation of a start, 0x100127B5)
	std::function<int()> rand;
	/// the listener's point (the camera), for a tracked channel without owner and the 3D culls
	std::function<std::optional<glm::vec3>()> camera;
	/// fn_00427200 for a Thing or Object owner: its Get3DSoundPos, nullopt = gone (LHSampleStop 0x1001439D); for a
	/// SoundTag owner its thing's point, nullopt = the channel keeps its point (SoundTag::Get3DSoundPos 0x71EC90)
	std::function<std::optional<glm::vec3>(const Owner&)> ownerPosition;
};

/// The backend (AudioSystem::Init, the tests)
void SetBackend(Backend backend);
/// A sample's record by its sound id, through the backend (nullptr when unknown)
[[nodiscard]] Sound* GetSound(entt::id_type sound);

/// One channel as the debug panel shows it
struct ChannelInfo
{
	Channel handle {k_NoChannel};
	entt::id_type sound {0};
	uint32_t bank {0};
	Owner owner {};
	int sample {0};
	int group {0};
	int priority {0};
	int volume {0};
	int pitch {0};
	bool is3D {false};
	bool track {false};
	bool atmos {false};
	bool playing {false};
};
[[nodiscard]] std::array<ChannelInfo, k_Channels> Channels();

/// LHSamplePlay 0x100113B0: the channel of a start, k_NoChannel when nothing started. In mode 2 with the sample already
/// playing, the playing channel (untouched).
Channel Start(const Options& options);

/// LHSampleStop 0x10012C50 with a sample: the first channel of (bank, owner, sample) stops with QMixer's 20 ms ramp to
/// 0 (0x10012D88..0x10012DCB: SampleOutput::StopRamped, the caller waits 20 ms as in the original). Nothing while the
/// audio is switched off, unless the channel is an atmos one (0x10012D5B).
void Stop(entt::id_type sound, Owner owner);
/// LHSampleStop(LH_SampleInfo*) 0x10012DF0 on a channel the caller keeps: the first channel of its (bank, owner,
/// sample) stops; nothing while switched off unless an atmos channel (0x10012E18..0x10012E22)
void Stop(Channel channel);
/// LHSampleStop with sample 0: every channel of the bank and owner, each with the 20 ms ramp (0x10012C78..0x10012D1A)
void StopOwner(uint32_t bank, Owner owner);
/// LHSampleStopAll 0x10012BF0: every channel in use that is not an atmos one (+0x00 == 0)
void StopAll();
/// LHSampleIsPlaying 0x10013ED0 (bank, owner, sample): the first channel of the three is in use
[[nodiscard]] bool IsPlaying(entt::id_type sound, Owner owner);
/// LHSampleIsPlaying 0x10013FB0 (bank, owner): the first channel of the bank and owner (any sample) is in use
/// (0x10013FF1: only that first one is looked at); nothing while switched off
[[nodiscard]] bool IsOwnerPlaying(uint32_t bank, Owner owner);
/// LHSampleGetPlayPosition 0x10014C00 (bank, owner, sample): the play position in ms of the first channel of the bank
/// and owner (the sample is not compared, 0x10014C29..0x10014C31), -1 when it is not in use or while switched off
[[nodiscard]] int64_t PlayPosition(uint32_t bank, Owner owner);
/// LHSampleGetPercentageDone 0x10015180: position / length of the first channel of (bank, owner, sample) in use, 1 for
/// none
[[nodiscard]] float PercentageDone(entt::id_type sound, Owner owner);
/// The channel of a start is still that start and in use
[[nodiscard]] bool IsPlaying(Channel channel);
/// LHSampleReleaseLoop 0x10012F20: the first channel of (bank, owner, sample) ends with its current pass (its remaining
/// loops go to 0); if that first one is not in use, nothing (no further search); nothing while switched off
void ReleaseLoop(entt::id_type sound, Owner owner);
/// LHSampleGetInfo 0x10013F60 +0x40 (GAudio's fn_0042A460): the loops of the first channel of (bank, owner, sample), in
/// use or not; 0 for none
[[nodiscard]] int Loops(entt::id_type sound, Owner owner);
/// The sound id of the channel of a start (0 when that start is no longer on its channel)
[[nodiscard]] entt::id_type SoundOf(Channel channel);
/// LH_AudioSystem::Rand(count) 0x10015710 on the channels' random generator (Backend::rand): 0..count - 1
[[nodiscard]] int Random(int count);
/// LHSampleSetPitch 0x10013520: the first channel of (bank, owner, sample) in use gets rate * percent / 100 (integers,
/// no deviation); nothing for 0, while switched off (unless an atmos channel) or when its pitch is already that
void SetPitch(entt::id_type sound, Owner owner, int percent);
/// LHSampleSetVolume 0x10013400 on a channel: the first channel of its (bank, owner, sample) gets 0..127 (QMixer's law);
/// nothing while switched off (unless an atmos channel), when not in use or when it already has that volume
void SetVolume(Channel channel, int volume);
/// LHSampleSetMasterVolume 0x100150E0: 0..127 (more is ignored, the same value too), re-applied to the channels in use
void SetMasterVolume(int master);
[[nodiscard]] int MasterVolume();

/// fn_004270D0, once per turn from GAudio::ProcessAudioGameTurn: LHSampleUpdate3DChannels 0x10014310 (every playing 3D
/// channel with +0x0C set and no AtmosInfo goes to its owner's Get3DSoundPos + offset, the camera for no owner; a gone
/// owner or the atmos one stops it, and so does a camera farther than its max distance, +0x6C = options +0x58) and then
/// LHListenerUpdate (QMixer's listener = the camera: position, forward and up, once a turn).
void UpdateChannels();

/// LHWaveSwitch 0x10015D40 (from LHGlobalSwitch 0x10015790): 0 = LHSampleStopAll and inactive (+0x14 = 0), 1 = active
/// again; nothing restarts
void Switch(bool on);
/// LHWaveIsActive 0x10015D30
[[nodiscard]] bool IsActive();
/// LHSampleClearInfoList 0x100142C0: every channel's bank, owner and sample cleared, only while active (0x100142CC: so
/// GAudio::Reset, which calls it between LHGlobalSwitch(0) and (1), clears nothing)
void ClearInfoList();
/// (openblack) every channel stopped and its OpenAL source deleted: a new map, the audio closing. The channels keep
/// their info, as in the original.
void ReleaseSources();
/// GAudio::OnThingDeleted (openblack): the channels owned by the thing see it gone at the next UpdateChannels, as
/// GameThing::IsAvailable() == 0 does in fn_00427200, even if its entity number is reused before
void OnThingDeleted(entt::entity thing);

/// Once a frame: the device output's loop counting (AudioManager::Update)
void UpdateFrame();

// ---- agua's names (inside src/Audio and the tests only: since milestone B4 the game calls audio::) -------------------
// The emitter handles are the Channel numbers as entities.

/// LHSamplePlay (Start)
entt::entity Play(const Options& options);
/// LHSampleSetVolume on a handle of Play
void SetVolume(entt::entity emitter, int volume);
/// fn_004270D0 (UpdateChannels)
void ProcessTurn();
/// SET_GAME_SOUND 0x7100B0 (audio::SetGameSound)
void SetGameSound(bool enabled);
/// HelpSystem::SetWideScreen 0x5C6AD0 (audio::SetScriptWideScreen)
void SetScriptWideScreen(bool on);
/// g_game+0x205A28 == 1 (0x4282F0, misnamed HelpSystem::GetWideScreenControl): GameQueries::insideCitadel
[[nodiscard]] bool IsInsideCitadel();
/// g_game+0x250188: GameQueries::videoPlaying
[[nodiscard]] bool IsVideoPlaying();
/// qmixer::Gain with the master 127 (the law test_audio_laws checks)
[[nodiscard]] float QMixerGain(int volume);
/// qmixer::DistanceGain
[[nodiscard]] float DistanceGain(float minDistance, float maxDistance, float scale, float distance);
/// qmixer::PolarRelative
[[nodiscard]] glm::vec3 PolarRelative(glm::vec3 position);
/// ReleaseSources
void Clear();

/// A Channel as agua's entity handle and back
[[nodiscard]] inline entt::entity AsEntity(Channel channel)
{
	return channel == k_NoChannel ? entt::entity {entt::null} : static_cast<entt::entity>(channel);
}
[[nodiscard]] inline Channel AsChannel(entt::entity emitter)
{
	return emitter == entt::null ? k_NoChannel : static_cast<Channel>(emitter);
}

} // namespace openblack::audio::sample_play
