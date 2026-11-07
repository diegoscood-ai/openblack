/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFootball.h"

#include <array>
#include <optional>

#include "3D/MapCoords.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "ECS/BallKick.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/FootballRules.h"
#include "ECS/LivingTurn.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/Implementations/VillagerMoveToObject.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/Football.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/WalkGoal.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
namespace rules = football_rules;

/// The turning speeds of a look: slow, normal and fast
constexpr uint32_t k_LookNormal = 1;
constexpr uint32_t k_LookFast = 2;
/// An idle player chooses again after this long in its substate's counter
constexpr int32_t k_IdleTurns = 600;
/// A goalie looking out chooses again after this many turns, or with the ball this near (metres)
constexpr uint8_t k_GoalieLookTurns = 20;
constexpr float k_GoalieLookNear = 6.0f;
/// A goalie further than this from its goal does not pass (metres)
constexpr float k_GoaliePassFromHome = 10.0f;
/// At home within this, away beyond it; the ball quite near within this (metres)
constexpr float k_AtHome = 2.0f;
constexpr float k_QuiteNear = 5.0f;
/// A pass goes only to a team-mate further than this from the ball (metres)
constexpr float k_PassAtLeast = 1.0f;
/// The run to where a flying ball comes down is aimed again once that has moved more than this (metres)
constexpr float k_ReaimBeyond = 1.0f;
/// The walk's arrival
constexpr uint32_t k_Arrived = 10;
/// The match's states the watchers look by
constexpr uint32_t k_PlayWaiting = 0;
constexpr uint32_t k_PlayPlaying = 1;
constexpr uint32_t k_PlayOver = 4;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The pitch the villager plays or watches at; null for none
entt::entity PitchOf(entt::entity villager)
{
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr && football::IsPitch(v->footballPitch) ? v->footballPitch : entt::null;
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

void ChooseAgain(entt::entity player)
{
	if (auto* v = VillagerOf(player); v != nullptr)
	{
		v->footballChooseAgain = true;
	}
}

void ResetCounter(entt::entity player)
{
	if (auto* action = Entities().TryGet<LivingAction>(player); action != nullptr)
	{
		action->turnsUntilStateChange = 0;
	}
}

int32_t Counter(entt::entity player)
{
	const auto* action = Entities().TryGet<const LivingAction>(player);
	return action != nullptr ? static_cast<int16_t>(action->turnsUntilStateChange) : 0;
}

/// Its side: 0 home, 1 away
uint32_t SideOf(entt::entity pitch, entt::entity player)
{
	return football::IsPlayerOnHomeTeam(pitch, player) ? 0u : 1u;
}

/// The goal mouth the player's side attacks (the home side the second)
map_coords::MapCoords AttackedGoal(entt::entity pitch, entt::entity player)
{
	return football::GoalPosition(pitch, football::IsPlayerOnHomeTeam(pitch, player) ? 1u : 0u);
}

/// The goal mouth the player's side keeps
map_coords::MapCoords OwnGoal(entt::entity pitch, entt::entity player)
{
	return football::GoalPosition(pitch, football::IsPlayerOnHomeTeam(pitch, player) ? 0u : 1u);
}

map_coords::MapCoords HomeSpot(entt::entity pitch, entt::entity player)
{
	return football::HomePosition(pitch, player, SideOf(pitch, player));
}

float DistanceTo(entt::entity player, const map_coords::MapCoords& point)
{
	return game_math::GetDistanceInMetres(object::MapCoordsOf(player), point);
}

float DistanceToBall(entt::entity pitch, entt::entity player)
{
	const auto ball = football::GetBall(pitch);
	return ball != entt::null ? DistanceTo(player, object::MapCoordsOf(ball)) : 0.0f;
}

/// The player touches the ball, with its own height as the margin
bool Touching(entt::entity pitch, entt::entity player)
{
	const auto ball = football::GetBall(pitch);
	return ball != entt::null && object::IsTouching(player, ball, object::GetHeight(player));
}

/// Touching it with the ball below the player's height: a kick
bool AtTheBall(entt::entity pitch, entt::entity player)
{
	if (!Touching(pitch, player))
	{
		return false;
	}
	const auto ball = football::GetBall(pitch);
	return object::GetHeight(player) > object::MapCoordsOf(ball).altitude;
}

void Kick(entt::entity pitch, const map_coords::MapCoords& to, float speed, bool scoring)
{
	static_cast<void>(ball_kick::KickBallAtDestination(football::GetBall(pitch), to, speed, scoring));
}

bool BallFlying(entt::entity ball)
{
	return ball != entt::null && Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().IsFlying(ball);
}

/// Where the ball comes down (where it was last kicked to); its place when never kicked
map_coords::MapCoords BallDestination(entt::entity ball)
{
	const auto* b = Entities().TryGet<const Ball>(ball);
	return b != nullptr && b->destination.has_value() ? map_coords::FromWorld(*b->destination) : object::MapCoordsOf(ball);
}

void LookAt(entt::entity villager, entt::entity object, uint32_t mode)
{
	static_cast<void>(LookAtPos(villager, town_queries::PosOf(object), mode));
}

// The preconditions (the original's "touching" one is in no table)
bool IsNotTouchingBall(entt::entity pitch, entt::entity player)
{
	return !Touching(pitch, player);
}
bool IsNearestBall(entt::entity pitch, entt::entity player)
{
	return football::BallRank(pitch, player) == 0;
}
bool IsNotNearestBall(entt::entity pitch, entt::entity player)
{
	// on neither side counts as not the nearest
	return static_cast<uint32_t>(football::BallRank(pitch, player)) > 0;
}
bool IsAtHome(entt::entity pitch, entt::entity player)
{
	return DistanceTo(player, HomeSpot(pitch, player)) < k_AtHome;
}
bool IsNotAtHome(entt::entity pitch, entt::entity player)
{
	return DistanceTo(player, HomeSpot(pitch, player)) > k_AtHome;
}
bool IsQuiteNearBall(entt::entity pitch, entt::entity player)
{
	return DistanceToBall(pitch, player) <= k_QuiteNear;
}
bool IsNotQuiteNearBall(entt::entity pitch, entt::entity player)
{
	return DistanceToBall(pitch, player) > k_QuiteNear;
}

// The shared parts of the processes

/// The run to the ball: at it, choose again; a flying ball (and the chance) is run to where it comes down, the
/// position's state after, through the run's own state; else the walk to the ball itself
void GoToBall(entt::entity pitch, entt::entity player, VillagerStates position, bool goalie)
{
	if (AtTheBall(pitch, player))
	{
		ChooseAgain(player);
		return;
	}
	const auto ball = football::GetBall(pitch);
	if (BallFlying(ball) && (goalie ? game_random::GameRand(10) >= 8 : game_random::GameRand(3) != 0))
	{
		static_cast<void>(SetupMoveToPos(player, map_coords::ToMetres(BallDestination(ball)), position));
		SetState(player, LivingAction::Index::Top, VillagerStates::FootballMoveToBall);
		return;
	}
	static_cast<void>(StartMoveToObject(player, ball, position));
}

/// A pass to the team-mate one nearer the goal attacked, at where it walks to if it is walking, when further than a
/// metre from the ball; then choose again. Not at the ball: the run to it
void Pass(entt::entity pitch, entt::entity player, VillagerStates position, bool goalie)
{
	if (!AtTheBall(pitch, player))
	{
		GoToBall(pitch, player, position, goalie);
		return;
	}
	const int32_t place = football::GoalRank(pitch, player) - 1;
	if (place >= 0)
	{
		const auto mate = football::RankedPlayer(pitch, SideOf(pitch, player), place);
		// (openblack, guard) a ranking shorter than the place has no team-mate there: the original reads through null
		if (mate != entt::null)
		{
			auto at = object::MapCoordsOf(mate);
			if (villager_teleport::IsMoving(mate))
			{
				if (const auto dest = GetDestPos(mate); dest.has_value())
				{
					at = map_coords::FromMetres(*dest);
				}
			}
			if (game_math::GetDistanceInMetres(object::MapCoordsOf(football::GetBall(pitch)), at) > k_PassAtLeast)
			{
				Kick(pitch, at, game_random::GameFloatRand(1.0f) + 6.0f, true);
				ResetCounter(player);
			}
		}
	}
	ChooseAgain(player);
}

/// A dribble: the ball kicked a fifth of the way to the goal attacked, spread by its length
void Dribble(entt::entity pitch, entt::entity player, VillagerStates position)
{
	if (!AtTheBall(pitch, player))
	{
		GoToBall(pitch, player, position, false);
		return;
	}
	const auto spot =
	    rules::DribbleSpot(map_coords::ToWorld(object::MapCoordsOf(player)), map_coords::ToWorld(AttackedGoal(pitch, player)));
	auto to = map_coords::FromWorld(spot);
	football::PerturbTarget(to, player);
	Kick(pitch, to, game_random::GameFloatRand(1.0f) + 6.0f, true);
	ResetCounter(player);
	ChooseAgain(player);
}

/// The walk to the player's home spot, its position's state after
void GoToHomeSpot(entt::entity pitch, entt::entity player, VillagerStates position)
{
	static_cast<void>(SetupMoveToPos(player, map_coords::ToMetres(HomeSpot(pitch, player)), position));
	ChooseAgain(player);
}

/// A turn towards the ball; long in it, or one of the two nearest the ball, choose again
void Idle(entt::entity pitch, entt::entity player)
{
	LookAt(player, football::GetBall(pitch), k_LookNormal);
	if (Counter(player) >= k_IdleTurns || static_cast<uint32_t>(football::BallRank(pitch, player)) < 2)
	{
		ChooseAgain(player);
	}
}

// The attackers' processes
void AttackerShoot(entt::entity pitch, entt::entity player)
{
	if (!AtTheBall(pitch, player))
	{
		GoToBall(pitch, player, VillagerStates::FootballAttacker, false);
		return;
	}
	auto goal = AttackedGoal(pitch, player);
	football::PerturbTarget(goal, player);
	Kick(pitch, goal, game_random::GameFloatRand(4.0f) + 12.0f, true);
	ResetCounter(player);
	ChooseAgain(player);
}
void AttackerPass(entt::entity pitch, entt::entity player)
{
	Pass(pitch, player, VillagerStates::FootballAttacker, false);
}
void AttackerLobNearGoal(entt::entity pitch, entt::entity player)
{
	if (!AtTheBall(pitch, player))
	{
		GoToBall(pitch, player, VillagerStates::FootballAttacker, false);
		return;
	}
	const float drawX = game_random::GameFloatRand(4.0f);
	const float drawZ = game_random::GameFloatRand(4.0f);
	auto to = rules::Scattered(AttackedGoal(pitch, player), drawX, drawZ, 2.0f);
	football::PerturbTarget(to, player);
	Kick(pitch, to, game_random::GameFloatRand(1.0f) + 3.0f, true);
	ResetCounter(player);
	ChooseAgain(player);
}
void AttackerDribble(entt::entity pitch, entt::entity player)
{
	Dribble(pitch, player, VillagerStates::FootballAttacker);
}
void AttackerMoveToBePassed(entt::entity pitch, entt::entity player)
{
	ResetCounter(player);
	const auto own = map_coords::ToWorld(OwnGoal(pitch, player));
	const auto attacked = map_coords::ToWorld(AttackedGoal(pitch, player));
	const float along = game_random::GameFloatRand(0.2f);
	const float across = game_random::GameFloatRand(18.0f);
	const auto spot = rules::MoveToBePassedSpot(own, attacked, along, across);
	static_cast<void>(SetupMoveToPos(player, map_coords::ToMetres(spot), VillagerStates::FootballAttacker));
	ChooseAgain(player);
}
void AttackerGoToBall(entt::entity pitch, entt::entity player)
{
	GoToBall(pitch, player, VillagerStates::FootballAttacker, false);
}
void AttackerGoHome(entt::entity pitch, entt::entity player)
{
	GoToHomeSpot(pitch, player, VillagerStates::FootballAttacker);
}

// The defenders' processes
void DefenderSave(entt::entity pitch, entt::entity player)
{
	if (!AtTheBall(pitch, player))
	{
		GoToBall(pitch, player, VillagerStates::FootballDefender, false);
		return;
	}
	const float drawX = game_random::GameFloatRand(10.0f);
	const float drawZ = game_random::GameFloatRand(10.0f);
	const auto to = rules::Scattered(AttackedGoal(pitch, player), drawX, drawZ, 5.0f);
	Kick(pitch, to, game_random::GameFloatRand(3.0f) + 8.0f, true);
	ResetCounter(player);
	ChooseAgain(player);
}
void DefenderDribble(entt::entity pitch, entt::entity player)
{
	Dribble(pitch, player, VillagerStates::FootballDefender);
}
void DefenderPass(entt::entity pitch, entt::entity player)
{
	Pass(pitch, player, VillagerStates::FootballDefender, false);
}
void DefenderMark(entt::entity pitch, entt::entity player)
{
	// the other side's attacker nearest the ball
	const auto marked = football::NearestAttacker(pitch, football::IsPlayerOnHomeTeam(pitch, player) ? 1u : 0u);
	if (marked != entt::null)
	{
		static_cast<void>(StartMoveToObject(player, marked, VillagerStates::FootballDefender));
	}
	ChooseAgain(player);
}
void DefenderGoToBall(entt::entity pitch, entt::entity player)
{
	GoToBall(pitch, player, VillagerStates::FootballDefender, false);
}
void DefenderGoHome(entt::entity pitch, entt::entity player)
{
	GoToHomeSpot(pitch, player, VillagerStates::FootballDefender);
}

// The goalies' processes
void GoalieSave(entt::entity pitch, entt::entity player)
{
	if (!AtTheBall(pitch, player))
	{
		GoToBall(pitch, player, VillagerStates::FootballGoalie, true);
		return;
	}
	auto to = object::MapCoordsOf(pitch);
	for (int i = 0; i < 3; ++i)
	{
		football::PerturbTarget(to, player);
	}
	Kick(pitch, to, game_random::GameFloatRand(1.0f) + 9.0f, true);
	ResetCounter(player);
	ChooseAgain(player);
}
void GoalieLook(entt::entity pitch, entt::entity player)
{
	LookAt(player, football::GetBall(pitch), k_LookNormal);
	if (auto* v = VillagerOf(player); v != nullptr)
	{
		++v->footballSubstateTurns;
		if (v->footballSubstateTurns > k_GoalieLookTurns)
		{
			v->footballChooseAgain = true;
		}
	}
	if (DistanceToBall(pitch, player) < k_GoalieLookNear)
	{
		ChooseAgain(player);
	}
}
void GoaliePass(entt::entity pitch, entt::entity player)
{
	if (DistanceTo(player, HomeSpot(pitch, player)) > k_GoaliePassFromHome)
	{
		ChooseAgain(player);
		return;
	}
	Pass(pitch, player, VillagerStates::FootballGoalie, true);
}
void GoalieGoToBall(entt::entity pitch, entt::entity player)
{
	GoToBall(pitch, player, VillagerStates::FootballGoalie, true);
}
void GoalieGoHome(entt::entity pitch, entt::entity player)
{
	static_cast<void>(SetupMoveToPos(player, map_coords::ToMetres(OwnGoal(pitch, player)), VillagerStates::FootballGoalie));
	ChooseAgain(player);
}

// The priorities
float Zero(entt::entity /*pitch*/, entt::entity /*player*/)
{
	return 0.0f;
}
float PassPriority(entt::entity pitch, entt::entity player)
{
	return football::GoalRank(pitch, player) - 1 >= 0 ? game_random::GameFloatRand(0.2f) + 0.8f : 0.0f;
}
float AttackerShootPriority(entt::entity pitch, entt::entity player)
{
	return rules::ShootPriority(DistanceTo(player, AttackedGoal(pitch, player)));
}
float DribblePriority(entt::entity pitch, entt::entity player)
{
	return rules::DribblePriority(DistanceTo(player, AttackedGoal(pitch, player)));
}
float AttackerMoveToBePassedPriority(entt::entity pitch, entt::entity player)
{
	return rules::MoveToBePassedPriority(DistanceToBall(pitch, player));
}
float GoHomePriority(entt::entity pitch, entt::entity player)
{
	return rules::GoHomePriority(DistanceTo(player, HomeSpot(pitch, player)));
}
float AttackerIdlePriority(entt::entity pitch, entt::entity player)
{
	const float toBall = DistanceToBall(pitch, player);
	return rules::IdlePriority(toBall, DistanceTo(player, HomeSpot(pitch, player)));
}
float NotGoHomePriority(entt::entity pitch, entt::entity player)
{
	return 1.0f - GoHomePriority(pitch, player);
}
float DefenderSavePriority(entt::entity /*pitch*/, entt::entity /*player*/)
{
	return game_random::GameFloatRand(0.3f) + 0.7f;
}
float DefenderClearPriority(entt::entity /*pitch*/, entt::entity /*player*/)
{
	return game_random::GameFloatRand(0.4f) + 0.6f;
}
float AnyPriority(entt::entity /*pitch*/, entt::entity /*player*/)
{
	return game_random::GameFloatRand(1.0f);
}

using Condition = bool (*)(entt::entity, entt::entity);
using Priority = float (*)(entt::entity, entt::entity);
using Process = void (*)(entt::entity, entt::entity);

/// A row of a position's table: its first condition (also the one that keeps it playing), an optional second, how
/// much the player wants it, and what it does
struct SubstateRow
{
	Condition first;
	Condition second;
	Priority priority;
	Process process;
};

constexpr std::array<SubstateRow, 8> k_AttackerRows {{
    {&IsNearestBall, nullptr, &AttackerShootPriority, &AttackerShoot},
    {&IsNearestBall, nullptr, &PassPriority, &AttackerPass},
    {&IsNearestBall, nullptr, &Zero, &AttackerLobNearGoal},
    {&IsNearestBall, nullptr, &DribblePriority, &AttackerDribble},
    {&IsNotNearestBall, nullptr, &AttackerMoveToBePassedPriority, &AttackerMoveToBePassed},
    {&IsNotTouchingBall, nullptr, &Zero, &AttackerGoToBall},
    {&IsNotTouchingBall, &IsNotAtHome, &GoHomePriority, &AttackerGoHome},
    {&IsNotQuiteNearBall, nullptr, &AttackerIdlePriority, &Idle},
}};

constexpr std::array<SubstateRow, 8> k_DefenderRows {{
    {&IsQuiteNearBall, nullptr, &DefenderSavePriority, &DefenderSave},
    // clearing is the save's kick
    {&IsQuiteNearBall, nullptr, &DefenderClearPriority, &DefenderSave},
    {&IsQuiteNearBall, nullptr, &DribblePriority, &DefenderDribble},
    {&IsNearestBall, nullptr, &PassPriority, &DefenderPass},
    {&IsNotTouchingBall, nullptr, &AnyPriority, &DefenderMark},
    {&IsNotTouchingBall, nullptr, &Zero, &DefenderGoToBall},
    {&IsNotNearestBall, &IsNotAtHome, &GoHomePriority, &DefenderGoHome},
    {&IsNotQuiteNearBall, nullptr, &NotGoHomePriority, &Idle},
}};

constexpr std::array<SubstateRow, 7> k_GoalieRows {{
    {&IsQuiteNearBall, &IsAtHome, &AnyPriority, &GoalieSave},
    // clearing is the save's kick
    {&IsQuiteNearBall, &IsAtHome, &AnyPriority, &GoalieSave},
    {&IsAtHome, nullptr, &AnyPriority, &GoalieLook},
    {&IsQuiteNearBall, nullptr, &PassPriority, &GoaliePass},
    {&IsNotTouchingBall, nullptr, &Zero, &GoalieGoToBall},
    {&IsNotAtHome, nullptr, &GoHomePriority, &GoalieGoHome},
    // idling is looking out
    {&IsNotQuiteNearBall, &IsAtHome, &NotGoHomePriority, &GoalieLook},
}};

template <size_t N>
void Assign(entt::entity player, const std::array<SubstateRow, N>& rows)
{
	auto* v = VillagerOf(player);
	if (v == nullptr || !v->footballChooseAgain)
	{
		return;
	}
	const auto pitch = v->footballPitch;
	const auto chosen = rules::ChooseSubstate(
	    static_cast<uint8_t>(N),
	    [&](uint8_t row) {
		    // both conditions are asked, the first first
		    const bool first = rows.at(row).first(pitch, player);
		    if (rows.at(row).second != nullptr && !rows.at(row).second(pitch, player))
		    {
			    return false;
		    }
		    return first;
	    },
	    [&](uint8_t row) { return rows.at(row).priority(pitch, player); });
	v = VillagerOf(player);
	v->footballSubstate = chosen;
	v->footballChooseAgain = false;
	v->footballSubstateTurns = 0;
}

template <size_t N>
uint32_t PlayPosition(LivingAction& action, const std::array<SubstateRow, N>& rows)
{
	auto& registry = Entities();
	const auto player = registry.ToEntity(action);
	const auto pitch = PitchOf(player);
	if (pitch == entt::null || football::GetBall(pitch) == entt::null)
	{
		return DecideWhatToDo(action);
	}
	const auto counter = static_cast<int16_t>(action.turnsUntilStateChange);
	if (counter < 1)
	{
		action.turnsUntilStateChange = static_cast<uint16_t>(counter + 1);
		return 1;
	}
	const auto substate = VillagerOf(player)->footballSubstate;
	if (substate >= N)
	{
		return 1;
	}
	rows.at(substate).process(pitch, player);
	if (!rows.at(substate).first(pitch, player))
	{
		ChooseAgain(player);
	}
	return 1;
}
} // namespace

void AssignFootballSubState(entt::entity player)
{
	switch (GetFinalState(player))
	{
	case VillagerStates::FootballAttacker:
		Assign(player, k_AttackerRows);
		break;
	case VillagerStates::FootballGoalie:
		Assign(player, k_GoalieRows);
		break;
	case VillagerStates::FootballDefender:
		Assign(player, k_DefenderRows);
		break;
	default:
		break;
	}
}

uint32_t FootballAttacker(LivingAction& action)
{
	return PlayPosition(action, k_AttackerRows);
}

uint32_t FootballGoalie(LivingAction& action)
{
	return PlayPosition(action, k_GoalieRows);
}

uint32_t FootballDefender(LivingAction& action)
{
	return PlayPosition(action, k_DefenderRows);
}

uint32_t FootballWonGoal(LivingAction& /*action*/)
{
	return 1;
}

uint32_t FootballLostGoal(LivingAction& /*action*/)
{
	return 1;
}

uint32_t StartMoveToPickUpBallForDeadBall(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto pitch = PitchOf(villager);
	if (pitch == entt::null || football::GetBall(pitch) == entt::null)
	{
		return 0;
	}
	static_cast<void>(StartMoveToObject(villager, football::GetBall(pitch), VillagerStates::ArrivedAtPickUpBallForDeadBall));
	return 1;
}

uint32_t ArrivedAtPickUpBallForDeadBall(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// (openblack, guard) with no pitch it only takes the next state
	if (const auto pitch = PitchOf(villager); pitch != entt::null)
	{
		LookAt(villager, pitch, k_LookFast);
	}
	SetTopState(villager, VillagerStates::ArrivedAtPutDownBallForDeadBallStart);
	return 1;
}

uint32_t ArrivedAtPutDownBallForDeadBallStart(LivingAction& action)
{
	PlayAnimThenSetState(Entities().ToEntity(action), VillagerStates::ArrivedAtPutDownBallForDeadBallEnd);
	return 1;
}

uint32_t ArrivedAtPutDownBallForDeadBallEnd(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto pitch = PitchOf(villager);
	// (openblack, guard) no pitch or no ball: the original reads through null
	if (pitch == entt::null || football::GetBall(pitch) == entt::null)
	{
		return 1;
	}
	constexpr float k_RestartSpeed = 10.0f;
	if (AtTheBall(pitch, villager))
	{
		Kick(pitch, object::MapCoordsOf(pitch), k_RestartSpeed, false);
	}
	static_cast<void>(StartMoveToObject(villager, football::GetBall(pitch), VillagerStates::ArrivedAtPickUpBallForDeadBall));
	football::SetPlayState(pitch, FootballPitch::PlayState::Playing);
	return 1;
}

uint32_t FootballMatchPaused(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto pitch = PitchOf(villager);
	if (pitch != entt::null)
	{
		if (const auto ball = football::GetBall(pitch); ball != entt::null)
		{
			LookAt(villager, ball, k_LookNormal);
		}
	}
	return 1;
}

uint32_t FootballWatchMatch(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (GetTown(villager) == entt::null)
	{
		return DecideWhatToDo(action);
	}
	const auto pitch = PitchOf(villager);
	// (openblack, guard) a watcher with no pitch decides afresh, as one with no ball does
	const auto ball = pitch != entt::null ? football::GetBall(pitch) : entt::null;
	if (ball == entt::null)
	{
		return DecideWhatToDo(action);
	}
	const auto* game = Entities().TryGet<const FootballPitch>(pitch);
	const auto state = game != nullptr ? static_cast<uint32_t>(game->playState) : k_PlayWaiting;
	if (state == k_PlayWaiting)
	{
		return LookAtPos(villager, town_queries::PosOf(pitch), k_LookNormal);
	}
	if (state == k_PlayPlaying)
	{
		return LookAtPos(villager, town_queries::PosOf(ball), k_LookNormal);
	}
	if (state == k_PlayOver)
	{
		if (auto* v = VillagerOf(villager); v != nullptr)
		{
			v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagFootball);
		}
		if (DecideWhatToDo(action) != 1)
		{
			static_cast<void>(villager::GoHome(villager));
		}
	}
	return 1;
}

uint32_t FootballMexicanWave(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (GetTown(villager) == entt::null)
	{
		return DecideWhatToDo(action);
	}
	if (const auto pitch = PitchOf(villager); pitch != entt::null)
	{
		LookAt(villager, pitch, k_LookFast);
	}
	PlayAnimThenSetState(villager, VillagerStates::FootballWatchMatch);
	return 1;
}

uint32_t FootballMoveToBall(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto pitch = PitchOf(villager);
	const auto ball = pitch != entt::null ? football::GetBall(pitch) : entt::null;
	if (ball == entt::null || !ecs::IsAvailable(ball))
	{
		return DecideWhatToDo(action);
	}
	const auto destination = BallDestination(ball);
	const auto goal = GetDestPos(villager);
	const auto heading = goal.has_value() ? map_coords::FromMetres(*goal) : object::MapCoordsOf(villager);
	if (game_math::GetDistanceInMetres(destination, heading) > k_ReaimBeyond)
	{
		auto* wallHug = registry.TryGet<WallHug>(villager);
		auto* transform = registry.TryGet<Transform>(villager);
		if (wallHug != nullptr && transform != nullptr)
		{
			walk_goal::Set(villager, wallHug->goal, map_coords::ToMetres(destination));
			InitStepsXZ(*transform, *wallHug);
		}
	}
	living_turn::MoveToStep(villager);
	const auto result = living_turn::WallHugMoveToResult(villager);
	if (result == k_Arrived)
	{
		// the arrival is taken: the next walk or the final state starts afresh, as MOVE_TO_POS's
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		SetTopStateToFinal(villager);
	}
	return result;
}

uint32_t ExitFootball(LivingAction& action, VillagerStates next)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	registry.Remove<WallHugObjectReference>(villager);
	if (IsStateExitFunctionSameAs(villager, next) || next == VillagerStates::MoveToPos || next == VillagerStates::MoveToObject)
	{
		return 1;
	}
	if (const auto pitch = PitchOf(villager); pitch != entt::null)
	{
		football::PlayerLeaves(pitch, villager);
	}
	if (auto* v = VillagerOf(villager); v != nullptr)
	{
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagFootball);
	}
	return 1;
}

} // namespace openblack::ecs::villager
