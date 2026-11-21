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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/FootballRules.h"

namespace openblack::ecs::components
{

/// The 40 watchers' places, all free
[[nodiscard]] constexpr std::array<entt::entity, football_rules::k_WavePlaces> EmptyWavePlaces()
{
	std::array<entt::entity, football_rules::k_WavePlaces> places {};
	places.fill(entt::null);
	return places;
}

/// A football pitch's game, on top of its Abode. One per pitch, assigned by ecs::football::Create (from
/// AbodeArchetype) as it is made, everything at its start; ecs::football is its only writer
/// (docs/wiki/world/buildings.md#the-football-pitch).
struct FootballPitch
{
	/// What the match is doing
	enum class PlayState : uint8_t
	{
		WaitingForPlayers = 0,
		Playing = 1,
		/// A goal has been scored: the players celebrate or mourn
		GoalScored = 3,
	};

	/// Set once it is built and made its town's pitch
	bool functional {false};
	PlayState playState {PlayState::WaitingForPlayers};
	/// The pitch's own town match: it takes villagers up and stops once the town wants other things (a script's match is
	/// not one); on from the start
	bool open {true};
	/// The ball it plays with, made on its middle when it has none; null until then
	entt::entity ball {entt::null};
	/// The first villager to come, who keeps the game
	entt::entity referee {entt::null};
	/// Each side's players, the newest first, at most five
	std::vector<entt::entity> homeTeam;
	std::vector<entt::entity> awayTeam;
	/// The watchers in the 40 places round the pitch, a free place null, and how many places were taken
	std::array<entt::entity, football_rules::k_WavePlaces> wave {EmptyWavePlaces()};
	uint32_t wavePlacesTaken {0};
	/// Turns the match has waited for players, and then has been played
	uint32_t waitingTurns {0};
	/// The ball has been put on the middle for the kick-off
	bool kickOffReady {false};
	/// The turns of a dead ball (always 0 again)
	uint32_t deadBallTurns {0};
	/// The Mexican wave: the next of the 40 places to rise, and the turns it rests after going round
	uint32_t waveIndex {0};
	uint32_t waveRest {0};
	/// Each side ranked by distance to the goal it attacks, nearest first (the home side's, the away side's)
	std::array<std::vector<entt::entity>, 2> ranked;
	/// Each side's attacker and defender nearest the ball, as the sides were last sorted (the attackers the defenders
	/// mark); kept until another is found
	std::array<entt::entity, 2> nearestAttacker {entt::null, entt::null};
	std::array<entt::entity, 2> nearestDefender {entt::null, entt::null};
	/// The player fetching a ball that has left the pitch
	entt::entity ballChaser {entt::null};
	/// The turns since the sides were last ranked while playing
	uint32_t playStateTimer {0};
	/// The turn the last match ended, 0 while none has
	uint32_t lastMatchTurn {0};
	/// The goals each side's tally holds (bytes, as kept): a goal in the first goal adds to the home tally, though the
	/// home side mourns it
	uint8_t homeScore {0};
	uint8_t awayScore {0};
	/// The side the last goal was given to (1 the away side's win, the home side mourning; 0 the home side's) and the
	/// turns since it
	uint32_t scoredSide {0};
	uint32_t goalTurns {0};
	/// The ball's heading the goal test last found, a unit vector in the pitch's space; (1, 0, 0) at the start
	glm::vec3 ballHeading {1.0f, 0.0f, 0.0f};
};

} // namespace openblack::ecs::components
