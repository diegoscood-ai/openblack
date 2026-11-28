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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The scripts a quest run (a land entry's quest=) starts, as written on the command line: kept apart from the land
// timeline's helpers so that the command line's options need nothing of the script library

namespace openblack::testbed_land_timeline
{

/// A value a script is started with from outside, as its caller would push it: a number, a marker made at a point
/// (as a script's "marker at"), or the thing of a script type nearest a map point (as a script's "get <type> at")
struct ScriptArgument
{
	enum class Kind : uint8_t
	{
		Number,
		Marker,
		Thing,
	};
	Kind kind {Kind::Number};
	float number {0.0f};
	/// A marker's point
	glm::vec3 point {0.0f};
	/// A thing's script type, by its number, the map point it is nearest to and the radius it is looked for in
	uint8_t thingType {0};
	glm::vec2 thingPoint {0.0f};
	float thingRadius {10.0f};
};
/// One argument as a quest run writes it: a number (30, -2.5), marker@<x>;<y>;<z>, or
/// thing@<script type>;<x>;<z>[;<radius>] (the type by its number, as the challenge file has it); none for anything else
[[nodiscard]] std::optional<ScriptArgument> ParseScriptArgument(std::string_view text);

/// A script a quest run starts, with the values it is started with; or, with a value, a global it sets first, as the
/// story before the quest would have (ProtectGateKeysFlag=1)
struct QuestScript
{
	std::string name;
	std::vector<ScriptArgument> arguments;
	std::optional<float> globalValue;
	/// Started and left running: the next starts at once, as a script's "run background script" does
	bool background {false};
};
/// A quest run's script as written: its name, letters, digits and underscores, then its arguments in parentheses,
/// separated by commas, if it takes any (SetupLand1, MiracleChallenges(marker@1500;0;2000)); or a global's name and
/// the number it is set to (ProtectGateKeysFlag=1); a script ending with & is left running while the next starts
/// (LeaveThroughVortexL1&); none for anything else
[[nodiscard]] std::optional<QuestScript> ParseQuestScript(std::string_view text);

} // namespace openblack::testbed_land_timeline
