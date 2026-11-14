/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// What a vortex that leads away from the land does with the things it takes, and how it remembers them for the next
// land. Each thing it takes is carried round and down into its middle by the effect that draws things in: drawn in
// faster and faster round the middle, lifted or lowered onto a funnel, and gone at the middle. As it is taken it is
// written to the crossing file as one line of the land scripts' language, placed relative to the vortex, so that the
// vortex the player arrives through on the next land can make it again. A pile it reaches gives up a pot of what it
// holds instead. Pure rules, tested without the game. Wiki: docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex.

namespace openblack::vortex::crossing
{

/// A pile within reach gives up a pot of at most this much
inline constexpr uint32_t k_PileTake = 1000;
/// The pot is drawn at a share of its own size: up to 0.3 of a random number times 0.4, plus 0.3
inline constexpr float k_PileShareRandom = 0.3f;
/// The share for that random number, the product and the sum rounded once, as the original works it
[[nodiscard]] float PileShare(float random);

/// A way into a land brings out this many villagers in all, those that came through the crossing file counted, as the
/// original (docs/wiki/world/vortex.md#bringing-them-out-the-out-vortex)
inline constexpr uint32_t k_ArrivingVillagers = 30;
/// A way into a land given no town takes the nearest town within this distance, as the original
/// (docs/wiki/world/vortex.md#vortex-details)
inline constexpr float k_ArrivalTownReach = 200.0f;

/// Whether a way into a land still has something to bring out: the rest of the crossing file, or fewer villagers out
/// than its 30. While it has, its fade-out waits, as the original (docs/wiki/world/vortex.md#vortex-details)
[[nodiscard]] constexpr bool StillBringsOut(bool fileLeft, uint32_t villagersOut)
{
	return fileLeft || villagersOut < k_ArrivingVillagers;
}

/// A new villager's tribe: its town's, or Norse without a town, as the original
/// (docs/wiki/world/vortex.md#bringing-them-out-the-out-vortex)
[[nodiscard]] constexpr Tribe NewVillagerTribe(std::optional<Tribe> townTribe)
{
	return townTribe.value_or(Tribe::NORSE);
}

/// A new villager is a housewife when its draw out of 1 is below a half, as the original
/// (docs/wiki/world/vortex.md#vortex-details)
[[nodiscard]] constexpr bool IsHousewife(float draw)
{
	return draw < 0.5f;
}
/// The jobs a new villager who is not a housewife is drawn from: forester, fisherman, farmer, shepherd and leader
inline constexpr uint32_t k_NewVillagerJobs = 5;
/// The rows each tribe has in the villagers' table, its housewife's first
inline constexpr int32_t k_VillagerRowsPerTribe = 7;
/// A new villager's row of the villagers' table: its tribe's housewife, or the job drawn after her, as the original
/// (docs/wiki/world/vortex.md#vortex-details)
[[nodiscard]] constexpr int32_t NewVillagerRow(Tribe tribe, bool housewife, uint32_t jobDraw)
{
	const int32_t row = static_cast<int32_t>(tribe) * k_VillagerRowsPerTribe;
	return housewife ? row : row + static_cast<int32_t>(jobDraw) + 1;
}

/// The draw a new villager's age comes from: a draw out of `range`, plus `youngest`
struct AgeDraw
{
	uint32_t range {0};
	uint32_t youngest {0};
};
/// Every fifth new villager, counted with itself, is a child of 1 to 9 and the others adults of 16 to 21, as the
/// original (docs/wiki/world/vortex.md#bringing-them-out-the-out-vortex)
[[nodiscard]] constexpr AgeDraw NewVillagerAgeDraw(uint32_t count)
{
	return count % 5 == 0 ? AgeDraw {.range = 9, .youngest = 1} : AgeDraw {.range = 6, .youngest = 16};
}

/// How the effect that draws things in moves them: its rule's settings, which the vortex's effect file leaves at
/// these, the rule's own
struct AttractSettings
{
	/// How fast a thing is drawn in: the share of its distance it loses each second, once fully blended in
	float radialSpeed {1.0f};
	/// How fast it is swung round the middle
	float thetaSpeed {1.0f};
	/// Inside this distance of the middle it can be gone
	float deletionRadius {4.0f};
	/// It is never drawn nearer the middle than this
	float minRadius {3.0f};
	/// The seconds over which its own way of moving blends into the vortex's
	float blendTime {8.0f};
	/// The seconds over which its velocity blends from the one it was taken with into the vortex's
	float blendTimeVelocity {1.0f};
	/// The funnel's shape: its height above the land at a distance r from the middle is sqrt(r * yOffset^2 /
	/// rZeroHeight) + yOffset, so it is at the land's height at rZeroHeight and below it nearer the middle
	float yOffset {-80.0f};
	float rZeroHeight {10.0f};
};

/// A thing being carried in, as it stands before a step
struct AttractAtom
{
	glm::vec3 position {0.0f};
	/// The velocity it was taken with
	glm::vec3 takenVelocity {0.0f};
	/// How high above the land it stood when taken
	float startHeight {0.0f};
	/// Seconds since it was taken
	float age {0.0f};
	/// It can't be destroyed: it reaches the end of its spiral sooner
	bool indestructible {false};
};

/// Where a step leaves a thing being carried in
struct AttractStep
{
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
};

/// The height of the land at a place across it, as the map gives it
using GroundAt = std::function<float(const map_coords::MapCoords&)>;

/// One step of dt seconds (invDt its inverse, as the particles keep it) round a vortex whose middle is at the origin:
/// where the thing goes, or none once it has reached the middle and is gone. Its distance shrinks, it swings round
/// faster the nearer it is, its velocity turns from the one it was taken with to the vortex's, and its height blends
/// from where it was taken to the funnel's. Each part is rounded to a float where the original stores it, and kept
/// unrounded where the original keeps it in its registers.
[[nodiscard]] std::optional<AttractStep> Attract(const AttractSettings& settings, const AttractAtom& atom, glm::vec3 origin,
                                                 float dt, float invDt, const GroundAt& ground);

/// A line of the crossing file: one land-script command making a thing again, relative to the vortex. The writer
/// (ECS/VortexSaveLines.h) puts it in the land scripts' words
struct VillagerLine
{
	/// The town's number, -1 for none
	int32_t town {-1};
	map_coords::MapCoords offset;
	/// Its tribe and its job, as the land scripts name them ("CELTIC_FARMER")
	std::string villagerType;
	uint32_t age {0};
};
/// A villager of no tribe, such as a shepherd of a story, by its row of the villagers' table
struct SpecialVillagerLine
{
	int32_t town {-1};
	map_coords::MapCoords offset;
	int32_t row {0};
	uint32_t age {0};
};
struct AnimalLine
{
	map_coords::MapCoords offset;
	/// Its kind's animal type
	int32_t type {0};
	/// Its flock's number and the flock's town's number, -1 for none
	int32_t flock {-1};
	int32_t town {-1};
	uint32_t age {0};
};
struct PotLine
{
	map_coords::MapCoords offset;
	int32_t row {0};
	/// Its kind of resource
	int32_t resource {0};
	uint32_t amount {0};
};
struct TreeLine
{
	/// A tree taken leaves its forest
	int32_t forest {-1};
	map_coords::MapCoords offset;
	int32_t row {0};
	bool nonScenic {false};
	float yAngle {0.0f};
	float scale {1.0f};
	float maxScale {1.0f};
};
/// What makes the thing made last an artefact of a player, worth so much. Written after the static thing's own line;
/// the town is never kept through a vortex, so the artefact joins the nearest town of its player as it is made again
struct ArtefactLine
{
	int32_t town {-1};
	/// Its player as the land scripts name them ("PLAYER_ONE")
	std::string player;
	float value {0.0f};
};
/// A rock or any other static thing lying about, with the line making it an artefact when it is one of a player's
struct MobileStaticLine
{
	map_coords::MapCoords offset;
	int32_t row {0};
	float xAngle {0.0f};
	float yAngle {0.0f};
	float zAngle {0.0f};
	float scale {1.0f};
	std::optional<ArtefactLine> artefact;
};
struct MobileObjectLine
{
	map_coords::MapCoords offset;
	int32_t row {0};
	float yAngle {0.0f};
	float scale {1.0f};
};
/// A scaffold: its town's number (-1 for none), its row of the scaffolds' table, its angle and its size
struct ScaffoldLine
{
	int32_t town {-1};
	map_coords::MapCoords offset;
	int32_t row {0};
	float yAngle {0.0f};
	float scale {1.0f};
};
struct DeadTreeLine
{
	map_coords::MapCoords offset;
	/// The player it belongs to as the land scripts name them ("PLAYER_ONE"); the neutral player's when none
	std::string player;
	int32_t row {0};
	float life {1.0f};
	float xAngle {0.0f};
	float yAngle {0.0f};
	float zAngle {0.0f};
};
/// A globe holding a miracle, written by the name of the miracle it gives; it is made again from the first seed that
/// casts that miracle, at the power-up that casts it
struct OneOffSpellLine
{
	map_coords::MapCoords offset;
	std::string magic;
};
using Line = std::variant<VillagerLine, SpecialVillagerLine, AnimalLine, PotLine, TreeLine, MobileStaticLine, MobileObjectLine,
                          ScaffoldLine, DeadTreeLine, OneOffSpellLine>;

} // namespace openblack::vortex::crossing
