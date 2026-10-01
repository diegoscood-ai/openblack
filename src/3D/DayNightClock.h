/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

namespace openblack
{

/// The original's day/night clock (GLandAlignement + GGameInfo + LH3DSky).
///
/// Two clocks in hours 0..24:
/// - visual time (GLandAlignement::VisualTime 0xBF3380) runs linearly, one day in `duration` seconds of game time
///   (1700 s by default), advanced once per game turn by UpdateTime 0x5E1FE0 (from GGame::ProcessTurn);
/// - script time is the visual time remapped piecewise-linearly so that the day/night thresholds of the cycle
///   (SetDayNightTimes 0xFA26A0..0xFA2694) land on the standard hours 3.5 / 7.5 / 8 / 8.5 (0xC395B0..0xC395BC).
///   GET_GAME_TIME / SET_GAME_TIME, the sun, the moon and the sky textures use it (fn_0086A160 / fn_0086A110).
class DayNightClock
{
public:
	/// Standard (script time) thresholds: full night, dusk start, dusk end, full day (0xC395B0..0xC395BC)
	static constexpr std::array<float, 4> k_StandardTimes = {3.5f, 7.5f, 8.0f, 8.5f};
	/// ResetGameTimeProperties 0x711520: 1700 s per day, 8.3 % night, 7 % change
	static constexpr float k_DefaultDuration = 1700.0f;
	static constexpr float k_DefaultNight = 0.083f;
	static constexpr float k_DefaultChange = 0.07f;

	/// GLandAlignement::Open 0x5E1D10: scale 1, default cycle, noon
	void Reset();
	/// GGame::ProcessTurn 0x54E6AE: UpdateTime(scale * 0.1, 0.1) once per game turn
	void ProcessTurn();

	/// GGameInfo::SetVisualTimeCycle 0x557620 (SET_GAME_TIME_PROPERTIES; fractions of the whole day)
	void SetCycle(float duration, float night, float change);
	/// SetVisualTimeCycleFromMapEditor 0x557BB0 (Land script SET_NIGHTTIME): night <= 1, change <= 1 - night
	void SetCycleFromMapEditor(float duration, float night, float change);
	/// SetVisualTimeScale 0x557610 (GAME_TIME_ON_OFF): 1 runs the clock, 0 stops it
	void SetScale(float scale) { _scale = scale; }
	/// SET_GAME_TIME: ForceVisualTime(ScriptToVisual(hour)) 0x5E22A0, with the sky's jump (sky_type::Jump)
	void ForceScriptTime(float hour);
	/// MOVE_GAME_TIME: SetTime(ScriptToVisual(hour), seconds) 0x5E22E0, the visual time slides there in `seconds`
	void MoveScriptTime(float hour, float seconds);

	[[nodiscard]] float GetVisualTime() const { return _visualTime; }
	/// GET_GAME_TIME (fn_0086A160)
	[[nodiscard]] float GetScriptTime() const { return VisualToScript(_visualTime); }
	/// LH3DSky::Time2SkyType 0x86A1B0 on the visual time, computed now: 2 night, 1 dusk, 0 day (sky_type::At). What the
	/// sky drew this frame is sky_type::Frame().
	[[nodiscard]] float GetSkyType() const { return Time2SkyType(_visualTime); }
	/// GGameInfo::IsVisualNight 0x5575E0: sky type > the double 1.2 (sky_type::IsVisualNight)
	[[nodiscard]] bool IsVisualNight() const;

	[[nodiscard]] float ScriptToVisual(float hour) const;
	[[nodiscard]] float VisualToScript(float hour) const;
	/// sky_type::At with this clock's thresholds
	[[nodiscard]] float Time2SkyType(float hour) const;
	[[nodiscard]] const std::array<float, 4>& GetDayNightTimes() const { return _times; }

private:
	/// GLandAlignement::SetTime 0x5E22E0: new target, and with `seconds` != 0 the slide speed
	void SetTarget(float hour, float seconds);

	float _visualTime {12.0f};     ///< 0xBF3380
	float _target {12.0f};         ///< 0xBF3384
	float _step {2.5f};            ///< 0xBF3388: hours per second of game time towards the target
	float _moveSeconds {0.0f};     ///< 0xD1A268: MOVE_GAME_TIME in progress
	float _dayRate {0.0f};         ///< 0xBF338C: hours per second
	float _nightRate {0.0f};       ///< 0xBF3390
	float _scale {1.0f};           ///< GGameInfo +0x44
	std::array<float, 4> _times {}; ///< 0xFA26A0, 0xFA269C, 0xFA2698, 0xFA2694
};

} // namespace openblack
