/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Confirmation.h"

#include <cmath>

#include "Audio/Audio.h"
#include "Audio/GAudio/Banks.h"
#include "Common/GameRandom.h"

namespace openblack::audio::confirmation
{
namespace
{
float s_Angle = 0.0f; ///< g_angle [0xC5E158]
float s_Pitch = 0.0f; ///< g_pitch [0xC5E15C]
State s_State;        ///< GConfirmation [0xD99BA0] (Init 0x71A560 from InitOneTimeOnly 0x54F03D: all 0)

void Feed(float& value, float a, float b)
{
	value = (a / b - value) * k_Smoothing + value; // fld a; fdiv b; fsub g; fmul 0.4; fadd g; fstp g
}
} // namespace

void FeedAngle(float a, float b)
{
	Feed(s_Angle, a, b); // fn_00454900
}

void FeedPitch(float a, float b)
{
	Feed(s_Pitch, a, b); // fn_00454930
}

float Angle()
{
	return s_Angle;
}

float Pitch()
{
	return s_Pitch;
}

void Start(State& state, const float* value, float range)
{
	state.value = value; // 0x71A618
	state.range = range; // 0x71A61F
	state.active = true;
	state.lastSay = 0;
	state.lastBetter = 0;
}

void Stop(State& state)
{
	state.active = false; // 0x71A640
}

void StartAngleSound(bool on)
{
	if (on)
	{
		s_Angle = 0.0f; // 0x70FFC1
		Start(s_State, &s_Angle, k_AngleRange);
	}
	else
	{
		Stop(s_State);
	}
}

void StartPitchSound(bool on)
{
	if (on)
	{
		s_Pitch = 0.0f; // 0x710001
		Start(s_State, &s_Pitch, k_PitchRange);
	}
	else
	{
		Stop(s_State);
	}
}

int Step(State& state, uint32_t turn, const std::function<float(float)>& floatRand,
         const std::function<uint32_t(int32_t)>& rand)
{
	if (!state.active || state.value == nullptr) // 0x71A656
	{
		return 0;
	}
	float v = *state.value / state.range; // 0x71A663..0x71A665
	if (!(v >= -1.0f))                    // fcom [0x8AB678]; test ah, 1: below (or unordered)
	{
		v = -1.0f;
	}
	else if (!(v <= 1.0f)) // fcom 1; test ah, 0x41
	{
		v = 1.0f;
	}
	const float a = std::fabs(v); // 0x71A697
	if (a == 1.0f)                // fcomp 1; test ah, 0x40
	{
		if (turn - state.lastBetter <= k_BetterTurns) // cmp edx, 0x64; jbe
		{
			return 0;
		}
		state.lastSay = turn;    // fn_0071A850 +0x1C
		state.lastBetter = turn; // 0x71A6D6
		return k_Better;
	}
	if (a > 0.0f) // fcomp 0; test ah, 0x41; jne
	{
		// fn_0071A730 -> fn_0071A8F0(a): the turns first, then the draw
		if (turn - state.lastSay <= k_SayTurns || !(std::fabs(a) > floatRand(1.0f)))
		{
			return 0;
		}
		const int sample = static_cast<int>(rand(k_YesEnd - k_Yes)) + k_Yes; // 0x71A741..0x71A752
		state.lastSay = turn;                                                // 0x71A77B
		return sample;
	}
	if (a < 0.0f) // 0x71A700: fn_0071A7F0, 1689 + LocalRand(14), unreachable (a = |v|)
	{
		if (turn - state.lastSay <= k_SayTurns || !(std::fabs(a) > floatRand(1.0f)))
		{
			return 0;
		}
		state.lastSay = turn;
		return static_cast<int>(rand(k_NoEnd - k_No)) + k_No;
	}
	return 0;
}

void Process(uint32_t turn)
{
	const int sample = Step(
	    s_State, turn, [](float x) { return game_random::LocalFloatRand(x); },
	    [](int32_t n) { return game_random::LocalRand(n); });
	if (sample == 0)
	{
		return;
	}
	// the options of Init 0x71A560: bank GAudio+0x3C0 (HelpSprites), volume 100, owner 0, 2D (+0x08 = 0), no track
	// (+0x0C = 0); the sample in +0x24; GAudio::PlaySoundEffect 0x429E30 (0x71A76B / 0x71A869)
	PlayOptions options;
	options.sample = {Bank(SfxBank::HelpSprites), sample};
	options.volume = k_Volume;
	options.is3D = false;
	options.track = false;
	PlaySoundEffect(options);
}

State& Get()
{
	return s_State;
}

} // namespace openblack::audio::confirmation
