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

#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <LHVMTypes.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/ScriptTypes.h"

/// A testbed scenario's commands played on one of the game's own lands, as a land entry's timeline= asks: the land is
/// the game's, loaded as a launch loads it, and nothing is set out on it. The rules of it that need no game, so that they
/// are tested on their own.
namespace openblack::testbed_land_timeline
{

/// A land is 512 cells of 10 units each way
inline constexpr float k_LandSize = 5120.0f;

/// What a scenario's points are measured from: the testbed's middle on the testbed, the land's corner (so that a point
/// is a map coordinate, x east and y the map's z) on a land
[[nodiscard]] glm::vec2 Origin(bool onLand, glm::vec2 testbedMiddle);

/// Whether a map coordinate lies on a land
[[nodiscard]] bool OnTheLand(glm::vec2 point);

/// The scenario played next once the one at `chapter` of `chapters` has given all its commands: the one after it, none
/// after the last or while it still has commands to give
[[nodiscard]] std::optional<size_t> NextChapter(size_t chapter, size_t chapters, bool done);

/// How a script global's value is held up against the one a command gives
enum class Compare : uint8_t
{
	Equal,
	NotEqual,
	Below,
	AtMost,
	Above,
	AtLeast,
};
[[nodiscard]] bool Holds(Compare compare, float value, float target);
/// As the log writes it: "==", "!=", "<", "<=", ">", ">="
[[nodiscard]] std::string_view Name(Compare compare);

/// What a command waiting on a script waits for: a task of it running, none running, or none running after one was
enum class TaskWait : uint8_t
{
	Running,
	NotRunning,
	Finished,
};
inline constexpr size_t k_TaskWaits = 3;
/// Whether the wait is over, by whether a task of the script runs now and whether one was seen running since the wait
/// began
[[nodiscard]] bool TaskWaitHolds(TaskWait wait, bool runningNow, bool seenRunning);

/// The place of a script global among the globals (the first of that name), none when there is none; the first place is
/// the scripts' null variable, never a global
[[nodiscard]] std::optional<size_t> GlobalIndex(std::span<const lhvm::VMVar> globals, std::string_view name);
/// A global's value as a number, by its type: the scripts keep most as floats, an object as its id
[[nodiscard]] float NumberOf(const lhvm::VMVar& global);
/// The value a number is set as in a global of that type, which it keeps
[[nodiscard]] lhvm::VMValue ValueOf(lhvm::DataType type, float number);
/// Whether a task of the script is running
[[nodiscard]] bool ScriptRunning(const std::map<uint32_t, lhvm::VMTask>& tasks, std::string_view script);

/// Whether a text has been shown, among the dialogue's last texts (the current one first), and with `read` whether it
/// has been read too: it no longer waits for the player's click
[[nodiscard]] bool TextHolds(std::span<const uint32_t> recentTexts, bool currentWaitsForClick, uint32_t text, bool read);

/// Whether the camera's eye is within the radius of a point, measured in three dimensions as a script's distance from the
/// camera to a marker is
[[nodiscard]] bool CameraWithin(glm::vec3 eye, glm::vec3 point, float radius);

/// The names of a list of them, separated by spaces
[[nodiscard]] std::vector<std::string_view> Names(std::string_view list);

/// A thing of the land a command acts on, picked as a script's CALL_NEAR picks one: of a script object type and
/// sub-type (the scripts' "any" for any sub-type), the nearest to a point within a radius. The point is a map
/// coordinate on a land (from the middle of the map on the testbed)
struct ThingNear
{
	script::ObjectType type {script::ObjectType::None};
	uint32_t subtype {ecs::script_type::k_AnySubtype};
	glm::vec2 point {0.0f};
	float radius {10.0f};
};

/// A thing that may be picked: its id, its script type and sub-type, and where it stands on the land (x, z)
struct ThingFacts
{
	uint32_t id {0};
	script::ObjectType type {script::ObjectType::None};
	uint32_t subtype {ecs::script_type::k_NoSubtype};
	glm::vec2 at {0.0f};
};

/// The thing of the kind nearest the point, within the radius (the first of those as pick), none when there is none
[[nodiscard]] std::optional<uint32_t> Nearest(std::span<const ThingFacts> things, const ThingNear& pick, glm::vec2 point);
/// Whether a thing is near enough to be worth asking for its kind
[[nodiscard]] bool WithinReach(glm::vec2 at, const ThingNear& pick, glm::vec2 point);

} // namespace openblack::testbed_land_timeline
