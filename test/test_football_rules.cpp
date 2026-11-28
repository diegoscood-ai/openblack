/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Football's rules that need no world (docs/wiki/world/buildings.md#the-football-pitch): whether the game has football,
// which part a villager taking up a pitch is given, and where the players and watchers walk to

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/FootballRules.h"

namespace rules = openblack::ecs::football_rules;
using openblack::map_coords::MapCoords;

TEST(FootballRules, FootballIsOnWithTheAddOnOutsideNetworkGamesOnly)
{
	EXPECT_TRUE(rules::EnabledAtGameStart(true, false));
	// a network game has no football, add-on or not
	EXPECT_FALSE(rules::EnabledAtGameStart(true, true));
	// without the add-on, never
	EXPECT_FALSE(rules::EnabledAtGameStart(false, false));
	EXPECT_FALSE(rules::EnabledAtGameStart(false, true));
}

TEST(FootballRules, TheFirstIsTheRefereeThenTheHomeSideThenTheAwaySideThenWatchers)
{
	using rules::Role;
	// a closed pitch, or one with no ball, takes no one
	EXPECT_EQ(rules::ChooseRole({.open = false}), Role::None);
	EXPECT_EQ(rules::ChooseRole({.hasBall = false}), Role::None);
	EXPECT_EQ(rules::ChooseRole({}), Role::Referee);
	EXPECT_EQ(rules::ChooseRole({.hasReferee = true}), Role::Home);
	EXPECT_EQ(rules::ChooseRole({.hasReferee = true, .home = 4}), Role::Home);
	EXPECT_EQ(rules::ChooseRole({.hasReferee = true, .home = 5}), Role::Away);
	EXPECT_EQ(rules::ChooseRole({.hasReferee = true, .home = 5, .away = 4}), Role::Away);
	EXPECT_EQ(rules::ChooseRole({.hasReferee = true, .home = 5, .away = 5}), Role::Watcher);
	// the referee comes before the sides, however full
	EXPECT_EQ(rules::ChooseRole({.home = 5, .away = 5}), Role::Referee);
}

TEST(FootballRules, TheWaitForPlayersStartsAgainWithTheFirstPlayer)
{
	EXPECT_FALSE(rules::RestartsTheWait(0, 0));
	EXPECT_TRUE(rules::RestartsTheWait(1, 0));
	EXPECT_TRUE(rules::RestartsTheWait(0, 1));
	EXPECT_FALSE(rules::RestartsTheWait(1, 1));
	EXPECT_FALSE(rules::RestartsTheWait(2, 0));
}

TEST(FootballRules, APlayingMatchWaitsAgainWhenASideHasNoOneOrBothHaveOne)
{
	// a side with no players
	EXPECT_TRUE(rules::BackToWaiting(0, 5));
	EXPECT_TRUE(rules::BackToWaiting(5, 0));
	EXPECT_TRUE(rules::BackToWaiting(0, 0));
	// one each
	EXPECT_TRUE(rules::BackToWaiting(1, 1));
	// one against more plays on, as does every side of two or more
	EXPECT_FALSE(rules::BackToWaiting(1, 2));
	EXPECT_FALSE(rules::BackToWaiting(2, 1));
	EXPECT_FALSE(rules::BackToWaiting(2, 2));
	EXPECT_FALSE(rules::BackToWaiting(5, 5));
}

TEST(FootballRules, TheFirstPlayerFurtherFromABallOffThePitchFetchesItTheAwaySidesAtTheSameDistance)
{
	// the home side's first further: it fetches
	EXPECT_EQ(rules::DeadBallFetcherSide(20.0f, 10.0f), 0u);
	// the away side's first further, or both as far: the away side's
	EXPECT_EQ(rules::DeadBallFetcherSide(10.0f, 20.0f), 1u);
	EXPECT_EQ(rules::DeadBallFetcherSide(15.0f, 15.0f), 1u);
}

TEST(FootballRules, TheMexicanWaveGoesRoundItsFortyPlacesThenRestsTwoHundredAndFiftySixTurns)
{
	rules::WaveClock wave;
	// a place a turn, from the first
	for (uint32_t i = 0; i < rules::k_WavePlaces; ++i)
	{
		const auto place = rules::WaveTurn(wave);
		ASSERT_TRUE(place.has_value());
		EXPECT_EQ(*place, i);
	}
	// after the 40th it rests 256 turns from the first again
	EXPECT_EQ(wave.index, 0u);
	EXPECT_EQ(wave.rest, 256u);
	for (uint32_t i = 0; i < 256; ++i)
	{
		EXPECT_FALSE(rules::WaveTurn(wave).has_value());
	}
	EXPECT_EQ(wave.rest, 0u);
	// then it goes round again
	const auto again = rules::WaveTurn(wave);
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(*again, 0u);
	EXPECT_EQ(wave.index, 1u);
}

TEST(FootballRules, AGoalInTheFirstMouthIsGivenOneAndAddsToTheHomeTallyWhileTheHomeSideMourns)
{
	// the first goal mouth: given 1, the home tally's, and the home side mourns it while the away side celebrates
	const auto first = rules::CreditFor(0);
	EXPECT_EQ(first.side, 1u);
	EXPECT_TRUE(first.homeTally);
	EXPECT_TRUE(rules::Mourns(true, first.side));
	EXPECT_FALSE(rules::Mourns(false, first.side));
	// the second: given 0, the away tally's, the home side celebrating
	const auto second = rules::CreditFor(1);
	EXPECT_EQ(second.side, 0u);
	EXPECT_FALSE(second.homeTally);
	EXPECT_FALSE(rules::Mourns(true, second.side));
	EXPECT_TRUE(rules::Mourns(false, second.side));
}

TEST(FootballRules, ABallIsLostPastAHundredMetresOnWaterOrInAGodsHandPastEighty)
{
	// on the pitch, or as far as 100 m, it stays
	EXPECT_FALSE(rules::BallIsLost(0.0f, false, false));
	EXPECT_FALSE(rules::BallIsLost(100.0f, false, false));
	EXPECT_TRUE(rules::BallIsLost(100.5f, false, false));
	// on water, however near
	EXPECT_TRUE(rules::BallIsLost(1.0f, true, false));
	// in a god's hand, past 80 m
	EXPECT_FALSE(rules::BallIsLost(80.0f, false, true));
	EXPECT_TRUE(rules::BallIsLost(80.5f, false, true));
	EXPECT_FALSE(rules::BallIsLost(80.5f, false, false));
}

TEST(FootballRules, APlayerWalksToASpotWithinFourMetresOfTheMiddle)
{
	// The original's own spot for a pitch whose middle is (1677721, 2621440), from the game's first two draws of 8 on
	// the campaign's seed (the oracle: the spot function with the seed 0x88F89F): 1687509, 2633972
	const float drawX = std::bit_cast<float>(0x40AFCBB0u);
	const float drawZ = std::bit_cast<float>(0x40BD31BDu);
	const MapCoords middle {1677721, 2621440, 0.0f};
	const auto spot = rules::PlayerSpot(middle, drawX, drawZ);
	EXPECT_EQ(spot.x, 1687509);
	EXPECT_EQ(spot.z, 2633972);
	// the middle's altitude is kept
	EXPECT_EQ(rules::PlayerSpot({1677721, 2621440, 1.5f}, drawX, drawZ).altitude, 1.5f);
	// a draw of 4 on each axis is the middle itself, 0 is 4 m short on each
	EXPECT_EQ(rules::PlayerSpot(middle, 4.0f, 4.0f), middle);
	const auto corner = rules::PlayerSpot(middle, 0.0f, 0.0f);
	EXPECT_NEAR(openblack::map_coords::ToMetres(corner.x), openblack::map_coords::ToMetres(middle.x) - 4.0f, 0.001f);
	EXPECT_NEAR(openblack::map_coords::ToMetres(corner.z), openblack::map_coords::ToMetres(middle.z) - 4.0f, 0.001f);
}

TEST(FootballRules, TheFirstFortyWatchersStandNineDegreesApart)
{
	// 9 degrees as the float the game keeps, times the places taken, a float product
	EXPECT_EQ(rules::WatcherAngle(0), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::WatcherAngle(1)), 0x3E20D97Cu);
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::WatcherAngle(10)), 0x3FC90FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::WatcherAngle(39)), 0x40C4090Fu);
}

TEST(FootballRules, AWatcherIsScatteredByUpToTwoMetres)
{
	const MapCoords point {1677721, 2621440, 0.25f};
	// a draw of 2 on each axis keeps the point; 0 and 4 are 2 m short and 2 m past
	EXPECT_EQ(rules::WatcherPlace(point, 2.0f, 2.0f), point);
	const auto place = rules::WatcherPlace(point, 0.0f, 4.0f);
	EXPECT_EQ(place.x, 1664613);
	EXPECT_EQ(place.z, 2634547);
	EXPECT_EQ(place.altitude, 0.25f);
}

TEST(FootballRules, TheMatchsClocksAreWholeTurns)
{
	// ten turns a second at 100 ms a turn
	EXPECT_EQ(rules::TurnsFor(rules::k_KickOffWaitSeconds, 100), 300u);
	EXPECT_EQ(rules::TurnsFor(rules::k_SendAwaySeconds, 100), 1800u);
	EXPECT_EQ(rules::TurnsFor(rules::k_RoleShuffleSeconds, 100), 150u);
	// the turns a second are an integer: 1000 / 300 is 3
	EXPECT_EQ(rules::TurnsFor(rules::k_KickOffWaitSeconds, 300), 90u);
}

TEST(FootballRules, WithThreeASideTheBallGoesToTheMiddleAfterThirtySecondsAndKicksOffOnceFewerThanFiveMove)
{
	// three a side or more, else the wait only counts towards sending the players home
	EXPECT_TRUE(rules::EnoughASide(3, 3));
	EXPECT_TRUE(rules::EnoughASide(5, 4));
	EXPECT_FALSE(rules::EnoughASide(2, 5));
	EXPECT_FALSE(rules::EnoughASide(5, 2));
	// at ten turns a second the clock must pass 300 turns for the ball to go to the middle, 1800 to send them home
	EXPECT_FALSE(rules::PastTheWait(300, rules::k_KickOffWaitSeconds, 100));
	EXPECT_TRUE(rules::PastTheWait(301, rules::k_KickOffWaitSeconds, 100));
	EXPECT_FALSE(rules::PastTheWait(1800, rules::k_SendAwaySeconds, 100));
	EXPECT_TRUE(rules::PastTheWait(1801, rules::k_SendAwaySeconds, 100));
	// the kick-off: ready, and fewer than five players moved this turn
	EXPECT_TRUE(rules::KicksOff(true, 0));
	EXPECT_TRUE(rules::KicksOff(true, 4));
	EXPECT_FALSE(rules::KicksOff(true, 5));
	EXPECT_FALSE(rules::KicksOff(false, 0));
}

TEST(FootballRules, AMatchsBeliefIsItsCountOverTheDurationTwiceInTurnsAtMostTwice)
{
	// a 180 s duration at ten turns a second: 1800 turns, then 18000
	EXPECT_FLOAT_EQ(rules::MatchBelief(9000, 180.0f, 0.5f, 100), 0.25f);
	EXPECT_FLOAT_EQ(rules::MatchBelief(90000, 180.0f, 0.5f, 100), 1.0f);
	// the count is never moved: nothing
	EXPECT_EQ(rules::MatchBelief(0, 180.0f, 0.5f, 100), 0.0f);
	// a duration of nothing: 0 over 0 is not a number, and it passes as less than 2
	EXPECT_TRUE(std::isnan(rules::MatchBelief(0, 0.0f, 0.5f, 100)));
}

TEST(FootballRules, ASideIsRankedNearestTheGoalFirstAnEqualOneAfter)
{
	const std::array<uint32_t, 5> distances {30, 10, 20, 10, 40};
	EXPECT_EQ(rules::RankOrder(distances), (std::vector<size_t> {1, 3, 2, 0, 4}));
	EXPECT_TRUE(rules::RankOrder({}).empty());
}

TEST(FootballRules, FivePlayTwoTwoAndOneThreePlayOneEach)
{
	using rules::Position;
	// five: 0 and 1 attack, 2 and 3 defend, 4 keeps goal
	EXPECT_EQ(rules::PositionForRank(0, 5).position, Position::Attacker);
	EXPECT_EQ(rules::PositionForRank(1, 5).number, 1);
	EXPECT_EQ(rules::PositionForRank(2, 5).position, Position::Defender);
	EXPECT_EQ(rules::PositionForRank(3, 5).number, 1);
	EXPECT_EQ(rules::PositionForRank(4, 5).position, Position::Goalie);
	// three: one each
	EXPECT_EQ(rules::PositionForRank(0, 3).position, Position::Attacker);
	EXPECT_EQ(rules::PositionForRank(1, 3).position, Position::Defender);
	EXPECT_EQ(rules::PositionForRank(1, 3).number, 0);
	EXPECT_EQ(rules::PositionForRank(2, 3).position, Position::Goalie);
	// alone: the goalie
	EXPECT_EQ(rules::PositionForRank(0, 1).position, Position::Goalie);
}

TEST(FootballRules, AKickLandsOnItsTargetAfterItsDistanceOverItsSpeed)
{
	// 10 m at 5 m a second: two seconds, so it is launched up at 9.81 m a second to fall back on the target
	const auto flat = rules::KickVelocity({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, 5.0f);
	EXPECT_EQ(flat.x, 5.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(flat.y), 0x411CF5C3u);
	EXPECT_EQ(flat.z, 0.0f);
	// 13 m at 6.5: two seconds again, each step a float
	const auto rising = rules::KickVelocity({1.0f, 2.0f, 3.0f}, {4.0f, 6.0f, 15.0f}, 6.5f);
	EXPECT_EQ(std::bit_cast<uint32_t>(rising.x), 0x3FC00000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(rising.y), 0x413CF5C3u);
	EXPECT_EQ(std::bit_cast<uint32_t>(rising.z), 0x40C00000u);
}

TEST(FootballRules, APlayersWishesGrowAndShrinkWithItsDistancesWithinTheirLimits)
{
	// shooting: wanted most at the goal, a tenth at 36 m and more
	EXPECT_EQ(rules::ShootPriority(0.0f), 1.0f);
	EXPECT_EQ(rules::ShootPriority(20.0f), 0.5f);
	EXPECT_EQ(rules::ShootPriority(50.0f), 0.1f);
	// dribbling: the other way round
	EXPECT_EQ(rules::DribblePriority(20.0f), 0.5f);
	EXPECT_EQ(rules::DribblePriority(2.0f), 0.1f);
	EXPECT_EQ(rules::DribblePriority(100.0f), 1.0f);
	// moving to be passed to: a fifth at most, from 4 m off the ball
	EXPECT_EQ(rules::MoveToBePassedPriority(2.0f), 2.0f * 0.05f);
	EXPECT_EQ(rules::MoveToBePassedPriority(10.0f), 0.2f);
	// going home: a tenth at least
	EXPECT_EQ(rules::GoHomePriority(10.0f), 0.5f);
	EXPECT_EQ(rules::GoHomePriority(1.0f), 0.1f);
	// standing idle: far from the ball and near home
	EXPECT_EQ(rules::IdlePriority(30.0f, 5.0f), 0.25f);
	EXPECT_EQ(rules::IdlePriority(5.0f, 0.0f), 0.0f);
	EXPECT_EQ(rules::IdlePriority(50.0f, 0.0f), 0.75f);
}

TEST(FootballRules, APlayersHomeSpotIsItsRowsShareAcrossAndAlongTheWayToTheGoal)
{
	EXPECT_EQ(rules::AcrossShare(0, 1, 0.8f), 0.0f);
	EXPECT_EQ(rules::AcrossShare(0, 2, 0.8f), -0.4f);
	EXPECT_EQ(rules::AcrossShare(1, 2, 0.8f), 0.4f);
	EXPECT_EQ(rules::AcrossShare(0, 3, 1.0f), -0.5f);
	EXPECT_EQ(rules::AcrossShare(2, 3, 1.0f), 0.5f);
	// the first attacker alone a third of the way up; the second of two to one side
	EXPECT_EQ(rules::AttackerShares(0, 1).across, 0.0f);
	EXPECT_EQ(rules::AttackerShares(0, 1).along, 0.33f);
	EXPECT_EQ(rules::AttackerShares(1, 2).across, 0.4f);
	// a defender back past the middle, the fourth a row further back
	EXPECT_EQ(rules::DefenderShares(0, 1).along, -0.76f);
	EXPECT_EQ(rules::DefenderShares(3, 2).across, -0.25f);
	EXPECT_EQ(rules::DefenderShares(3, 2).along, -0.76f - 0.1f);
	// half way to a goal 10 m along x, and half of that across
	const auto spot = rules::FormationSpot({0.0f, 1.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {.across = 0.5f, .along = 0.5f});
	EXPECT_EQ(spot.x, 5.0f);
	EXPECT_EQ(spot.y, 1.0f);
	EXPECT_EQ(spot.z, 5.0f);
}

TEST(FootballRules, AnAttackerWaitsForAPassUpTheWayAndToItsSide)
{
	// 0.6 + 0.4 of the way from its goal to the other, no way across
	const auto onTheLine = rules::MoveToBePassedSpot({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, 0.4f, 9.0f);
	EXPECT_EQ(onTheLine.x, openblack::map_coords::ToFixed(10.0f));
	EXPECT_EQ(onTheLine.z, openblack::map_coords::ToFixed(0.0f));
	// a metre across the way
	const auto aside = rules::MoveToBePassedSpot({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, 0.4f, 10.0f);
	EXPECT_EQ(aside.x, openblack::map_coords::ToFixed(10.0f));
	EXPECT_EQ(aside.z, openblack::map_coords::ToFixed(1.0f));
}

TEST(FootballRules, ADribbleKicksTheBallAFifthOfTheWayToTheGoal)
{
	const auto spot = rules::DribbleSpot({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	EXPECT_EQ(spot.x, 2.0f);
	EXPECT_EQ(spot.y, 0.0f);
	EXPECT_EQ(spot.z, 0.0f);
	// at the goal already: where it stands
	const auto there = rules::DribbleSpot({3.0f, 1.0f, 4.0f}, {3.0f, 1.0f, 4.0f});
	EXPECT_EQ(there, glm::vec3(3.0f, 1.0f, 4.0f));
}

TEST(FootballRules, APlayerPlaysTheFirstHeaviestSubstateItsConditionsAllow)
{
	std::vector<uint8_t> weighed;
	const std::array<bool, 3> allowed {false, true, true};
	const std::array<float, 3> priority {9.0f, 0.5f, 0.5f};
	const auto chosen = rules::ChooseSubstate(
	    3, [&](uint8_t row) { return allowed.at(row); },
	    [&](uint8_t row) {
		    weighed.push_back(row);
		    return priority.at(row);
	    });
	// a row its conditions refuse is never weighed; an equal one after does not win
	EXPECT_EQ(chosen, 1);
	EXPECT_EQ(weighed, (std::vector<uint8_t> {1, 2}));
	// nothing allowed, or nothing wanted: no row
	EXPECT_EQ(rules::ChooseSubstate(
	              3, [](uint8_t) { return false; }, [](uint8_t) { return 1.0f; }),
	          3);
	EXPECT_EQ(rules::ChooseSubstate(
	              3, [](uint8_t) { return true; }, [](uint8_t) { return -1.0f; }),
	          3);
	// wanted not at all is still above the floor
	EXPECT_EQ(rules::ChooseSubstate(
	              3, [](uint8_t) { return true; }, [](uint8_t) { return 0.0f; }),
	          0);
}

// A defender's clear or save: aimed within 5 metres of the goal on each axis from draws of 10, kicked at 8 to 11 metres
// a second from a draw of 3 (docs/wiki/project/progress/story/minigames.md)
TEST(FootballRules, ADefenderKicksAtEightToElevenMetresASecondWithinFiveMetresOfTheGoal)
{
	EXPECT_FLOAT_EQ(rules::k_DefenderKickDraw, 3.0f);
	EXPECT_FLOAT_EQ(rules::DefenderKickSpeed(0.0f), 8.0f);
	EXPECT_FLOAT_EQ(rules::DefenderKickSpeed(rules::k_DefenderKickDraw), 11.0f);
	EXPECT_FLOAT_EQ(rules::k_DefenderScatterDraw, 10.0f);
	EXPECT_FLOAT_EQ(rules::k_DefenderScatterHalf, 5.0f);
	// the scatter: a draw of 0 puts it 5 short on the axis, a full draw 5 past
	const MapCoords goal {.x = openblack::map_coords::ToFixed(100.0f), .z = openblack::map_coords::ToFixed(100.0f)};
	const auto low = rules::Scattered(goal, 0.0f, 0.0f, rules::k_DefenderScatterHalf);
	const auto high =
	    rules::Scattered(goal, rules::k_DefenderScatterDraw, rules::k_DefenderScatterDraw, rules::k_DefenderScatterHalf);
	EXPECT_NEAR(openblack::map_coords::ToMetres(low.x), 95.0f, 0.01f);
	EXPECT_NEAR(openblack::map_coords::ToMetres(high.z), 105.0f, 0.01f);
}

// A goalkeeper's save is kicked at 9 to 10 metres a second from a draw of 1
TEST(FootballRules, AGoalkeeperKicksAtNineToTenMetresASecond)
{
	EXPECT_FLOAT_EQ(rules::k_GoalieKickDraw, 1.0f);
	EXPECT_FLOAT_EQ(rules::GoalieKickSpeed(0.0f), 9.0f);
	EXPECT_FLOAT_EQ(rules::GoalieKickSpeed(rules::k_GoalieKickDraw), 10.0f);
}

// Going for a flying ball: the goalkeeper runs to where it comes down on a draw of 10 of 8 or more, one time in five;
// the others on a draw of 3 that is not 0, two times in three
TEST(FootballRules, TheGoalkeeperRunsToAFlyingBallOneTimeInFiveTheOthersTwoInThree)
{
	EXPECT_EQ(rules::FlyingBallDraw(true), 10u);
	EXPECT_EQ(rules::FlyingBallDraw(false), 3u);
	uint32_t goalie = 0;
	for (uint32_t draw = 0; draw < rules::FlyingBallDraw(true); ++draw)
	{
		goalie += rules::RunsToFlyingBall(true, draw) ? 1u : 0u;
	}
	EXPECT_EQ(goalie, 2u);
	EXPECT_FALSE(rules::RunsToFlyingBall(true, 7));
	EXPECT_TRUE(rules::RunsToFlyingBall(true, 8));
	EXPECT_FALSE(rules::RunsToFlyingBall(false, 0));
	EXPECT_TRUE(rules::RunsToFlyingBall(false, 1));
	EXPECT_TRUE(rules::RunsToFlyingBall(false, 2));
}

// An attacker shoots at 12 to 16 metres a second, lobs near the goal at 3 to 4 aiming within 2 metres of it, and taps
// the ball on in a dribble at 6 to 7 (docs/wiki/project/progress/story/minigames.md)
TEST(FootballRules, AnAttackerShootsAtTwelveToSixteenLobsAtThreeToFourAndDribblesAtSixToSeven)
{
	EXPECT_FLOAT_EQ(rules::ShootSpeed(0.0f), 12.0f);
	EXPECT_FLOAT_EQ(rules::ShootSpeed(rules::k_ShootDraw), 16.0f);
	EXPECT_FLOAT_EQ(rules::LobSpeed(0.0f), 3.0f);
	EXPECT_FLOAT_EQ(rules::LobSpeed(rules::k_LobDraw), 4.0f);
	EXPECT_FLOAT_EQ(rules::k_LobScatterDraw, 4.0f);
	EXPECT_FLOAT_EQ(rules::k_LobScatterHalf, 2.0f);
	EXPECT_FLOAT_EQ(rules::DribbleSpeed(0.0f), 6.0f);
	EXPECT_FLOAT_EQ(rules::DribbleSpeed(rules::k_DribbleDraw), 7.0f);
}

// Each choice is weighed by chance: a pass 0.8 to 1, a save 0.7 to 1, a clear 0.6 to 1, a mark 0 to 1
TEST(FootballRules, AChoiceIsWeighedPassFromPointEightSaveSevenClearSixMarkFromNothing)
{
	EXPECT_FLOAT_EQ(rules::PassWeight(0.0f), 0.8f);
	EXPECT_FLOAT_EQ(rules::PassWeight(rules::k_PassDraw), 1.0f);
	EXPECT_FLOAT_EQ(rules::SaveWeight(0.0f), 0.7f);
	EXPECT_FLOAT_EQ(rules::SaveWeight(rules::k_SaveWeightDraw), 1.0f);
	EXPECT_FLOAT_EQ(rules::ClearWeight(0.0f), 0.6f);
	EXPECT_FLOAT_EQ(rules::ClearWeight(rules::k_ClearWeightDraw), 1.0f);
	EXPECT_FLOAT_EQ(rules::k_MarkWeightDraw, 1.0f);
}
