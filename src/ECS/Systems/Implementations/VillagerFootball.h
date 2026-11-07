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

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

/// A villager playing or watching football: the players' substates (each position's table of what it may play, when,
/// and how much it wants to), the dead ball's fetch and kick, the pause, the watchers and their Mexican wave, the run
/// to where a flying ball comes down, and the shared exit of the football states
/// (docs/wiki/world/buildings.md#the-football-pitch).
namespace openblack::ecs::villager
{

/// A player told to choose again chooses its substate among its position's rows (an attacker's or a defender's
/// eight, a goalie's seven): each row whose conditions hold is weighed and the first strictly heaviest is played;
/// then it no longer chooses, its substate's turns from 0. Anyone not in a position: nothing
void AssignFootballSubState(entt::entity player);

/// States 73 FOOTBALL_ATTACKER, 74 FOOTBALL_GOALIE and 75 FOOTBALL_DEFENDER: no pitch or no ball, deciding afresh. Its
/// counter under 1 counts up; else its substate plays, and once that substate's first condition no longer holds the
/// player chooses again. 1
uint32_t FootballAttacker(components::LivingAction& action);
uint32_t FootballGoalie(components::LivingAction& action);
uint32_t FootballDefender(components::LivingAction& action);
/// States 76 FOOTBALL_WON_GOAL and 77 FOOTBALL_LOST_GOAL: 1
uint32_t FootballWonGoal(components::LivingAction& action);
uint32_t FootballLostGoal(components::LivingAction& action);
/// State 78 START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL: with a pitch and a ball, the walk to the ball (then 79), 1;
/// else 0
uint32_t StartMoveToPickUpBallForDeadBall(components::LivingAction& action);
/// State 79 ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL: a quick turn towards the pitch, then 80. 1
uint32_t ArrivedAtPickUpBallForDeadBall(components::LivingAction& action);
/// State 80 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START: its clip, then 81. 1
uint32_t ArrivedAtPutDownBallForDeadBallStart(components::LivingAction& action);
/// State 81 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END: the ball touched and below the player's height is kicked to
/// the pitch's middle at 10 (a kick that cannot score); the walk to the ball again (then 79), and the match plays on.
/// 1
uint32_t ArrivedAtPutDownBallForDeadBallEnd(components::LivingAction& action);
/// State 82 FOOTBALL_MATCH_PAUSED: with a pitch and a ball, a turn towards the ball. 1
uint32_t FootballMatchPaused(components::LivingAction& action);
/// State 83 FOOTBALL_WATCH_MATCH: no town, pitch or ball, deciding afresh. Waiting: a turn towards the pitch;
/// playing: towards the ball (each the turn's result); a fourth state no longer playing football, deciding or else
/// going home. 1
uint32_t FootballWatchMatch(components::LivingAction& action);
/// State 84 FOOTBALL_MEXICAN_WAVE: no town, deciding afresh; else a quick turn towards the pitch and the wave's clip,
/// then watching again. 1
uint32_t FootballMexicanWave(components::LivingAction& action);
/// State 222 FOOTBALL_MOVE_TO_BALL: no pitch or no available ball, deciding afresh; else the walk aimed again at
/// where the ball comes down when that has moved more than a metre, its step, and on arrival the final state. The
/// walk's result
uint32_t FootballMoveToBall(components::LivingAction& action);

/// The exit of the football states (71 to 84): the circle it went round let go; to another football state or a walk,
/// nothing else; else it no longer chases the ball, it leaves its side, and it no longer plays football. 1
uint32_t ExitFootball(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager
