/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "QMixerLaws.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

using namespace openblack::audio;

float qmixer::Gain(int volume, int master)
{
	const int v = std::clamp(volume, 0, k_MaxVolume);
	const int m = std::clamp(master, 0, k_MaxVolume);
	// floor(m * v / 127) * 258 (0x100133C1..0x100133E3, 0x1001511D..0x10015143), / 32767 (0x18007AE8)
	return static_cast<float>(m * v / 127 * 258) / 32767.0f;
}

float qmixer::DistanceGain(float minDistance, float maxDistance, float scale, float distance)
{
	if (distance > maxDistance)
	{
		return 0.0f;
	}
	if (distance <= minDistance || scale == 0.0f)
	{
		return 1.0f;
	}
	if (scale == 1.0f)
	{
		return minDistance / distance;
	}
	return minDistance / ((distance - minDistance) * scale + minDistance);
}

glm::vec3 qmixer::PolarRelative(glm::vec3 position)
{
	// 0x100122BC..0x10012522 on the game's thread, so with the FPU at 24 bits (fn_007DEE00: fninit and `and cw, 0xFCFF`
	// at 0x7DEE0D): every fadd / fsub / fmul / fdiv / fsqrt rounds to a float's 24-bit mantissa, while the loads of the
	// doubles (and fcomp against them) are exact and fpatan keeps the full precision. The doubles of LHaudiodllR: 0
	// (0x10030460), 180 (0x10030458), 90 (0x10030448), 270 (0x10030440) and 0.31847133757961782 = 1 / 3.14 (0x10030450,
	// LHaudio's "1 / pi"; the old float reading of the disassembler showed it as 0.318471). Each angle is
	// atan(..) * 180 * that double, in that order; the QMIX_POLAR it sends (azimuth +0x20, range +0x24, elevation +0x28)
	// holds floats. Here each FPU step is a double operation rounded to a float (R).
	const auto R = [](double v) { return static_cast<float>(v); };
	constexpr double k_180 = 180.0;                  // 0x10030458
	constexpr double k_InvPi = 0.31847133757961782; // 0x10030450
	const auto degrees = [&R](double atanValue) { return R(R(atanValue * k_180) * k_InvPi); };
	const float x = position.x; // fld [esp+0x94] (fstp qword [esp+0x18], 0x100122C3: exact)
	const float y = position.y;
	const float z = position.z;
	const float xx = x * x;
	const float yy = y * y;
	const float range = std::sqrt((z * z + xx) + yy); // 0x100122D5..0x100122EB
	float elevation = 0.0f;                           // 0x10012304
	if (z != 0.0f)
	{
		if (y == 0.0f && x == 0.0f)
		{
			elevation = z < 0.0f ? -90.0f : 90.0f; // 0xC2B40000 / 0x42B40000 (0x1001233F / 0x10012349)
		}
		else
		{
			// 0x10012353..0x10012369: fadd, fsqrt, fdivr (24 bits each), fpatan, the two fmul
			elevation = degrees(std::atan(static_cast<double>(z / std::sqrt(xx + yy))));
		}
	}
	// __ftol 0x1001F874 (truncation) and the integer absolute value (cdq / xor / sub); fild is exact
	const auto ftolAbs = [](float v) { return static_cast<float>(std::abs(static_cast<int32_t>(v))); };
	float azimuth = 0.0f;
	if (x == 0.0f)
	{
		azimuth = y < 0.0f ? 180.0f : 0.0f; // 0x1001239C / 0x1001238F
	}
	else if (y == 0.0f)
	{
		azimuth = x > 0.0f ? 90.0f : 270.0f; // 0x100123C9 / 0x100123E9 (x is not 0 here)
	}
	else if (x > 0.0f)
	{
		azimuth = y > 0.0f ? R(90.0 - degrees(std::atan(static_cast<double>(y / x))))           // 0x10012414..0x1001242E
		                   : R(90.0 + degrees(std::atan(static_cast<double>(ftolAbs(y) / x)))); // 0x10012444..0x10012470
	}
	else
	{
		azimuth = y < 0.0f ? R(270.0 - degrees(std::atan(static_cast<double>(ftolAbs(y) / ftolAbs(x))))) // 0x1001249B..
		                   : R(270.0 + degrees(std::atan(static_cast<double>(y / ftolAbs(x)))));          // 0x100124EE..
	}
	// QMixer 0x1800AA85..0x1800AAFC, also at 24 bits: k = pi (the double 0x18037658) * the float 1 / 180 (0x18036550,
	// 0.0055555557), rounded; az = k * azimuth (stored, fstp [esp+0x64]) and el = k * elevation; flat = cos(el) * range
	// and up = sin(el) * range; right = sin(az) * flat and ahead = cos(az) * flat (fsin / fcos at full precision, each
	// fmul rounded)
	constexpr double k_Pi = 3.1415926535897931;            // 0x18037658
	constexpr float k_InvDegrees = 0.0055555556900799274f; // 0x18036550
	const float k = R(k_Pi * static_cast<double>(k_InvDegrees));
	const float az = k * azimuth;
	const float el = k * elevation;
	const float flat = R(std::cos(static_cast<double>(el)) * range);
	const float up = R(std::sin(static_cast<double>(el)) * range);
	const float right = R(std::sin(static_cast<double>(az)) * flat);
	const float ahead = R(std::cos(static_cast<double>(az)) * flat);
	return {right, up, ahead};
}

float qmixer::FrequencyRatio(int sampleRate, int percent)
{
	if (sampleRate <= 0)
	{
		return static_cast<float>(percent) / 100.0f;
	}
	const auto frequency = static_cast<uint32_t>(sampleRate) * static_cast<uint32_t>(std::max(percent, 0)) / 100u;
	return static_cast<float>(frequency) / static_cast<float>(sampleRate);
}

int qmixer::StartPitch(int pitch, int deviation, int rand15)
{
	auto p = static_cast<uint32_t>(pitch > 0 ? pitch : 100);
	const uint32_t d = static_cast<uint32_t>(std::max(deviation, 0)) * p / 100;
	p -= d;
	p += static_cast<uint32_t>(std::clamp(rand15, 0, 32767)) * (2 * d) / 32767;
	return p == 0 ? 100 : static_cast<int>(p);
}
