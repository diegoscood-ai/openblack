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
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// Football's rules that need no world: whether the game has football at all, which part a villager taking up the
/// town's pitch is given, and the places the players and the watchers walk to. Pure rules, tested with made-up
/// values (docs/wiki/world/buildings.md#the-football-pitch).
namespace openblack::ecs::football_rules
{

/// Players a side at most
inline constexpr uint32_t k_TeamSize = 5;
/// The places round the pitch the first watchers take, and the Mexican wave goes round
inline constexpr uint32_t k_WavePlaces = 40;
/// A player's spot is drawn this wide in each of x and z, centred on the pitch's middle (metres)
inline constexpr float k_PlayerSpread = 8.0f;
/// A watcher's place is drawn this wide in each of x and z round its point by the pitch (metres)
inline constexpr float k_WatcherSpread = 4.0f;
/// How far outside the pitch's edge a watcher's point is (metres)
inline constexpr float k_WatcherOutside = 2.0f;

/// Whether the game has football: the add-on installed, and not a network game, read once as the game starts.
/// openblack plays no network game, so with the add-on football is always on
[[nodiscard]] constexpr bool EnabledAtGameStart(bool addOnInstalled, bool networkGame)
{
	return addOnInstalled && !networkGame;
}

/// The part a villager taking up the pitch is given
enum class Role : uint8_t
{
	/// The pitch takes no one: it is closed or it has no ball
	None,
	/// The first to come keeps the game; it stays where it is
	Referee,
	Home,
	Away,
	/// Both sides full: the villager comes to watch
	Watcher,
};

/// What a pitch has when a villager comes to it
struct PitchSides
{
	/// The pitch takes villagers at all (it does from the start)
	bool open {true};
	bool hasBall {true};
	bool hasReferee {false};
	uint32_t home {0};
	uint32_t away {0};
};

/// The part a villager is given: none when the pitch is closed or has no ball; else the referee when there is none
/// yet; else the home side while it has fewer than five, then the away side while it has fewer than five; else a
/// watcher
[[nodiscard]] constexpr Role ChooseRole(const PitchSides& sides)
{
	if (!sides.open || !sides.hasBall)
	{
		return Role::None;
	}
	if (!sides.hasReferee)
	{
		return Role::Referee;
	}
	if (sides.home < k_TeamSize)
	{
		return Role::Home;
	}
	if (sides.away < k_TeamSize)
	{
		return Role::Away;
	}
	return Role::Watcher;
}

/// Whether the wait for players starts again: the side just joined is the only player of both sides
[[nodiscard]] constexpr bool RestartsTheWait(uint32_t home, uint32_t away)
{
	return home + away == 1;
}

/// How many turns some seconds of the match take: the whole turns a second (1000 / the milliseconds a turn, an integer
/// division) times the seconds, a float product, truncated
[[nodiscard]] uint32_t TurnsFor(float seconds, uint32_t msPerTurn);

/// Seconds of the match's own clocks
inline constexpr float k_KickOffWaitSeconds = 30.0f; ///< with three a side, the ball goes to the middle after this
inline constexpr float k_SendAwaySeconds = 180.0f;   ///< with fewer, the players go home after this
inline constexpr float k_RoleShuffleSeconds = 15.0f; ///< the sides are ranked again after this
inline constexpr uint32_t k_GoalPauseTurns = 70;     ///< the play pauses this many turns after a goal
inline constexpr uint32_t k_TownMatchTurns = 0x708;  ///< a town match may stop once it has run this many turns
inline constexpr uint32_t k_WaveRestTurns = 0x100;   ///< the Mexican wave rests this long after going round
inline constexpr uint32_t k_KickOffMovingLimit = 5;  ///< kick-off once fewer than this many players are still moving
inline constexpr uint32_t k_FewestASide = 3;         ///< fewer than this on a side and the match does not start

/// The belief a town gains at the end of a match: (the match's count / (the duration's turns x the turns a second))
/// at most 2, times the town's belief for a match. Each step as the original: the duration's turns truncated, then
/// times the turns a second again and truncated, the count over that (a float division: 0 over 0 is not a number,
/// which passes the "at most 2" test)
[[nodiscard]] float MatchBelief(uint32_t count, float durationSeconds, float beliefForMatch, uint32_t msPerTurn);

/// The order a side is ranked in by distance to the goal it attacks: each player in the side's order is put before the
/// first already ranked who is strictly further, else last (the first goes in alone). The indices of `distances`
[[nodiscard]] std::vector<size_t> RankOrder(std::span<const uint32_t> distances);

/// What a ranked player plays: the last ranked keeps goal; the first half (rounded down) attack, numbered from 0; the
/// rest defend, numbered from 0
enum class Position : uint8_t
{
	Attacker,
	Goalie,
	Defender,
};
struct RankedPosition
{
	Position position {Position::Goalie};
	/// The player's place among its position's (attackers and defenders; 0 for the goalie, which keeps its last one)
	uint8_t number {0};
	bool numbered {false};
};
[[nodiscard]] constexpr RankedPosition PositionForRank(size_t rank, size_t count)
{
	if (rank == count - 1)
	{
		return {.position = Position::Goalie};
	}
	const size_t half = count >> 1;
	if (rank < half)
	{
		return {.position = Position::Attacker, .number = static_cast<uint8_t>(rank), .numbered = true};
	}
	return {.position = Position::Defender, .number = static_cast<uint8_t>(rank - half), .numbered = true};
}

/// The launch of a kick from `from` to `to` (world points): its flight takes t = distance / speed under a gravity of
/// 9.81, so the velocity is ((dx - 0) / t, (dy - (-9.81 x t x t x 0.5)) / t, (dz - 0) / t), each step a float (the
/// distance sqrt((dz dz + dy dy) + dx dx), 1 / t taken once)
[[nodiscard]] glm::vec3 KickVelocity(const glm::vec3& from, const glm::vec3& to, float speed);

/// The players' priorities for their substates, from distances in metres (each clamp, product and sum a float)
/// Shooting: 1 - (the distance to the goal attacked, within 0..40) x 0.025, within 0.1..1
[[nodiscard]] float ShootPriority(float toGoal);
/// Dribbling: (the distance to the goal attacked, within 0..40) x 0.025, within 0.1..1
[[nodiscard]] float DribblePriority(float toGoal);
/// Moving to be passed to: (the distance to the ball, within 0..20) x 0.05, within 0..0.2
[[nodiscard]] float MoveToBePassedPriority(float toBall);
/// Going home: (the distance to the player's home spot, within 0..20) x 0.05, within 0.1..1
[[nodiscard]] float GoHomePriority(float toHome);
/// Standing idle: (1 - (the distance home, within 0..10) x 0.1) x ((the distance to the ball, within 10..40) - 10) x
/// 0.025, within 0..1
[[nodiscard]] float IdlePriority(float toBall, float toHome);

/// A player's share across the line from the pitch's middle to the goal its side attacks, for the place `i` of `k`
/// in its row, `width` the row's width: alone in the middle; two at (i - 0.5) x width; more spread over the width
[[nodiscard]] float AcrossShare(uint32_t i, uint32_t k, float width);
/// Where an attacker's or a defender's home spot is: its share across and along the line from the pitch's middle to
/// the goal attacked (attackers 0.33 of the way, defenders -0.76, each further row of five attackers or three
/// defenders 0.1 back)
struct FormationShares
{
	float across {0.0f};
	float along {0.0f};
};
[[nodiscard]] FormationShares AttackerShares(uint32_t number, uint32_t attackers);
[[nodiscard]] FormationShares DefenderShares(uint32_t number, uint32_t defenders);
/// The home spot in the world: from the pitch's middle `a` towards the goal attacked `b`, along by `along` and across
/// (to the left of the way) by `across`
[[nodiscard]] glm::vec3 FormationSpot(const glm::vec3& a, const glm::vec3& b, const FormationShares& shares);

/// Where an attacker goes to be passed to: from its own goal `own` towards the goal attacked `attacked` by
/// 0.6 + a draw of 0.2, then across the way by a draw of 18 less 9 (along the way's normal in the ground plane)
[[nodiscard]] map_coords::MapCoords MoveToBePassedSpot(const glm::vec3& own, const glm::vec3& attacked, float drawAlong,
                                                       float drawAcross);

/// Where a dribble kicks the ball to: a fifth of the way from the player `me` to the goal it attacks `goal` (the way
/// made unit unless it is nothing, the distance sqrt((dy dy + dx dx) + dz dz), each step a float)
[[nodiscard]] glm::vec3 DribbleSpot(const glm::vec3& me, const glm::vec3& goal);

/// A player's choice of substate among `count` rows: each row whose conditions hold (`allowed`) is weighed
/// (`priority`, asked only for those), the first strictly heaviest is taken; none above -0.0001 leaves `count`, which
/// no row answers to
[[nodiscard]] uint8_t ChooseSubstate(uint8_t count, const std::function<bool(uint8_t)>& allowed,
                                     const std::function<float(uint8_t)>& priority);

/// A point moved by a draw on each axis, less half its width: each axis in metres plus (draw - half), back to the
/// map's units with the utility functions' rounding; the altitude kept
[[nodiscard]] map_coords::MapCoords Scattered(const map_coords::MapCoords& point, float drawX, float drawZ, float half);

/// Where a player walks to: the pitch's middle moved by up to 4 metres each way on each axis, from two draws in 0..8
/// (x first, then z)
[[nodiscard]] map_coords::MapCoords PlayerSpot(const map_coords::MapCoords& middle, float drawX, float drawZ);

/// The angle round the pitch of the next of the 40 watchers' places, 9 degrees after the one before, from 0
[[nodiscard]] float WatcherAngle(uint32_t placesTaken);

/// Where a watcher stands: its point by the pitch moved by up to 2 metres each way on each axis, from two draws in
/// 0..4 (x first, then z)
[[nodiscard]] map_coords::MapCoords WatcherPlace(const map_coords::MapCoords& point, float drawX, float drawZ);

} // namespace openblack::ecs::football_rules
