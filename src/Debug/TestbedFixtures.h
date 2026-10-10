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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/ScriptHighlight.h"
#include "Enums.h"

/// The things a test scenario sets out on the testbed before it runs, each in a line or two: a miracle dispenser, the
/// weather, the creature free, leashed or penned, a village, a player's temple, a fire, trees and piles, a script
/// highlight, a seed in the hand and a recorded hand demo to play. Each is plain data, checked by Problems() without
/// the game, then put down through the game's own archetypes and systems by Place().
namespace openblack::testbed_fixtures
{

/// The n-th press (grab or action button down) of the scenario's hand demo: the point on the land the recorded hand
/// acts at, moved by nudge
struct DemoPress
{
	size_t press {0};
	glm::vec2 nudge {0.0f};
};
/// Where a fixture stands: from the middle of the map (x east, y north), or at a press of the hand demo
using Where = std::variant<glm::vec2, DemoPress>;

/// A miracle dispenser with its bubble ready to take
struct Dispenser
{
	MagicType magic {MagicType::Fireball};
	Where at {glm::vec2 {0.0f}};
	/// Turns between bubbles once the first is taken; 0 for one bubble only
	uint32_t periodTurns {0};
	float yawDegrees {0.0f};
};

/// A storm at a point: rain, snow and lightning within its radii for its life
struct Storm
{
	Where at {glm::vec2 {0.0f}};
	float innerRadius {100.0f};
	float outerRadius {300.0f};
	float lifeSeconds {120.0f};
	/// 0 to 1, as much as the weather allows
	float rain {1.0f};
	float snow {0.0f};
	/// Degrees; at or below freezing the rain falls as snow
	float temperature {10.0f};
	/// Fork lightning every so many seconds, from the first to the second; none when not given
	std::optional<glm::vec2> forkSeconds;
	/// Sheet lightning, its flash and thunder, likewise
	std::optional<glm::vec2> sheetSeconds;
};

/// How the creature is held
enum class Hold : uint8_t
{
	Free,
	/// On a leash of its kind
	Leashed,
	/// Kept within a radius of where it stands, as in its pen
	Penned,
	/// In its owner's temple's pen: set out at the temple's pen point and left free, the game keeping its home there
	/// and drawing it at the pen's size, as for a creature by its own temple. Needs that player's temple among the
	/// fixtures
	TemplePen,
};

/// The player's creature, made as a map script makes it
struct Creature
{
	CreatureType species {CreatureType::Tiger};
	Where at {glm::vec2 {0.0f}};
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	Hold hold {Hold::Free};
	LeashType leash {LeashType::Rope};
	/// The creature file to load, from the game's creature folder; the profile's creature when empty
	std::string_view file;
	/// Further leashes it knows, as a land's script teaches them; a temple's leash posts show only the leashes its
	/// player's creature knows
	std::vector<LeashType> knows;
};

/// A town with its huts, its villagers and a storage pit, laid out round its centre
struct Village
{
	Where at {glm::vec2 {0.0f}};
	Tribe tribe {Tribe::CELTIC};
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	size_t huts {4};
	size_t villagers {8};
	/// Food and wood in the storage pit; none when false
	bool storagePit {true};
	int32_t food {1000};
	int32_t wood {1000};
	/// Lays the huts and villagers out the same way every time
	uint32_t seed {1};
};

/// A player's temple, built, as a map script's citadel command makes it: its heart, its entrance and its three leash
/// posts. A player has one temple
struct Temple
{
	Where at {glm::vec2 {0.0f}};
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	/// The turn about the vertical in the map script's units, thousandths of a radian, as the citadel command takes it
	int32_t rotation {0};
};

/// A fire: one of the scenario's other fixtures or objects set alight, or a bonfire lit at a point
struct Fire
{
	/// The index of a scenario object, or a point where a bonfire is made and lit
	std::variant<size_t, Where> what {Where {glm::vec2 {0.0f}}};
	float speed {1.0f};
};

/// Trees of one kind, spread round a point
struct Trees
{
	TreeInfo type {TreeInfo::Oak};
	Where at {glm::vec2 {0.0f}};
	size_t count {1};
	/// Radius of the spread; all at the point when 0
	float spread {0.0f};
	float scale {1.0f};
};

/// A pile of wood, food or the like
struct Pile
{
	PotInfo type {PotInfo::WoodPile_1};
	Where at {glm::vec2 {0.0f}};
	int32_t amount {500};
};

/// A script highlight standing on the land, as a map script's highlight command makes one: a challenge scroll or a
/// did-you-know sign, with no challenge, unturned and at full size. No script holds it, and nothing lifts or lights it
struct Highlight
{
	ecs::script_highlight::Info info {ecs::script_highlight::Info::Silver};
	Where at {glm::vec2 {0.0f}};
};

/// A recorded hand demo from the game's hand demo folder, played from a time into the scenario. The demo moves the
/// camera, the cursor and the buttons; its presses are where DemoPress fixtures stand.
struct HandDemo
{
	std::string_view name;
	float startSeconds {0.0f};
	/// The flat land's altitude to replay it on, for a demo recorded with the camera lower than the default plane
	std::optional<uint8_t> planeAltitude;
};

/// Every fixture of a scenario
struct Fixtures
{
	std::vector<Dispenser> dispensers;
	std::vector<Storm> storms;
	std::vector<Creature> creatures;
	std::vector<Village> villages;
	std::vector<Temple> temples;
	std::vector<Fire> fires;
	std::vector<Trees> trees;
	std::vector<Pile> piles;
	std::vector<Highlight> highlights;
	/// A miracle's seed put in the player's hand as the scenario starts
	std::optional<MagicType> handSeed;
	std::optional<HandDemo> handDemo;
	/// The scenario writes into the game's folder (a land change saves the creature's mind and physique): it runs only
	/// on a copy of the game's data, never on the game itself
	bool writesGameData {false};
};

[[nodiscard]] bool Empty(const Fixtures& fixtures);

/// What is wrong with the fixtures, each in a sentence; none when they can be set out. pressCount: the presses of the
/// scenario's hand demo, when it has one
[[nodiscard]] std::vector<std::string> Problems(const Fixtures& fixtures, std::optional<size_t> pressCount = std::nullopt);

/// The map point (x, z) of a Where: the middle plus its offset, or the press's point plus its nudge; none for a press
/// the demo does not have. presses: where the hand demo's presses meet the land, in order
[[nodiscard]] std::optional<glm::vec2> Resolve(const Where& where, glm::vec2 middle, std::span<const glm::vec2> presses);

/// Huts stand at least this far apart, centre to centre, metres
inline constexpr float k_HutSpacing = 16.0f;
/// The smallest ring the huts stand on, which keeps them clear of the storage pit in the village's middle
inline constexpr float k_MinHutRing = 20.0f;

/// Where a village's huts stand round its centre: on a ring of a radius that fits them, the first due north, turned
/// a little by the seed. Offsets from the village's centre
[[nodiscard]] std::vector<glm::vec2> HutOffsets(const Village& village);

/// Where count things stand within a circle of radius spread, evenly filled from its middle outwards, the same every
/// time; all at the middle when spread is 0. Offsets from the middle
[[nodiscard]] std::vector<glm::vec2> SpreadOffsets(size_t count, float spread);

/// The marker file a copy of the game's data carries, so that a scenario that writes may run on it
inline constexpr std::string_view k_GameDataCopyMarker = "openblack_game_data_copy.txt";

/// Sets the fixtures out on the loaded testbed, logging a line for each through log, and for each one that cannot be
/// set out. middle: the map's middle; presses as for Resolve; objectAt: the scenario's object of an index, for a fire
/// set to one. The hand demo and writesGameData are the runner's, not set out here. Never throws
/// placedCreature: called with each creature fixture as it is made, in order, so that the scenario's commands can be
/// given to it (the runner numbers them after the scenario's own creatures)
void Place(const Fixtures& fixtures, glm::vec2 middle, std::span<const glm::vec2> presses,
           const std::function<std::optional<entt::entity>(size_t)>& objectAt, const std::function<void(std::string)>& log,
           const std::function<void(entt::entity)>& placedCreature = {});

} // namespace openblack::testbed_fixtures
