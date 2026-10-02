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
#include <span>

/// The sky type of the original, LH3DSky's continuous 2 (night) .. 1 (dusk) .. 0 (day) of the visual hour, and
/// everything built on it. Wiki: day-night-weather.md, "Tipo de cielo".
///
/// The thresholds are LH3DSky's statics [0xFA26A0] / [0xFA269C] / [0xFA2698] / [0xFA2694] (A..D), written only by
/// SetDayNightTimes 0x869FA0 from GGameInfo::SetVisualTimeCycle 0x557620 (DayNightClock::SetCycle) and, once, by the
/// sky's set-up fn_0086A3B0 (4.5 / 7 / 7.5 / 8.25, 0x86A3B7..0x86A3E0, overwritten right after by the cycle). In the
/// campaign they are always 0.786 / 1.206 / 1.626 / 2.046 visual hours (the default cycle 1700 / 0.083 / 0.07; no CHL
/// script calls SET_GAME_TIME_PROPERTIES and Land2 / Land3 SET_NIGHTTIME repeat the defaults).
///
/// Once a frame the sky drawing samples the visual hour (DrawSky 0x5E2226 -> fn_0086A2C0): that is Frame() /
/// FrameHour(), what the sky dome, the land light table and the sound map read. Code that calls Time2SkyType on the
/// visual time itself in the original (fn_005E5830, IsVisualNight, UpdateTime, the fireflies) uses At(visual hour).
///
/// Convention: this is the original's sky type. openblack's old Sky::GetCurrentSkyType ran the other way (0 night ..
/// 2 day, = 2 - sky type) on the script hour with made-up thresholds; it is gone (LandLightTable::Build takes T).
namespace openblack::sky_type
{

/// A..D: [0xFA26A0], [0xFA269C], [0xFA2698], [0xFA2694]
using Thresholds = std::array<float, 4>;

/// fn_0086A3B0's SetDayNightTimes(4.5, 7, 7.5, 8.25) (pushes 0x40900000 / 0x40E00000 / 0x40F00000 / 0x41040000)
constexpr Thresholds k_SetUpThresholds = {4.5f, 7.0f, 7.5f, 8.25f};

/// LH3DSky::SetDayNightTimes 0x869FA0: A -> 0xFA26A0, B -> 0xFA269C, C -> 0xFA2698, D -> 0xFA2694
void SetThresholds(float a, float b, float c, float d);
[[nodiscard]] const Thresholds& GetThresholds();

/// LH3DSky::Time2SkyType 0x86A1B0 with the given thresholds (DayNightClock::Time2SkyType forwards here): fold at 12
/// only when hour > 12, then strict < in turn against A..D: 2, 2 - (h - A) / (B - A), 1, 1 - (h - C) / (D - C), 0.
/// No division by zero can happen: with A = B (or C = D) the ramp branch is unreachable.
[[nodiscard]] float At(float hour, const Thresholds& thresholds);
/// The same with LH3DSky's thresholds
[[nodiscard]] float At(float hour);

/// fn_0086A2C0: the hour brought into [0, 24) (0x86A2C4..0x86A308), [0xFA26C4] = hour (0x86A310) and [0xFA26BC] =
/// Time2SkyType(hour) (0x86A31C). DrawSky 0x5E21FD..0x5E2226 calls it once a frame with GLandAlignement::VisualTime
/// [0xBF3380]: Renderer::DrawScene does, before the land light table and the dome.
void SampleFrame(float visualHour);
/// [0xFA26BC], the sky type of the last SampleFrame (0 before the first one, .bss)
[[nodiscard]] float Frame();
/// [0xFA26C4], the hour of the last SampleFrame
[[nodiscard]] float FrameHour();

/// fn_0086A270, the hour jump of fn_005E22A0 (0x5E22CB; ForceVisualTime 0x5575D0, DayNightClock::ForceScriptTime):
/// SampleFrame(hour), then the dome is rebuilt whole at once with that sky type (DomeBlend::Jump). Its tail jump to
/// the light table fn_00869850 is not needed: openblack rebuilds the table every frame (Renderer::UpdateLandLight).
void Jump(float visualHour);

/// GGameInfo::IsVisualNight 0x5575E0: Time2SkyType(GetVisualTime()) > 1.2, against the double [0x8D8758] (bytes 33 33
/// 33 33 33 33 F3 3F, exactly 1.2), so the float 1.2f (1.20000005) is already night. ChildAtCreche 0x757CFF has the
/// same compare inline.
[[nodiscard]] bool IsVisualNight(float skyType);

/// fn_00557AE0 (thiscall on GGameInfo, `ret 8`), the evening ramp of the villagers' Relaxation 0x7488C0 / Sleep
/// 0x748960 desires: u = 24 - visual (stored as float), s = D + o (stored); u < s -> 1; !((D + w) + o > u) -> 0
/// (0x557B4C); else 1 - (u - s) / w. Callers pass (0.5 info+0xEC, 0.5 info+0xEC) at 0x748907 and (1, 0) at 0x74898F.
/// No openblack caller yet (the town's desires, V3).
[[nodiscard]] float EveningRamp(float visualHour, float width, float offset);

/// The time column of the land light table, fn_00869850 0x86985E..0x8698AD: x = (2 - T) * 6.0f ([0x8AB35C]), a fold
/// x > 12 -> 24 - x that can never run, then x * 2.5f ([0x8C581C]) = (2 - T) * 15; the caller takes ftol of it as the
/// column and the fraction * 256 ([0x8D45CC]) as the weight.
[[nodiscard]] float LightColumn(float skyType);
/// The haze factor of fn_00869850 0x869D5F..0x869D7A: v = T > 1 ? 2 - T : T, v * v (0 by day and in full night, 1 in
/// full dusk); with v * v > 0 the near / far haze distances become v2 * 0.0075 + 0.0025 and v2 * 0.00013888883 +
/// 0.0011111111 ([0x9A3BDC], [0x9A3B18], [0x9A3BD8], [0x9A3BE0])
[[nodiscard]] float HazeFactor(float skyType);

/// One fn_0086B7F0(T, first row, rows) call: rows [firstRow, firstRow + rowCount) of the three dynamic dome textures
/// ([0xFA2738 + 4a], one per alignment) are blended again with sky type `skyType`
struct DomeBlock
{
	float skyType;
	int firstRow;
	int rowCount;
};

/// The dome's two-texture blend weight of fn_0086B7F0 (0x86B890..0x86B8E8): T <= 1 (`test ah, 0x41`, `je`) ->
/// w = ftol(T * 255.0f) ([0x8AB270]) between the day (time of day 0) and the dusk (1) textures, else
/// w = ftol((T - 1) * 255) between the dusk and the night (2) ones. The lower texture gets 255 - w, the upper w.
struct DomeWeight
{
	int lower; ///< time-of-day index of the source with 255 - w: 0 _day, 1 _dusk, 2 _night (0xC3963C / 30 / 24)
	int upper; ///< the one with w
	int weight; ///< w
};
[[nodiscard]] DomeWeight DomeWeightOf(float skyType);

/// fn_0086B9A0's 555 path (0x86BACD..0x86BB3D, taken when [0xEDD46C] = 0; no direct write of [0xEDD46C] found, the
/// 565 path is not ported): tables low[i] = (i (255 - w)) >> 8 (0xFA2554) and
/// high[i] = (i w) >> 8 (0xFA2514), each 5-bit channel out = low[lower] + high[upper]; bit 15 comes out 0. The sum of
/// the weights is 255 / 256, so even with an integer sky type a channel of 31 gives 30.
[[nodiscard]] uint16_t BlendTexel555(uint16_t lower, uint16_t upper, int weight);
/// fn_0086B9A0 over whole rows: dst[i] = BlendTexel555(lower[i], upper[i], weight)
void BlendRows555(std::span<uint16_t> dst, std::span<const uint16_t> lower, std::span<const uint16_t> upper,
                  int weight);

/// The dome's slow follow of the sky type, LH3DSky's [0xFA26C0] (sky type the dome is being built with) and [0xFA26B8]
/// (rows built so far). fn_0086A330, once a frame after SampleFrame (DrawSky 0x5E222B): when the whole dome is built
/// and |Frame() - built| > 0.03 (the double [0x99A168], bytes 00 00 00 E0 51 B8 9E 3F = (double)0.03f; `test ah,
/// 0x41`, strict), the new sky type is latched and the rows start again from 0; while rows are missing, 32 more
/// ([0x86A389] push 0x20) are blended with the latched sky type, the first block in the same frame as the latch.
/// The rows are [0xEDD470] ? 128 : 256. [0xEDD470] is written at start-up by fn_00823AD0 from the detail level's table
/// [0x9A38E0 + 4 level] = 1, 1, 0, 0, 0, 0, 0 (0x823C5B..0x823C69; graphics::DetailLevel::skyNoBlend), and set back on
/// the two exits of fn_0082A8E0 (0x82AB1B / 0x82AB30). At levels 0 and 1 fn_00869670 is false: no blend, a single copy
/// of the day textures, a per-T tint of the dome colour (0x86B1C1..0x86B2A4) and 128 rows. That path is not ported:
/// openblack always blends 256 rows, whatever the detail level.
class DomeBlend
{
public:
	static constexpr int k_Rows = 256;      ///< [0xEDD470] ? 128 : 256 (0x86A335..0x86A348), the blend path only
	static constexpr int k_RowsPerFrame = 32; ///< push 0x20 (0x86A389)
	static constexpr double k_Hysteresis = static_cast<double>(0.03f); ///< [0x99A168]

	/// What a frame asks the dome textures to redo, in order: a pending whole rebuild of Jump and this frame's block
	struct Blocks
	{
		std::array<DomeBlock, 2> blocks {};
		int count {0};
	};

	/// fn_0086A330 (without its first call, the land light table fn_00869850, which Renderer::UpdateLandLight does)
	[[nodiscard]] Blocks Advance(float frameSkyType);
	/// fn_0086A270's part: [0xFA26C0] = T, [0xFA26B8] = 0 and fn_0086B7F0(T, 0, 256), handed out by the next Advance
	void Jump(float frameSkyType);

	[[nodiscard]] float Built() const { return _built; }
	[[nodiscard]] int RowsDone() const { return _rowsDone; }

private:
	float _built {0.0f};             ///< [0xFA26C0] (.bss, and 0 again at fn_0086A3B0 0x86A5AB)
	int _rowsDone {k_Rows};          ///< [0xFA26B8]: fn_0086A3B0 builds the whole dome at set-up (0x86A595)
	bool _rebuildPending {false};
	float _rebuildSkyType {0.0f};
};

/// The dome blend of LH3DSky (one, like its statics)
[[nodiscard]] DomeBlend& Dome();

} // namespace openblack::sky_type
