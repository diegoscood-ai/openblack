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
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "Enums.h"

/// The rules of the game's statistics that game_stats does not keep: the miracles cast by kind and the figures worked out
/// from the towns, for the temple's world scroll. Free of the game's state so they can be tested on their own.
namespace openblack::statistics
{

/// What a miracle counts towards in the player's totals of miracles cast
enum class MiracleKind
{
	Aggressive,
	Nice,
	Creature,
};
/// The total a magic type counts towards, none for those the game doesn't count (the hungry, frightened, tired, ill and
/// thirsty creature miracles, and none)
[[nodiscard]] std::optional<MiracleKind> MiracleKindOf(MagicType type);
/// How many miracles of a kind a player has cast, from their count of each magic type
[[nodiscard]] uint32_t MiraclesCast(std::span<const uint32_t> castsOfType, MiracleKind kind);

/// What one of the player's towns has now, for the figures worked out from the towns
struct TownCounts
{
	uint32_t men {0};
	uint32_t women {0};
	/// Its homes with places for people, its civic buildings and its wonders
	uint32_t abodes {0};
	uint32_t civic {0};
	uint32_t wonders {0};
	/// Its adults and children, and the places for adults and for children in its homes
	uint32_t adults {0};
	uint32_t children {0};
	uint32_t adultPlaces {0};
	uint32_t childPlaces {0};
	/// Its disciples of each kind (VillagerDisciple)
	std::array<uint32_t, 13> disciples {};
	/// The people living in its homes and its homeless, and of them those who are not women, counted from those lists
	/// as they stand (the world scroll's share of men reads these, not the town's running counts)
	uint32_t housed {0};
	uint32_t housedMen {0};
};

/// The game's count of the player's buildings: through the towns in order it keeps running counts of homes and civic
/// buildings and adds both to the total town by town, so with more than one town the earlier towns count again
[[nodiscard]] uint32_t TotalBuildings(std::span<const TownCounts> towns);

/// A share written as a whole percentage: the share of 100, rounded towards zero
[[nodiscard]] int32_t Percent(float share);

/// What the world scroll in the temple's main room tells the player
struct WorldFigures
{
	/// Every villager in the world not dying
	int32_t population {0};
	int32_t believersPercent {0};
	int32_t malePercent {0};
	int32_t deaths {0};
	int32_t births {0};
	int32_t sacrifices {0};
	/// The running count of buildings, and the wonders standing
	int32_t buildings {0};
	int32_t wonders {0};
	int32_t disciples {0};
	/// Builders, breeders, fishermen, farmers, foresters, missionaries, craftsmen and traders
	std::array<int32_t, 8> discipleKinds {};
};

/// What the world scroll is worked out from
struct WorldInputs
{
	uint32_t worldPopulation {0};
	/// The player's towns
	std::span<const TownCounts> towns;
	/// The player's missionaries preaching in towns that aren't theirs, and in any town
	uint32_t missionariesInOtherTowns {0};
	uint32_t missionaries {0};
	/// The player's counted deaths, births and sacrifices
	uint32_t deaths {0};
	uint32_t births {0};
	uint32_t sacrifices {0};
};
[[nodiscard]] WorldFigures World(const WorldInputs& inputs);

/// What a creature counts of its life for its player's statistics
struct CreatureTallies
{
	uint32_t peopleKilled {0};
	uint32_t animalsKilled {0};
	uint32_t creaturesKilled {0};
	uint32_t battlesFought {0};
	uint32_t battlesWon {0};
	uint32_t poos {0};
	uint32_t mushroomsEaten {0};
};

/// The game's own frame rate for its statistics: the frames drawn over at least three seconds. It is measured again only
/// once at least four frames have been drawn in that time and the rate is five or more; until then the time continues.
class FrameRateMeter
{
public:
	static constexpr uint32_t k_LeastMilliseconds = 3000;
	static constexpr uint32_t k_LeastFrames = 4;
	static constexpr int32_t k_LeastRate = 5;

	/// A frame is drawn: the clock in milliseconds and the frames drawn so far
	void Frame(uint32_t milliseconds, uint32_t frames);
	[[nodiscard]] int32_t Rate() const noexcept { return _rate; }

private:
	int32_t _rate {k_LeastRate};
	bool _started {false};
	bool _framesNoted {false};
	uint32_t _since {0};
	uint32_t _framesSince {0};
};

/// The help record's events that the Tech Stats figures count
enum class HelpEvent : uint32_t
{
	PickedUp = 2,
	Thrown = 4,
	Gestures = 24,
	Rotations = 25,
	Pitches = 28,
	Zooms = 29,
	Dragging = 33,
	FightBlocks = 36,
	FightAttacks = 37,
	FightSteps = 39,
	InterfaceActions = 43,
	LookingAtGround = 44,
	LookingAtSky = 46,
};
/// The Tech Stats figures from the player's help record: the game turns each kind of event happened on
struct HelpFigures
{
	uint32_t distanceDragged {0};
	uint32_t timeLookingAtSky {0};
	uint32_t timeLookingAtGround {0};
	uint32_t gestures {0};
	uint32_t zooms {0};
	uint32_t rotations {0};
	uint32_t pitches {0};
	uint32_t interfaceActions {0};
	uint32_t thrown {0};
	uint32_t pickedUp {0};
	uint32_t fightAttacks {0};
	uint32_t fightBlocks {0};
	uint32_t fightSteps {0};
};
/// The figures from the help record's count of each event. The distance dragged is its count times 3, the zooms their
/// count times 9.
[[nodiscard]] HelpFigures HelpFiguresFrom(const std::function<uint32_t(HelpEvent)>& count);

/// The most and the fewest of a figure that the game keeps
struct Extremes
{
	uint32_t highest {0};
	uint32_t lowest {0};
};
/// The buildings a player finished: all of them, and its homes, civic buildings and wonders
struct BuildingsBuilt
{
	uint32_t total {0};
	uint32_t abodes {0};
	uint32_t civic {0};
	uint32_t wonders {0};
};

/// One player's figures in the Game Statistics box, taken as the box opens
struct PlayerFigures
{
	PlayerNames player {PlayerNames::PLAYER_ONE};

	float influence {0.0f};
	uint32_t godsDefeated {0};
	float alignmentChange {0.0f};
	uint32_t peopleKilled {0};
	/// The belief in the player summed over every town, times 1000, now and at its most and least
	float belief {0.0f};
	float maxBelief {0.0f};
	float minBelief {0.0f};
	uint32_t artefacts {0};
	float creatureGrowth {0.0f};
	float creatureAlignmentChange {0.0f};
	uint32_t buildingsDestroyed {0};

	/// The creature's damage, hunger and tiredness, each out of 100
	float creatureDamage {0.0f};
	float creatureHunger {0.0f};
	float creatureTiredness {0.0f};
	uint32_t creatureAge {0};
	float creatureAlignment {0.0f};
	CreatureTallies creature;

	uint32_t births {0};
	uint32_t deaths {0};
	uint32_t population {0};
	uint32_t men {0};
	uint32_t women {0};
	uint32_t capacity {0};
	/// The most and fewest people, men and women, as 0 while the fewest has not been taken yet
	Extremes populationExtremes;
	Extremes menExtremes;
	Extremes womenExtremes;
	uint32_t converted {0};
	float satisfaction {0.0f};
	uint32_t towns {0};
	uint32_t totalBuildings {0};
	uint32_t abodes {0};
	uint32_t civic {0};
	uint32_t wonders {0};
	BuildingsBuilt built;
	std::array<uint32_t, 8> disciplesMade {};
	uint32_t foodUsed {0};
	uint32_t woodUsed {0};

	std::array<uint32_t, 42> castsOfType {};
	float chantsUsed {0.0f};
	uint32_t sacrifices {0};

	/// The finished points of the player's influence and population histories, oldest first
	std::vector<float> influenceHistory;
	std::vector<float> populationHistory;
};

/// The belief figures are shown 1000 times the belief
inline constexpr float k_BeliefScale = 1000.0f;
/// The belief in the player summed over every town in turn, times 1000
[[nodiscard]] float TotalBelief(std::span<const float> beliefInPlayer);

/// The player's town figures from their towns in order: the people, men and women, the places in their homes, the
/// towns, their homes, civic buildings and wonders, and the running count of buildings
void SetTownFigures(PlayerFigures& figures, std::span<const TownCounts> towns);

/// The alignment and size of the player's creature now
struct CreatureBody
{
	float alignment {0.0f};
	float size {0.0f};
};
/// The needs of the player's creature now: its life, energy and exhaustion, 0 to 1, and its age
struct CreatureNeedsNow
{
	float life {0.0f};
	float energy {0.0f};
	float exhaustion {0.0f};
	uint32_t age {0};
};
/// What the box reads of the player's creature: its body and needs, when it has them, and its tallies
struct CreatureNow
{
	std::optional<CreatureBody> body;
	std::optional<CreatureNeedsNow> needs;
	CreatureTallies tallies;
};
/// The player's creature figures from their creature now and the alignment and size it started with. Without a
/// creature every creature figure stays 0.
void SetCreatureFigures(PlayerFigures& figures, const std::optional<CreatureNow>& creature, float alignmentAtStart,
                        float sizeAtStart);

/// The figures the game keeps once, for the Tech Stats tab
struct TechFigures
{
	uint32_t linesOfCode {0};
	float maxFrameRate {0.0f};
	float minFrameRate {0.0f};
	uint32_t objectsCreated {0};
	/// Game turns over 10
	uint32_t timePlayed {0};
	uint32_t scriptInstructions {0};
	/// The number of figures the game says it keeps
	uint32_t statisticsCount {0};
	HelpFigures help;
};

/// Everything the Game Statistics box shows, taken as it opens
struct BoxFigures
{
	/// Players one up to playersInGame, in player order
	std::vector<PlayerFigures> players;
	/// The human and computer players among the first four places
	uint32_t playersInGame {0};
	/// The land, which gives some players another colour
	int32_t landNumber {0};
	TechFigures tech;
};
/// At most this many players are shown
inline constexpr size_t k_BoxPlayers = 4;
/// A player as the Game Statistics box counts it: its place and its type (1 the human, 2 a computer god)
struct BoxPlayer
{
	PlayerNames name;
	int32_t type {0};
};
/// How many players the box shows, from player one up: the human and computer gods among the first four places, at
/// most four. A gap in their places shows the empty player and leaves out the last god
[[nodiscard]] constexpr uint32_t PlayersShown(std::span<const BoxPlayer> players)
{
	constexpr int32_t k_Human = 1;
	constexpr int32_t k_Computer = 2;
	const auto counted = std::ranges::count_if(players, [](const BoxPlayer& player) {
		return (player.type == k_Human || player.type == k_Computer) && static_cast<size_t>(player.name) < k_BoxPlayers;
	});
	return static_cast<uint32_t>(std::min(static_cast<size_t>(counted), k_BoxPlayers));
}
/// The game time played that the box shows: game turns over 10
[[nodiscard]] constexpr uint32_t TimePlayed(uint32_t turn)
{
	return turn / 10;
}

} // namespace openblack::statistics
