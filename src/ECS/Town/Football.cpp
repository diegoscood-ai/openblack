/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Football.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/FootballPitch.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/FootballGeometry.h"
#include "ECS/FootballRules.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/RelaxationRules.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerFootball.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/Implementations/VillagerMoveToObject.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStores.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "EngineConfig.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::football
{
using components::FootballPitch;
namespace rules = football_rules;

namespace
{
/// A player's spot is drawn over 8 m on each axis, a watcher's over 4 m
constexpr float k_PlayerDraw = rules::k_PlayerSpread;
constexpr float k_WatcherDraw = rules::k_WatcherSpread;
/// A ball further than this from its pitch, or held by a god's hand further than the second, is taken away (metres)
constexpr float k_BallLostMetres = 100.0f;
constexpr float k_BallHeldOffMetres = 80.0f;
/// A player given a new position starts it with this counter
constexpr uint16_t k_PositionTakenTurns = 0x14;
/// The guidance alignment the match's belief is given with
constexpr int k_MatchBeliefGuidance = 1;
/// Past the 40 places a watcher stands at a random angle round the pitch: a draw of a whole turn, as the float the game
/// keeps
constexpr float k_FullTurn = std::bit_cast<float>(0x40C90FDBu);

FootballPitch* PitchComponent(entt::entity pitch)
{
	auto& registry = Locator::entitiesRegistry::value();
	return pitch != entt::null && registry.Valid(pitch) ? registry.TryGet<FootballPitch>(pitch) : nullptr;
}

/// The ball made on the pitch's middle, as a football is made anywhere: no one's, unturned, given to no town
void CreateBall(entt::entity pitch, FootballPitch& game)
{
	const auto middle = map_coords::ToWorld(object::MapCoordsOf(pitch));
	game.ball = archetypes::BallArchetype::Create(middle);
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Football: pitch {} made its ball {} at ({:.2f}, {:.2f}, {:.2f})", entt::to_integral(pitch),
		                   entt::to_integral(game.ball), middle.x, middle.y, middle.z);
	}
}

/// The first free place of the 40 round the pitch takes the watcher, counted; none free: nothing
void TakeWavePlace(FootballPitch& game, entt::entity villager)
{
	for (auto& place : game.wave)
	{
		if (place == entt::null)
		{
			place = villager;
			++game.wavePlacesTaken;
			return;
		}
	}
}

/// Where a watcher walks to: the next of the 40 places while they last (taken first), else a random angle; then the
/// point 2 m outside the pitch at that angle, scattered by up to 2 m each way
map_coords::MapCoords WatcherPlace(entt::entity pitch, FootballPitch& game, entt::entity villager)
{
	float angle = 0.0f;
	if (game.wavePlacesTaken < rules::k_WavePlaces)
	{
		TakeWavePlace(game, villager);
		angle = rules::WatcherAngle(game.wavePlacesTaken);
	}
	else
	{
		angle = game_random::GameFloatRand(k_FullTurn);
	}
	const auto edge = object::GetNearestEdge(pitch, angle, rules::k_WatcherOutside);
	const float drawX = game_random::GameFloatRand(k_WatcherDraw);
	const float drawZ = game_random::GameFloatRand(k_WatcherDraw);
	return rules::WatcherPlace(edge, drawX, drawZ);
}

/// Where a player walks to: within 4 m each way of the pitch's middle
map_coords::MapCoords PlayerSpot(entt::entity pitch)
{
	const auto middle = object::MapCoordsOf(pitch);
	const float drawX = game_random::GameFloatRand(k_PlayerDraw);
	const float drawZ = game_random::GameFloatRand(k_PlayerDraw);
	return rules::PlayerSpot(middle, drawX, drawZ);
}
/// The goal model, Dataootball.l3d, which the first pitch made loads: its box list holds the two goal mouths
entt::id_type GoalModelId()
{
	static const auto k_Id = resources::HashIdentifier(std::string("data/football"));
	return k_Id;
}

void LoadGoalModel()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(GoalModelId()))
	{
		return;
	}
	try
	{
		meshes.Load(GoalModelId(), resources::L3DLoader::FromDiskTag {},
		            Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "football.l3d");
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Football: cannot load the goal model football.l3d: {}", e.what());
	}
}

/// The goal mouth `index` of the goal model: the box list's while it holds that many; with no box list, the model's
/// own box for index 0; none otherwise, and none without the model
std::optional<football_geometry::GoalBox> GoalBoxOf(uint32_t index)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(GoalModelId()))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(GoalModelId());
	const auto& boxes = mesh->GetBoxes();
	if (!boxes.empty())
	{
		if (index >= boxes.size())
		{
			return std::nullopt;
		}
		return football_geometry::GoalBox {.centre = boxes.at(index).centre, .halfSize = boxes.at(index).halfSize};
	}
	if (index != 0)
	{
		return std::nullopt;
	}
	const auto box = mesh->GetBoundingBox();
	return football_geometry::GoalBox {.centre = box.Center(), .halfSize = box.Size() * 0.5f};
}

/// The pitch's own matrix: its point (its ground plus its altitude), its turn and its scale
affine::AffineMatrix PitchMatrixOf(entt::entity pitch)
{
	const auto& transform = Locator::entitiesRegistry::value().Get<const components::Transform>(pitch);
	const auto point = map_coords::ToWorld(object::MapCoordsOf(pitch));
	return football_geometry::PitchMatrix(point, map_cells::detail::YAngleOf(transform.rotation), transform.scale.x);
}

/// The pitch's drawn matrix in the original's 12 floats
affine::AffineMatrix PlacementOf(entt::entity pitch)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto cells = object_placement::Of(registry, pitch).cells;
	affine::AffineMatrix out;
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			out.m.at(static_cast<size_t>(row * 3 + column)) = cells[row][column];
		}
	}
	return out;
}

/// The match's part of a pitch's start and of its end: its sides, its watchers' places, its tallies and its clocks
/// emptied, its ball forgotten (it stays where it is), the pitch open to villagers again; the play state, the goal's
/// clock, the side last given a goal and the ball's chaser stay
void SetToZero(FootballPitch& game)
{
	game.waveIndex = 0;
	game.waitingTurns = 0;
	game.wavePlacesTaken = 0;
	game.referee = entt::null;
	game.wave = components::EmptyWavePlaces();
	game.kickOffReady = false;
	game.open = true;
	game.homeScore = 0;
	game.awayScore = 0;
	game.deadBallTurns = 0;
	game.waveRest = 0;
	game.ball = entt::null;
	game.lastMatchTurn = 0;
	game.nearestAttacker = {entt::null, entt::null};
	game.nearestDefender = {entt::null, entt::null};
	game.homeTeam.clear();
	game.awayTeam.clear();
	game.ranked.at(0).clear();
	game.ranked.at(1).clear();
}

/// The villager's top state
VillagerStates TopStateOf(entt::entity villager)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const components::LivingAction>(villager);
	return action != nullptr
	           ? static_cast<VillagerStates>(action->states.at(static_cast<size_t>(components::LivingAction::Index::Top)))
	           : VillagerStates::InvalidState;
}

/// Too far from the pitch (100 m), on water, or in a god's hand more than 80 m from the pitch. (approximate) only the
/// local player's hand is asked, as the villagers' walks ask it
bool BallIsLost(entt::entity pitch, entt::entity ball)
{
	const auto ballPos = object::MapCoordsOf(ball);
	const auto pitchPos = object::MapCoordsOf(pitch);
	if (game_math::GetDistanceInMetres(ballPos, pitchPos) > k_BallLostMetres ||
	    sea_cells::IsWater(Locator::entitiesRegistry::value().Get<const components::Transform>(ball).position))
	{
		return true;
	}
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject(); held.has_value() && *held == ball)
		{
			return game_math::GetDistanceInMetres(ballPos, pitchPos) > k_BallHeldOffMetres;
		}
	}
	return false;
}

/// Each side ranked by distance to the goal it attacks (the home side the second goal mouth, the away side the
/// first), nearest first
void RankSides(entt::entity pitch)
{
	for (uint32_t side = 0; side < 2; ++side)
	{
		auto* game = PitchComponent(pitch);
		game->ranked.at(side).clear();
		const auto& team = side == 0 ? game->homeTeam : game->awayTeam;
		const auto goal = GoalPosition(pitch, side == 0 ? 1u : 0u);
		game = PitchComponent(pitch);
		std::vector<uint32_t> distances;
		distances.reserve(team.size());
		for (const auto player : team)
		{
			distances.push_back(static_cast<uint32_t>(game_math::FastDistance(goal, object::MapCoordsOf(player))));
		}
		auto& ranked = game->ranked.at(side);
		for (const auto i : rules::RankOrder(distances))
		{
			ranked.push_back(team.at(i));
		}
	}
}

/// The positions of a ranked side: the first half attack, the rest defend and the last keeps goal; a player whose
/// position changes takes it at once, its counter at 20, and chooses what to play again; then each chooses its
/// substate
void AssignPositions(entt::entity pitch, uint32_t side)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ranked = PitchComponent(pitch)->ranked.at(side);
	for (size_t rank = 0; rank < ranked.size(); ++rank)
	{
		const auto player = ranked.at(rank);
		const auto position = rules::PositionForRank(rank, ranked.size());
		auto* v = registry.TryGet<components::Villager>(player);
		if (v != nullptr && position.numbered)
		{
			v->footballNumber = position.number;
		}
		const auto state = position.position == rules::Position::Goalie     ? VillagerStates::FootballGoalie
		                   : position.position == rules::Position::Attacker ? VillagerStates::FootballAttacker
		                                                                    : VillagerStates::FootballDefender;
		if (villager::GetFinalState(player) != state)
		{
			villager::SetTopState(player, state);
			if (auto* action = registry.TryGet<components::LivingAction>(player); action != nullptr)
			{
				action->turnsUntilStateChange = k_PositionTakenTurns;
			}
			if (auto* again = registry.TryGet<components::Villager>(player); again != nullptr)
			{
				again->footballChooseAgain = true;
			}
		}
		AssignSubstate(player);
	}
}

/// Each player of a ranked side chooses its substate
void AssignSubstates(entt::entity pitch, uint32_t side)
{
	const auto ranked = PitchComponent(pitch)->ranked.at(side);
	for (const auto player : ranked)
	{
		AssignSubstate(player);
	}
}

/// A side sorted by distance to the ball, nearest first (an equal one stays behind); then its attacker nearest the
/// ball and, only nearer than that, its defender nearest the ball. A pitch with no ball or one no longer about makes
/// a new one first
void SortSide(entt::entity pitch, uint32_t side)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto ball = GetBall(pitch);
	if (ball == entt::null || !ecs::IsAvailable(ball))
	{
		CreateBall(pitch, *PitchComponent(pitch));
		ball = GetBall(pitch);
	}
	auto* game = PitchComponent(pitch);
	auto& team = side == 0 ? game->homeTeam : game->awayTeam;
	if (team.empty() || ball == entt::null)
	{
		return;
	}
	const auto ballPos = object::MapCoordsOf(ball);
	const auto distanceOf = [&](entt::entity player) {
		return static_cast<uint32_t>(game_math::FastDistance(object::MapCoordsOf(player), ballPos));
	};
	std::ranges::stable_sort(team, {}, distanceOf);
	// the nearest so far is kept between the attackers' and the defenders' searches
	uint32_t nearest = 0xFFFFFFFF;
	for (const auto player : team)
	{
		const auto d = distanceOf(player);
		if (villager::GetFinalState(player) == VillagerStates::FootballAttacker && d < nearest)
		{
			game->nearestAttacker.at(side) = player;
			nearest = d;
		}
	}
	for (const auto player : team)
	{
		const auto d = distanceOf(player);
		if (villager::GetFinalState(player) == VillagerStates::FootballDefender && d < nearest)
		{
			game->nearestDefender.at(side) = player;
			nearest = d;
		}
	}
	static_cast<void>(registry);
}

/// The end of a town match: every player goes home, no longer playing; every watcher in the 40 places goes home; the
/// town gains the match's belief; then the pitch waits again, its match emptied
void StopGame(entt::entity pitch)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* game = PitchComponent(pitch);
	const auto players = [&] {
		auto all = game->homeTeam;
		all.insert(all.end(), game->awayTeam.begin(), game->awayTeam.end());
		return all;
	}();
	const auto watchers = game->wave;
	for (const auto player : players)
	{
		// deciding afresh written straight into the top state: a walking player keeps its football final state, whose
		// exit takes it off its side as it goes home, and the town's count of what it was doing is not taken back
		villager::SetTopStateRaw(player, VillagerStates::DecideWhatToDo);
		villager::GoHome(player);
		if (auto* v = registry.TryGet<components::Villager>(player); v != nullptr)
		{
			v->flags = static_cast<uint16_t>(v->flags & ~components::Villager::k_FlagFootball);
		}
	}
	for (const auto watcher : watchers)
	{
		if (watcher != entt::null)
		{
			villager::GoHome(watcher);
		}
	}
	const auto town = abode_villagers::TownOf(pitch);
	if (town != entt::null && Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value().town;
		// the match's count: never moved by anything, so always 0
		constexpr uint32_t k_MatchCount = 0;
		const float belief =
		    rules::MatchBelief(k_MatchCount, info.footballMatchDuration, info.beliefForFooballMatch, game_clock::MsPerTurn());
		const auto owner = registry.Get<const components::Town>(town).owner;
		town_stores::AddToBelief(town, owner, belief, pitch, true, k_MatchBeliefGuidance);
	}
	game = PitchComponent(pitch);
	game->playState = FootballPitch::PlayState::WaitingForPlayers;
	SetToZero(*game);
}

/// Fewer than five players of both sides moved this turn
bool FewStillMoving(entt::entity pitch)
{
	const auto* game = PitchComponent(pitch);
	uint32_t moving = 0;
	for (const auto* team : {&game->homeTeam, &game->awayTeam})
	{
		for (const auto player : *team)
		{
			if (villager_teleport::IsMoving(player))
			{
				++moving;
			}
		}
	}
	return moving < rules::k_KickOffMovingLimit;
}

/// The waiting match: with fewer than three a side, after 180 s every player goes home and the ball's calls stop; with
/// three or more, after 30 s the ball (unless in flight) goes to the middle, and the kick-off comes once fewer than five
/// players are still moving: the sides ranked and given their positions
void WaitForPlayers(entt::entity pitch, entt::entity ball)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* game = PitchComponent(pitch);
	const auto ms = game_clock::MsPerTurn();
	if (game->homeTeam.size() < rules::k_FewestASide || game->awayTeam.size() < rules::k_FewestASide)
	{
		++game->waitingTurns;
		if (game->waitingTurns > rules::TurnsFor(rules::k_SendAwaySeconds, ms))
		{
			auto players = game->homeTeam;
			players.insert(players.end(), game->awayTeam.begin(), game->awayTeam.end());
			for (const auto player : players)
			{
				if (player != entt::null)
				{
					villager::SetTopState(player, VillagerStates::GoHome);
				}
			}
			game = PitchComponent(pitch);
			if (game->ball != entt::null)
			{
				effects::reactions::RemoveAllReactionsInitiatedByObject(game->ball);
			}
		}
		return;
	}
	++game->waitingTurns;
	if (game->waitingTurns > rules::TurnsFor(rules::k_KickOffWaitSeconds, ms))
	{
		const bool present = ball != entt::null && registry.Valid(ball);
		const bool flying = present && Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().IsFlying(ball);
		if (present && !flying)
		{
			map_cells::MoveMapObject(ball, map_coords::ToWorld(object::MapCoordsOf(pitch)));
		}
		game = PitchComponent(pitch);
		game->kickOffReady = true;
		game->waitingTurns = 0;
	}
	if (!game->kickOffReady || !FewStillMoving(pitch))
	{
		return;
	}
	game->playState = FootballPitch::PlayState::Playing;
	game->playStateTimer = rules::TurnsFor(rules::k_RoleShuffleSeconds, ms);
	RankSides(pitch);
	AssignPositions(pitch, 0);
	AssignPositions(pitch, 1);
}

/// The playing match: a side with no players, or one each, and it waits again. A town match that has run long enough
/// stops once the town most wants something other than playtime or relaxation. The ball along the pitch: the sides
/// sorted by the ball, ranked by the goals, given their positions again every 15 s, and each player its substate; the
/// ball off the pitch: the player of the two sides' first who is further from it fetches it, while the others stand
/// paused
void Play(entt::entity pitch)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* game = PitchComponent(pitch);
	const auto home = game->homeTeam.size();
	const auto away = game->awayTeam.size();
	if (home == 0 || away == 0 || (home == 1 && away == 1))
	{
		game->playState = FootballPitch::PlayState::WaitingForPlayers;
		return;
	}
	if (game->open)
	{
		++game->waitingTurns;
		const auto town = abode_villagers::TownOf(pitch);
		const auto wanted = town != entt::null ? town_desire::GetSortedDesires(town).at(0).index : 0u;
		if (wanted != static_cast<uint32_t>(TownDesireInfo::ForRelaxation) &&
		    wanted != static_cast<uint32_t>(TownDesireInfo::ForPlaytime) && game->waitingTurns > rules::k_TownMatchTurns)
		{
			game->lastMatchTurn = game_clock::Turn();
			StopGame(pitch);
		}
	}
	const auto ball = GetBall(pitch);
	game = PitchComponent(pitch);
	if (ball != entt::null)
	{
		if (ProcessBall(pitch, ball))
		{
			game = PitchComponent(pitch);
			game->ballChaser = entt::null;
			SortSide(pitch, 0);
			SortSide(pitch, 1);
			RankSides(pitch);
			game = PitchComponent(pitch);
			if (game->playStateTimer < rules::TurnsFor(rules::k_RoleShuffleSeconds, game_clock::MsPerTurn()))
			{
				++game->playStateTimer;
			}
			else
			{
				AssignPositions(pitch, 0);
				AssignPositions(pitch, 1);
				PitchComponent(pitch)->playStateTimer = 0;
			}
			AssignSubstates(pitch, 0);
			AssignSubstates(pitch, 1);
		}
		else if (!PitchComponent(pitch)->homeTeam.empty() && !PitchComponent(pitch)->awayTeam.empty())
		{
			// (openblack, guard) a match stopped this very turn has no sides left: the original reads an empty side's
			// first player there
			game = PitchComponent(pitch);
			const auto homeFirst = game->homeTeam.front();
			const auto awayFirst = game->awayTeam.front();
			const auto ballPos = object::MapCoordsOf(ball);
			const auto homeDistance = static_cast<float>(game_math::FastDistance(object::MapCoordsOf(homeFirst), ballPos));
			const auto awayDistance = static_cast<float>(game_math::FastDistance(object::MapCoordsOf(awayFirst), ballPos));
			const auto fetcher = homeDistance <= awayDistance ? awayFirst : homeFirst;
			auto players = game->homeTeam;
			players.insert(players.end(), game->awayTeam.begin(), game->awayTeam.end());
			for (const auto player : players)
			{
				game = PitchComponent(pitch);
				if (player != fetcher && player != game->ballChaser &&
				    game->playState != FootballPitch::PlayState::GoalScored &&
				    TopStateOf(player) != VillagerStates::FootballMatchPaused)
				{
					villager::SetTopState(player, VillagerStates::FootballMatchPaused);
				}
			}
			game = PitchComponent(pitch);
			if (game->ballChaser == entt::null)
			{
				static_cast<void>(villager::StartMoveToObject(fetcher, ball, VillagerStates::ArrivedAtPickUpBallForDeadBall));
				game = PitchComponent(pitch);
				game->ballChaser = fetcher;
				game->deadBallTurns = 0;
				return;
			}
		}
	}
	PitchComponent(pitch)->deadBallTurns = 0;
	static_cast<void>(registry);
}

/// The match's turn, once a game turn: the Mexican wave goes round its 40 places (a filled place whose watcher stands
/// still rises), resting 256 turns after each round; then the match's state
void MatchTurn(entt::entity pitch, entt::entity ball)
{
	auto* game = PitchComponent(pitch);
	if (game->waveRest == 0)
	{
		const auto watcher = game->wave.at(game->waveIndex);
		if (watcher != entt::null && !villager_teleport::IsMoving(watcher))
		{
			villager::SetTopState(watcher, VillagerStates::FootballMexicanWave);
		}
		game = PitchComponent(pitch);
		++game->waveIndex;
		if (game->waveIndex >= rules::k_WavePlaces)
		{
			game->waveRest = rules::k_WaveRestTurns;
			game->waveIndex = 0;
		}
	}
	else
	{
		--game->waveRest;
	}
	switch (PitchComponent(pitch)->playState)
	{
	case FootballPitch::PlayState::WaitingForPlayers:
		WaitForPlayers(pitch, ball);
		break;
	case FootballPitch::PlayState::Playing:
		Play(pitch);
		break;
	case FootballPitch::PlayState::GoalScored:
		game = PitchComponent(pitch);
		++game->goalTurns;
		if (game->goalTurns > rules::k_GoalPauseTurns)
		{
			game->playState = FootballPitch::PlayState::Playing;
			game->playStateTimer = rules::TurnsFor(rules::k_RoleShuffleSeconds, game_clock::MsPerTurn());
		}
		break;
	}
}

/// Its ball gone: the players go home (a script's cup final is not ported) and the ball's calls stop
void BallGone(entt::entity pitch)
{
	auto* game = PitchComponent(pitch);
	auto players = game->homeTeam;
	players.insert(players.end(), game->awayTeam.begin(), game->awayTeam.end());
	for (const auto player : players)
	{
		if (player != entt::null)
		{
			villager::SetTopState(player, VillagerStates::GoHome);
		}
	}
	// the ball was let go of just before: nothing to stop
}

/// Every reference to what is no longer about is let go: the ball (then BallGone), the referee, the players of both
/// sides and their ranks, the nearest attackers and defenders, the ball's chaser and the watchers' places (their
/// count stays)
void LetGoOfWhatHasGone(entt::entity pitch)
{
	auto* game = PitchComponent(pitch);
	const auto gone = [](entt::entity e) { return e != entt::null && !ecs::IsAvailable(e); };
	if (gone(game->ball))
	{
		game->ball = entt::null;
		BallGone(pitch);
		game = PitchComponent(pitch);
	}
	if (gone(game->referee))
	{
		game->referee = entt::null;
	}
	std::erase_if(game->homeTeam, gone);
	std::erase_if(game->awayTeam, gone);
	for (auto& ranked : game->ranked)
	{
		std::erase_if(ranked, gone);
	}
	for (auto& nearest : game->nearestAttacker)
	{
		if (gone(nearest))
		{
			nearest = entt::null;
		}
	}
	for (auto& nearest : game->nearestDefender)
	{
		if (gone(nearest))
		{
			nearest = entt::null;
		}
	}
	if (gone(game->ballChaser))
	{
		game->ballChaser = entt::null;
	}
	for (auto& place : game->wave)
	{
		if (gone(place))
		{
			place = entt::null;
		}
	}
}
} // namespace

bool IsEnabled()
{
	return Locator::config::has_value() && Locator::config::value().footballEnabled;
}

bool IsPitch(entt::entity entity)
{
	return PitchComponent(entity) != nullptr;
}

void Create(entt::entity pitch)
{
	Locator::entitiesRegistry::value().Assign<FootballPitch>(pitch);
	// the goal model, once, as the first pitch is made
	LoadGoalModel();
}

map_coords::MapCoords GoalPosition(entt::entity pitch, uint32_t index)
{
	if (const auto box = GoalBoxOf(index); box.has_value())
	{
		return map_coords::FromWorld(football_geometry::GoalPointInWorld(PlacementOf(pitch), *box));
	}
	return object::MapCoordsOf(pitch);
}

bool IsInsideGoal(entt::entity pitch, entt::entity ball, uint32_t index)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* b = registry.TryGet<const components::Ball>(ball);
	auto* game = PitchComponent(pitch);
	if (b == nullptr || game == nullptr || b->kick == 0)
	{
		return false;
	}
	const auto inverse = football_geometry::InverseInPlace(PitchMatrixOf(pitch));
	const auto from = football_geometry::PointInPitchSpaceForGoal(inverse, b->previousPoint);
	const auto to = football_geometry::PointInPitchSpaceForGoal(inverse, b->point);
	if (const auto heading = football_geometry::BallHeading(from, to); heading.has_value())
	{
		game->ballHeading = *heading;
	}
	const auto box = GoalBoxOf(index);
	if (!box.has_value() || !football_geometry::CrossesGoalMouth(from, to, *box))
	{
		return false;
	}
	// TODO(football): the original also puts "GOAL!!!!!" in the players' on-screen messages, which openblack does not
	// draw
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Football: GOAL!!!!! at pitch {}, goal mouth {}", entt::to_integral(pitch), index);
	}
	return true;
}

void GoalHasBeenScored(entt::entity pitch, uint32_t side)
{
	auto* game = PitchComponent(pitch);
	if (game == nullptr)
	{
		return;
	}
	game->playState = FootballPitch::PlayState::GoalScored;
	game->goalTurns = 0;
	game->scoredSide = side;
	// the home side mourns a goal given to the away side and celebrates its own; the away side the other way round.
	// (openblack) copies of the sides: a player's state change may not change them, but the lists are read whole first
	const auto home = game->homeTeam;
	const auto away = game->awayTeam;
	const auto mourn = VillagerStates::FootballLostGoal;
	const auto celebrate = VillagerStates::FootballWonGoal;
	for (const auto player : home)
	{
		villager::SetTopState(player, side != 0 ? mourn : celebrate);
	}
	for (const auto player : away)
	{
		villager::SetTopState(player, side == 0 ? mourn : celebrate);
	}
}

bool ProcessBall(entt::entity pitch, entt::entity ball)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* game = PitchComponent(pitch);
	if (game == nullptr || !registry.Valid(ball) || !registry.AllOf<components::Ball>(ball))
	{
		return false;
	}
	const auto point = map_coords::ToWorld(object::MapCoordsOf(ball));
	registry.Get<components::Ball>(ball).point = point;
	const auto local = football_geometry::BallInPitchSpace(football_geometry::InverseInPlace(PitchMatrixOf(pitch)), point);
	if (game->playState == FootballPitch::PlayState::Playing)
	{
		if (IsInsideGoal(pitch, ball, 0))
		{
			GoalHasBeenScored(pitch, 1);
			game = PitchComponent(pitch);
			++game->homeScore;
		}
		else if (IsInsideGoal(pitch, ball, 1))
		{
			GoalHasBeenScored(pitch, 0);
			game = PitchComponent(pitch);
			++game->awayScore;
		}
	}
	registry.Get<components::Ball>(ball).previousPoint = point;
	// along the pitch: within the pitch model's half-length
	const auto* mesh = registry.TryGet<const components::Mesh>(pitch);
	const auto half = mesh != nullptr ? object::MeshHalfExtents(mesh->id) : std::nullopt;
	return football_geometry::AlongThePitch(local.x, half.has_value() ? half->x : 0.0f);
}

entt::entity PitchOf(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<const components::Town>(town) : nullptr;
	if (t == nullptr)
	{
		return entt::null;
	}
	// (openblack) the town keeps its pitch after the pitch has gone, as the original keeps the pointer: a gone one is
	// read as none
	return IsPitch(t->footballPitch) && ecs::IsAvailable(t->footballPitch) ? t->footballPitch : entt::null;
}

void MakeFunctional(entt::entity pitch)
{
	auto* game = PitchComponent(pitch);
	const auto town = abode_villagers::TownOf(pitch);
	if (game == nullptr || town == entt::null)
	{
		return;
	}
	Locator::entitiesRegistry::value().Get<components::Town>(town).footballPitch = pitch;
	game->functional = true;
	game->playState = FootballPitch::PlayState::WaitingForPlayers;
	// a ball still about is kept; none, or one no longer about, and a new one is made. (openblack) a ball that has
	// gone is not about, where the original reads its freed memory
	if (game->ball == entt::null || !ecs::IsAvailable(game->ball))
	{
		CreateBall(pitch, *game);
	}
}

entt::entity GetBall(entt::entity pitch)
{
	auto* game = PitchComponent(pitch);
	if (game == nullptr)
	{
		return entt::null;
	}
	if (game->ball == entt::null)
	{
		CreateBall(pitch, *game);
	}
	return game->ball;
}

void Process(entt::entity pitch)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!IsPitch(pitch))
	{
		return;
	}
	// The ball first, made when the pitch has none. Too far off, in water, or held too far off by a god's hand: it
	// goes, a new one is made on the middle, and the turn ends
	const auto ball = GetBall(pitch);
	if (ball != entt::null && registry.Valid(ball) && BallIsLost(pitch, ball))
	{
		ecs::ToBeDeleted(ball, false);
		if (auto* game = PitchComponent(pitch); game != nullptr)
		{
			game->ball = entt::null;
		}
		static_cast<void>(GetBall(pitch));
		return;
	}
	// The match takes its turn once a game turn, whichever pitch asks first
	auto& last = registry.Context().footballTurn;
	if (const auto turn = game_clock::Turn(); turn != last)
	{
		last = turn;
		MatchTurn(pitch, ball);
	}
	// Then, every time, what has gone is let go
	LetGoOfWhatHasGone(pitch);
}

void DeleteDependants(entt::entity pitch)
{
	if (PitchComponent(pitch) == nullptr)
	{
		return;
	}
	// each list's players still about decide afresh, then the list empties: the home side, the away side, then the two
	// rankings. (openblack) a copy of each list is read: a player's state change may change them
	using List = std::vector<entt::entity>& (*)(FootballPitch&);
	constexpr std::array<List, 4> k_Lists {
	    [](FootballPitch& g) -> std::vector<entt::entity>& { return g.homeTeam; },
	    [](FootballPitch& g) -> std::vector<entt::entity>& { return g.awayTeam; },
	    [](FootballPitch& g) -> std::vector<entt::entity>& { return g.ranked.at(0); },
	    [](FootballPitch& g) -> std::vector<entt::entity>& { return g.ranked.at(1); },
	};
	for (const auto list : k_Lists)
	{
		const auto players = list(*PitchComponent(pitch));
		for (const auto player : players)
		{
			if (player != entt::null && ecs::IsAvailable(player))
			{
				villager::SetTopState(player, VillagerStates::DecideWhatToDo);
			}
		}
		list(*PitchComponent(pitch)).clear();
	}
}

void AssignSubstate(entt::entity player)
{
	villager::AssignFootballSubState(player);
}

bool IsPlayerOnHomeTeam(entt::entity pitch, entt::entity player)
{
	const auto* game = PitchComponent(pitch);
	return game != nullptr && std::ranges::find(game->homeTeam, player) != game->homeTeam.end();
}

int32_t BallRank(entt::entity pitch, entt::entity player)
{
	const auto* game = PitchComponent(pitch);
	if (game == nullptr)
	{
		return -1;
	}
	const auto& side = IsPlayerOnHomeTeam(pitch, player) ? game->homeTeam : game->awayTeam;
	const auto found = std::ranges::find(side, player);
	return found != side.end() ? static_cast<int32_t>(found - side.begin()) : -1;
}

int32_t GoalRank(entt::entity pitch, entt::entity player)
{
	const auto* game = PitchComponent(pitch);
	if (game == nullptr)
	{
		return -1;
	}
	const auto& ranked = game->ranked.at(IsPlayerOnHomeTeam(pitch, player) ? 0 : 1);
	const auto found = std::ranges::find(ranked, player);
	return found != ranked.end() ? static_cast<int32_t>(found - ranked.begin()) : -1;
}

entt::entity RankedPlayer(entt::entity pitch, uint32_t side, int32_t place)
{
	const auto* game = PitchComponent(pitch);
	if (game == nullptr || place < 0)
	{
		return entt::null;
	}
	const auto& ranked = game->ranked.at(side);
	return static_cast<size_t>(place) < ranked.size() ? ranked.at(static_cast<size_t>(place)) : entt::null;
}

entt::entity NearestAttacker(entt::entity pitch, uint32_t index)
{
	const auto* game = PitchComponent(pitch);
	return game != nullptr ? game->nearestAttacker.at(index) : entt::null;
}

void PerturbTarget(map_coords::MapCoords& target, entt::entity player)
{
	const float half = game_math::GetDistanceInMetres(target, object::MapCoordsOf(player)) * 0.5f;
	const float range = half + half;
	const float drawX = game_random::GameFloatRand(range);
	const float drawZ = game_random::GameFloatRand(range);
	target = rules::Scattered(target, drawX, drawZ, half);
}

map_coords::MapCoords HomePosition(entt::entity pitch, entt::entity player, uint32_t side)
{
	const auto* game = PitchComponent(pitch);
	const auto* v = Locator::entitiesRegistry::value().TryGet<const components::Villager>(player);
	if (game == nullptr || v == nullptr)
	{
		return object::MapCoordsOf(pitch);
	}
	const auto state = villager::GetFinalState(player);
	const auto count = static_cast<uint32_t>(game->ranked.at(side).size());
	const auto spot = [&](const rules::FormationShares& shares) {
		const auto middle = map_coords::ToWorld(object::MapCoordsOf(pitch));
		const auto goal = map_coords::ToWorld(GoalPosition(pitch, side == 0 ? 1u : 0u));
		return map_coords::FromWorld(rules::FormationSpot(middle, goal, shares));
	};
	switch (state)
	{
	case VillagerStates::FootballAttacker:
		return spot(rules::AttackerShares(v->footballNumber, count >> 1));
	case VillagerStates::FootballGoalie:
		return GoalPosition(pitch, side);
	case VillagerStates::FootballDefender:
		return spot(rules::DefenderShares(v->footballNumber, (count - 1) >> 1));
	default:
		return object::MapCoordsOf(pitch);
	}
}

void SetPlayState(entt::entity pitch, FootballPitch::PlayState state)
{
	if (auto* game = PitchComponent(pitch); game != nullptr)
	{
		game->playState = state;
		game->playStateTimer = rules::TurnsFor(rules::k_RoleShuffleSeconds, game_clock::MsPerTurn());
	}
}

void PlayerLeaves(entt::entity pitch, entt::entity player)
{
	auto* game = PitchComponent(pitch);
	if (game == nullptr)
	{
		return;
	}
	if (game->ballChaser == player)
	{
		game->ballChaser = entt::null;
	}
	// out of the home side if it is there, else of the away side; that always answers yes, so the wave keeps its place
	if (std::ranges::find(game->homeTeam, player) != game->homeTeam.end())
	{
		std::erase(game->homeTeam, player);
	}
	else
	{
		std::erase(game->awayTeam, player);
	}
}

uint32_t PlayerCount(entt::entity pitch)
{
	const auto* game = PitchComponent(pitch);
	return game != nullptr ? static_cast<uint32_t>(game->homeTeam.size() + game->awayTeam.size()) : 0u;
}

float GetVillagerActivityDesire(entt::entity pitch, entt::entity villager)
{
	const auto* game = PitchComponent(pitch);
	if (game == nullptr)
	{
		return 0.0f;
	}
	const relaxation_rules::FootballGame sides {
	    .firstTeam = static_cast<uint32_t>(game->homeTeam.size()),
	    .secondTeam = static_cast<uint32_t>(game->awayTeam.size()),
	    .playing = game->referee != entt::null,
	    .lastGameTurn = game->lastMatchTurn,
	};
	return relaxation_rules::FootballDesire(villager::GetAge(villager), sides, villager::CurrentTurn(),
	                                        [](float range) { return game_random::GameFloatRand(range); });
}

bool SetVillagerActivity(entt::entity pitch, entt::entity villager)
{
	auto* game = PitchComponent(pitch);
	auto& registry = Locator::entitiesRegistry::value();
	if (game == nullptr || !registry.Valid(villager) || !registry.AllOf<components::Villager>(villager))
	{
		return false;
	}
	// the ball first, made when the pitch has none
	const auto ball = GetBall(pitch);
	game = PitchComponent(pitch);
	const auto role = rules::ChooseRole({
	    .open = game->open,
	    .hasBall = ball != entt::null,
	    .hasReferee = game->referee != entt::null,
	    .home = static_cast<uint32_t>(game->homeTeam.size()),
	    .away = static_cast<uint32_t>(game->awayTeam.size()),
	});
	switch (role)
	{
	case rules::Role::None:
		return false;
	case rules::Role::Referee:
		// the referee stays where it is
		game->referee = villager;
		break;
	case rules::Role::Home:
	case rules::Role::Away:
	{
		auto& side = role == rules::Role::Home ? game->homeTeam : game->awayTeam;
		side.insert(side.begin(), villager);
		const auto spot = PlayerSpot(pitch);
		static_cast<void>(
		    villager::SetupMoveToPos(villager, map_coords::ToMetres(spot), VillagerStates::FootballWaitForKickOff));
		break;
	}
	case rules::Role::Watcher:
	{
		const auto place = WatcherPlace(pitch, *game, villager);
		static_cast<void>(villager::SetupMoveToPos(villager, map_coords::ToMetres(place), VillagerStates::FootballWatchMatch));
		break;
	}
	}
	game = PitchComponent(pitch);
	if (game != nullptr &&
	    rules::RestartsTheWait(static_cast<uint32_t>(game->homeTeam.size()), static_cast<uint32_t>(game->awayTeam.size())))
	{
		game->waitingTurns = 0;
	}
	auto& v = registry.Get<components::Villager>(villager);
	v.flags = static_cast<uint16_t>(v.flags | components::Villager::k_FlagFootball);
	v.footballPitch = pitch;
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		constexpr std::array k_RoleNames {"none", "referee", "home", "away", "watcher"};
		SPDLOG_LOGGER_INFO(logger, "Football: villager {} takes up pitch {} as {}", entt::to_integral(villager),
		                   entt::to_integral(pitch), k_RoleNames.at(static_cast<size_t>(role)));
	}
	return true;
}

uint32_t WaitForKickOff(components::LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto* v = registry.TryGet<const components::Villager>(villager);
	// (openblack, guard) a villager with no pitch waits on
	const auto* game = v != nullptr ? PitchComponent(v->footballPitch) : nullptr;
	if (game != nullptr && game->playState == FootballPitch::PlayState::Playing)
	{
		villager::SetTopState(villager, VillagerStates::FootballAttacker);
	}
	return 1;
}

} // namespace openblack::ecs::football
