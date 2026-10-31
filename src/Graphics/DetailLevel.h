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
/// The level openblack starts at: the top one, where the original starts at its default (the user's choice of
/// 2026-10-21; the checks run at it too)
inline constexpr uint8_t k_Start = k_Top;

/// Whether the models with a finer mesh are drawn with it
[[nodiscard]] constexpr bool DrawsHighMeshes(uint8_t level)
{
	return level >= k_Top;
}

/// The options box's detail control: five entries, HELP_TEXT_DIALOG_DETAIL_00 (Minimum) to _04 (Maximum Detail)
inline constexpr int k_MenuEntries = 5;
inline constexpr int k_MenuTopEntry = k_MenuEntries - 1;

/// The level an entry of the options box's detail control sets. The original's control is the detail index itself,
/// entry n index n; here the first four are the levels 0 to 3, and the top one, Maximum Detail, is the top level 6,
/// the one with the high meshes (the user's decision of 2026-10-20; the original's is 4)
[[nodiscard]] constexpr uint8_t LevelOfMenuEntry(int entry)
{
	return entry >= k_MenuTopEntry ? k_Top : static_cast<uint8_t>(std::max(entry, 0));
}

/// The entry the control shows for a level: its own for 0 to 3; Maximum for 4 to 6, as the original shows its top
/// entry at its default level 4 (the levels 4 and 5 have no entry of their own)
[[nodiscard]] constexpr int MenuEntryOf(uint8_t level)
{
	return std::min(static_cast<int>(level), k_MenuTopEntry);
}

/// The level once the player leaves the Options page: Maximum Detail is always the top level (the user's decision of
/// 2026-10-20: picking it gives the high meshes, also when the control already showed it at the default 4); another
/// entry sets its own level when the control was stepped to it, else the level stays, so that 5 is kept until the
/// player picks an entry
[[nodiscard]] constexpr uint8_t LevelAfterOptions(uint8_t level, int entryWhenOpened, int entryWhenLeft)
{
	if (entryWhenLeft >= k_MenuTopEntry)
	{
		return k_Top;
	}
	return entryWhenLeft == entryWhenOpened ? level : LevelOfMenuEntry(entryWhenLeft);
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
	/// Per level, not read from the player's settings at level 5: the game's pictures at half size, and with them the
	/// sky dome without the day / dusk / night blend: the day pictures at 128 x 128, tinted by the time of day
	/// (sky_dome::NoBlendColour). openblack takes only the dome's part of it
	bool skyNoBlend;
	/// "Weather": the atmosphere's rain and snow, read once at start-up, and the sky's dome, which an evil sky darkens
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

[[nodiscard]] constexpr bool SkyNoBlend(uint8_t level)
{
	return Get(level).skyNoBlend;
}

[[nodiscard]] constexpr bool Clouds(uint8_t level)
{
	return Get(level).clouds;
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
