/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <DanceFile.h>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/DanceMoves.h"
#include "ECS/DanceRules.h"
#include "ECS/DanceShapes.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/VillagerScriptRules.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Dance;
using openblack::ecs::components::DanceGroup;
namespace rules = openblack::ecs::dance_rules;
namespace moves = openblack::ecs::dance_moves;
namespace shapes = openblack::ecs::dance_shapes;

namespace
{
void Put(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutAction(std::vector<uint8_t>& bytes, std::vector<uint32_t> groups, uint32_t type, uint32_t first, uint32_t second)
{
	Put(bytes, static_cast<uint32_t>(groups.size()));
	for (const auto group : groups)
	{
		Put(bytes, group);
	}
	Put(bytes, type);
	Put(bytes, first);
	Put(bytes, second);
	for (int i = 2; i < 14; ++i)
	{
		Put(bytes, 0);
	}
}

/// A dance of two groups named M and W, sharing their dancers half and half from its first key frame
std::vector<uint8_t> TwoGroupDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 1); // version
	Put(bytes, 2); // key frames
	Put(bytes, std::bit_cast<uint32_t>(0.0f));
	Put(bytes, 1);
	Put(bytes, 2); // actions
	PutAction(bytes, {0}, 6, 0, 50);
	PutAction(bytes, {1}, 6, 0, 50);
	Put(bytes, std::bit_cast<uint32_t>(20.0f));
	Put(bytes, 0);
	Put(bytes, 1);
	PutAction(bytes, {1}, 16, 2, 0);
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // clock
	Put(bytes, 2);                             // groups
	Put(bytes, 14);
	Put(bytes, 0);
	Put(bytes, 1); // loops
	for (const char name : {'M', 'W'})
	{
		Put(bytes, 1);
		bytes.push_back(static_cast<uint8_t>(name));
	}
	return bytes;
}

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// A dance of one group sharing all its dancers, about a place
entt::entity OneGroupDance(Registry& registry, glm::vec2 place = {1000.0f, 2000.0f})
{
	auto file = std::make_shared<dance::DanceFile>();
	file->groupCount = 1;
	const auto entity =
	    dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .place = map_coords::FromMetres(place)}, file);
	auto& groups = registry.Get<Dance>(entity).groups;
	groups.shared = {0};
	rules::SetWeights(groups);
	return entity;
}
} // namespace

TEST(DanceRules, TheRateGoesInStepsOfFourTenths)
{
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.5f), 2.0f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.25f), 0.8f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.99f), 3.6f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(1.0f), 4.0f);
}

TEST(DanceRules, AWorshipSitesDanceIsDancedWhileItsDancersChant)
{
	Dance dance;
	// As it is made: at a quarter speed, stopped
	rules::SetSpeed(dance, rules::k_MadeSpeed);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 1.0f);
	// Then set going at half speed: danced, starting over at the new rate
	dance.clock = 12.0f;
	rules::SetWorshipSpeed(dance, 0.5f);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.dancingRate, 2.0f);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// The same rate again goes on where it was
	dance.clock = 5.0f;
	rules::SetWorshipSpeed(dance, 0.55f);
	EXPECT_FLOAT_EQ(dance.clock, 5.0f);
	rules::SetWorshipSpeed(dance, 0.0f);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
}

TEST(DanceRules, ADanceStartsOnceMoreThanHalfOfThoseOnTheirWayHaveComeOrAfterTheLongestWait)
{
	Dance dance;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	EXPECT_FALSE(dance.firstDancerTurn.has_value());
	dance.dancers = 2;
	dance.onTheirWay = 4;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	EXPECT_EQ(dance.firstDancerTurn, 100u);
	dance.dancers = 3;
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 101));
	dance.dancers = 1;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100 + 899));
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 100 + 900));
}

TEST(DanceRules, AScriptsDanceStartsOnceItHasADancer)
{
	// None are ever on their way to a script's dance, so its first dancer will do
	Dance dance;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	dance.dancers = 1;
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 120));
	EXPECT_EQ(dance.firstDancerTurn, 120u);
}

TEST(DanceRules, TheClockRunsWhileItIsDancedAndStartsOverAfterItsLoop)
{
	Dance dance {.autostart = true, .loopLength = 1};
	EXPECT_EQ(rules::LoopTurns(dance), 600u);
	dance.dancers = 1;
	rules::ProcessTurn(dance, 50, {});
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_EQ(dance.startTurn, 50u);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	dance.clock = 598.0f;
	rules::ProcessTurn(dance, 51, {});
	EXPECT_FLOAT_EQ(dance.clock, 599.0f);
	rules::ProcessTurn(dance, 52, {});
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// No dancers, no clock
	dance.dancers = 0;
	rules::ProcessTurn(dance, 53, {});
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// One that doesn't start by itself waits to be started
	Dance waiting {.loopLength = 1, .dancers = 5};
	rules::ProcessTurn(waiting, 10, {});
	EXPECT_EQ(waiting.state, Dance::State::Stopped);
}

TEST(DanceRules, KeyFramesComeByTheHalfSecondAndTheClockLoops)
{
	EXPECT_TRUE(rules::KeyFrameDue(20.0f, 24.0f));
	EXPECT_FALSE(rules::KeyFrameDue(20.0f, 25.0f));
	EXPECT_FLOAT_EQ(rules::NextClock(5.0f, 1), 6.0f);
	EXPECT_FLOAT_EQ(rules::NextClock(599.0f, 1), 0.0f);
	EXPECT_FLOAT_EQ(rules::NextClock(599.0f, 2), 600.0f);
}

TEST(DanceRules, SharedGroupsTakeNewcomersInTurnByTheirShares)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(TwoGroupDance()), dance::DanceResult::Success);
	Dance dance;
	dance.groups.all.resize(2);
	rules::ApplyKeyFramesUpTo(dance, file, 0.0f);
	// Half and half: one each in a round of two
	EXPECT_EQ(dance.groups.shared, (std::vector<std::size_t> {0, 1}));
	EXPECT_EQ(dance.groups.all[0].weight, 1u);
	EXPECT_EQ(dance.groups.roundLength, 2u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(2), 0, DanceGroup::k_Women), 1u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(3), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(dance.dancers, 3u);
	// A dancer of another kind finds no group
	EXPECT_FALSE(rules::AddDancer(dance, Entity(4), 1, DanceGroup::k_Men).has_value());
}

TEST(DanceRules, AGroupTakingOneSexPassesItsTurnOnToTheNext)
{
	Dance dance;
	auto& groups = dance.groups;
	groups.all.resize(2);
	groups.all[0].sexes = DanceGroup::k_Women;
	groups.shared = {0, 1};
	groups.all[0].quota = 50;
	groups.all[1].quota = 50;
	rules::SetWeights(groups);
	// A man on the women's turn goes to the second group, and the round moves on
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 1u);
	EXPECT_EQ(groups.round, 1);
}

TEST(DanceRules, GroupsWithAFixedNumberAreFilledFirst)
{
	Dance dance;
	auto& groups = dance.groups;
	groups.all.resize(2);
	groups.all[0].limited = true;
	groups.all[0].quota = 1;
	groups.limited = {0};
	groups.all[1].quota = 100;
	groups.shared = {1};
	rules::SetWeights(groups);
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(2), 0, DanceGroup::k_Men), 1u);
	rules::RemoveDancer(dance, 0, Entity(1));
	EXPECT_EQ(groups.all[0].limitedDancers, 0u);
	EXPECT_EQ(dance.dancers, 1u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(3), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::FirstDancer(groups, groups.all.size(), Entity(3)), Entity(2));
	// Only the groups the file names are looked in
	EXPECT_EQ(rules::FirstDancer(groups, 1, Entity(3)), entt::entity {entt::null});
}

TEST(DanceRules, AKeyFrameNamingAGroupPastTheLastMakesOneGroupMore)
{
	openblack::dance::DanceKeyFrame keyFrame;
	openblack::dance::DanceAction shape;
	shape.type = 0;
	// Listed 3 then 1: done to 1 first, which makes group 1; then 3 makes group 2, and names no group still
	shape.groups = {3, 1};
	keyFrame.actions.push_back(shape);
	Dance made;
	made.groups.all.resize(1);
	rules::ApplyKeyFrame(made, keyFrame, {});
	EXPECT_EQ(made.groups.all.size(), 3u);
}

TEST(Dances, ADanceIsMadeStoppedAtAQuarterSpeedWithItsFilesGroups)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true}, file);
	const auto& dance = registry.Get<const Dance>(entity);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.speed, 0.25f);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_EQ(dance.loopLength, 1u);
	ASSERT_EQ(dance.groups.all.size(), 2u);
	EXPECT_EQ(dance.groups.all[0].name, "M");
	EXPECT_EQ(dance.groups.all[1].name, "W");
}

TEST(Dances, ADanceWhoseFileCantBeOpenedHasOneGroupAndNoKeyFrames)
{
	Registry registry;
	const auto entity = dances::Create(registry, {.type = DanceInfo::DancePrayInwards}, nullptr);
	const auto& dance = registry.Get<const Dance>(entity);
	EXPECT_EQ(dance.groups.all.size(), 1u);
	EXPECT_TRUE(dance.groups.shared.empty());
	EXPECT_TRUE(dance.groups.limited.empty());
	EXPECT_EQ(dance.groups.roundLength, 100u);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	EXPECT_EQ(dance.loopLength, 5u);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	// Its one group takes no dancer
	const auto dancer = registry.Create();
	EXPECT_FALSE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	EXPECT_EQ(dances::Size(registry, entity), 0u);
}

TEST(Dances, ADanceStopsForATurnWhenItsTimeIsUp)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity = dances::Create(
	    registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true, .duration = 30, .madeByScript = true}, file);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	EXPECT_EQ(dances::DanceOf(registry, dancer), entity);
	std::vector<entt::entity> finished;
	dances::TurnContext context {.turn = 1,
	                             .available = [](entt::entity) { return true; },
	                             .finished = [&finished](entt::entity e) { finished.push_back(e); }};
	dances::ProcessTurn(registry, entity, context);
	const auto& dance = registry.Get<Dance>(entity);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	// 31 turns on it stops, and starts again the turn after
	context.turn = 32;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(registry.Get<Dance>(entity).state, Dance::State::Stopped);
	context.turn = 33;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(registry.Get<Dance>(entity).state, Dance::State::Dancing);
	// Its dancers are told when it goes
	dances::Destroy(registry, entity, context.finished);
	EXPECT_EQ(finished, std::vector<entt::entity> {dancer});
	EXPECT_FALSE(registry.AllOf<components::Dancer>(dancer));
}

TEST(Dances, ADanceGoesWhenWhatItIsDancedForGoes)
{
	Registry registry;
	const auto owner = registry.Create();
	const auto entity = dances::Create(registry, {.type = DanceInfo::CitadelDance_1, .owner = owner}, nullptr);
	dances::TurnContext context {.turn = 1, .available = [](entt::entity) { return false; }, .finished = {}};
	dances::ProcessTurn(registry, entity, context);
	EXPECT_FALSE(registry.Valid(entity));
}

TEST(DanceShapes, TheShapesAreWorkedOutAsTheGameDoes)
{
	// Every point of a shape at once: each point weighed by its place, so that a point out of place shows too
	const auto weighedSum = [](const shapes::Shape& shape) {
		int64_t sum = 0;
		for (size_t k = 0; k < shape.size(); ++k)
		{
			sum += static_cast<int64_t>(k + 1) * (3 * static_cast<int64_t>(shape[k].x) + 7 * static_cast<int64_t>(shape[k].y));
		}
		return sum;
	};
	const auto circle = shapes::Build(0);
	EXPECT_EQ(circle[0], glm::ivec2(65535, 0));
	EXPECT_EQ(circle[1], glm::ivec2(65515, 1608));
	EXPECT_EQ(circle[64], glm::ivec2(0, 65535));
	EXPECT_EQ(circle[100], glm::ivec2(-50659, 41574));
	// The sine and the cosine are not rounded to a float before they are multiplied
	EXPECT_EQ(circle[54], glm::ivec2(15923, 63570));
	EXPECT_EQ(circle[74], glm::ivec2(-15923, 63570));
	EXPECT_EQ(circle[138], glm::ivec2(-63570, -15923));
	EXPECT_EQ(circle[246], glm::ivec2(63570, -15923));
	EXPECT_EQ(weighedSum(circle), -4809760068);
	const auto wavy = shapes::Build(1);
	EXPECT_EQ(wavy[5], glm::ivec2(70501, 8695));
	EXPECT_EQ(wavy[37], glm::ivec2(36585, 46879));
	EXPECT_EQ(weighedSum(wavy), -4853608447);
	const auto spiral = shapes::Build(2);
	EXPECT_EQ(spiral[10], glm::ivec2(1787, 1619));
	EXPECT_EQ(spiral[255], glm::ivec2(61343, -4522));
	EXPECT_EQ(weighedSum(spiral), -1463442656);
	const auto octagon = shapes::Build(10);
	EXPECT_EQ(octagon[0], glm::ivec2(65535, 0));
	EXPECT_EQ(octagon[33], glm::ivec2(44891, 46939));
	EXPECT_EQ(octagon[100], glm::ivec2(-48739, 40547));
	EXPECT_EQ(octagon[255], glm::ivec2(64935, -1448));
	EXPECT_EQ(weighedSum(octagon), -4561453056);
	const auto triangle = shapes::Build(8);
	EXPECT_EQ(triangle[0], glm::ivec2(-65535, 0));
	EXPECT_EQ(triangle[1], glm::ivec2(-64378, 667));
	EXPECT_EQ(triangle[42], glm::ivec2(-16962, 28043));
	EXPECT_EQ(triangle[85], glm::ivec2(32767, 56754));
	EXPECT_EQ(triangle[94], glm::ivec2(32767, 44735));
	EXPECT_EQ(triangle[127], glm::ivec2(32767, 667));
	EXPECT_EQ(triangle[170], glm::ivec2(32767, -56754));
	EXPECT_EQ(triangle[212], glm::ivec2(-15805, -28710));
	EXPECT_EQ(triangle[255], glm::ivec2(-65535, 0));
	EXPECT_EQ(weighedSum(triangle), -2895563378);
	const auto heart = shapes::Build(9);
	EXPECT_EQ(heart[0], glm::ivec2(0, -65534));
	EXPECT_EQ(heart[1], glm::ivec2(-2419, -65444));
	EXPECT_EQ(heart[20], glm::ivec2(-32627, -35790));
	EXPECT_EQ(heart[41], glm::ivec2(-3625, -201));
	EXPECT_EQ(heart[42], glm::ivec2(0, 0));
	EXPECT_EQ(heart[63], glm::ivec2(-32761, 32161));
	EXPECT_EQ(heart[84], glm::ivec2(-1210, 65511));
	EXPECT_EQ(heart[85], glm::ivec2(0, 65534));
	EXPECT_EQ(heart[128], glm::ivec2(66304, 32381));
	EXPECT_EQ(heart[169], glm::ivec2(129526, 770));
	EXPECT_EQ(heart[170], glm::ivec2(131068, 0));
	EXPECT_EQ(heart[212], glm::ivec2(66304, -32381));
	EXPECT_EQ(heart[255], glm::ivec2(0, -65534));
	EXPECT_EQ(weighedSum(heart), 3813671733);
	// The square's sides and the wave's first half stop after their first points
	const auto square = shapes::Build(4);
	EXPECT_EQ(square[0], glm::ivec2(-0xFFFF, -0xFFE0));
	EXPECT_EQ(square[1], glm::ivec2(0, 0));
	EXPECT_EQ(square[192], glm::ivec2(0xFFE0, -0xFFFF));
	const auto wave = shapes::Build(3);
	EXPECT_EQ(wave[0], glm::ivec2(0, -8160));
	EXPECT_EQ(wave[1], glm::ivec2(0, 0));
	EXPECT_EQ(wave[128].y, -8160);
	EXPECT_EQ(wave[155], glm::ivec2(-63570, -4717));
	EXPECT_EQ(wave[165], glm::ivec2(-63570, -3442));
	EXPECT_EQ(weighedSum(wave), 657463520);
	EXPECT_EQ(shapes::Build(5)[3], glm::ivec2(0, 3 * 512 - 65536));
	EXPECT_EQ(shapes::Build(6)[3], glm::ivec2(3 * 512 - 65536, 0));
	EXPECT_EQ(shapes::Build(11)[2], glm::ivec2(2 * 224 + 0x2000, 0));
	EXPECT_EQ(std::string(shapes::FileName(19)), "Arc");
}

TEST(DanceMoves, APointTurnsThroughTheGamesSineTable)
{
	// Unturned, it still goes through the table's 2048 steps
	const auto same = moves::RotatePointByAngle({3.0f, 0.0f}, 0.0f);
	EXPECT_FLOAT_EQ(same.x, 3.0f);
	EXPECT_FLOAT_EQ(same.y, 0.0f);
	const auto quarter = moves::RotatePointByAngle({0.0f, 2.0f}, 0.0f);
	EXPECT_NEAR(quarter.x, 0.0f, 1e-6f);
	EXPECT_FLOAT_EQ(quarter.y, 2.0f);
	const auto half = moves::RotatePointByAngle({1.0f, 0.0f}, 3.1415927f);
	EXPECT_FLOAT_EQ(half.x, -1.0f);
}

TEST(DanceMoves, AMoveSharesTheDancesRateOverTheBeatsLeft)
{
	components::DanceGroup group;
	group.radius = 10.0f;
	moves::StartMove(group, {.action = 11, .first = std::bit_cast<uint32_t>(20.0f), .second = 0, .end = 10}, 0, 0.8f);
	EXPECT_FLOAT_EQ(group.radiusRate, 0.8f);
	moves::ProcessGroup(group, 0);
	EXPECT_FLOAT_EQ(group.radius, 10.8f);
	EXPECT_TRUE(group.moved);
	// At its end beat the move stops
	moves::ProcessGroup(group, 10);
	EXPECT_EQ(group.move.action, 0u);
	moves::ProcessGroup(group, 11);
	EXPECT_FALSE(group.moved);
	EXPECT_FLOAT_EQ(group.radius, 11.6f);

	// A spin goes round 256 over the move, and starts round again
	components::DanceGroup spinning;
	moves::StartMove(spinning, {.action = 1, .first = 0, .second = 0, .end = 100}, 0, 1.0f);
	EXPECT_FLOAT_EQ(spinning.spinRate, 2.56f);
	spinning.spin = 255.0f;
	moves::ProcessGroup(spinning, 1);
	EXPECT_NEAR(spinning.spin, 1.56f, 1e-5f);
	// Backwards it comes round the other way
	spinning.move.first = 1;
	moves::ProcessGroup(spinning, 2);
	EXPECT_NEAR(spinning.spin, 255.0f, 1e-4f);
}

TEST(DanceMoves, ADancersPlaceIsRoundItsGroupsShape)
{
	Dance dance;
	dance.place = map_coords::FromMetres({1000.0f, 2000.0f});
	dance.groups.all.resize(1);
	auto& group = dance.groups.all[0];
	group.radius = 10.0f;
	group.dancers = {Entity(1), Entity(2)};
	// Half the radius out, the first along x and the second on the far side
	const auto first = map_coords::ToMetres(moves::SlotPosition(dance, 0, 0, moves::Shapes()));
	EXPECT_NEAR(first.x, 1005.0f, 0.01f);
	EXPECT_NEAR(first.y, 2000.0f, 0.01f);
	const auto second = map_coords::ToMetres(moves::SlotPosition(dance, 0, 1, moves::Shapes()));
	EXPECT_NEAR(second.x, 995.0f, 0.01f);
	EXPECT_NEAR(second.y, 2000.0f, 0.01f);
	// Moved off, the group's centre moves by whole metres
	group.offset = {2.7f, -1.2f};
	const auto centre = map_coords::ToMetres(moves::GroupCentre(dance, 0));
	EXPECT_NEAR(centre.x, 1002.0f, 0.01f);
	EXPECT_NEAR(centre.y, 1999.0f, 0.01f);
}

TEST(DanceMoves, DancersFaceAsTheirGroupsMoveHasThem)
{
	EXPECT_EQ(moves::Facing({.action = moves::Action::FaceCentre, .towardsCentre = 300}), 300);
	EXPECT_EQ(moves::Facing({.action = moves::Action::FaceAway, .towardsCentre = 300}), 1324);
	EXPECT_EQ(moves::Facing({.action = moves::Action::FaceAway, .towardsCentre = 1500}), 476);
	EXPECT_EQ(moves::Facing({.action = moves::Action::TurnOnTheSpot, .facing = 2000}), 80);
	EXPECT_FALSE(moves::Facing({.action = moves::Action::Turn, .distance = 1.0f}).has_value());
	EXPECT_FALSE(moves::Facing({.action = moves::Action::Clip}).has_value());
}

TEST(DanceMoves, ADancersClipFollowsItsGroupsMove)
{
	const auto never = [](uint32_t) -> uint32_t { return 0; };
	EXPECT_EQ(moves::DanceClip({}, never), static_cast<int32_t>(AnimId::PStand));
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .stopped = true}, never), 0x171);
	// The clip table: a man's DanceA, a woman's Pray
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 10, .first = 0}, never), static_cast<int32_t>(AnimId::PMDanceA));
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 10, .first = 22, .female = true}, never),
	          static_cast<int32_t>(AnimId::PPray));
	// One of three dances, drawn
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 4, .female = true}, [](uint32_t) -> uint32_t { return 2; }),
	          static_cast<int32_t>(AnimId::PFDanceC));
	// Growing, they walk
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 11, .walkClip = 7}, never), 7);
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 1, .rate = 0.0f, .walkClip = 7}, never),
	          static_cast<int32_t>(AnimId::PStand));
}

TEST(Dances, AKeyFramesMoveSetsItsGroupGoingAndItsDancersClipsAgain)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	dance::DanceAction grow;
	grow.groups = {0};
	grow.type = 3;
	grow.arguments[4] = 11;
	grow.arguments[5] = std::bit_cast<uint32_t>(15.0f);
	grow.arguments[7] = 100;
	file->keyFrames.at(1).actions.push_back(grow);
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true}, file);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	auto& dance = registry.Get<Dance>(entity);
	dance.clock = 20.0f;
	std::vector<entt::entity> playedAgain;
	dances::TurnContext context {.turn = 1,
	                             .available = [](entt::entity) { return true; },
	                             .finished = {},
	                             .playClipAgain = [&playedAgain](entt::entity e) { playedAgain.push_back(e); }};
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(playedAgain, std::vector<entt::entity> {dancer});
	const auto& group = registry.Get<const Dance>(entity).groups.all[0];
	// At a quarter speed (rate 0.8) over the 80 beats left: (15 - 20) * 0.01 a turn
	EXPECT_FLOAT_EQ(group.radiusRate, -0.05f);
	EXPECT_FLOAT_EQ(group.radius, 19.95f);
}

TEST(Dances, EachActionStartingAMovePlaysItsDancersClipsAgainBeforeTheMovesGoOn)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	// Two moves for the same group in one key frame, both over by now
	dance::DanceAction grow;
	grow.groups = {0};
	grow.type = 3;
	grow.arguments[4] = 11;
	grow.arguments[5] = std::bit_cast<uint32_t>(15.0f);
	grow.arguments[7] = 0;
	auto shrink = grow;
	shrink.arguments[5] = std::bit_cast<uint32_t>(5.0f);
	file->keyFrames.at(1).actions.push_back(grow);
	file->keyFrames.at(1).actions.push_back(shrink);
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true}, file);
	// The two groups share the men half and half, by turns: the first and third go to group 0, the one between to 1
	const auto first = registry.Create();
	const auto other = registry.Create();
	const auto second = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, first, DanceGroup::k_Men));
	ASSERT_TRUE(dances::AddDancer(registry, entity, other, DanceGroup::k_Men));
	ASSERT_TRUE(dances::AddDancer(registry, entity, second, DanceGroup::k_Men));
	ASSERT_EQ(registry.Get<const Dance>(entity).groups.all[0].dancers, (std::vector<entt::entity> {first, second}));
	registry.Get<Dance>(entity).clock = 20.0f;
	// Each dancer's clip again, as it is told, with the move the action has just started
	std::vector<std::pair<entt::entity, uint32_t>> playedAgain;
	dances::TurnContext context {.turn = 1, .available = [](entt::entity) { return true; }, .finished = {}};
	context.playClipAgain = [&registry, &playedAgain, entity](entt::entity e) {
		const auto& move = registry.Get<const Dance>(entity).groups.all[0].move;
		playedAgain.emplace_back(e, move.action == 11 ? move.first : 0u);
	};
	dances::ProcessTurn(registry, entity, context);
	const auto grown = std::bit_cast<uint32_t>(15.0f);
	const auto shrunk = std::bit_cast<uint32_t>(5.0f);
	const std::vector<std::pair<entt::entity, uint32_t>> expected {
	    {first, grown}, {second, grown}, {first, shrunk}, {second, shrunk}};
	EXPECT_EQ(playedAgain, expected);
	// The move was over by the time the groups' moves went on
	EXPECT_EQ(registry.Get<const Dance>(entity).groups.all[0].move.action, 0u);
}

TEST(Dances, TheyTakeTheirTurnsNewestFirstAndKeepTheirOrderWhenOneGoes)
{
	Registry registry;
	const auto first = dances::Create(registry, {.type = DanceInfo::DanceAroundPerson}, nullptr);
	const auto second = dances::Create(registry, {.type = DanceInfo::DanceAroundPerson}, nullptr);
	const auto third = dances::Create(registry, {.type = DanceInfo::DanceAroundPerson}, nullptr);
	EXPECT_EQ(dances::InTurnOrder(registry), (std::vector<entt::entity> {third, second, first}));
	// The oldest goes: the others keep their order, and a new one comes first
	dances::Destroy(registry, first, {});
	EXPECT_EQ(dances::InTurnOrder(registry), (std::vector<entt::entity> {third, second}));
	const auto fourth = dances::Create(registry, {.type = DanceInfo::DanceAroundPerson}, nullptr);
	EXPECT_EQ(dances::InTurnOrder(registry), (std::vector<entt::entity> {fourth, third, second}));
}

TEST(Dances, WhatADanceIsAboutHasGoneWhenItIsGoingOrAVillagerIsDying)
{
	static_assert(dances::IsStillAbout(false, std::nullopt));
	// Anything going has gone, a villager too
	EXPECT_FALSE(dances::IsStillAbout(true, std::nullopt));
	EXPECT_FALSE(dances::IsStillAbout(true, VillagerStates::InDance));
	// A villager has gone once it works towards dying, and not before
	EXPECT_FALSE(dances::IsStillAbout(false, VillagerStates::Dying));
	EXPECT_TRUE(dances::IsStillAbout(false, VillagerStates::SetDying));
	EXPECT_TRUE(dances::IsStillAbout(false, VillagerStates::InScript));
}

TEST(Dances, ADanceThatGoesPassesOverItsDancersThatAreGoing)
{
	Registry registry;
	const auto entity = OneGroupDance(registry);
	const auto staying = registry.Create();
	const auto going = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, staying, DanceGroup::k_Men));
	ASSERT_TRUE(dances::AddDancer(registry, entity, going, DanceGroup::k_Men));
	registry.Assign<components::Unavailable>(going);
	std::vector<entt::entity> finished;
	dances::Destroy(registry, entity, [&finished](entt::entity e) { finished.push_back(e); });
	EXPECT_FALSE(registry.Valid(entity));
	// Only the dancer that is not going leaves and is told
	EXPECT_EQ(finished, std::vector<entt::entity> {staying});
	EXPECT_FALSE(registry.AllOf<components::Dancer>(staying));
	EXPECT_TRUE(registry.AllOf<components::Dancer>(going));
	EXPECT_EQ(dances::DanceOf(registry, going), entt::entity {entt::null});
}

TEST(Dances, ADanceThatIsGoingWaitsWhenWhatItIsAboutGoes)
{
	Registry registry;
	const auto owner = registry.Create();
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .owner = owner}, nullptr);
	registry.Assign<components::Unavailable>(entity);
	const dances::TurnContext context {.turn = 1, .available = [](entt::entity) { return false; }, .finished = {}};
	dances::ProcessTurn(registry, entity, context);
	ASSERT_TRUE(registry.Valid(entity));
	EXPECT_EQ(registry.Get<const Dance>(entity).owner, owner);
	// Not going, it goes
	registry.Remove<components::Unavailable>(entity);
	dances::ProcessTurn(registry, entity, context);
	EXPECT_FALSE(registry.Valid(entity));
}

TEST(Dances, ADancerKnowsHowFarItsPlaceIsAndWhichWayToTurn)
{
	Registry registry;
	const auto entity = OneGroupDance(registry);
	auto& group = registry.Get<Dance>(entity).groups.all[0];
	group.radius = 10.0f;
	group.move.action = static_cast<uint32_t>(moves::Action::TurnOnTheSpot);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	// In no group, nothing
	EXPECT_FALSE(dances::DancerTurnOf(registry, registry.Create(), {}, 0).has_value());
	// Its place: half the radius out along x
	const auto farOff = dances::DancerTurnOf(registry, dancer, map_coords::FromMetres({995.0f, 2000.0f}), 2000);
	ASSERT_TRUE(farOff.has_value());
	const auto place = map_coords::ToMetres(farOff->place);
	EXPECT_NEAR(place.x, 1005.0f, 0.01f);
	EXPECT_NEAR(place.y, 2000.0f, 0.01f);
	// 10 m off it walks round what is in the way, 2 m off straight there
	EXPECT_NEAR(farOff->distance, 10.0f, 0.01f);
	EXPECT_GT(farOff->distance, dances::k_WalkRoundDistance);
	const auto nearBy = dances::DancerTurnOf(registry, dancer, map_coords::FromMetres({1003.0f, 2000.0f}), 2000);
	ASSERT_TRUE(nearBy.has_value());
	EXPECT_NEAR(nearBy->distance, 2.0f, 0.01f);
	EXPECT_LT(nearBy->distance, dances::k_WalkRoundDistance);
	// Standing where a walk there leaves it, it is there
	const auto there = dances::DancerTurnOf(registry, dancer, farOff->place, 2000);
	ASSERT_TRUE(there.has_value());
	EXPECT_EQ(there->distance, 0.0f);
	// Its group's move turns it an eighth of a turn
	EXPECT_EQ(there->facing, std::optional<uint16_t> {80});
}

TEST(Dances, AWalkingDancersPlaceMovesWithItsGroup)
{
	Registry registry;
	const auto entity = OneGroupDance(registry);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	EXPECT_FALSE(dances::MovedPlace(registry, dancer).has_value());
	auto& group = registry.Get<Dance>(entity).groups.all[0];
	group.moved = true;
	group.offset = {3.0f, 0.0f};
	const auto moved = dances::MovedPlace(registry, dancer);
	ASSERT_TRUE(moved.has_value());
	const auto now = dances::DancerTurnOf(registry, dancer, {}, 0);
	ASSERT_TRUE(now.has_value());
	EXPECT_EQ(*moved, now->place);
	// In no group, no place
	EXPECT_FALSE(dances::MovedPlace(registry, registry.Create()).has_value());
}

TEST(VillagerDance, TheWalkToItsPlaceIsWrittenStraightAndKeepsTheDance)
{
	components::LivingAction action(VillagerStates::MoveToPos, 7);
	action.states[static_cast<size_t>(components::LivingAction::Index::Final)] = static_cast<uint8_t>(VillagerStates::InDance);
	action.turnsSinceStateChange = 9;
	villager_dance::SetWalkingToPlace(action);
	EXPECT_EQ(action.states[static_cast<size_t>(components::LivingAction::Index::Top)],
	          static_cast<uint8_t>(VillagerStates::MoveToDancePos));
	// The dance stays the state it works towards; the turns in the state start again, the state's counter stays
	EXPECT_EQ(action.states[static_cast<size_t>(components::LivingAction::Index::Final)],
	          static_cast<uint8_t>(VillagerStates::InDance));
	EXPECT_EQ(action.turnsSinceStateChange, 0);
	EXPECT_EQ(action.turnsUntilStateChange, 7);
}

TEST(VillagerDance, ADancerToldToDecideLeavesItsDanceButStaysInItsState)
{
	namespace script_rules = openblack::ecs::villager_script_rules;
	// Deciding what to do: a final state no script lets interrupt it, with another exit
	const auto decide = script_rules::ExitDance(
	    VillagerStates::DecideWhatToDo, {.isScriptState = false, .isScriptInterruptable = false, .isFinalState = true}, false);
	EXPECT_TRUE(decide.leavesDance);
	EXPECT_FALSE(decide.mayGo);
	// Another of the script's states: it stays in its dance and goes
	const auto script = script_rules::ExitDance(
	    VillagerStates::InScript, {.isScriptState = true, .isScriptInterruptable = false, .isFinalState = true}, false);
	EXPECT_FALSE(script.leavesDance);
	EXPECT_TRUE(script.mayGo);
	// Picked up: out of the dance, into the hand
	const auto held = script_rules::ExitDance(
	    VillagerStates::InHand, {.isScriptState = false, .isScriptInterruptable = false, .isFinalState = true}, false);
	EXPECT_TRUE(held.leavesDance);
	EXPECT_TRUE(held.mayGo);
}
