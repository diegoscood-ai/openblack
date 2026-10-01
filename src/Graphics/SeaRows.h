/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

/// The CPU side of the original's sea (fn_008792E0 / fn_00879500 / fn_00879930 / fn_0087A090): the screen rows the sea
/// is drawn in and the wind drift of its texture. The rows themselves are built per pixel in fs_water.
namespace openblack::graphics::sea
{

/// fn_00879500: the quad whose screen extent gives the sea rows, 30000 x 30000 on y = 0 centred on (2560, 2560)
/// (corners 0xC6426000 / 0x46893000)
inline constexpr float k_RowsQuadMinimum = -12440.0f;
inline constexpr float k_RowsQuadMaximum = 17560.0f;
/// fn_0087A090 (detail level 0, WaterTiling = 0): a world quad of +-70000 (0x4788B800) with 50 texture repeats
inline constexpr float k_Level0HalfSize = 70000.0f;
inline constexpr float k_Level0Period = 2 * k_Level0HalfSize / 50.0f; // 2800

/// fn_00879500's result: the lowest and highest screen y (pixels from the top) of the rows quad clipped by the frustum,
/// and 1 / view depth at those two vertices ([0xFA9384] / [0xFA9388] = rhw = near / z, divided by near in
/// fn_00879930). top is raised to 0 and bottom lowered to height - 1 without correcting their 1 / z.
struct ScreenRange
{
	float top;
	float bottom;
	float inverseDepthTop;
	float inverseDepthBottom;
};

/// fn_00879500: nullopt when the quad is off the screen (nothing to draw)
/// @param viewProjection world to clip space (w = view depth)
/// @param nearDistance the near plane the quad is clipped with
[[nodiscard]] std::optional<ScreenRange> ComputeScreenRange(const glm::mat4& viewProjection, glm::vec2 viewportSize,
                                                            float nearDistance);

/// fn_00879930's rows: vertex rows r = 0..count at screen y first + 2 r; their 1 / depth is inverseDepth + r * step
/// (affine in the screen y, from the two range vertices)
struct Rows
{
	int first;          ///< ftol(top)
	int count;          ///< n = (ftol(bottom) - first + 2) / 2; the triangles cover rows 0..n
	float inverseDepth; ///< of row 0
	float inverseStep;  ///< per row: (1/z bottom - 1/z top) / (n - 1)
	bool softTop;       ///< row 0 gets alpha 0x20 (its y > 0.5: the sea starts inside the screen)
};
[[nodiscard]] Rows MakeRows(const ScreenRange& range);

/// The sea texture offsets ([0xFA9370] / [0xFA9374] for levels 1..6, [0xFA9390] / [0xFA9394] for level 0), moved by the
/// ambient wind every drawn frame.
class Drift
{
public:
	/// fn_00879930 0x879963 (always) and 0x879A69 (only when the rows quad is on screen): off += wind.xz * ms * -1/330,
	/// then off -= trunc(off / P) * P
	void ScrollRows(float milliseconds, glm::vec2 wind, float period);
	/// fn_0087A090: off0 += wind.xz * ms * -1/330000, then off0 -= trunc(off0) (in texture repeats)
	void ScrollLevel0(float milliseconds, glm::vec2 wind);

	[[nodiscard]] glm::vec2 GetRowsOffset() const { return _rows; }
	[[nodiscard]] glm::vec2 GetLevel0Offset() const { return _level0; }

private:
	glm::vec2 _rows {0.0f};
	glm::vec2 _level0 {0.0f};
};

/// g_ambient_wind_direction (0xEA9E70 / 0xEA9E78): FastNormalize(int8 LH3DAtmos::ambient[4] / 8, 0, ambient[5] / 8),
/// set every turn by GGame::ProcessTurn 0x54E5DC. ambient[4..5] are only written by InitStaticsValues / fn_00835AD0 (0),
/// GGame::Load and the Internet weather, so in a game it is 0 and the sea does not drift.
inline constexpr glm::vec2 k_AmbientWind {0.0f, 0.0f};

} // namespace openblack::graphics::sea
