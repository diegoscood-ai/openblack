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
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/GAudio/BankTables.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/ThingMusic.h"

// The music part of GAudio (runblack.exe): the 85 music banks, the group positions GAudio+0x18, the script music
// (+0x28 / +0x180), the alignment and tribe music (+0x1C / +0x20 / +0x24 / +0x18C / +0x190), the music attached to
// objects (+0x184) and ProcessMusic 0x427DF0, over MusicEngine (LHMusic). Every call must be made under the music lock
// (game_music::Lock()), the one the music thread holds when the engine calls back (end of a track, markers).
// Sources: dev\tmp_dis\audio\music.md §2.2, §2.4..§2.6, script.md §2.4, §2.5, and the disassembly of 0x426B40..0x428230
// and 0x429180..0x429950 (music_dis_process.txt, script_dis_thingmusic.txt and bwdis.py), cited at each step.

namespace openblack::audio
{

class MusicBank;
class MusicEngine;
struct ScriptAudioState;

/// GetDiscreteAlignmentValue 0x414730: ftol(min((a + 1) / 2 * 7, 6)), 0..6
[[nodiscard]] int DiscreteAlignment(float alignment);
/// fn_00426C80: the table 0x9C99F0 {0, 0, 1, 1, 1, 2, 2} (0 evil, 1 neutral, 2 good), 1 from 7 on
[[nodiscard]] int AlignmentIndex(int discrete);
/// fn_00427410: the tribe music, 5 (CELTIC_TOWN_NEUTRAL) from tribe 9 on, else the table 0x9C9A0C
/// {4, 4, 7, 10, 13, 16, 19, 22, 25} + alignment index
[[nodiscard]] int TribeMusicType(int alignmentIndex, int tribe);
/// fn_00427430: the chant of a tribe, 5 (CELTIC_TOWN_NEUTRAL) from tribe 9 on, else the table 0x9C9A30
/// {28, 28, 30, 32, 34, 36, 38, 40, 42} (*_CHANT), + 1 (*_CHANT_VOX) with more than 8 dancers
[[nodiscard]] int ChantMusicType(int tribe, uint32_t dancers);

class GameMusic
{
public:
	/// The bank of a MUSIC_TYPE as GAudio+0x2C + 4 * type holds it (nullptr when LHBankRegister failed)
	using BankProvider = std::function<MusicBank*(MusicType type)>;

	/// GSoundInfo townTriggerDistance / townTriggerOffDistance (0xD9A934 / 0xD9A938, read from info.dat)
	struct TownTrigger
	{
		float distance;
		float offDistance;
	};

	/// The music part of the GAudio constructor fn_00426D40: the 85 banks of 0x9C9748 (0x426E82..0x426F1F, through the
	/// provider, which registers them), then GAudio+0x18 = malloc(LHMusicGetTotalGroups() * 4) and fn_00428190 (every
	/// position 1). engine may be null (no OpenAL): then nothing plays and IsInstalled is false.
	GameMusic(MusicEngine* engine, const BankProvider& banks, ScriptAudioState& script, GameQueries queries,
	          TownTrigger townTrigger);
	~GameMusic();
	GameMusic(const GameMusic&) = delete;
	GameMusic& operator=(const GameMusic&) = delete;

	/// GAudio::IsInstalled 0x426D30 = LHWaveIsInstalled; here the music engine's existence (approximated: openblack has
	/// no LHaudio sample system yet)
	[[nodiscard]] bool IsInstalled() const { return _engine != nullptr; }

	/// The music part of GAudio::Reset 0x426CA0 (GGame::Init / ClearMap)
	void Reset();
	/// The music part of ProcessAudioGameTurn 0x427080: ProcessMusic when the audio is active (0x427086), then always
	/// fn_00429700 (0x4270C8)
	void ProcessAudioGameTurn(bool waveActive);
	/// ProcessMusic 0x427DF0
	void ProcessMusic();

	/// StartScriptMusic 0x428230: +0x28 = type; with a type, +0x180 = 0 (it starts again even if it is the same)
	void StartScriptMusic(int type);

	// --- the CHL functions (GScript, 0x70FB20..0x710144) -------------------------------------------------------------
	/// START_MUSIC 0x70FB20: an error outside 0..0x55 (and it goes on), +0x98 = +0x9C = 0, StartScriptMusic
	void ScriptStartMusic(int type);
	/// STOP_MUSIC 0x70FB90: StartScriptMusic(0)
	void ScriptStopMusic() { StartScriptMusic(0); }
	/// MUSIC_PLAYED 350 0x70FBA0: GAudio+0x28 != type; nullopt without audio (the original then runs TEXT_READ,
	/// 0x70FBE4)
	[[nodiscard]] std::optional<bool> ScriptMusicPlayed(int type) const;
	/// LAST_MUSIC_LINE 0x710050: GScript+0x98 >= ftol(line) (unsigned); nullopt without audio (TEXT_READ, 0x7100A6)
	[[nodiscard]] std::optional<bool> ScriptLastMusicLine(float line) const;
	/// ATTACH_MUSIC 0x70FBF0: an error outside 1..84 (0x70FC31), then AddThingMusic anyway if the script's thing is
	/// valid (nullopt: GetScriptGameThing gave none, 0x70FC47)
	void ScriptAttachMusic(int type, std::optional<ThingId> thing);
	/// GET_MUSIC_ENUM_DISTANCE 0x70FDE0: outside 1..84 an error and a push of 0, then (always) GetPlayDistance(type):
	/// the values pushed, in order
	[[nodiscard]] std::vector<float> ScriptGetMusicEnumDistance(int type) const;

	// --- ThingMusicInfo (AudioMusicThing.cpp) --------------------------------------------------------------------
	/// AddThingMusic 0x429230
	void AddThingMusic(int type, ThingId thing);
	/// RemoveThingMusic 0x429340
	void RemoveThingMusic(ThingId thing) { _thingMusic.Remove(thing); }
	/// fn_00429880 (MOVE_MUSIC)
	void MoveThingMusic(ThingId from, ThingId to) { _thingMusic.Move(from, to); }
	/// fn_004298A0 (ENABLE_DISABLE_MUSIC)
	void EnableThingMusic(ThingId thing, int on) { _thingMusic.Enable(thing, on); }
	/// SetPlayPosition 0x4298C0 (SET_MUSIC_PLAY_POSITION)
	void SetPlayPosition(ThingId thing, glm::vec3 point) { _thingMusic.SetPlayPosition(thing, point); }
	/// RestartMusicThing 0x429910
	void RestartMusicThing(ThingId thing);
	/// IsMusicThingFinished 0x4298F0
	[[nodiscard]] int IsMusicThingFinished(ThingId thing) { return _thingMusic.IsFinished(thing); }
	/// fn_004293B0 (GET_MUSIC_OBJ_DISTANCE): GetPlayDistance of the thing's type, 0 without an info
	[[nodiscard]] float GetMusicObjDistance(ThingId thing);
	/// GetPlayDistance 0x4293E0: LHBankGetMusicMaxDistance of the type's bank, 100 without a bank or if negative
	[[nodiscard]] float GetPlayDistance(int type) const;

	// --- state (debug window, tests) -----------------------------------------------------------------------------
	[[nodiscard]] int GetScriptType() const { return _scriptType; }          ///< +0x28
	[[nodiscard]] int GetScriptStarted() const { return _scriptStarted; }    ///< +0x180
	[[nodiscard]] int GetAlignmentType() const { return _alignmentType; }    ///< +0x1C
	[[nodiscard]] uint32_t GetSilenceTurns() const { return _silenceTurns; } ///< +0x20
	[[nodiscard]] int GetFinishedType() const { return _finishedType; }      ///< +0x24
	[[nodiscard]] std::optional<uint32_t> GetCurrentTown() const { return _currentTown; } ///< +0x18C
	[[nodiscard]] int GetCitadelSamplesStopped() const { return _citadelSamplesStopped; } ///< [0xC56164]
	[[nodiscard]] const std::vector<int>& GetGroupPositions() const { return _positions; } ///< +0x18
	[[nodiscard]] const ThingMusicList& GetThingMusic() const { return _thingMusic; }
	/// The text of GDebug::SetMessage: "Music Playing=<MUSIC_TYPE name>" or "Music Playing=NONE"
	[[nodiscard]] std::string_view GetPlayingMessage() const { return _playing; }
	[[nodiscard]] MusicBank* GetBank(int type) const;

private:
	/// fn_00428190: every group position 1
	void ResetPositions();
	/// fn_004281C0: before each LHMusicPlay, for each channel at status 1 with a group > 0, pos[group - 1] = its
	/// audible chunk + 2
	void SavePositions();
	/// GAudio+0x18[group - 1] as the callers read it
	[[nodiscard]] int GroupPosition(int group) const;

	/// ProcessCitadelMusic 0x427B60
	bool ProcessCitadelMusic();
	/// fn_00427CA0
	bool ProcessScriptMusic();
	/// ProcessChantMusic 0x427790: the chant of the worship site near the camera (GameQueries::chantSite), 3D at its
	/// dance centre
	bool ProcessChantMusic();
	/// fn_00429790
	bool ProcessThingMusic();
	/// fn_00429500
	bool PlayThingMusic(ThingMusicInfo& info, glm::vec3 thingPosition);
	/// fn_00429420: a bank, and the camera nearer than GetPlayDistance(type) to the thing (3D)
	[[nodiscard]] bool ThingMusicInRange(int type, glm::vec3 thingPosition) const;
	/// fn_00429680: the type's bank plays on a channel whose status is not 0
	[[nodiscard]] bool IsThingMusicPlaying(const ThingMusicInfo& info) const;
	/// fn_004296C0: LHMusicStop(its channel, fade)
	void StopThingMusic(const ThingMusicInfo& info, int fade);
	/// fn_00429700: the infos of things no longer available go (every turn)
	void PurgeThingMusic();
	/// ProcessAlignmentMusic 0x4279C0 (the symbol says LoginBox::ControlCallback)
	bool ProcessAlignmentMusic();
	/// fn_00427460
	int AlignmentMusicType(const CameraState& camera);

	/// The three callbacks of LH_MusicPlayOptions +0x44 / +0x48
	void OnScriptMusicFinished(int type);     ///< 0x426B80
	void OnScriptMusicMarker(std::string_view label); ///< 0x426BA0
	void OnAlignmentMusicFinished(int group); ///< 0x426B40

	void SetPlaying(std::string_view message);

	MusicEngine* _engine;
	ScriptAudioState& _script;
	GameQueries _queries;
	TownTrigger _townTrigger;
	std::array<MusicBank*, static_cast<size_t>(MusicType::_COUNT)> _banks {}; ///< +0x2C
	std::vector<int> _positions;            ///< +0x18
	int _alignmentType {-1};                ///< +0x1C
	uint32_t _silenceTurns {0};             ///< +0x20
	int _finishedType {0};                  ///< +0x24
	int _scriptType {0};                    ///< +0x28
	int _scriptStarted {0};                 ///< +0x180
	ThingMusicList _thingMusic;             ///< +0x184 / +0x188
	std::optional<uint32_t> _currentTown;   ///< +0x18C
	/// [0xC56164]: the samples were stopped on entering the citadel (a global: GAudio::Reset does not clear it)
	int _citadelSamplesStopped {0};
	std::string_view _playing {"Music Playing=NONE"};
	/// The engine keeps the callbacks after a channel is freed: they reach this object only while it lives
	std::shared_ptr<GameMusic*> _self;
};

/// The game's GAudio music (one, as GGlobal::Global+0x14...)
namespace game_music
{
/// After music::Start: the GameMusic over the music system (or without one)
void Start(GameQueries queries, GameMusic::TownTrigger townTrigger);
/// Before music::Shutdown
void Shutdown();
/// GGame::EndTurn's GAudio::ProcessAudioGameTurn (its music part), under the lock; and the test hook
/// OPENBLACK_TEST_SCRIPT_MUSIC="<type>[@<turn>]" (START_MUSIC(type) at that game turn, 30 by default)
void ProcessTurn(uint32_t turn, bool waveActive);
/// The lock of every call to Get(): the music system's (the engine calls back under it), or a lock of its own
[[nodiscard]] std::unique_lock<std::recursive_mutex> Lock();
/// nullptr before Start
[[nodiscard]] GameMusic* Get();
} // namespace game_music

} // namespace openblack::audio
