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

/// Whether a playing match goes back to waiting for players: a side has none, or both have only one each
[[nodiscard]] constexpr bool BackToWaiting(size_t home, size_t away)
{
	return home == 0 || away == 0 || (home == 1 && away == 1);
}

/// Which side's first player fetches a ball off the pitch (0 home, 1 away): the one further from the ball, by the
/// distances of the two sides' first players; at the same distance the away side's
[[nodiscard]] constexpr uint32_t DeadBallFetcherSide(float homeDistance, float awayDistance)
{
	return homeDistance <= awayDistance ? 1u : 0u;
}

/// What a goal in a goal mouth (0 the goal model's first, 1 its second) counts for: the side it is given (the first
/// mouth's 1, the second's 0), and whether it adds to the home tally (the first mouth's) or the away tally
struct GoalCredit
{
	uint32_t side {0};
	bool homeTally {false};
};
[[nodiscard]] constexpr GoalCredit CreditFor(uint32_t mouth)
{
	return mouth == 0 ? GoalCredit {.side = 1, .homeTally = true} : GoalCredit {.side = 0, .homeTally = false};
}

/// Whether a side's players mourn a goal given to `side` (else they celebrate): the home side mourns one given 1, the
/// away side one given 0
[[nodiscard]] constexpr bool Mourns(bool homeSide, uint32_t side)
{
	return homeSide ? side != 0 : side == 0;
}

/// A ball further than this from its pitch, or held by a god's hand further than the second, is taken away (metres)
inline constexpr float k_BallLostMetres = 100.0f;
inline constexpr float k_BallHeldOffMetres = 80.0f;

/// Whether the pitch's ball is taken away and a new one made: more than 100 m from the pitch, on water, or held by a
/// god's hand (any player's, the computer gods' too) more than 80 m from it
[[nodiscard]] constexpr bool BallIsLost(float metresFromPitch, bool onWater, bool heldByAGod)
{
	return metresFromPitch > k_BallLostMetres || onWater || (heldByAGod && metresFromPitch > k_BallHeldOffMetres);
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

/// Both sides have the fewest a match starts with; with fewer, the waiting match only counts towards sending its
/// players home
[[nodiscard]] constexpr bool EnoughASide(size_t home, size_t away)
{
	return home >= k_FewestASide && away >= k_FewestASide;
}

/// A match's clock, just moved on, is past a wait of some seconds: it has more turns than the seconds take
[[nodiscard]] bool PastTheWait(uint32_t turns, float seconds, uint32_t msPerTurn);

/// An attacker's shot at the goal: kicked at 12 metres a second plus a draw of 4
inline constexpr float k_ShootDraw = 4.0f;
[[nodiscard]] constexpr float ShootSpeed(float draw)
{
	return draw + 12.0f;
}
/// An attacker's lob near the goal: aimed within 2 metres of it on each axis (two draws of 4, less 2), kicked at 3
/// metres a second plus a draw of 1
inline constexpr float k_LobScatterDraw = 4.0f;
inline constexpr float k_LobScatterHalf = 2.0f;
inline constexpr float k_LobDraw = 1.0f;
[[nodiscard]] constexpr float LobSpeed(float draw)
{
	return draw + 3.0f;
}
/// A dribble's tap: kicked at 6 metres a second plus a draw of 1
inline constexpr float k_DribbleDraw = 1.0f;
[[nodiscard]] constexpr float DribbleSpeed(float draw)
{
	return draw + 6.0f;
}
/// How much a player wants each choice: a pass 0.8 plus a draw of 0.2 (when it may pass), a save 0.7 plus a draw of
/// 0.3, a clear 0.6 plus a draw of 0.4, a mark a draw of 1
inline constexpr float k_PassDraw = 0.2f;
inline constexpr float k_SaveWeightDraw = 0.3f;
inline constexpr float k_ClearWeightDraw = 0.4f;
inline constexpr float k_MarkWeightDraw = 1.0f;
[[nodiscard]] constexpr float PassWeight(float draw)
{
	return draw + 0.8f;
}
[[nodiscard]] constexpr float SaveWeight(float draw)
{
	return draw + 0.7f;
}
[[nodiscard]] constexpr float ClearWeight(float draw)
{
	return draw + 0.6f;
}

/// A defender's clear or save: aimed at the goal it attacks scattered by up to 5 metres each way on each axis (two
/// draws of 10, less 5), kicked at 8 metres a second plus a draw of 3
inline constexpr float k_DefenderScatterDraw = 10.0f;
inline constexpr float k_DefenderScatterHalf = 5.0f;
inline constexpr float k_DefenderKickDraw = 3.0f;
[[nodiscard]] constexpr float DefenderKickSpeed(float draw)
{
	return draw + 8.0f;
}
/// A goalkeeper's save: kicked at 9 metres a second plus a draw of 1
inline constexpr float k_GoalieKickDraw = 1.0f;
[[nodiscard]] constexpr float GoalieKickSpeed(float draw)
{
	return draw + 9.0f;
}
/// Whether a player going for the ball runs to where a flying ball comes down: the goalkeeper on a draw of 10 of 8 or
/// more (one time in five), the others on a draw of 3 that is not 0 (two times in three); the draw is made only when
/// the ball flies
[[nodiscard]] constexpr uint32_t FlyingBallDraw(bool goalie)
{
	return goalie ? 10u : 3u;
}
[[nodiscard]] constexpr bool RunsToFlyingBall(bool goalie, uint32_t draw)
{
	return goalie ? draw >= 8u : draw != 0u;
}

/// The kick-off comes once it is ready and fewer than five players moved this turn
[[nodiscard]] constexpr bool KicksOff(bool kickOffReady, uint32_t moving)
{
	return kickOffReady && moving < k_KickOffMovingLimit;
}

/// The Mexican wave's round of the 40 places: the next place to rise and the turns it still rests
struct WaveClock
{
	uint32_t index {0};
	uint32_t rest {0};
};

/// The wave's turn: while it rests, a turn less of rest and no place; else the place whose watcher may rise this turn,
/// and the clock on to the next, resting 256 turns after the 40th and starting again from the first
[[nodiscard]] std::optional<uint32_t> WaveTurn(WaveClock& clock);

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
