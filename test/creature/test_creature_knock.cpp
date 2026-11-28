/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature's knock (Creature/CreatureKnock.h): the bones' radii by the creature's size, the move a bone must pass,
// its map cell, what it may knock and how, the reach and the model's points it must meet, the push and the home's blow
// left on the move for the next thing, and the body's and the fighter's states that start and stop it

#include <cstddef>

#include <array>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureKnock.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
namespace knock = openblack::creature_knock;
using creature_layers::BodyAction;

TEST(CreatureKnock, TheBonesKnockWithinAShareOfTheStandingHeightTheHeadAndLimbsByTheirScales)
{
	const knock::BoneIndices indices {
	    .head = 1, .rump = 2, .rightHand = 3, .leftHand = 4, .rightFoot = 5, .leftFoot = 6, .rightArmpit = 7, .leftArmpit = 8};
	knock::Bones before {};
	before.at(0).place = {1.0f, 2.0f, 3.0f};
	const auto bones = knock::Initialise(before, indices, 1.0f);
	// fifteen high: the head (1.8 - 0.5) x 15 / 8, the rump 15 / 6, the hands and feet (1.5 - 0.25) x 15 / 8, the
	// armpits 15 / 8
	EXPECT_FLOAT_EQ(bones.at(0).radius, 2.4375f);
	EXPECT_FLOAT_EQ(bones.at(1).radius, 2.5f);
	for (size_t i = 2; i < 6; ++i)
	{
		EXPECT_FLOAT_EQ(bones.at(i).radius, 2.34375f);
	}
	EXPECT_FLOAT_EQ(bones.at(6).radius, 1.875f);
	EXPECT_FLOAT_EQ(bones.at(7).radius, 1.875f);
	EXPECT_EQ(bones.at(0).bone, 1u);
	EXPECT_EQ(bones.at(7).bone, 8u);
	// the places stay as they were
	EXPECT_EQ(bones.at(0).place, glm::vec3(1.0f, 2.0f, 3.0f));
	// the head's scale is taken at a size of at most 2
	EXPECT_FLOAT_EQ(knock::Initialise({}, indices, 3.0f).at(0).radius, 0.8f * 45.0f * 0.125f);
}

TEST(CreatureKnock, ABoneKnocksOnlyOnceItMovedMoreThanThreeQuartersOfTheSize)
{
	const float threshold = knock::MoveThresholdSq(2.0f);
	EXPECT_FLOAT_EQ(threshold, 2.25f);
	const auto still = knock::MoveOf({0.0f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f}, threshold);
	EXPECT_FALSE(still.knocks);
	const auto moved = knock::MoveOf({1.0f, 1.0f, 1.0f}, {2.0f, 2.0f, 2.0f}, threshold);
	EXPECT_TRUE(moved.knocks);
	EXPECT_EQ(moved.by, glm::vec3(1.0f, 1.0f, 1.0f));
}

TEST(CreatureKnock, TheCellIsTheTenthsCutTowardsZero)
{
	EXPECT_EQ(knock::CellOf({25.0f, 100.0f, 39.9f}), glm::ivec2(2, 3));
	EXPECT_EQ(knock::CellOf({-5.0f, 0.0f, 10.0f}), glm::ivec2(0, 1));
}

TEST(CreatureKnock, WhatTheKnockMayTake)
{
	const knock::Facts villager {.hasModel = true, .villager = true, .living = true};
	EXPECT_EQ(knock::KindOf(villager), knock::Kind::Living);
	auto immovable = villager;
	immovable.immovable = true;
	EXPECT_EQ(knock::KindOf(immovable), knock::Kind::None);
	auto indestructible = villager;
	indestructible.indestructible = true;
	EXPECT_FALSE(knock::CanKnock(indestructible));
	auto noModel = villager;
	noModel.hasModel = false;
	EXPECT_FALSE(knock::CanKnock(noModel));
	// a bird of the dove class is a living thing it leaves alone
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .animal = true, .dove = true, .living = true}));
	EXPECT_EQ(knock::KindOf({.hasModel = true, .animal = true, .living = true}), knock::Kind::Living);
	// never a creature, the temple's heart, a tree, a storage pit, a town centre or a fragment
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .creature = true, .living = true}));
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .templeHeart = true}));
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .tree = true, .canBecomePhysicsObject = true}));
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .storagePit = true, .home = true}));
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .townCentre = true, .home = true}));
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .fragment = true, .canBecomePhysicsObject = true}));
	// a home, not a field
	EXPECT_EQ(knock::KindOf({.hasModel = true, .home = true}), knock::Kind::Home);
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .home = true, .field = true}));
	// an animated model, or anything that can't fly or is immovable
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .animatedModel = true, .canBecomePhysicsObject = true}));
	EXPECT_EQ(knock::KindOf({.hasModel = true, .canBecomePhysicsObject = true}), knock::Kind::Thing);
	EXPECT_FALSE(knock::CanKnock({.hasModel = true}));
	EXPECT_FALSE(knock::CanKnock({.hasModel = true, .immovable = true, .canBecomePhysicsObject = true}));
}

TEST(CreatureKnock, AHomeIsReachedByTheWholeBoneOtherThingsByThreeTenthsOfIt)
{
	const glm::vec3 bone {0.0f, 5.0f, 0.0f};
	// 2 away, radius 1: a home within 1 + 2, a living thing within 1 + 0.6
	EXPECT_TRUE(knock::WithinReach(knock::Kind::Home, {2.0f, 0.0f}, 1.0f, bone, 2.0f));
	EXPECT_FALSE(knock::WithinReach(knock::Kind::Living, {2.0f, 0.0f}, 1.0f, bone, 2.0f));
	EXPECT_TRUE(knock::WithinReach(knock::Kind::Thing, {0.0f, 1.5f}, 1.0f, bone, 2.0f));
	EXPECT_FALSE(knock::WithinReach(knock::Kind::None, {0.0f, 0.0f}, 1.0f, bone, 2.0f));
}

TEST(CreatureKnock, TheBoneMustMeetAPointOfTheModelThroughItsMatrix)
{
	const knock::Rows rows {
	    .x = {0.0f, 0.0f, -1.0f}, .y = {0.0f, 1.0f, 0.0f}, .z = {1.0f, 0.0f, 0.0f}, .place = {10.0f, 0.0f, 20.0f}};
	EXPECT_EQ(knock::ToWorld(rows, {1.0f, 2.0f, 3.0f}), glm::vec3(13.0f, 2.0f, 19.0f));
	const std::array points {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 2.0f, 3.0f)};
	EXPECT_TRUE(knock::MeetsAnyPoint(rows, points, {13.5f, 2.0f, 19.0f}, 1.0f));
	EXPECT_FALSE(knock::MeetsAnyPoint(rows, points, {14.0f, 2.0f, 19.0f}, 1.0f));
}

TEST(CreatureKnock, EachPushLeavesTheMoveSevenTimesBiggerForTheNextThing)
{
	glm::vec3 move {1.0f, 0.5f, -2.0f};
	EXPECT_EQ(knock::Push(move), glm::vec3(7.0f, 3.5f, -14.0f));
	EXPECT_EQ(knock::Push(move), glm::vec3(49.0f, 24.5f, -98.0f));
}

TEST(CreatureKnock, AHomeIsStruckAlongTheMoveAtFourTimesItsLengthAndLeavesItOfLengthOne)
{
	glm::vec3 move {3.0f, 0.0f, 4.0f};
	const auto blow = knock::HomeBlow(move);
	EXPECT_FLOAT_EQ(blow.x, 12.0f);
	EXPECT_FLOAT_EQ(blow.z, 16.0f);
	EXPECT_FLOAT_EQ(move.x, 0.6f);
	EXPECT_FLOAT_EQ(move.z, 0.8f);
	glm::vec3 none {0.0f};
	EXPECT_EQ(knock::HomeBlow(none), glm::vec3(0.0f));
	EXPECT_EQ(none, glm::vec3(0.0f));
}

TEST(CreatureKnock, TheFightersStatesThatStartAndStopIt)
{
	using creature_fight::State;
	EXPECT_EQ(knock::FightMark(State::Start), knock::Mark::Start);
	EXPECT_EQ(knock::FightMark(State::Action), knock::Mark::Start);
	EXPECT_EQ(knock::FightMark(State::CastStart), knock::Mark::Start);
	EXPECT_EQ(knock::FightMark(State::Faint), knock::Mark::SetUpAndStart);
	EXPECT_EQ(knock::FightMark(State::Stance), knock::Mark::Stop);
	EXPECT_EQ(knock::FightMark(State::Lying), knock::Mark::Stop);
	EXPECT_EQ(knock::FightMark(State::Idle), knock::Mark::Stop);
	EXPECT_FALSE(knock::FightMark(State::Finish).has_value());
	EXPECT_FALSE(knock::FightMark(State::Block).has_value());
	EXPECT_FALSE(knock::FightMark(State::Cast).has_value());
	EXPECT_FALSE(knock::FightMark(State::GetUp).has_value());
}

TEST(CreatureKnock, AFighterEnteringAStateLeavesItsLastMarkForTheKnock)
{
	creature_fight::Fighter fighter;
	creature_fight::Enter(fighter, creature_fight::State::Action);
	creature_fight::Enter(fighter, creature_fight::State::Finish);
	EXPECT_EQ(fighter.knockMark, knock::Mark::Start);
	creature_fight::Enter(fighter, creature_fight::State::Stance);
	EXPECT_EQ(fighter.knockMark, knock::Mark::Stop);
}

TEST(CreatureKnock, TheBodysStatesThatStartAndStopIt)
{
	namespace animations = creature_layers::animations;
	EXPECT_EQ(knock::BodyStateOf(BodyAction {}), knock::BodyState::Standing);
	BodyAction sleep {.kind = BodyAction::Kind::Sequence,
	                  .animations = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}};
	EXPECT_EQ(knock::BodyStateOf(sleep), knock::BodyState::SleepStart);
	sleep.phase = BodyAction::Phase::Loop;
	EXPECT_EQ(knock::BodyStateOf(sleep), knock::BodyState::Sleeping);
	sleep.phase = BodyAction::Phase::End;
	EXPECT_EQ(knock::BodyStateOf(sleep), knock::BodyState::SleepEnd);
	namespace fight = creature_fight::animations;
	BodyAction faint {.kind = BodyAction::Kind::Sequence, .animations = {fight::k_Faint, fight::k_Faint, fight::k_GetUp}};
	EXPECT_EQ(knock::BodyStateOf(faint), knock::BodyState::Fainting);
	faint.phase = BodyAction::Phase::Loop;
	EXPECT_EQ(knock::BodyStateOf(faint), knock::BodyState::LyingFainted);
	faint.phase = BodyAction::Phase::End;
	EXPECT_EQ(knock::BodyStateOf(faint), knock::BodyState::GettingUp);
	EXPECT_EQ(knock::BodyStateOf({.kind = BodyAction::Kind::Once, .animations = {animations::k_Summon}}),
	          knock::BodyState::Other);
	EXPECT_EQ(knock::BodyStateOf({.kind = BodyAction::Kind::Sequence, .timedByPlayer = true}), knock::BodyState::Fighting);

	EXPECT_EQ(knock::BodyMark(knock::BodyState::Standing), knock::Mark::Stop);
	EXPECT_EQ(knock::BodyMark(knock::BodyState::Sleeping), knock::Mark::Stop);
	EXPECT_EQ(knock::BodyMark(knock::BodyState::LyingFainted), knock::Mark::Stop);
	EXPECT_EQ(knock::BodyMark(knock::BodyState::SleepStart), knock::Mark::SetUpAndStart);
	EXPECT_EQ(knock::BodyMark(knock::BodyState::SleepEnd), knock::Mark::SetUpAndStart);
	EXPECT_EQ(knock::BodyMark(knock::BodyState::Fainting), knock::Mark::SetUpAndStart);
	EXPECT_FALSE(knock::BodyMark(knock::BodyState::GettingUp).has_value());
	EXPECT_FALSE(knock::BodyMark(knock::BodyState::Other).has_value());
	EXPECT_FALSE(knock::BodyMark(knock::BodyState::Fighting).has_value());
}
