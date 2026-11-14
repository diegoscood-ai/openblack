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

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/FootballPitch.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

/// The football pitch: a town's abode of its own kind, its ball and the villagers who take it up to play or watch
/// (docs/wiki/world/buildings.md#the-football-pitch). Its pure rules are ecs::football_rules; its state is
/// components::FootballPitch, which only this writes.
namespace openblack::ecs::football
{

/// Whether the game has football (EngineConfig::footballEnabled, read once as the game starts): towns want a pitch and
/// scaffolds join up to eight only then, and a town runs its pitch's turn a second time each turn. False without a
/// config
[[nodiscard]] bool IsEnabled();

/// The entity is a pitch with its game (components::FootballPitch)
[[nodiscard]] bool IsPitch(entt::entity entity);

/// The pitch's own part of its making: its game at its start (no ball yet, waiting for players, open to villagers).
/// AbodeArchetype calls it for every pitch it makes, a plan's or a whole one
void Create(entt::entity pitch);

/// The town's pitch: the last one made functional, while it is still there; null when none
[[nodiscard]] entt::entity PitchOf(entt::entity town);

/// The pitch's own part of becoming functional, before the abode's part: with a town, it becomes the town's pitch,
/// functional and waiting for players, and makes a ball on its middle unless it has one still about. With or without
/// football. Nothing without a town
void MakeFunctional(entt::entity pitch);

/// The pitch's ball, made on its middle when it has none (null for what is not a pitch)
entt::entity GetBall(entt::entity pitch);

/// The pitch's turn: the town's abode pass runs it every time, football or not, and the town's turn once more when
/// football is on. It starts by making a ball when it has none
void Process(entt::entity pitch);

/// A goal's point in the world (index 0 or 1, the goal model's first or second goal mouth): its centre through the
/// pitch's drawn matrix, as a map position; the pitch's own position for an index the goal model has no goal for, or
/// without the goal model
[[nodiscard]] map_coords::MapCoords GoalPosition(entt::entity pitch, uint32_t index);

/// The pitch's look at its ball this turn: the ball's point taken again; while the match is playing, a goal in the
/// first goal mouth is the home tally's (GoalHasBeenScored(1)), else one in the second the away tally's
/// (GoalHasBeenScored(0)); the point kept for the next turn's test. Whether the ball is still along the pitch
bool ProcessBall(entt::entity pitch, entt::entity ball);

/// Whether the ball has gone into the goal mouth `index` since the last turn: only a ball a kick has set moving; the
/// ball's heading is kept on the pitch on the way
bool IsInsideGoal(entt::entity pitch, entt::entity ball, uint32_t index);

/// A goal: the match pauses for the scorers' celebration, the side given it is kept, and every home player becomes a
/// mourner when `side` is 1 (else a celebrant), every away player the other
void GoalHasBeenScored(entt::entity pitch, uint32_t side);

/// The pitch goes: every player of both sides and of their rankings still about decides afresh, and the lists empty
void DeleteDependants(entt::entity pitch);

/// A player chooses what it plays now among its position's substates, when it has been told to choose again
void AssignSubstate(entt::entity player);

/// The player is on the home side
[[nodiscard]] bool IsPlayerOnHomeTeam(entt::entity pitch, entt::entity player);
/// The player's place in its side (as last sorted by the ball: 0 the nearest); -1 when on neither
[[nodiscard]] int32_t BallRank(entt::entity pitch, entt::entity player);
/// The player's place in its side's ranking by the goal it attacks (0 the nearest); -1 when not ranked
[[nodiscard]] int32_t GoalRank(entt::entity pitch, entt::entity player);
/// The player at a place of a side's ranking (side 0 home, 1 away); null past its end
[[nodiscard]] entt::entity RankedPlayer(entt::entity pitch, uint32_t side, int32_t place);
/// The attacker of side `index` (0 home, 1 away) nearest the ball, as the sides were last sorted
[[nodiscard]] entt::entity NearestAttacker(entt::entity pitch, uint32_t index);
/// A kick's aim spread by its length: half the distance from the player to the target, each axis moved by a draw of
/// twice that less it (x first, then z)
void PerturbTarget(map_coords::MapCoords& target, entt::entity player);
/// A player's home spot: an attacker's or a defender's place in its side's formation towards the goal it attacks
/// (side: 0 when the player is on the home side), a goalie's the goal mouth `side`, anyone else's the pitch's middle
[[nodiscard]] map_coords::MapCoords HomePosition(entt::entity pitch, entt::entity player, uint32_t side);
/// The match's state set, with its 15 s counter started again
void SetPlayState(entt::entity pitch, components::FootballPitch::PlayState state);
/// A player leaves the match: it no longer chases the ball, and it leaves its side (the home side's first, else the
/// away side's); its place in the wave is kept
void PlayerLeaves(entt::entity pitch, entt::entity player);

/// The players of both sides (a script's size of the pitch); 0 for what is not a pitch
[[nodiscard]] uint32_t PlayerCount(entt::entity pitch);

/// How much a villager wants to take the pitch up (relaxation_rules::FootballDesire on the pitch's sides, referee and
/// last match); 0 for what is not a pitch
[[nodiscard]] float GetVillagerActivityDesire(entt::entity pitch, entt::entity villager);

/// A relaxing villager takes the pitch up, as football_rules::ChooseRole says: the first is its referee and stays
/// where it is; a player joins the home side, then the away side, newest first, and walks to a spot within 4 m each
/// way of the middle to wait for the kick-off; once both sides have five, a watcher walks to a place round the pitch.
/// With only one player on both sides the wait for players starts again. The villager is marked as taken by football
/// and keeps the pitch; it stays in its town. False when the pitch takes no one (closed, or no ball)
bool SetVillagerActivity(entt::entity pitch, entt::entity villager);

/// The state of a player waiting at its spot for the kick-off: once its pitch's match is playing it becomes an
/// attacker; until then it waits. Returns 1
uint32_t WaitForKickOff(components::LivingAction& action);

} // namespace openblack::ecs::football
