/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>

/// Black & White's graphics detail levels, 0 to 6, 4 by default. Level 5 is the custom one, where every setting is read
/// from the player's own settings, and whose values here are where they start.
namespace openblack::graphics::detail_level
{

inline constexpr uint8_t k_Default = 4;
/// The top level: the villagers, animals and birds are drawn with their high meshes there (components::DetailMesh),
/// by the user's decision of 2026-10-15; the v1.42 exe draws them at every level
inline constexpr uint8_t k_Top = 6;

/// Whether the models with a finer mesh are drawn with it
[[nodiscard]] constexpr bool DrawsHighMeshes(uint8_t level)
{
	return level >= k_Top;
}

/// The settings of a level
struct Level
{
	float waterTiling;     ///< how finely the sea's texture repeats, 0 to 1 (SeaPeriod); at 0 a nearly still square
	bool landReflection;   ///< "LandRef": the land mirrored under the sea
	bool fog;              ///< "Fog": the distance haze
	bool clouds;           ///< "Clouds" and "CloudShadows": whether the sky has clouds
	bool useHighTexture;   ///< 256 px landscape textures (else 128)
	uint8_t rainSplash;    ///< "RainSplash"
	bool shadowsOnObjects; ///< "ShadowsOnObjects" (startup only): the hand's dynamic shadow also falls on objects
	/// Per level, not read from the player's settings at level 5: the sky dome without the day / dusk / night blend,
	/// 128 rows. Not ported: openblack's dome always blends (sky_type::DomeBlend)
	bool skyNoBlend;
	/// "Weather": among other things, an evil sky darkens the sky's dome. Not read yet
	bool weather;

	/// The sea texture's period, 2000 - 1800 x waterTiling
	[[nodiscard]] float SeaPeriod() const { return 2000.0f - 1800.0f * waterTiling; }
};

inline constexpr std::array<Level, 7> k_Levels = {{
    {0.0f, false, false, false, false, 0, false, true, false},
    {0.2f, false, false, false, false, 0, false, true, false},
    {0.4f, false, false, false, false, 3, false, false, false},
    {0.6f, true, true, true, false, 5, true, false, true},
    {0.8f, true, true, true, true, 8, true, false, true},
    {0.5f, true, true, true, true, 8, true, false, true},
    {1.0f, true, true, true, true, 8, true, false, true},
}};

/// How far out models keep their finer meshes (see mesh_detail::Reach): the lowest levels swap them sooner. Its own
/// table rather than a field of Level, so that the levels' initialisers stay as they are
inline constexpr std::array<float, 7> k_ModelDetail = {0.2f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f};

/// A level's settings; a level past the last is the last
[[nodiscard]] constexpr const Level& Get(uint8_t level)
{
	return k_Levels.at(std::min<size_t>(level, k_Levels.size() - 1));
}

[[nodiscard]] constexpr bool Fog(uint8_t level)
{
	return Get(level).fog;
}

[[nodiscard]] constexpr bool Weather(uint8_t level)
{
	return Get(level).weather;
}

[[nodiscard]] constexpr bool Clouds(uint8_t level)
{
	return Get(level).clouds;
}

[[nodiscard]] constexpr float ModelDetail(uint8_t level)
{
	return k_ModelDetail.at(std::min<size_t>(level, k_ModelDetail.size() - 1));
}

[[nodiscard]] constexpr float WaterTiling(uint8_t level)
{
	return Get(level).waterTiling;
}

/// Whether what is drawn in front of the sun's glare samples hides them, besides the land: at the detail indices 3, 4
/// and 6. It takes the index the original runs with (draw_list::DetailIndexOf), not openblack's level, so it is on at
/// the levels 3 to 6. The original also needs a 16-bit depth buffer on a card that is not a PowerVR KYRO, which
/// openblack's always is taken to be
[[nodiscard]] constexpr bool GlareDepthTest(int32_t detailIndex)
{
	return detailIndex == 3 || detailIndex == 4 || detailIndex == 6;
}

} // namespace openblack::graphics::detail_level
