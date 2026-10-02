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

#include <array>
#include <functional>
#include <optional>
#include <string_view>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Audio/GAudio/AudioSystem.h"
#include "Audio/GAudio/BankTables.h"
#include "Audio/LH/AnimEffects.h"
#include "Audio/LH/SamplePlay.h"
#include "Audio/Services/Advisor.h"
#include "Audio/Services/Voices.h"

// The public audio API of openblack (layer 4 of dev\tmp_dis\audio\PLAN.md §2.1, §2.3 with the design fixes of §8.6):
// the game includes only this header. The names follow GAudio (runblack.exe); every function cites its original. No
// argument has a default: each caller passes what the original caller passes. Milestones B1..B7
// (docs/bw1-notes/audio.md). The script's
// sound effects (B6: PLAY / STOP_SOUND_EFFECT, GAME_SOUND_PLAYING, ATTACH / DETACH_SOUND_TAG) are in ScriptSound.h.
//
// Rules for the callers (PLAN §2.1, §8.6):
//  - nobody outside src/Audio calls OpenAL; since B5 every sample plays on the 16 channels through this header (the old
//    emitters of AudioManager, CreateEmitter / PlayEmitter / PlaySound / PlayMusic, and the AudioEmitter component are
//    gone), and since B11a there is one engine: one device (Device.h, the only caller of OpenAL), one bank loader
//    (Banks.h), no AudioManager and no Locator::audio;
//  - the audio includes no ECS component: the positions of the owners come from GameQueries (things) and from
//    RegisterObject (other objects). It does use the team's plain maths and clocks: ecs::map_coords (MapCoords.h),
//    gutils (GUtilsDistance.h), game_clock (GameClock.h) and sky_type (3D/SkyType.h).

namespace openblack::ecs::map_coords
{
struct MapCoords;
}

namespace openblack::audio
{
struct GameQueries;

// ---- banks and samples (0x9CB3F8) -----------------------------------------------------------------------------------
// SfxBank (BankTables.h) is AUDIO_SFX_BANK_TYPE; BankId any registered bank (AudioSystem.h): Bank(SfxBank),
// FindBank(path), SampleId(bank, number).

/// A sample: a bank and its 1-based number (.sad +0x104), LH_SAMPLE
struct Sample
{
	BankId bank {k_NoBank};
	int number {0};
};

/// The bank of a creature species, LH3DCreature::LoadBinary 0x4EBD81 ("audio\sfx\creature\%s.sad", registered
/// with GAudio::RegisterBank fn_00428620 into +0x5288): k_NoBank when the species has no bank
[[nodiscard]] BankId CreatureBank(std::string_view species);

/// GAudio::GetGSFXSampleMaxDistance 0x42A430: LHSampleGetMaxDistance (the .sad +0x26C raw), 0 for no bank
[[nodiscard]] float MaxDistance(Sample sample);

// ---- owners --------------------------------------------------------------------------------------------------------
// Owner, k_OwnerAdvisor .. k_OwnerVoice (SamplePlay.h).

/// The Get3DSoundPos of an owner that is not a game thing (vtable +0x10 through fn_00427200: PSysSound, the hand, the
/// fire...): nullopt = gone (a tracked channel stops)
using ObjectPositionFn = std::function<std::optional<glm::vec3>()>;
/// Owner::Object(id) gets its position from `position` (UpdateChannels, once a turn)
void RegisterObject(uint32_t id, ObjectPositionFn position);
void UnregisterObject(uint32_t id);
/// (openblack) A new id for Owner::Object, never given before: the original compares the owners' pointers (PSysSound,
/// FireEffect, PHandFX, the gesture's atom data...), openblack gives each such object a number of its own
/// An id whose channels are never tracked (track 0: the PHandFX, the FireGraphic's steam, the gesture's atom data)
/// needs no RegisterObject: UpdateChannels only asks the tracked ones for their point.
[[nodiscard]] uint32_t NewObjectId();

// ---- GAudio::PlaySoundEffect and its family (0x429D60..0x42A100) ---------------------------------------------------

/// LH_SamplePlayOptions of a sample of a bank (the options variant of GAudio::PlaySoundEffect 0x429E30: GGuidance,
/// PSysSound and SoundTag fill their own)
struct PlayOptions: sample_play::Options
{
	Sample sample;
};
/// GAudio::PlaySoundEffect(LH_SamplePlayOptions*) 0x429E30 (the filters are documented in AudioSystem.h). Returns the
/// channel (the original returns nothing).
Channel PlaySoundEffect(const PlayOptions& options);

/// GAudio::PlaySoundEffect(Base*, sample, mode, loops, +0x10, is3D, AUDIO_SFX_BANK_TYPE) 0x429D60 -> 0x429DA0: a 3D one
/// without owner or with an unavailable owner plays nothing (0x429DB5..0x429DCC); at the owner's Get3DSoundPos, else at
/// (0, 0, 0) for a 2D one; then 0x42A040
Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool flag10, bool is3D, SfxBank bank);
/// 0x429DA0, with a bank
Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool flag10, bool is3D, BankId bank);
/// GAudio::PlaySoundEffect(Base*, LHPoint&, sample, mode, loops, +0x10, is3D, AUDIO_SFX_BANK_TYPE) 0x42A000 -> 0x42A040:
/// nothing for sample 0 (0x42A04C); GAudio's options +0x240 get bank +0x04, owner +0x20, sample +0x24, +0x10, is3D
/// +0x08 and track +0x0C = is3D, the point +0x30, no offset, loops +0x4C, mode +0x50; a 3D one of an unavailable owner
/// plays nothing (0x42A0D0); then 0x429E30
Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool flag10, bool is3D,
                          SfxBank bank);
/// 0x42A040, with a bank
Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool flag10, bool is3D,
                          BankId bank);
/// GAudio::PlaySoundEffect(Base*, LHPoint& pos, LHPoint& offset, sample, track, mode, loops, +0x10, is3D, bank) 0x42A100
/// (SoundTag fn_0071E680): like 0x42A040 with the offset +0x3C and track +0x0C (its low byte, 0x42A194) of its own; the
/// unavailable owner test only for is3D and track (0x42A19A..0x42A1B4). (The 8th argument is +0x10 and the 7th the
/// loops: 0x42A121 / 0x42A17A, engine.md §1.6 had them as one.)
Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track, int mode, int loops,
                          bool flag10, bool is3D, BankId bank);

/// GAudio::StopPlayingSoundEffect(sample, owner, type) 0x42A210 -> LHSampleStop(bank, owner, sample) 0x10012C50: the
/// first channel of the three; sample 0 = every channel of the owner in the bank
void StopSoundEffect(int sample, Owner owner, SfxBank bank);
void StopSoundEffect(int sample, Owner owner, BankId bank);
/// fn_004287D0 -> LHSampleStopAll 0x10012BF0 (not the atmos channels)
void StopAllSoundEffects();
/// fn_0042A330 (owner, sample, type) / fn_0042A310 (owner, sample, bank) -> LHSampleReleaseLoop 0x10012F20
void ReleaseLoop(Owner owner, int sample, SfxBank bank);
void ReleaseLoop(Owner owner, int sample, BankId bank);
/// fn_0042A280 (owner, sample, type) / fn_0042A2D0 (owner, sample, bank) -> LHSampleIsPlaying 0x10013ED0
[[nodiscard]] bool IsPlaying(Owner owner, int sample, SfxBank bank);
[[nodiscard]] bool IsPlaying(Owner owner, int sample, BankId bank);
/// fn_0042A2B0 (owner, type) -> LHSampleIsPlaying(bank, owner) 0x10013FB0: any sample of the owner
[[nodiscard]] bool IsPlaying(Owner owner, SfxBank bank);
/// fn_00428740 (bank, owner, sample, pitch) -> LHSampleSetPitch 0x10013520
void SetPitch(BankId bank, Owner owner, int sample, int percent);
/// LHSampleSetVolume 0x10013400 on a channel the caller keeps (LHAtmos, PSysSound fn_006D1110 0x6D1239)
void SetVolume(Channel channel, int volume);
/// LHSampleIsPlaying(LH_SampleInfo*) 0x10014070 on a channel the caller keeps
[[nodiscard]] bool IsPlaying(Channel channel);
/// LHSampleIsPlaying(bank, owner, LH_SampleInfo**) 0x10014010, called straight by PSysSound fn_006D11A0 (0x6D120A): the
/// first channel of the bank and owner (any sample) when it is in use, else k_NoChannel (also while switched off)
[[nodiscard]] Channel PlayingChannel(Owner owner, BankId bank);
/// LH_SampleInfo +0x38: the volume 0..127 of a channel the caller got (PSysSound's fade 0x6D1223), 0 for none
[[nodiscard]] int Volume(Channel channel);

/// The global cyclic counters some callers add to a first sample (sfx_inventory.md "contador cíclico"): the sample is
/// base + counter, then the counter goes up and back to 0 at its count (Abode::InterfaceTap 0x4068F4..0x40690F and the
/// other sites); the tree mulch adds first and masks with 3 (0x63AA39)
enum class Counter : uint8_t
{
	KnockRoof,          ///< [0xC4CC7C] 0..8: G_KnockRoofMulti 110 + c (Abode::InterfaceTap 0x40694A)
	CitadelSparkEffect, ///< [0xC5E3E4] 0..4: G_CitadelSpark 206 + c (fn_004686B0 0x468815)
	CitadelSparkDamage, ///< [0xC5E3E8] 0..4: G_CitadelSpark 206 + c (CitadelHeart::ProcessTransferedDamageEffect 0x468B32)
	CreatureRockTap,    ///< [0xC6421C] 0..3: G_RockTap 139 + c (fn_0048BCC0 0x48C433)
	CreatureSquash,     ///< [0xC64220] 0..2: G_SquashAnimal 143 + c (fn_0048BCC0 0x48C510)
	HandInWater,        ///< [0xD18228] 0..9: G_HandInWater 99 + c (fn_005D1AB0 0x5D2167)
	TreeMulch,          ///< [0xD4437C] (c + 1) & 3: G_TreeMulch 155 + c (Object::DoDeleteObjectAndTakeResource 0x63AA93)
	RockTap,            ///< [0xD559AC] 0..3: G_RockTap 130 + c (Rock::InterfaceTap 0x6E751D)
	ScaffoldCombine,    ///< [0xD95AF8] 0..3: G_ScaffoldCombine 201 + c (Scaffold::ApplyThisToObject 0x6E9AC9)
	ScaffoldTap,        ///< [0xD95AFC] 0..3: G_ScaffoldTap 151 + c (Scaffold::InterfaceTap 0x6E9E7E)

	_Count
};
/// The counter's value to add to the first sample, advancing it
[[nodiscard]] int NextCounter(Counter counter);

/// GetTickCount() of the original (milliseconds of the process's clock, wrapping at 2^32): the callers that pick a sample
/// with it instead of a random generator (Tree::DropSfx 0x74BCB6: 83 + t % 3, Tree::ApplyWaterSpell 0x74C4B3: 120 + t % 9,
/// PhysicsObject::GameTurnUpdate 0x645BF8: 69 + t % 5, CameraModeNew3::FlyToPosFoc 0x458986: 46 + (t & 3), fn_0066D1A0)
[[nodiscard]] uint32_t TickCount();

/// OPENBLACK_SFX_TRACE=1: one "SFX:" line in the log for each call of the family above, of SamplePlayAnimEffect and of
/// the tags' plays (bank / sample, 2D or 3D, the point, the mode and pitch it starts with, the owner and what came of it)
[[nodiscard]] bool SfxTrace();

// ---- the script's switches and GAudio's state filters ---------------------------------------------------------------
// SetGameSound (SET_GAME_SOUND 0x7100B0), SetScriptWideScreen (HelpSystem::SetWideScreen 0x5C6AD0), IsInsideCitadel
// (0x4282F0), IsVideoPlaying (g_game+0x250188): AudioSystem.h. The citadel and the interface states come from
// GameQueries::insideCitadel / interfaceState.

// ---- life cycle (GGame / GAudio) -------------------------------------------------------------------------------------

/// The GAudio ctor 0x426D40 (after the 11 banks of 0x9CB3F8, which Game registers as it reads the .sad): the sample
/// master volume of the configuration (fn_00428250: BWSetup AudioSampleMasterVolume), the queries, the channels' backend
void Init(GameQueries queries);
/// GAudio::ToBeDeleted 0x426FE0: fn_004282B0 would save the master volumes (openblack has no settings file for them
/// yet); the channels stop and their sources go before the OpenAL context
void Shutdown();
/// GGame::EndTurn 0x54E960 unpaused: GSoundMap::Update 0x71D6F0, SoundTag::ProcessSoundTags 0x71E5F0 (the street
/// lanterns' too), then after turn 5 (g_game+0x205A40 > 5, 0x54E997) GAudio::ProcessAudioGameTurn 0x427080, else
/// AtmosProcess(0) 0x4286C0. ProcessAudioGameTurn, only while LHWaveIsActive (0x427086): ProcessMusic 0x427DF0, the atmos
/// targets fn_00429100 and ProcessAtmosBanks 0x428FE0, fn_004270D0 (UpdateChannels + LHListenerUpdate) and
/// LHAtmosProcess(1) unless a video plays (0x4270B1); always fn_00429700 (the ThingMusicInfo purge). The turn is
/// game_clock::Turn() (g_game+0x205A40) and the sky type GSoundMap reads is sky_type::Frame() ([0xFA26BC], 0x71DDF1: the
/// last frame's, written by DrawSky).
void ProcessTurn();
/// GGame::EndTurn while paused (g_game+0x14 & 4, 0x54E993; PauseGame 0x54AE20 toggles that bit): AtmosProcess(0)
/// 0x4286C0. (Pending: the paused EndTurn of the original also runs GSoundMap::Update 0x54E96F and
/// SoundTag::ProcessSoundTags 0x54E989 before that test; openblack's pause has no turn clock, Game calls this once a
/// frame, so they do not run while paused.)
void Paused();
/// Once a frame: the sample master of the configuration applied live (as the options dialog does, 0x5145A3) and the
/// channels' finite loops
void UpdateFrame();
/// A game thing deleted: the channels it owns see it unavailable (GameThing::IsAvailable() == 0) at the next turn.
/// Game does not need to call it today: the entity handles are versioned, so GameQueries::thingPosition already answers
/// nullopt for a destroyed thing (fn_00427200 -> 0, LHSampleStop 0x1001439D). It is for an owner that stays valid in
/// the registry while the game treats it as gone.
void OnThingDeleted(entt::entity thing);
/// GGame::ClearMap 0x552D98 / GGame::Init 0x54F474 -> GAudio::Reset 0x426CA0 (call before the registry reset): +0x18C,
/// the music groups (fn_00428190), +0x28 / +0x24 / +0x180 / +0x190 = 0, +0x1C = -1; LHMusicStop(0), LHAtmosProcess(0),
/// LHSampleStopAll, LHGlobalSwitch(0), the wait for no channel playing, LHSampleClearInfoList (a no-op while switched
/// off, 0x100142CC), LHGlobalSwitch(1); ReleaseAllThingMusicInfo 0x4291B0. Also the map's SoundTags (deleted with the
/// objects of ClearMap) and, in openblack, the channels' OpenAL sources.
void ClearMap();
/// LHScreen's activation callback 0x642470 (AltTabDeactivate 0x7DE6D0 when the window is minimised, AltTabReactivate
/// 0x7DE6F0 when it is restored: Game maps them to SDL's MINIMIZED / RESTORED, not to the focus) -> fn_00428720 ->
/// LHGlobalSwitch 0x10015790: off = LHWaveSwitch(0) (LHSampleStopAll, inactive) and LHMusicSwitch(0) (LHMusicStop(0),
/// inactive); on = both active again, nothing restarts
void OnFocus(bool active);
/// LHSampleSetMasterVolume 0x100150E0 (0..127) through the configuration's AudioSampleMasterVolume (the options slider
/// 0x64: ftol(slider * 127), fn_00428600)
void SetSampleMasterVolume(int volume);
/// LHSampleGetMasterVolume 0x10015170
[[nodiscard]] int SampleMasterVolume();

// ---- anim effects (B2) and SoundTags (B3) ---------------------------------------------------------------------------

/// GAudio::SamplePlayAnimEffect(void* owner, float dist, long* key, int action, LH_AudioBank*, int track, float min,
/// float max) 0x42A4B0 (milestone B2; AnimKey and AnimAction: AnimEffects.h). `distance` is what the caller measured
/// (the camera's distance to the object: fn_00516510, Tree::Draw 0x74AFFD) and gates the play (k_MaxDistance and the
/// sample's max distance); the point is the owner's (fn_00427200). Play: LHSampleGetAnimEffectNumber 0x42A4F9 (no row,
/// nothing), the filters of the sample's user parameter (1 not while a script holds the wide screen, only 2 inside the
/// citadel, the banks after SET_GAME_SOUND false, not 4 in the interface states 0x10 / 0x16 / 0x17) and, when tracking,
/// an unavailable owner (0x42A51B..0x42A5B8); then LHSamplePlayAnimEffect 0x10014A20 with min / max (> 0 overrides the
/// .sad's). Stop / Release: straight to 0x100146F0 for the row's samples, without filters (0x42A4BC). Returns the
/// channel of a play.
Channel SamplePlayAnimEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank, bool track,
                             float minDistance, float maxDistance);

/// SoundTag 0x71E300..0x71ED90 (milestone B3): a sample tied to a thing or a point, linked in g_game+0x205C1C (newest
/// first) and processed once a turn (SoundTag::ProcessSoundTags 0x71E5F0, GGame::EndTurn 0x54E989). The channel's
/// owner is the tag itself (0x71E6F1). A tag of a thing (re)plays through GAudio::PlaySoundEffect 0x42A100 every turn
/// while active, so its play mode decides (2: nothing while it plays; 3: restarts); one whose thing is gone becomes a
/// tag of the dead object (CreateSoundTagForDeadObject 0x71ECD0). A tag of a point plays once and goes when its
/// sample stops; a 3D one with a delay waits for the sound to reach the camera at 347 per second (CheckDelay 0x71E760).
namespace tags
{
using TagId = uint32_t;
inline constexpr TagId k_NoTag = 0;
/// SoundTag::Create(GameThingWithPos*, sample, track, mode, loops, +0x40 (the options' +0x10), is3D, AUDIO_SFX_BANK_TYPE,
/// delay) 0x71E840:
/// no offset; the tag's point is the thing's (0x71E336..0x71E349); the delay is kept only when is3D (0x71E56B)
TagId Create(entt::entity thing, int sample, bool track, int mode, int loops, bool flag10, bool is3D, SfxBank bank,
             int delay);
/// fn_0071E8C0: the same with an offset (+0x1C) added by the channel (the street lantern: (0, Object::GetHeight, 0))
TagId Create(entt::entity thing, glm::vec3 offset, int sample, bool track, int mode, int loops, bool flag10, bool is3D,
             SfxBank bank, int delay);
/// fn_0071EA40, SoundTag::Create(LHPoint&, ...): a point tag (track ignored, +0x30 = 0 without a thing, 0x71E568); it
/// plays at once through GAudio::PlaySoundEffect 0x429E30 (owner the tag, track 0, 0x71EACE..0x71EB33) unless it is 3D
/// with a delay
TagId Create(glm::vec3 point, int sample, bool track, int mode, int loops, bool flag10, bool is3D, SfxBank bank,
             int delay);
/// SoundTag::Create(MapCoords&, ...) 0x71EB60: the point (x, LH3DIsland::GetAltitude + the height above the land, z)
/// (0x71EB71..0x71EBBF; the altitude from GameQueries::landAltitude), x and z scaled by 10 / 65536 ([0x8AA3A4],
/// 0x71EB8A / 0x71EBA6: ecs::map_coords::ToMetres), then fn_0071EA40.
/// (Not ported: fn_0071E920, the same point tag in an atmos bank (ctor fn_0071E460 sets +0x34, so GetBank 0x71E610
/// takes GAudio+0x194 + 4 * type), played at once through 0x429E30 unless 3D with a delay; its only caller is the
/// thunder of GWeather::Update 0x83FC62 through the callback [0xEEA388] = 0x429CE0: sample 2 + GetTickCount() % 11,
/// mode 2, 3D, type 12, delay 1. Pending with the weather's thunder.)
TagId CreateAtMapCoords(const ecs::map_coords::MapCoords& coords, int sample, bool track, int mode, int loops,
                        bool flag10, bool is3D, SfxBank bank, int delay);
/// The same for a MapCoords its caller already holds in metres (x, z = ToMetres of the 16.16 values, the altitude
/// above the land: magic::ToMap's map positions); x and z are not quantised again
TagId CreateAtMapCoords(float x, float z, float heightAboveLand, int sample, bool track, int mode, int loops, bool flag10,
                        bool is3D, SfxBank bank, int delay);
/// fn_0071E640 (SoundTag::SetActive): an active tag (+0x4C == 1) turned off stops its sample (GAudio 0x42A210); +0x4C
void SetActive(TagId tag, bool active);
/// SoundTag::Remove(thing, sample, AUDIO_SFX_BANK_TYPE) 0x71EBE0: every tag of the three (fn_0071ED60) is deleted
/// (ToBeDeleted: a playing loop is released first)
void Remove(entt::entity thing, int sample, SfxBank bank);
/// SoundTag::Remove(..., int stop) 0x71EC30: the same, stopping the sample first when `stop` (GAudio 0x42A210)
void Remove(entt::entity thing, int sample, SfxBank bank, bool stop);
/// SoundTag::ToBeDeleted 0x71ECB0 -> CreateSoundTagForDeadObject 0x71ECD0: the thing forgotten; a sample that plays
/// with loops (fn_0042A460: LHSampleGetInfo +0x40) has its loop released (0x42A310) and the tag lives on until it
/// stops; otherwise the tag goes at once (its sample, if any, plays on)
void Delete(TagId tag);
/// SoundTag::GetRandomSample 0x71ED40: first + GRand::LocalRand(count) (0 for count 0)
[[nodiscard]] int RandomSample(int first, int count);
/// The tag still exists (it has not gone in ProcessSoundTags or Delete)
[[nodiscard]] bool Exists(TagId tag);
} // namespace tags

/// SOUND_EXISTS 0x710100 -> GAudio::IsInstalled 0x426D30 -> LHWaveIsInstalled (milestone B6): the wave device was made.
/// (approximated) openblack's: the audio is initialised on a real OpenAL device (device::Open succeeded).
[[nodiscard]] bool SoundExists();

// ---- voices (B7) ----------------------------------------------------------------------------------------------------
// The voices of the texts and of the script (RUN_TEXT's fn_005C5F90, SAY_SOUND 0x70F8E0, SAY_SOUND_EFFECT_PLAYING
// 0x710280, the click's cut 0x5C6AAD): Voices.h, audio::voices. The advisors (HelpDudeControl / HelpDude, owner 0x270C,
// the lip-sync of AutoVoiceParams::CalcKey 0x428850): Advisor.h, audio::advisor. STOP_SOUND_EFFECT with isSay
// (0x70FA50): ScriptSound.h.

// The music (LHMusic + GAudio: milestones A3..A9) has its own headers: MusicEngine.h / MusicStream.h (LHMusic),
// GameMusic.h (ProcessMusic, the script's music, the master volume), ThingMusic.h (ThingMusicInfo).

} // namespace openblack::audio
