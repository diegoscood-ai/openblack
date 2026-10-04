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

#include <optional>

#include <entt/entity/fwd.hpp>

/// The scripts' timers of runblack.exe W120 (ScriptTimer.cpp, research dev\documentacion\intro\spec_timers_events.md).
///
/// - A ScriptTimer (vtable 0x8E0BAC, 0x30 bytes, fn_007115A0) is a GameThingWithPos at (0, 0, 0) (SetToZero 0x5705D0)
///   with two words: +0x28 the game turn it was set at (g_game +0x205A40, game_clock::Turn()) and +0x2C its length in
///   turns. It is never processed: the time left is worked out from the turn when asked, so it runs with the game turns
///   (it stops in pause and follows the game speed). GetScriptObjectType 0x711600 = 0x11 (SCRIPT_OBJECT_TYPE_TIMER),
///   IsScriptTimer 0x5612F0 = 1, GetSaveType 0x561310 = 0x7D, IsDeletedWhenReleasedFromScript 0x561300 = 1: when no
///   script variable holds it any more it is deleted (GScript::ReleaseScriptThingIntoTheGame 0x70F61E..0x70F670).
/// - The countdown timer is GScript's own (g_game +0x250090): +8 on, +0xC turns left, +0x10 shown. GScript::Process
///   0x6EB6BA counts it down once a turn (fn_006EB930); GGame::Process3dEngine 0x54E449..0x54E4B4 draws it.
///
/// openblack: a timer is an entity with only components::ScriptTimer (no Transform: the original's position is never
/// set and nothing reads it); its script slot is ecs::script_held's. Its deletion on release is ScriptHeld.cpp's
/// (edit_timers_scriptheld.py, pending review by Personas): without it a timer stays until the end of the land.
namespace openblack::ecs::components
{

/// ScriptTimer +0x28 / +0x2C
struct ScriptTimer
{
	uint32_t startTurn {0};     ///< +0x28: game_clock::Turn() when set (fn_00711610)
	int32_t durationTurns {0};  ///< +0x2C: the length in turns (GGameInfo::NumGameTicksPerSecond 0x711630's result)
};

} // namespace openblack::ecs::components

namespace openblack::ecs::script_timer
{

/// fn_00711610(turns) (thiscall): +0x28 = g_game +0x205A40, +0x2C = turns
void SetTurns(components::ScriptTimer& timer, int32_t turns, uint32_t now);
/// 0x711630 (misnamed GGameInfo::NumGameTicksPerSecond in the symbols; thiscall on the timer): fn_00711610(ftol(1000 /
/// [0xD01A38] (unsigned div) x seconds)) = game_clock::TicksForSeconds
void SetSeconds(components::ScriptTimer& timer, float seconds, uint32_t now);
/// fn_007116C0: g_game +0x205A40 - +0x28 (unsigned)
[[nodiscard]] uint32_t ElapsedTurns(const components::ScriptTimer& timer, uint32_t now);
/// fn_00711670: d = +0x2C - elapsed (signed, `jns`), 0 below 0; fild d, fimul [0xD01A38], fmul 0.001f [0x8AA3B0]
[[nodiscard]] float RemainingSeconds(const components::ScriptTimer& timer, uint32_t now, uint32_t msPerTurn);
/// fn_007116D0: fild elapsed, fimul [0xD01A38], fmul 0.001f (it keeps growing after the timer ran out)
[[nodiscard]] float SecondsSinceSet(const components::ScriptTimer& timer, uint32_t now, uint32_t msPerTurn);

/// fn_007115A0(seconds): new ScriptTimer, SetSeconds(seconds) at the current turn. CREATE_TIMER 146 (0x6F1D20) and
/// CREATE 027 with SCRIPT_OBJECT_TYPE_TIMER (fn_006F11A0 0x6F1253: the sub-type as the seconds, fild qword). The
/// caller adds it to the script (AddScriptGameThing(t, 1)). (not ported) GameThing::SetScriptNameOfCreate 0x56FA70
[[nodiscard]] entt::entity Create(float seconds);
/// IsScriptTimer (vt +0x4D0)
[[nodiscard]] bool IsTimer(entt::entity thing);
/// SET_TIMER_TIME 0x711318..0x711324 on a timer: false when `thing` is not one
bool SetTime(entt::entity thing, float seconds);
/// GET_TIMER_TIME_REMAINING 0x7113EE: nullopt when `thing` is not a timer
[[nodiscard]] std::optional<float> Remaining(entt::entity thing);
/// GET_TIMER_TIME_SINCE_SET 0x71148E: nullopt when `thing` is not a timer
[[nodiscard]] std::optional<float> SinceSet(entt::entity thing);

/// ScriptTimer::Save 0x711700 / Load 0x7117B0 write and read, after GameThingWithPos's own block, +0x28 then +0x2C (4
/// bytes each). (not ported) openblack does not save the scripts' things; this is the record those would write
struct SaveRecord
{
	uint32_t startTurn;
	int32_t durationTurns;
};
[[nodiscard]] SaveRecord ToSave(const components::ScriptTimer& timer);
[[nodiscard]] components::ScriptTimer FromSave(const SaveRecord& record);

} // namespace openblack::ecs::script_timer

namespace openblack::ecs::script_countdown
{

/// GScript +0x8 / +0xC / +0x10
struct State
{
	bool on {false};        ///< +0x8 (COUNTDOWN_TIMER_EXISTS 0x7111D0 pushes it)
	int32_t turnsLeft {0};  ///< +0xC
	bool shown {false};     ///< +0x10 (HIDE 0x7111F0 / REVEAL 0x711210)
};

/// GScript::InitialiseCountDownTimer 0x6EB8B0 (START_COUNTDOWN_TIMER 084, 0x711150): on and shown; seconds <= 0 is the
/// script error "Invalid time for timer" (returned as false), below 0 it counts as 0; turns = ftol(1000 / [0xD01A38] x
/// seconds)
bool Start(float seconds);
/// REMOVE_COUNTDOWN_TIMER 087 (0x711180): +8 = 0
void Remove();
/// GetCountDownTimerRemainingTime 0x6EB950 (GET_COUNTDOWN_TIMER 092, 0x7111A0): turns left / (1000 / [0xD01A38]), both
/// unsigned integer divisions, as a float: whole seconds (it reads +0xC even when the timer is off)
[[nodiscard]] float RemainingSeconds();
/// COUNTDOWN_TIMER_EXISTS 099 (0x7111D0)
[[nodiscard]] bool Exists();
/// HIDE_COUNTDOWN_TIMER 103 (0x7111F0) / REVEAL_COUNTDOWN_TIMER 144 (0x711210)
void SetShown(bool shown);
/// fn_006EB930 from GScript::Process 0x6EB6BA (once a turn, before the scripts run): when on, --turns, off at 0
void ProcessTurn();
/// What GGame::Process3dEngine 0x54E449..0x54E4B4 draws while on and shown: sprintf "Time: %.1f" of RemainingSeconds
/// at (320, 90), size 24.0, red below 10 s else green (0x54E47B..0x54E4B4: 0xFF in the first colour argument below 10,
/// in the second otherwise; (inferred) the arguments are r, g, b), through CreatureMentalEditor::DrawTextA 0x4DF310.
/// (pending) openblack has no such text drawer: nobody draws it yet
struct Display
{
	bool visible;
	float seconds;
	bool red;
};
[[nodiscard]] Display GetDisplay();
[[nodiscard]] const State& Get();
/// A new game / the tests ((inferred) GScript's ctor leaves the three at 0; not read)
void Reset();

} // namespace openblack::ecs::script_countdown
