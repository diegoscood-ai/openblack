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

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
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

/// Where a task stands in its script: the source line of its next instruction, and the native it called last, the
/// nearest call at or before it within a few instructions (what a wait loops on); empty when none is that near
struct TaskPlace
{
	uint32_t line {0};
	std::string lastCall;
};
[[nodiscard]] TaskPlace WhereIs(const lhvm::VMTask& task, std::span<const lhvm::VMInstruction> instructions,
                                const std::vector<lhvm::NativeFunction>* functions);

/// Whether a text has been shown, among the dialogue's last texts (the current one first), and with `read` whether it
/// has been read too: it no longer waits for the player's click
[[nodiscard]] bool TextHolds(std::span<const uint32_t> recentTexts, bool currentWaitsForClick, uint32_t text, bool read);

/// Whether a text has been shown and said to its end, among the dialogue's last texts (the current one first): an
/// earlier one has, and the current one has once the dialogue reads it as read, as a script's wait until read does
[[nodiscard]] bool TextSpoken(std::span<const uint32_t> recentTexts, bool currentRead, uint32_t text);

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
	/// Passes over an animal that is dying or dead, as a pick meant for a living one of a pack
	bool aliveOnly {false};
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

/// A building or a villager a town's knock-down may take: its id, the id of the town it belongs to, and whether it is
/// still on the map
struct TownMember
{
	uint32_t id {0};
	uint32_t town {0};
	bool available {true};
};

/// What knocking a town down deletes, each kind in the order given
struct KnockDown
{
	std::vector<uint32_t> buildings;
	std::vector<uint32_t> villagers;
};

/// A town wiped out, as a village destroyed by miracles is: every building of the town and every villager of it that
/// is still on the map. A village whose people survive is not gone: they plan its buildings again and rebuild it, and a
/// town counts as destroyed only while none of its buildings stands
[[nodiscard]] KnockDown KnockDownTargets(uint32_t town, std::span<const TownMember> buildings,
                                         std::span<const TownMember> villagers);

/// A throw by the player's hand at a point of the land, made as a player makes one: the Action button pressed with the
/// thing held, the pointer moved at a steady speed over the land for a run-up, and the button let go while it still
/// moves. The hand follows the pointer's point on the land at its holding height, and by the end of the run-up it moves
/// as fast as that point, which is the speed the thing leaves it with. The thing is taken to fly under gravity alone
/// from where it is let go, its upward speed the slope's under the hand: the air's drag on it is left out, so the
/// landing is near the point rather than on it.
struct HandThrowAim
{
	/// The point of the land the pointer is over as the run-up starts, and the point aimed at (x and z)
	glm::vec2 from {0.0f};
	glm::vec2 target {0.0f};
	/// How high above the land under it the thing hangs from the hand
	float heightAboveLand {0.0f};
	/// How long the pointer moves for before the button is let go
	float runUpSeconds {0.5f};
	/// How far the thing trails the pointer's point, as the seconds of motion it is behind
	float trailSeconds {0.0f};
	float gravity {9.81f};
	/// The fastest the hand throws
	float maxSpeed {124.0f};
};
/// How the pointer moves for the throw, and where the thing is let go
struct HandThrow
{
	glm::vec2 from {0.0f};
	/// How fast the pointer's point moves over the land, in units a second (x and z)
	glm::vec2 velocity {0.0f};
	float runUpSeconds {0.0f};
	/// Where the thing leaves the hand (x and z), how fast it rises as it does, and how long it then flies
	glm::vec2 release {0.0f};
	float upSpeed {0.0f};
	float flightSeconds {0.0f};
	/// Whether the speed is within the hand's fastest throw; above it the throw falls short
	bool withinTopSpeed {true};
};
/// The run-up's speed that lands the thing at the target, given the land's height at a point; none for a run-up of no
/// time, or one the thing can't fly from
[[nodiscard]] std::optional<HandThrow> PlanHandThrow(const HandThrowAim& aim, const std::function<float(glm::vec2)>& ground);
/// The point of the land under the pointer some seconds into the run-up, held at its end once it is over
[[nodiscard]] glm::vec2 RunUpPoint(const HandThrow& plan, float seconds);
/// Where the pointer is on the screen some seconds into the run-up: the run-up's point of the land seen through the
/// camera (`project`, none off the screen); the screen motion of the throw is how it changes from frame to frame
[[nodiscard]] std::optional<glm::vec2> RunUpPixel(const HandThrow& plan, float seconds,
                                                  const std::function<std::optional<glm::vec2>(glm::vec2)>& project);

} // namespace openblack::testbed_land_timeline
