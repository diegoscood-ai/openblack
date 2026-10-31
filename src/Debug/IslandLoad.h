/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cctype>
#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace openblack::debug::gui
{

/// What an island's script is: one of the story's lands, a playground, or anything else
enum class IslandKind : uint8_t
{
	StoryLand,
	Playground,
	Other,
};

struct IslandScript
{
	IslandKind kind {IslandKind::Other};
	/// The story land's number, 1 to 5; 0 for the others
	int land {0};
};

namespace detail
{
[[nodiscard]] inline std::string LowerCase(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}
} // namespace detail

/// The kind of an island's script path: a script in a Playgrounds folder is a playground; Land1.txt to Land5.txt in any
/// other folder is that story land; anything else is other. Names are compared without case
[[nodiscard]] inline IslandScript ClassifyIslandScript(const std::filesystem::path& scriptPath)
{
	if (detail::LowerCase(scriptPath.parent_path().filename().generic_string()) == "playgrounds")
	{
		return {.kind = IslandKind::Playground};
	}
	const auto name = detail::LowerCase(scriptPath.filename().generic_string());
	if (name.size() == 9 && name.starts_with("land") && name.ends_with(".txt") && name[4] >= '1' && name[4] <= '5')
	{
		return {.kind = IslandKind::StoryLand, .land = name[4] - '0'};
	}
	return {};
}

/// The growing-up stage the playground start gives the player's creature: fully grown
inline constexpr int32_t k_PlaygroundCreatureStage = 13;

/// The debug bar's "Load Island".
///
/// Without the creature only the island is loaded. With it, the load goes as the game's own land change, which saves
/// the player's creature to the profile, and around it what the game does there:
/// - a story land: every script stops first, as the story's land control does before each land, and that script too,
///   since it would load the next land on top. After the load the land's control script starts (for Land 1 the whole
///   story's, as at a new game's start); it brings the creature back when the land's story does;
/// - a playground: every script stops first, as the playground start resets the scripts. After the load the profile's
///   creature is made at the first player's temple, knowing every leash and fully grown, as the playground start
///   does; no temple, no creature;
/// - anything else: only the land change.
///
/// `Game` is the game or a test's fake, with LoadMap and ChangeLand as the game has them, StopAllTasks, StartScript,
/// TempleCreatureHome (an optional point), LoadMyCreature (an optional creature), KnowAllLeashes, SetCreatureStage
/// and Log
template <typename Game>
bool LoadIsland(Game& game, const std::filesystem::path& scriptPath, bool takeCreature)
{
	if (!takeCreature)
	{
		return game.LoadMap(scriptPath);
	}
	const auto island = ClassifyIslandScript(scriptPath);
	if (island.kind == IslandKind::Other)
	{
		return game.ChangeLand(scriptPath);
	}
	game.StopAllTasks();
	if (!game.ChangeLand(scriptPath))
	{
		game.Log(fmt::format("Load Island: {} did not load; the old land's scripts are stopped", scriptPath.generic_string()));
		return false;
	}
	if (island.kind == IslandKind::StoryLand)
	{
		game.StartScript(island.land == 1 ? std::string("LandControlAll") : fmt::format("LandControl{}", island.land));
		return true;
	}
	if (const auto home = game.TempleCreatureHome(); home.has_value())
	{
		if (const auto creature = game.LoadMyCreature(*home); creature.has_value())
		{
			// whatever its file said, it knows the evil, rope and good leashes, then grows up
			game.KnowAllLeashes(*creature);
			game.SetCreatureStage(*creature, k_PlaygroundCreatureStage);
		}
		else
		{
			game.Log(fmt::format("Load Island: the profile's creature could not be made on {}", scriptPath.generic_string()));
		}
	}
	else
	{
		game.Log(fmt::format("Load Island: {} has no temple of the first player, so no creature is made",
		                     scriptPath.generic_string()));
	}
	return true;
}

} // namespace openblack::debug::gui
