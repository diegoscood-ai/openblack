/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyType.h"

#include <cassert>
#include <cmath>

using namespace openblack;

namespace
{
constexpr float k_Half = 12.0f;        ///< [0x8CF1C0]
constexpr float k_Day = 24.0f;         ///< [0x8CA26C]
constexpr float k_Night = 2.0f;        ///< [0x8AB478]
constexpr float k_Dusk = 1.0f;         ///< [0x8AA390]
constexpr double k_NightAbove = 1.2;   ///< the double [0x8D8758] (0x5575F3)
constexpr float k_ColumnHours = 6.0f;  ///< [0x8AB35C] (0x86985E)
constexpr float k_ColumnScale = 2.5f;  ///< [0x8C581C]
constexpr float k_WeightSteps = 255.0f; ///< [0x8AB270] (0x86B8A6 / 0x86B8C3)

/// [0xFA26A0], [0xFA269C], [0xFA2698], [0xFA2694]: 0 in the file
sky_type::Thresholds g_thresholds {};
/// [0xFA26BC]
float g_frame = 0.0f;
/// [0xFA26C4]
float g_frameHour = 0.0f;
sky_type::DomeBlend g_dome;
} // namespace

void sky_type::SetThresholds(float a, float b, float c, float d)
{
	g_thresholds = {a, b, c, d};
}

const sky_type::Thresholds& sky_type::GetThresholds()
{
	return g_thresholds;
}

float sky_type::At(float hour, const Thresholds& thresholds)
{
	// 0x86A1B1..0x86A1D4: `test ah, 0x41` / `jne`, so only hour > 12 folds (not 12 itself, not NaN)
	const float t = hour > k_Half ? k_Day - hour : hour;
	// 0x86A1DC..0x86A246: each compare is `fcomp` / `test ah, 1` / `je`, so C0 = 1 takes the branch: "<" or unordered.
	// Written as !(t >= x) so that a NaN hour gives 2 (night) like the original.
	if (!(t >= thresholds[0]))
	{
		return k_Night;
	}
	if (!(t >= thresholds[1]))
	{
		// 0x86A206..0x86A21A: fdivp (DE F9) is st1 / st0, (h - A) / (B - A)
		return k_Night - (t - thresholds[0]) / (thresholds[1] - thresholds[0]);
	}
	if (!(t >= thresholds[2]))
	{
		return k_Dusk;
	}
	if (!(t >= thresholds[3]))
	{
		return k_Dusk - (t - thresholds[2]) / (thresholds[3] - thresholds[2]);
	}
	return 0.0f; // the local set to 0 at 0x86A1B5
}

float sky_type::At(float hour)
{
	return At(hour, g_thresholds);
}

void sky_type::SampleFrame(float visualHour)
{
	// 0x86A2C4..0x86A308: while (h < 0) h += 24; if (!(h < 24)) do h -= 24 while (!(h < 24)), in the x87 register.
	// Float here: the same hypothesis as DayNightClock::SetCycle, the FPU at 24 bits (D3D's default, inferido), so
	// h = -1e-7 gives 24 - 1e-7 -> 24 -> 0 (with 53 / 64 bits it would stay 23.9999999 and be stored as 24.0f).
	// A NaN loops forever in the original (fcom unordered sets C0); here it falls through (openblack difference).
	float h = visualHour;
	while (h < 0.0f)
	{
		h += k_Day;
	}
	while (h >= k_Day)
	{
		h -= k_Day;
	}
	g_frameHour = h;                      // 0x86A310
	g_frame = At(g_frameHour);            // 0x86A317..0x86A31C
}

float sky_type::Frame()
{
	return g_frame;
}

float sky_type::FrameHour()
{
	return g_frameHour;
}

void sky_type::Jump(float visualHour)
{
	SampleFrame(visualHour); // 0x86A275
	g_dome.Jump(g_frame);    // 0x86A280..0x86A2A9
}

bool sky_type::IsVisualNight(float skyType)
{
	// fcomp qword [0x8D8758]; strict
	return static_cast<double>(skyType) > k_NightAbove;
}

float sky_type::EveningRamp(float visualHour, float width, float offset)
{
	const float d = g_thresholds[3];       // [0xFA2694]
	const float u = k_Day - visualHour;    // 0x557AE8..0x557AEE, stored
	const float s = d + offset;            // 0x557AF2..0x557AFC, stored
	if (u < s)
	{
		return 1.0f; // 0x557B0F
	}
	if (!((d + width) + offset > u))
	{
		return 0.0f; // 0x557B4C
	}
	return 1.0f - (u - s) / width; // 0x557B34..0x557B40
}

float sky_type::LightColumn(float skyType)
{
	float x = (k_Night - skyType) * k_ColumnHours;
	if (x > k_Half)
	{
		x = k_Day - x; // the fold of 0x86986C (fsubrp DE E2); never taken for a sky type in [0, 2]
	}
	return x * k_ColumnScale;
}

float sky_type::HazeFactor(float skyType)
{
	const float v = skyType > k_Dusk ? k_Night - skyType : skyType; // 0x869D5F..0x869D6E (fsubr D8 2D)
	return v * v;
}

sky_type::DomeWeight sky_type::DomeWeightOf(float skyType)
{
	// 0x86B890..0x86B8D3; __ftol truncates
	if (!(skyType > k_Dusk))
	{
		return {0, 1, static_cast<int>(skyType * k_WeightSteps)};
	}
	return {1, 2, static_cast<int>((skyType - k_Dusk) * k_WeightSteps)};
}

uint16_t sky_type::BlendTexel555(uint16_t lower, uint16_t upper, int weight)
{
	// 0x86B9F6..0x86BA03: the caller passes 255 - w (0x86B8E3), the callee masks it with 0xFF and takes 255 - that
	const int lowWeight = (255 - weight) & 0xFF;
	const int highWeight = 255 - lowWeight;
	auto channel = [&](int shift) {
		const int lo = (lower >> shift) & 0x1F;
		const int hi = (upper >> shift) & 0x1F;
		// 0xFA2554[i] = (i lowWeight) >> 8, 0xFA2514[i] = (i highWeight) >> 8 (0x86BA10..0x86BA3A)
		return ((lo * lowWeight) >> 8) + ((hi * highWeight) >> 8);
	};
	// 0x86BAE0..0x86BB3D: red (>> 10), green (>> 5), blue; the shifts leave bit 15 at 0
	return static_cast<uint16_t>((channel(10) << 10) | (channel(5) << 5) | channel(0));
}

void sky_type::BlendRows555(std::span<uint16_t> dst, std::span<const uint16_t> lower, std::span<const uint16_t> upper,
                            int weight)
{
	assert(lower.size() >= dst.size() && upper.size() >= dst.size());
	for (size_t i = 0; i < dst.size(); ++i)
	{
		dst[i] = BlendTexel555(lower[i], upper[i], weight);
	}
}

sky_type::DomeBlend::Blocks sky_type::DomeBlend::Advance(float frameSkyType)
{
	Blocks out;
	if (_rebuildPending)
	{
		out.blocks[out.count++] = {_rebuildSkyType, 0, k_Rows};
		_rebuildPending = false;
	}
	// 0x86A34E..0x86A379: fld / fsub / fabs in the x87 register, rounded to float under the 24-bit FPU hypothesis
	// of DayNightClock::SetCycle (inferido), then fcomp against the double [0x99A168]
	if (_rowsDone >= k_Rows && static_cast<double>(std::fabs(frameSkyType - _built)) > k_Hysteresis)
	{
		_built = frameSkyType;
		_rowsDone = 0;
	}
	// 0x86A37F..0x86A39D
	if (_rowsDone < k_Rows)
	{
		out.blocks[out.count++] = {_built, _rowsDone, k_RowsPerFrame};
		_rowsDone += k_RowsPerFrame;
	}
	return out;
}

void sky_type::DomeBlend::Jump(float frameSkyType)
{
	_built = frameSkyType; // 0x86A299
	_rowsDone = 0;         // 0x86A29F
	_rebuildPending = true;
	_rebuildSkyType = frameSkyType; // fn_0086B7F0(T, 0, rows) at 0x86A2A9
}

sky_type::DomeBlend& sky_type::Dome()
{
	return g_dome;
}
