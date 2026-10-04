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

#include <bit>
#include <functional>

// GConfirmation (SoundConfirmation.cpp, the static at 0xD99BA0; research dev\documentacion\intro\spec_jc_specials.md
// section 11): the good spirit's "yes" while the player turns or tilts the camera the way a tutorial asks
// (TeachRotate / TeachPitch in HandDemos.txt). START_ANGLE_SOUND 285 (GScript::StartAngleSound 0x70FFA0) watches the
// turn, CHL 348 (also named START_ANGLE_SOUND, GScript::StartPitchSound 0x70FFE0) the tilt; GGame::ProcessTurn 0x54E731
// runs Process once a turn. The camera feeds the two values (CameraModeNew3, fn_00454900 / fn_00454930).
namespace openblack::audio::confirmation
{

inline constexpr float k_Smoothing = std::bit_cast<float>(0x3ECCCCCDu); ///< [0x8C7A44] 0.4
inline constexpr float k_AngleRange = std::bit_cast<float>(0x40E00000u); ///< 7 (0x70FFB7)
inline constexpr float k_PitchRange = std::bit_cast<float>(0x40A00000u); ///< 5 (0x70FFF7)
inline constexpr uint32_t k_SayTurns = 0x14;    ///< fn_0071A8F0 0x71A901: more than 20 turns since the last sample
inline constexpr uint32_t k_BetterTurns = 0x64; ///< Process 0x71A6BC: more than 100 turns since the last "better"
/// HelpSprites (GAudio+0x3C0) samples of the table rows read (the third field of 12-byte rows):
inline constexpr int k_Yes = 1704;     ///< [0x963BA0] HELP_TEXT_ROTATION_YES_01; LocalRand(1718 [0x963C48] - 1704)
inline constexpr int k_YesEnd = 1718;
inline constexpr int k_No = 1689;      ///< [0x963C54] ROTATION_NO_01; LocalRand(1703 [0x963CFC] - 1689): never reached
inline constexpr int k_NoEnd = 1703;
inline constexpr int k_Better = 1686;  ///< [0x963DA4] ROTATION_BETTER_14 ("Estupendo"), fn_0071A850
inline constexpr int k_Volume = 0x64;  ///< Init 0x71A58C: +0x28 = 100 (+0x2C = 90: not modelled, as in Guidance)

/// What the per-turn step reads and writes (the static GConfirmation at 0xD99BA0)
struct State
{
	const float* value {nullptr}; ///< +0x08: g_angle [0xC5E158] or g_pitch [0xC5E15C]
	float range {1.0f};           ///< +0x0C
	uint32_t lastBetter {0};      ///< +0x14 (0xD99BB4)
	bool active {false};          ///< +0x18 (0xD99BB8)
	uint32_t lastSay {0};         ///< +0x1C (0xD99BBC)
};

/// fn_00454900(a, b) / fn_00454930(a, b) (CameraModeNew3): g = (a / b - g) x 0.4 + g, each step a float; a is the turn
/// (tilt) of the frame and b the camera's seconds of the frame ([esp+0xAC] = GetCameraTimeInc x 0.001), or a = 0 on the
/// frames without one (0x46053F / 0x46055C). (pending) the seven call sites wait for the port of CameraModeNew3::Update
void FeedAngle(float a, float b);
void FeedPitch(float a, float b);
[[nodiscard]] float Angle(); ///< [0xC5E158]
[[nodiscard]] float Pitch(); ///< [0xC5E15C]

/// GScript::StartAngleSound 0x70FFA0 after its POP: on, g_angle = 0 and Start(&g_angle, 7, 1); off, Stop
void StartAngleSound(bool on);
/// GScript::StartPitchSound 0x70FFE0: the same with g_pitch and 5
void StartPitchSound(bool on);

/// Start 0x71A610(value, range, unused): +0x08, +0x0C, active, +0x1C = +0x14 = 0. Stop 0x71A640: not active
void Start(State& state, const float* value, float range);
void Stop(State& state);

/// Process 0x71A650 as a pure step: the sample to play this turn, 0 for none. v = clamp(value / range, -1, 1) (the x87
/// compares: a NaN becomes -1), a = |v|; a == 1: 1686 when more than 100 turns since the last one (both stamps set);
/// a > 0: fn_0071A730, when more than 20 turns since the last sample and a > LocalFloatRand(1) (drawn only then), 1704 +
/// LocalRand(14). The "no" branch (v < 0, 1689 + LocalRand(14)) tests a, never negative: it never plays (the original's
/// bug, kept)
[[nodiscard]] int Step(State& state, uint32_t turn, const std::function<float(float)>& floatRand,
                       const std::function<uint32_t(int32_t)>& rand);
/// Once a turn (GGame::ProcessTurn 0x54E731, from audio::guidance::ProcessGameTurn): Step on the game's local random
/// stream, then GAudio::PlaySoundEffect 0x429E30 of the sample, 2D, volume 100, no owner
void Process(uint32_t turn);

/// The one GConfirmation (for the tests)
[[nodiscard]] State& Get();

} // namespace openblack::audio::confirmation
