/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameMusic.h"

#include <cstdlib>

#include <algorithm>
#include <string>
#include <utility>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/LH/MusicBank.h"
#include "Audio/LH/MusicEngine.h"
#include "Audio/LH/MusicStream.h"
#include "Audio/Services/ScriptAudioState.h"

namespace openblack::audio
{

namespace
{
/// ProcessMusic's GDebug::SetMessage texts: "Music Playing=%s" (0x9CAFA0) with the MUSIC_TYPE name of 0x9C974C
std::string_view PlayingMessage(int type)
{
	static const auto k_Messages = []() {
		std::array<std::string, static_cast<size_t>(MusicType::_COUNT)> messages;
		for (size_t i = 0; i < messages.size(); ++i)
		{
			messages[i] = "Music Playing=" + std::string(k_MusicBanks[i].name);
		}
		return messages;
	}();
	if (type < 0 || type >= static_cast<int>(MusicType::_COUNT))
	{
		// (not in the original, which reads past the name table 0x9C974C; never reached: a type out of 0..84 has no
		// bank here, so nothing plays)
		return "Music Playing=?";
	}
	return k_Messages[static_cast<size_t>(type)];
}

/// 0x9CAFB4
constexpr std::string_view k_PlayingNone = "Music Playing=NONE";

std::shared_ptr<spdlog::logger> Logger()
{
	return spdlog::get("audio");
}

std::shared_ptr<spdlog::logger> ScriptLogger()
{
	return spdlog::get("scripting");
}

/// GScript::ScriptErrorMessage 0x6F62B0: the script goes on
void ScriptError(std::string_view message)
{
	if (auto logger = ScriptLogger())
	{
		SPDLOG_LOGGER_ERROR(logger, "{}", message);
	}
}

bool g_Trace = std::getenv("OPENBLACK_MUSIC_TRACE") != nullptr;
} // namespace

int DiscreteAlignment(float alignment)
{
	// 0x414730: fld a; fsub -1.0 (0x8AB640); fld 1.0; fsub -1.0; fdivp; fmul 7.0 (0x8AB688); below 6.0 (0x8AB35C) it is
	// kept, else 6.0; __ftol (truncation). The constants are exact; the game's FPU is at 24 bits (fn_007DEE00, `and cw,
	// 0xFCFF` at 0x7DEE0D), so each step rounds to a float
	float value = (alignment - -1.0f) / (1.0f - -1.0f) * 7.0f;
	if (!(value < 6.0f))
	{
		value = 6.0f;
	}
	return static_cast<int>(value);
}

int AlignmentIndex(int discrete)
{
	// fn_00426C80: from 7 on (signed, 0x426C87) 1; else the table 0x9C99F0
	constexpr std::array<int, 7> k_Table = {0, 0, 1, 1, 1, 2, 2};
	if (discrete >= 7)
	{
		return 1;
	}
	// below 0 the original reads before the table (approximated: neutral); DiscreteAlignment of -1..1 is 0..6
	if (discrete < 0)
	{
		return 1;
	}
	return k_Table[static_cast<size_t>(discrete)];
}

int TribeMusicType(int alignmentIndex, int tribe)
{
	// fn_00427410: from tribe 9 on (signed, 0x427414) 5 = CELTIC_TOWN_NEUTRAL whatever the alignment; else the table
	// 0x9C9A0C (the first *_TOWN_EVIL of the tribe's music) + the index
	constexpr std::array<int, 9> k_Table = {4, 4, 7, 10, 13, 16, 19, 22, 25};
	if (tribe >= 9)
	{
		return 5;
	}
	// below 0 the original reads before the table (approximated: as from 9 on)
	if (tribe < 0)
	{
		return 5;
	}
	return k_Table[static_cast<size_t>(tribe)] + alignmentIndex;
}

GameMusic::GameMusic(MusicEngine* engine, const BankProvider& banks, ScriptAudioState& script, GameQueries queries,
                     TownTrigger townTrigger)
    : _engine(engine)
    , _script(script)
    , _queries(std::move(queries))
    , _townTrigger(townTrigger)
    , _self(std::make_shared<GameMusic*>(this))
{
	// 0x426E8E..0x426F1F: GAudio+0x2C[i] = LHBankRegister(path, 0) for the 85 entries with a path
	for (size_t i = 0; i < _banks.size(); ++i)
	{
		_banks[i] = k_MusicBanks[i].path.empty() || !banks ? nullptr : banks(static_cast<MusicType>(i));
	}
	// 0x426F2B..0x426F40: GAudio+0x18 = malloc(LHMusicGetTotalGroups() * 4) (13 with the game's banks), then
	// fn_00428190
	_positions.assign(_engine != nullptr ? _engine->GetTotalGroups() : 0u, 1);
	ResetPositions();
}

GameMusic::~GameMusic()
{
	*_self = nullptr;
}

MusicBank* GameMusic::GetBank(int type) const
{
	// GAudio+0x2C + 4 * type. Outside 0..84 the original reads the next fields of GAudio (85 is +0x180, which is 0
	// whenever ProcessScriptMusic looks: no bank) (approximated: no bank)
	if (type < 0 || type >= static_cast<int>(_banks.size()))
	{
		return nullptr;
	}
	return _banks[static_cast<size_t>(type)];
}

void GameMusic::ResetPositions()
{
	// fn_00428190: for i < LHMusicGetTotalGroups(), pos[i] = 1
	std::fill(_positions.begin(), _positions.end(), 1);
}

void GameMusic::SavePositions()
{
	// fn_004281C0: only with LHMusicIsActive (0x4281C6)
	if (_engine == nullptr || !_engine->IsActive())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		const auto& ch = _engine->GetChannel(i);
		// 0x4281E4..0x4281FD: status 1, group > 0 (signed), group - 1 < LHMusicGetTotalGroups() (unsigned)
		if (ch.status != MusicStatus::Playing || ch.group <= 0)
		{
			continue;
		}
		const auto index = static_cast<uint32_t>(ch.group - 1);
		if (index >= _engine->GetTotalGroups() || index >= _positions.size())
		{
			continue;
		}
		_positions[index] = static_cast<int>(ch.playingChunk) + 2; // 0x428208..0x428212: +0x48 + 2
	}
}

int GameMusic::GroupPosition(int group) const
{
	// mov edx, [GAudio+0x18]; mov edx, [edx + group * 4 - 4] (0x427AB6, 0x4275FE, 0x4295CA). Group 0 (BigFight, the 3D
	// banks) reads the word before the array, the heap's (approximated: 0, which LHMusicPlay starts at chunk 1,
	// 0x1000ED6A..0x1000ED77)
	if (group <= 0 || static_cast<size_t>(group) > _positions.size())
	{
		return 0;
	}
	return _positions[static_cast<size_t>(group - 1)];
}

void GameMusic::SetPlaying(std::string_view message)
{
	if (message != _playing && g_Trace)
	{
		if (auto logger = Logger())
		{
			SPDLOG_LOGGER_INFO(logger, "game music: {}", message);
		}
	}
	_playing = message;
}

void GameMusic::Reset()
{
	// GAudio::Reset 0x426CA0
	_currentTown.reset();   // 0x426CA6 +0x18C = 0
	ResetPositions();       // 0x426CAC
	_scriptType = 0;        // 0x426CB6 +0x28
	_finishedType = 0;      // 0x426CB9 +0x24
	_scriptStarted = 0;     // 0x426CBC +0x180
	// 0x426CC2 +0x190 = 0: the camera's alignment comes from GameQueries::cameraAlignment here
	_alignmentType = -1;    // 0x426CC8 +0x1C
	if (_engine != nullptr) // 0x426CCF: with the audio system
	{
		_engine->Stop(0);   // 0x426CD3 LHMusicStop(0): cut
		// 0x426CF6 / 0x426D23: LHGlobalSwitch(0) then (1) = LHMusicSwitch(0) (LHMusicStop(0), inactive) and (1)
		// (active again, nothing restarts), around the samples' StopAll and ClearInfoList (not music)
		_engine->Switch(0);
		_engine->Switch(1);
	}
	_thingMusic.Clear(); // 0x426D28 ReleaseAllThingMusicInfo
	SetPlaying(k_PlayingNone);
}

void GameMusic::ProcessAudioGameTurn(bool waveActive)
{
	// 0x427086: LHWaveIsActive (sample_play::IsActive, LH_AudioSystem+0x14). The ambient (fn_00429100,
	// ProcessAtmosBanks, LHAtmosProcess) and the listener are not music: audio::ProcessTurn does them.
	if (_engine != nullptr && waveActive)
	{
		ProcessMusic(); // 0x427092
	}
	PurgeThingMusic(); // 0x4270C8, always
}

void GameMusic::ProcessMusic()
{
	const auto none = [this]() {
		// 0x427E95..0x427EB3: "Music Playing=NONE", +0x180 = 0, +0x1C = -1; nothing is stopped
		SetPlaying(k_PlayingNone);
		_scriptStarted = 0;
		_alignmentType = -1;
	};

	if (_queries.videoPlaying && _queries.videoPlaying()) // 0x427DF8 g_game+0x250188
	{
		none();
		return;
	}
	// 0x427E09 LHMusicIsInstalled, 0x427E1D LandNumber == 6
	if (_engine == nullptr || !_engine->IsInstalled() || (_queries.landNumber && _queries.landNumber() == 6))
	{
		return;
	}
	if (_queries.citadelMusic && _queries.citadelMusic()) // 0x427E2C ProcessCitadelMusic
	{
		none();
		return;
	}
	if (ProcessScriptMusic()) // 0x427E37 fn_00427CA0
	{
		_alignmentType = -1; // 0x427EB3
		return;
	}
	if ((_queries.creatureFightMusic && _queries.creatureFightMusic()) || // 0x427E42 fn_00427660
	    (_queries.chantMusic && _queries.chantMusic()) ||                 // 0x427E4D ProcessChantMusic
	    (_queries.creatureDanceMusic && _queries.creatureDanceMusic()) || // 0x427E58 ProcessCreatureDanceMusic
	    ProcessThingMusic())                                              // 0x427E63 fn_00429790
	{
		none();
		return;
	}
	if (ProcessAlignmentMusic()) // 0x427E6E
	{
		_scriptStarted = 0; // 0x427E77
		return;
	}
	SavePositions();  // 0x427E85
	_engine->Stop(1); // 0x427E8F LHMusicStop(1): everything fades out
	none();
}

void GameMusic::StartScriptMusic(int type)
{
	_scriptType = type; // 0x428236
	if (type != 0)
	{
		_scriptStarted = 0; // 0x42823B
	}
}

bool GameMusic::ProcessScriptMusic()
{
	if (_scriptType == 0)
	{
		// 0x427DC4..0x427DDD: the script music was stopped: fade out
		if (_scriptStarted != 0)
		{
			_scriptStarted = 0;
			_engine->Stop(1);
			_alignmentType = -1;
		}
		return false;
	}
	if (_scriptStarted != 0)
	{
		SetPlaying(PlayingMessage(_scriptType)); // 0x427CBC..0x427CD0
		return true;
	}
	// 0x427CED..0x427D0D: fn_00427C90(fn_00426C80(Discrete(+0x190))) ignores the alignment and returns +0x28
	auto* bank = GetBank(_scriptType);
	if (bank == nullptr)
	{
		return false; // 0x427D16
	}
	MusicPlayOptions options; // LH_MusicPlayOptions ctor 0x1000D970 (0x427CE7)
	options.bank = bank;      // 0x427D35
	options.volume = 0x7F;    // 0x427D39
	options.startChunk = 1;   // 0x427D23
	options.sync = 0;         // 0x427D1F
	options.fade = 0;         // 0x427D41
	options.is3D = 0;         // 0x427D45
	options.pitch = 0x64;     // 0x427D49
	const std::weak_ptr<GameMusic*> self = _self;
	options.finished = [self](int data) { // 0x427D51: 0x426B80
		if (auto p = self.lock(); p && *p != nullptr)
		{
			(*p)->OnScriptMusicFinished(data);
		}
	};
	options.marker = [self](std::string_view label) { // 0x427D59: 0x426BA0
		if (auto p = self.lock(); p && *p != nullptr)
		{
			(*p)->OnScriptMusicMarker(label);
		}
	};
	options.userData = _scriptType; // 0x427D2F
	SavePositions();                // 0x427D61
	_engine->Play(options);         // 0x427D6E
	_scriptStarted = 1;             // 0x427D77
	SetPlaying(PlayingMessage(_scriptType));
	return true;
}

void GameMusic::OnScriptMusicFinished(int type)
{
	// 0x426B80: the music that ended is still the script's: no script music any more
	if (type == _scriptType)
	{
		_scriptType = 0;
	}
}

void GameMusic::OnScriptMusicMarker(std::string_view label)
{
	// 0x426BA0: switch (label[0] - 'L'), 0..0x2B, byte table 0x426C10 -> jump table 0x426C04
	if (label.empty())
	{
		return; // '\0' is below 'L': the default case
	}
	switch (label[0])
	{
	case 'L':
	case 'l':
		// 0x426BBE..0x426BE5: +0x98 = atoi(label + 1), +0x9C = 1
		_script.musicLine = static_cast<uint32_t>(std::atoi(std::string(label.substr(1)).c_str()));
		_script.musicBeat = 1;
		break;
	case 'P':
	case 'p':
	case 'W':
	case 'w':
		++_script.musicBeat; // 0x426BF0..0x426BFC
		break;
	default:
		break;
	}
}

void GameMusic::OnAlignmentMusicFinished(int group)
{
	// 0x426B40: group - 1 < LHMusicGetTotalGroups() (unsigned): pos = 1 and fn_004279A0 (+0x24 = +0x1C, +0x20 = 0,
	// +0x1C = -1); a group 0 does nothing
	const auto index = static_cast<uint32_t>(group - 1);
	if (_engine == nullptr || index >= _engine->GetTotalGroups())
	{
		return;
	}
	if (index < _positions.size())
	{
		_positions[index] = 1; // 0x426B61
	}
	_finishedType = _alignmentType; // 0x4279A3
	_silenceTurns = 0;              // 0x4279A6
	_alignmentType = -1;            // 0x4279AD
}

bool GameMusic::ProcessAlignmentMusic()
{
	// 0x4279D7: a camera
	const auto camera = _queries.camera ? _queries.camera() : std::nullopt;
	if (!camera)
	{
		return false;
	}
	// 0x4279E9..0x427A01: not while the script's wide screen is on; 0x427A07: nor while its bars move
	if ((_queries.scriptWideScreen && _queries.scriptWideScreen()) ||
	    (_queries.wideScreenChanging && _queries.wideScreenChanging()))
	{
		return false;
	}
	// 0x427A20: ENABLE_DISABLE_ALIGNMENT_MUSIC; 0x427A2E: game turn > 20 (unsigned)
	if (_script.alignmentMusic == 0)
	{
		return false;
	}
	if (!(_queries.turn && _queries.turn() > 0x14))
	{
		return false;
	}
	const int type = AlignmentMusicType(*camera); // 0x427A46
	if (type == 0)
	{
		return false; // 0x427A4F
	}
	// 0x427A55..0x427A68: after this type ended by itself, 3500 turns of silence while it stays the chosen one
	if (_finishedType == type)
	{
		++_silenceTurns;
		if (_silenceTurns < 0xDAC)
		{
			return false;
		}
	}
	_finishedType = 0; // 0x427A6E
	auto* bank = GetBank(type);
	if (bank == nullptr)
	{
		return false; // 0x427A7B
	}
	if (_alignmentType != type) // 0x427A81
	{
		SavePositions(); // 0x427A8C
		const int group = bank->GetGroupId(); // 0x427A95 LHBankGetMusicGroupId
		MusicPlayOptions options;
		options.sync = 1;                          // 0x427A9E: [esp+0x24] = opts+0x1C (before the push at 0x427ABA)
		options.bank = bank;                       // 0x427AAA
		options.volume = 0x50;                     // 0x427AAE: 80
		options.startChunk = GroupPosition(group); // 0x427AB6 / 0x427ABE
		options.fade = 1;                          // 0x427AC2
		options.is3D = 0;                          // 0x427ACA
		options.pitch = 0x64;                      // 0x427AD2
		const std::weak_ptr<GameMusic*> self = _self;
		options.finished = [self](int data) { // 0x427ADA: 0x426B40
			if (auto p = self.lock(); p && *p != nullptr)
			{
				(*p)->OnAlignmentMusicFinished(data);
			}
		};
		options.userData = group; // 0x427AE2
		// 0x427AE6..0x427B01: loops (opts+0x18: [esp+0x24] after the push edx at 0x427B00; LHMusicPlay copies it to
		// the channel's loops at 0x1000E13E) = start >= LHBankGetNumberOfSamples / 2 (signed, the division rounds to
		// 0). music.md §2.5.7 calls it "sync"; the offsets say loops (audit 2026-10-01).
		const auto half = static_cast<int32_t>(bank->GetSegmentCount()) / 2;
		options.loops = options.startChunk >= half ? 1 : 0;
		_engine->Play(options); // 0x427B08
		_alignmentType = type;  // 0x427B0E
	}
	SetPlaying(PlayingMessage(type)); // 0x427B11..0x427B25
	return true;
}

int GameMusic::AlignmentMusicType(const CameraState& camera)
{
	// fn_00427460
	const float alignment = _queries.cameraAlignment ? _queries.cameraAlignment() : 0.0f; // +0x190
	const int a = AlignmentIndex(DiscreteAlignment(alignment));                           // 0x427474..0x42747F
	// 0x427484..0x427493: fn_00602160(camera, townTriggerOffDistance)
	const auto town = _queries.nearestTown ? _queries.nearestTown(_townTrigger.offDistance) : std::nullopt;
	// 0x42749B..0x4274B6: the kept town is forgotten when it is no longer available
	std::optional<MusicTown> current;
	if (_currentTown)
	{
		current = _queries.town ? _queries.town(*_currentTown) : std::nullopt;
		if (!current)
		{
			_currentTown.reset();
		}
	}
	// 0x4274BC..0x4274D3: a town, and the camera lower than townTriggerOffDistance over the land
	if (town && camera.heightAboveGround < _townTrigger.offDistance)
	{
		// 0x4274EC..0x4274FF: within townTriggerDistance (<=) it becomes the kept town
		if (town->distance <= _townTrigger.distance)
		{
			_currentTown = town->id; // 0x427554
			return TribeMusicType(a, town->tribe);
		}
		// 0x427501..0x427535: another kept town still within townTriggerOffDistance (<) keeps its music; the nearest
		// one itself between the two distances does not
		if (_currentTown && *_currentTown != town->id && current && current->distance < _townTrigger.offDistance)
		{
			return TribeMusicType(a, current->tribe);
		}
	}
	_currentTown.reset(); // 0x427572
	return a + 1;         // 0x427579: GENERIC_EVIL / NEUTRAL / GOOD
}

void GameMusic::AddThingMusic(int type, ThingId thing)
{
	// 0x42923E: only an available thing
	if (!_queries.thingPosition || !_queries.thingPosition(thing))
	{
		return;
	}
	// 0x42924C..0x42925E: a thing that has music already only changes its type
	if (auto* info = _thingMusic.Get(thing); info != nullptr)
	{
		info->type = type;
		return;
	}
	// 0x429265..0x4292E8: a new info; if its bank plays already, it is cut (fn_004296C0(info, 0)); then at the head
	auto& info = _thingMusic.AddFront(type, thing);
	if (IsThingMusicPlaying(info))
	{
		StopThingMusic(info, 0);
	}
}

void GameMusic::RestartMusicThing(ThingId thing)
{
	auto* info = _thingMusic.Get(thing);
	if (info == nullptr)
	{
		return;
	}
	info->finished = 0; // 0x429927
	info->started = 0;  // 0x42992E
	if (IsThingMusicPlaying(*info))
	{
		StopThingMusic(*info, 0); // 0x429943
	}
}

float GameMusic::GetMusicObjDistance(ThingId thing)
{
	const auto* info = _thingMusic.Get(thing);
	return info != nullptr ? GetPlayDistance(info->type) : 0.0f; // 0x4293C7 / 0x4293D0
}

float GameMusic::GetPlayDistance(int type) const
{
	// 0x4293E0: 100 (0x8AB41C) without a bank; LHBankGetMusicMaxDistance (f32 @+0x26C of the first segment), and 100 if
	// it is negative (0x4293FA..0x429409; 0 stays 0)
	const auto* bank = GetBank(type);
	if (bank == nullptr)
	{
		return 100.0f;
	}
	const float distance = bank->GetMaxDistance();
	return distance < 0.0f ? 100.0f : distance;
}

bool GameMusic::IsThingMusicPlaying(const ThingMusicInfo& info) const
{
	// fn_00429680: bank, LHMusicGetInfo(bank), LHMusicGetStatus != 0
	const auto* bank = GetBank(info.type);
	if (bank == nullptr || _engine == nullptr)
	{
		return false;
	}
	const int channel = _engine->GetInfo(bank);
	return channel != k_NoMusicChannel && _engine->GetStatus(channel) != MusicStatus::Free;
}

void GameMusic::StopThingMusic(const ThingMusicInfo& info, int fade)
{
	// fn_004296C0
	if (!IsThingMusicPlaying(info))
	{
		return;
	}
	const auto* bank = GetBank(info.type);
	const int channel = _engine->GetInfo(bank);
	if (channel != k_NoMusicChannel)
	{
		_engine->Stop(channel, fade); // 0x4296F4 LHMusicStop(info, fade)
	}
}

bool GameMusic::ThingMusicInRange(int type, glm::vec3 thingPosition) const
{
	// fn_00429420: no bank, nothing
	if (GetBank(type) == nullptr)
	{
		return false;
	}
	// 0x429479..0x4294C1: the 3D distance to LH3DTech::g_camera (approximated without a camera: out of range)
	const auto camera = _queries.camera ? _queries.camera() : std::nullopt;
	if (!camera)
	{
		return false;
	}
	const float distance = glm::length(thingPosition - camera->position);
	// 0x4294CD..0x4294DC: in range when GetPlayDistance > the distance
	return GetPlayDistance(type) > distance;
}

bool GameMusic::PlayThingMusic(ThingMusicInfo& info, glm::vec3 thingPosition)
{
	// fn_00429500
	auto* bank = GetBank(info.type);
	if (bank == nullptr)
	{
		return false; // 0x429520
	}
	const int group = bank->GetGroupId(); // 0x429526
	if (!ThingMusicInRange(info.type, thingPosition))
	{
		return false; // 0x42953F (the range is the thing's, even with a play position)
	}
	// 0x429545..0x4295A3: the play position of SET_MUSIC_PLAY_POSITION, or the thing's
	const glm::vec3 position = info.hasPlayPosition != 0 ? info.playPosition : thingPosition;

	MusicPlayOptions options;
	options.sync = group > 0 ? 1 : 0;          // 0x4295A3..0x4295B3 (+0x1C)
	options.fade = group > 0 ? 1 : 0;          // (+0x20)
	options.bank = bank;                       // 0x4295BE
	options.volume = 0x7F;                     // 0x4295C2
	options.startChunk = GroupPosition(group); // 0x4295CA / 0x4295DC
	options.is3D = 1;                          // 0x4295E0
	options.pitch = 0x64;                      // 0x4295E4
	options.position = position;               // 0x4295CE / 0x4295D8 / 0x4295EC
	SavePositions();                           // 0x4295F0

	// 0x4295FD..0x429630: no channel for the bank, started and finished -> 0; finished -> 1 without playing; else play
	// (every turn: LHMusicPlay re-triggers the channel that has the bank)
	const int existing = _engine->GetInfo(bank);
	if (existing == k_NoMusicChannel && info.started != 0 && info.finished != 0)
	{
		return false;
	}
	if (info.finished != 0)
	{
		return true;
	}
	const int channel = _engine->Play(options);  // 0x42963A
	_engine->Set3DPosition(channel, position);    // 0x429656
	info.started = 1;                             // 0x42964F
	return true;
}

bool GameMusic::ProcessThingMusic()
{
	// fn_00429790, from the head of the list
	bool inRange = false;
	auto& infos = _thingMusic.GetInfos();
	for (size_t i = 0; i < infos.size();)
	{
		const auto position = _queries.thingPosition ? _queries.thingPosition(infos[i].thing) : std::nullopt;
		if (!position)
		{
			// 0x4297FE..0x42984A: the thing is no longer available: its info goes
			infos.erase(infos.begin() + static_cast<std::ptrdiff_t>(i));
			continue;
		}
		auto& info = infos[i];
		if (info.enabled != 0 && info.finished == 0)
		{
			// 0x4297D4..0x4297DF: the first one that plays takes the music
			if (PlayThingMusic(info, *position))
			{
				return true;
			}
		}
		else if (ThingMusicInRange(info.type, *position))
		{
			inRange = true; // 0x4297F7: disabled or finished but in range: no alignment music either
		}
		++i;
	}
	return inRange;
}

void GameMusic::PurgeThingMusic()
{
	// fn_00429700
	auto& infos = _thingMusic.GetInfos();
	infos.erase(std::remove_if(infos.begin(), infos.end(),
	                           [this](const ThingMusicInfo& info) {
		                           return !_queries.thingPosition || !_queries.thingPosition(info.thing);
	                           }),
	            infos.end());
}

void GameMusic::ScriptStartMusic(int type)
{
	// 0x70FB34..0x70FB47: below 0 or above 0x55 an error, and it goes on
	if (type < 0 || type > 0x55)
	{
		ScriptError("Jonty - Strange Music Type in script");
	}
	_script.musicLine = 0; // 0x70FB56 GScript+0x98
	_script.musicBeat = 0; // 0x70FB6B GScript+0x9C
	StartScriptMusic(type);
}

std::optional<bool> GameMusic::ScriptMusicPlayed(int type) const
{
	if (!IsInstalled())
	{
		return std::nullopt; // 0x70FBE4 TextRead
	}
	return _scriptType != type; // 0x70FBC9..0x70FBD0
}

std::optional<bool> GameMusic::ScriptLastMusicLine(float line) const
{
	if (!IsInstalled())
	{
		return std::nullopt; // 0x7100A6 TextRead
	}
	// 0x71007A..0x71009C: ftol(line), then +0x98 >= it, unsigned (cmp; sbb; inc)
	const auto wanted = static_cast<uint32_t>(static_cast<int32_t>(line));
	return _script.musicLine.load() >= wanted;
}

void GameMusic::ScriptAttachMusic(int type, std::optional<ThingId> thing)
{
	// 0x70FC31..0x70FC3F: outside 1..0x54 an error; AddThingMusic all the same (0x70FC47..0x70FC53)
	if (type <= 0 || type >= 0x55)
	{
		ScriptError("Jonty - Strange Music Type in script");
	}
	if (thing)
	{
		AddThingMusic(type, *thing);
	}
}

std::vector<float> GameMusic::ScriptGetMusicEnumDistance(int type) const
{
	std::vector<float> pushes;
	// 0x70FDF4..0x70FE28: outside 1..0x54 "Invalid music man!" and a push of 0.0
	if (type <= 0 || type >= 0x55)
	{
		ScriptError("Invalid music man!");
		pushes.push_back(0.0f);
	}
	pushes.push_back(GetPlayDistance(type)); // 0x70FE34..0x70FE4A, always
	return pushes;
}

namespace game_music
{
namespace
{
std::unique_ptr<GameMusic> g_GameMusic;
std::recursive_mutex g_FallbackMutex;
} // namespace

std::unique_lock<std::recursive_mutex> Lock()
{
	if (auto* system = music::Get(); system != nullptr)
	{
		return std::unique_lock(system->GetMutex());
	}
	return std::unique_lock(g_FallbackMutex);
}

void Start(GameQueries queries, GameMusic::TownTrigger townTrigger)
{
	auto lock = Lock();
	auto* system = music::Get();
	MusicEngine* engine = system != nullptr ? &system->GetEngine() : nullptr;
	g_GameMusic = std::make_unique<GameMusic>(
	    engine, [](MusicType type) { return music::GetBank(type); }, GetScriptAudioState(), std::move(queries),
	    townTrigger);
}

void Shutdown()
{
	auto lock = Lock();
	g_GameMusic.reset();
}

void ProcessTurn(uint32_t turn, bool waveActive)
{
	auto lock = Lock();
	if (!g_GameMusic)
	{
		return;
	}
	// A test hook, not a behaviour of the original: a START_MUSIC of the script at a given turn
	static const auto k_TestHook = []() -> std::optional<std::pair<int, uint32_t>> {
		const char* spec = std::getenv("OPENBLACK_TEST_SCRIPT_MUSIC");
		if (spec == nullptr)
		{
			return std::nullopt;
		}
		const std::string text(spec);
		const auto at = text.find('@');
		const int type = std::atoi(text.substr(0, at).c_str());
		const auto when = at == std::string::npos ? 30u : static_cast<uint32_t>(std::atoi(text.substr(at + 1).c_str()));
		return std::make_pair(type, when);
	}();
	static bool s_TestDone = false;
	if (k_TestHook && !s_TestDone && turn >= k_TestHook->second)
	{
		s_TestDone = true;
		if (auto logger = Logger())
		{
			SPDLOG_LOGGER_INFO(logger, "game music test: START_MUSIC({}) at turn {}", k_TestHook->first, turn);
		}
		g_GameMusic->ScriptStartMusic(k_TestHook->first);
	}
	g_GameMusic->ProcessAudioGameTurn(waveActive);
}

GameMusic* Get()
{
	return g_GameMusic.get();
}
} // namespace game_music

} // namespace openblack::audio
