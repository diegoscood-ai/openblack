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

/// The player's help statistics of runblack.exe W120: HelpProfile (g_game +0x250060, HelpProfile::Create 0x5C4500 from
/// GGame::Init 0x54F4F3) and the counters of CameraHelp (CameraHelp::CameraHelpCallback 0x449140). Research:
/// dev\documentacion\intro\spec_timers_events.md.
///
/// - One CameraHelpAccumulator (0x10C bytes) per HELP_EVENT_TYPE (49, names at 0xBF084C): +0 the total count, +4 a
///   smoothed rate (float), +8 the ring head, +9 how many ring slots are used (<= 64), +0xA "triggered this turn",
///   +0xC 64 trigger times in HelpProfile::AccumulatedTime ms [0xC5AFD8].
/// - Trigger 0x449040 counts at most once a turn: the flag +0xA blocks it until HelpProfile::Process 0x5C4660 clears it
///   (GGame::ProcessTurn 0x54E6A9, after GScript::Process and HelpSystem::Process), which also advances
///   AccumulatedTime by 100 ms. So GET_TOTAL_EVENTS(n) is "the turns in which the event happened".
/// - Nothing is counted while the game is paused (g_game +0x14 & 4) or a script holds the wide screen (HelpSystem +0x45E8
///   && +0x45EC): HelpProfile::Trigger 0x5C46E8..0x5C4706, Process 0x5C4669..0x5C4687.
/// - The counts belong to the player profile, not to the game: Save 0x5C4820 / Load 0x5C4830 write and read the 49
///   accumulators (0x334C bytes) to "<user path>\helpstats.dat" (GGame::Save 0x554569, GGame::Load 0x554D1A,
///   PlayerProfile::GetProfileByName 0x66BC9A, WriteBackToRegistry 0x66BDFD). (not ported) openblack has no profiles:
///   the counts start at 0 every run.
namespace openblack::help_profile
{

/// HELP_EVENT_TYPE, by the names of the table 0xBF084C
enum class Event : int32_t
{
	Dummy = 0,
	HandMove = 1,
	PickUp = 2,
	Catch = 3,
	Throw = 4,
	Give = 5,
	Supply = 6,
	Sacrifice = 7,
	Tap = 8,
	CastSpell = 9,
	CastCreatureSpell = 10,
	CastAll = 11,
	GetSpell = 12,
	StopSpell = 13,
	RepeatGesture = 14,
	SelectGesture = 15,
	StageGesture = 16,
	GetSpellGesture = 17,
	PowerUpGesture = 18,
	GestureCreatureSpecial = 19,
	GestureCancelPowerUp = 20,
	GestureCancelSelect = 21,
	GestureCancelHeld = 22,
	GestureOnCast = 23,
	GestureTotal = 24,
	Rotate = 25,
	RotateCW = 26,
	RotateCCW = 27,
	Pitch = 28,
	Zoom = 29,
	DoubleClickPos = 30,
	DoubleClickObject = 31,
	ZoomToCitadel = 32,
	Drag = 33,
	Reminder = 34,
	ScriptActivate = 35,
	FightBlock = 36,
	FightAttack = 37,
	FightSpell = 38,
	FightStep = 39,
	AttachLeash = 40,
	DetachLeash = 41,
	HelpQuery = 42,
	AllInterface = 43,
	LookAtLand = 44,
	LookAtLandTooClose = 45,
	LookAtSky = 46,
	FocusLeash = 47,
	AllEvents = 48,
};
inline constexpr int32_t k_EventCount = 0x31; ///< HelpProfile::Create 0x5C452B: 49 accumulators
inline constexpr uint32_t k_RingSize = 0x40;  ///< +0xC: 64 times (`and 0x3F`)
/// HelpProfile::Process 0x5C46CF: AccumulatedTime += 0x64 a turn
inline constexpr uint32_t k_MsPerProcess = 100;
/// [0x8C7674]: the smoothing of +4 (Process 0x5C46BB, fn_00449240)
inline constexpr float k_RateSmoothing = 0.005f;

/// CameraHelpAccumulator
struct Accumulator
{
	int32_t totalTriggerCount {0};          ///< +0 (GET_TOTAL_EVENTS reads it with fild: signed)
	float rate {0.0f};                      ///< +4: rate += (triggered - rate) x 0.005 each Process
	uint8_t head {0};                       ///< +8
	int8_t used {0};                        ///< +9 (movsx in GetTriggerPerSecond)
	bool triggeredThisTurn {false};         ///< +0xA
	std::array<uint32_t, k_RingSize> times {}; ///< +0xC

	/// CameraHelpAccumulator::Reset 0x448F20: +0, +4, +8, +9, +0xA to 0 (the ring is left as it is)
	void Reset();
	/// CameraHelpAccumulator::Trigger 0x449040(now): once a turn, ++count, times[head] = now, head = (head + 1) & 63,
	/// used = min(used + 1, 64)
	void Trigger(uint32_t now);
	/// The step of HelpProfile::Process 0x5C46A1..0x5C46CD / fn_00449240: rate += ((int)flag - rate) x 0.005; flag = 0
	void EndTurn();
	/// GetTimeSinceLastUsed 0x448F40: 0 if never; (now - times[(head - 1) & 63]) x 0.001; a time in the future (a
	/// profile loaded over a newer clock) brings every slot down to now (fn_00448F90) and gives 0
	[[nodiscard]] float TimeSinceLastUsed(uint32_t now);
	/// HelpProfile::GetTriggerPerSecond 0x448FC0: 0 with fewer than 2 times; d = now - times[(head - used) & 63];
	/// d < 0 clamps the ring (fn_00448F90) and gives 0, d == 0 gives 0, else (used x 1000 - 1000) / d
	[[nodiscard]] float TriggersPerSecond(uint32_t now);
	/// fn_00448F90: every time later than now becomes now
	void ClampTimes(uint32_t now);
};

/// CameraHelpReason (bw1-decomp CameraHelp.h): the high byte picks the table, the low byte the entry
enum class CameraReason : int32_t
{
	Exclusion0 = 0x100, ///< 0x45DF7D (CameraExclusion::InsideExclusion)
	Exclusion1 = 0x101, ///< 0x45DF2C (CameraExclusion::InsideInclusion)
	Exclusion2 = 0x102, ///< 0x45C05D
	Move0 = 0x200,      ///< 0x45C235, 0x45CBA7, 0x45DC0C, 0x45F87A
	Move1 = 0x201,      ///< 0x45F77F
	Move2 = 0x202,      ///< 0x45FC10, 0x45FF30
	Move3 = 0x203,      ///< 0x45FE4A
	Rotate = 0x300,     ///< -> Event::Rotate (25)
	RotateCW = 0x301,   ///< -> 26
	RotateCCW = 0x302,  ///< -> 27
	Pitch = 0x303,      ///< -> 28
	Zoom = 0x304,       ///< -> 29
	DoubleClickPos = 0x305,    ///< -> 30
	DoubleClickObject = 0x306, ///< -> 31
	ZoomToCitadel = 0x307,     ///< -> 32
	Drag = 0x308,              ///< -> 33
};
/// CameraModeNew3::Update's input mask (the third argument, built at 0x45C06A..0x45C0C5), the names at 0x9CDDF0
enum class CameraInput : uint32_t
{
	Keyboard = 0x01,         ///< [0xC5B0E8] && ![0xC5B0EC]
	MouseButton = 0x02,      ///< the mode +0x8C & 1
	BothMouseButtons = 0x04, ///< [0xC5B0B0]
	WheelSpin = 0x08,        ///< the local +0x6C
	WheelDown = 0x10,        ///< [0xC5B0EC]
};

/// What HelpProfile::ProcessSpecialTriggers 0x5C45A0 reads of the player's camera mode (CameraModeNew3, nothing for
/// another mode): +0xC8 (the centre of the screen is on the land, inferred), +0x2E4 (the heading distance, inferred)
/// and GCamera::CalculatePitch 0x443070
struct CameraView
{
	bool screenCentreOnLand {false}; ///< CameraModeNew3 +0xC8
	float headingDistance {0.0f};    ///< CameraModeNew3 +0x2E4
	float pitch {0.0f};              ///< CalculatePitch
};
struct Queries
{
	/// g_game +0x14 & 4. Unset: game_clock::IsPaused()
	std::function<bool()> paused;
	/// HelpSystem +0x45E8 && +0x45EC. Unset: help::Get()->IsScriptWideScreen() (false without a help system)
	std::function<bool()> scriptWideScreen;
	/// [0xC4CCEE] (meaning pending: GGame clears it at 0x54B1C5 before GoInsideCitadel; the hand is not drawn while it
	/// is set). Unset: false
	std::function<bool()> processBlocked;
	/// The player's CameraModeNew3, nullopt while another mode (the script's) is current. (pending, camera) openblack's
	/// DefaultWorldCameraModel has no +0xC8 / +0x2E4 yet. Unset: nullopt (events 44..46 never come)
	std::function<std::optional<CameraView>()> playerCamera;
};
void SetQueries(Queries queries);

/// HelpProfile::Trigger 0x5C46E0(type): nothing paused or in a script's wide screen; the type's accumulator, then
/// 14..23 also GestureTotal (24, +0x1928), 1..42 AllInterface (43, +0x2D0C), 9..11 CastAll (11, +0xB8C) and every type
/// AllEvents (48, +0x3248)
void Trigger(Event type);
/// HelpProfile::Process 0x5C4660, once a game turn (GGame::ProcessTurn 0x54E6A9): nothing paused, in a script's wide
/// screen or with [0xC4CCEE]; else ProcessSpecialTriggers, the 49 EndTurn, AccumulatedTime += 100 and fn_00449240 (the
/// same EndTurn for CameraHelp's 12 accumulators)
void Process();
/// HelpProfile::ProcessSpecialTriggers 0x5C45A0: with the player's camera mode: centre on the land -> LookAtLand (44),
/// then heading distance < 15 [0x915448] and pitch > 0.55 [0x91544C] -> LookAtLandTooClose (45); not on the land and
/// pitch < -0.3 [0x915450] -> LookAtSky (46)
void ProcessSpecialTriggers();

/// CameraHelp::CameraHelpCallback 0x449140(reason, point, inputs): 0x3nn -> Trigger(25 + nn) and, for each bit of
/// `inputs`, CameraHelp's input accumulator (0xC5A320, 5); 0x2nn -> 0xC5A860 + nn (4); 0x1nn -> 0xC5AC90 + nn (3).
/// The point is not read
void CameraHelpCallback(CameraReason reason, uint32_t inputs);
/// The calls CameraModeNew3::Update makes for the player's rotate, pitch and zoom (0x45C38E..0x45C3AD, 0x45C6A8..
/// 0x45C838; spec_timers_events.md §2.3), from openblack's DefaultWorldCameraModel: `yaw` is its _rotateAroundDelta.y
/// ([0xC5B0C8], turned into radians as yaw x pi / width), `pitch` its _rotateAroundDelta.x ([0xC5B0C4], x 0.002) and
/// `zoom` its zoomDelta ([0xC5B0CC] x 0.0015 x the distance factor). Zoom when zoom != 0; Rotate when |yaw| > 0.01
/// (the double 0.01 [0x8C7A10]) and then RotateCW (yaw > 0) or RotateCCW (yaw < 0); Pitch when pitch != 0.
/// (approximate) the original also needs the mode +0x8C to be 2 or 3 and, for the pitch, no auto-pitch (bl); the
/// camera features (CameraHelp::EnabledFeatures 0x02 rotate, 0x01 pitch, 0x04 zoom) zero the deltas before, once
/// openblack's camera reads them (pending, camera)
void OnPlayerCameraMove(float yaw, float pitch, float zoom, uint32_t inputs);

/// GET_TOTAL_EVENTS 237 (GScript::GetTotalEvents 0x70B910) and the other two readers: nullopt for a type outside 1..48
/// (`jle` / `cmp 0x31; jl`: the script error "Invalid event" (0xC20664), the script gets 0.0)
[[nodiscard]] std::optional<float> TotalEvents(int32_t type);
/// GET_EVENTS_PER_SECOND 235 (0x70B7F0): GetTriggerPerSecond
[[nodiscard]] std::optional<float> EventsPerSecond(int32_t type);
/// GET_TIME_SINCE 236 (0x70B880): GetTimeSinceLastUsed
[[nodiscard]] std::optional<float> TimeSince(int32_t type);

[[nodiscard]] const Accumulator& Get(Event type);
/// HelpProfile::AccumulatedTime [0xC5AFD8]
[[nodiscard]] uint32_t AccumulatedTime();
/// HelpProfile::SetToZero 0x5C4770 (from its ctor 0x5C4548 -> 0x5C4590): the 49 Reset, then CameraHelp::ResetStats
/// 0x4491E0 (its 12). AccumulatedTime is a static and keeps going
void SetToZero();
/// The tests: SetToZero, AccumulatedTime 0 and no queries
void Reset();

} // namespace openblack::help_profile
