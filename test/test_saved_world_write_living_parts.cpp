/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The parts every moving and living object writes, made from what openblack holds, else the part its load kept, else
// its constructor's values: a villager's and an animal's from their components, the kept values openblack does not
// hold, the failures that name the object, and the frozen Land 1 autosave's villagers and animals written back from
// entities holding what their records say (docs/wiki/engine/saved-games.md#writing-the-world-file)

#include <cstdint>
#include <cstdlib>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <TownRecords.h>
#include <VillagerRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteLivingParts.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// An object some part names: a class of its own, so that the context numbers it
struct Named
{
};

ClassWriters NamingWriters()
{
	ClassWriters writers;
	writers.Register(
	    gamesave::FootpathRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<Named>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	return writers;
}

entt::entity MakeNamed(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Named>(entity);
	return entity;
}

/// A frozen profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / relative, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

/// The part with its references' numbers zeroed, after checking that each is named where `expected` names one
gamesave::WallHugPart WithoutNumbers(gamesave::WallHugPart part, const gamesave::WallHugPart& expected)
{
	const auto check = [](gamesave::Ref& ref, const gamesave::Ref& other) {
		EXPECT_EQ(ref.number != 0, other.number != 0);
		ref.number = 0;
	};
	if (part.circle.has_value() && expected.circle.has_value())
	{
		check(part.circle->object, expected.circle->object);
	}
	check(part.footpath, expected.footpath);
	check(part.target, expected.target);
	return part;
}

gamesave::LivingFields WithoutNumbers(gamesave::LivingFields part, const gamesave::LivingFields& expected)
{
	const auto check = [](gamesave::Ref& ref, const gamesave::Ref& other) {
		EXPECT_EQ(ref.number != 0, other.number != 0);
		ref.number = 0;
	};
	check(part.footpath, expected.footpath);
	check(part.footpathNode, expected.footpathNode);
	check(part.reaction, expected.reaction);
	check(part.dataPath, expected.dataPath);
	check(part.scriptReminder, expected.scriptReminder);
	check(part.flock, expected.flock);
	check(part.otherObject, expected.otherObject);
	check(part.footpathAgain, expected.footpathAgain);
	check(part.danceGroup, expected.danceGroup);
	return part;
}

gamesave::WallHugPart Unnumbered(gamesave::WallHugPart part)
{
	return WithoutNumbers(part, part);
}

gamesave::LivingFields Unnumbered(gamesave::LivingFields part)
{
	return WithoutNumbers(part, part);
}

/// The components every living thing may hold, as a living thing whose part is `living` holds them
void HoldShared(ecs::Registry& registry, entt::entity entity, const gamesave::WallHugPart& move,
                const gamesave::LivingFields& living)
{
	if (living.footpath.number != 0 || move.footpath.number != 0)
	{
		registry.Assign<components::LivingFootpath>(
		    entity, components::LivingFootpath {
		                .next = move.footpath.number != 0 ? MakeNamed(registry) : entt::null,
		                .footpath = living.footpath.number != 0 ? MakeNamed(registry) : entt::null,
		                .node = living.footpathNode.number != 0 ? 0 : -1,
		                .reverse = (living.status & 0x0008) != 0,
		            });
	}
	if (living.specular != std::array<uint8_t, 4> {})
	{
		registry.Assign<components::SpecularColour>(
		    entity, components::SpecularColour {.colour = {living.specular[2], living.specular[1], living.specular[0]}});
	}
	if (living.danceGroup.number != 0)
	{
		const auto dance = registry.Create();
		auto& groups = registry.Assign<components::Dance>(dance).groups;
		groups.all.resize(1);
		groups.all[0].dancers.assign(living.thirdByte, entt::null);
		groups.all[0].dancers.push_back(entity);
		registry.Assign<components::Dancer>(entity, components::Dancer {.dance = dance, .group = 0});
	}
	if (!living.reactionsDone.empty())
	{
		auto& memory = registry.Assign<components::ReactionRecords>(entity);
		for (const auto& done : living.reactionsDone)
		{
			memory.records.at(memory.count++) = {.type = static_cast<uint8_t>(done.reaction), .turn = done.turn};
		}
	}
}

/// A villager entity holding what a villager's record says
entt::entity HoldVillager(ecs::Registry& registry, const gamesave::MobilePart& /*mobile*/, const gamesave::WallHugPart& move,
                          const gamesave::LivingFields& living)
{
	const auto entity = registry.Create();
	auto& villager = registry.Assign<components::Villager>(entity);
	villager.birthTurn = living.birthTurn;
	villager.status = static_cast<uint16_t>(living.status & 0x0471);
	villager.flock = living.flock.number != 0 ? MakeNamed(registry) : entt::null;
	villager.flockOrder = living.secondByte;
	villager.crecheWalk = std::bit_cast<int32_t>(living.secondWord);
	if ((living.status & 0x0002) != 0)
	{
		registry.Assign<components::Poisoned>(entity);
	}
	if ((living.status & 0x0080) != 0)
	{
		registry.Assign<components::DownedVillager>(entity);
	}
	auto& action = registry.Assign<components::LivingAction>(entity, VillagerStates::InvalidState,
	                                                         std::bit_cast<uint16_t>(move.turnsToStateChange));
	action.states = {living.action[0], living.action[1], living.action[2]};
	action.turnsSinceStateChange = static_cast<uint16_t>(living.action[4] | (living.action[5] << 8U));
	if (living.reaction.number != 0 || living.otherObject.number != 0)
	{
		registry.Assign<components::LivingReaction>(
		    entity, components::LivingReaction {.reaction = living.reaction.number,
		                                        .object = living.otherObject.number != 0 ? MakeNamed(registry) : entt::null});
	}
	auto& wallHug = registry.Assign<components::WallHug>(entity);
	wallHug.speed = map_coords::ToMetres(static_cast<int32_t>(move.speed));
	wallHug.gameAngle = move.angle;
	wallHug.turnsUntilStepRebuild = move.turnsToStepRebuild.value_or(int8_t {0});
	wallHug.goal = {map_coords::ToMetres(move.goal.x), map_coords::ToMetres(move.goal.z)};
	wallHug.step = {move.step.x, move.step.z};
	if (move.circle.has_value())
	{
		auto& circle = registry.Assign<components::WallHugObjectReference>(entity);
		circle.stepsAway = move.circle->turnsToObject;
		circle.entity = move.circle->object.number != 0 ? MakeNamed(registry) : entt::null;
	}
	HoldShared(registry, entity, move, living);
	return entity;
}

/// An animal entity holding what an animal's record says
entt::entity HoldAnimal(ecs::Registry& registry, const gamesave::WallHugPart& move, const gamesave::LivingFields& living)
{
	const auto entity = registry.Create();
	auto& brain = registry.Assign<components::AnimalBrain>(entity);
	brain.counter = move.turnsToStateChange;
	brain.speed = move.speed;
	brain.angle = move.angle;
	brain.moveState = move.moveState;
	if (move.circle.has_value())
	{
		brain.turnsToObj = move.circle->turnsToObject;
		brain.hugCircle.set = move.circle->object.number != 0;
		brain.hugCircle.owner = move.circle->object.number != 0 ? MakeNamed(registry) : entt::null;
	}
	brain.goal = {map_coords::ToMetres(move.goal.x), map_coords::ToMetres(move.goal.z)};
	brain.goalAltitude = move.goal.altitude;
	brain.target = move.target.number != 0 ? MakeNamed(registry) : entt::null;
	brain.step = {move.step.x, move.step.z};
	brain.topState = living.action[0];
	brain.finalState = living.action[1];
	brain.previousState = living.action[2];
	brain.turnsSinceStateChange = static_cast<uint16_t>(living.action[4] | (living.action[5] << 8U));
	brain.reaction = living.reaction.number;
	brain.predator = living.otherObject.number != 0 ? MakeNamed(registry) : entt::null;
	brain.birthTurn = std::bit_cast<int32_t>(living.birthTurn);
	brain.status = static_cast<uint16_t>(living.status & 0x00B1);
	auto& animal = registry.Assign<components::Animal>(entity);
	animal.flock = living.flock.number != 0 ? MakeNamed(registry) : entt::null;
	animal.flockOrder = living.secondByte;
	HoldShared(registry, entity, move, living);
	return entity;
}

/// A kept part with a value in every field
LivingParts KeptParts()
{
	LivingParts parts;
	parts.mobile.mobileWord = 0x0102;
	parts.move = {.turnsToStateChange = 40,
	              .speed = 655,
	              .angle = 0x123,
	              .moveState = 0x05,
	              .goal = {.x = 0x10000, .z = 0x20000, .altitude = 1.5f},
	              .step = {.x = 30, .altitude = 0.25f, .z = -40}};
	parts.living.action = {7, 8, 9, 0x5A, 3, 1};
	parts.living.firstWord = 1234;
	parts.living.birthTurn = 99;
	parts.living.firstByte = 0x77;
	parts.living.scriptReminder.number = 0;
	parts.living.status = 0x0104;
	parts.living.secondWord = 0x00020003;
	parts.living.thirdByte = 6;
	return parts;
}
} // namespace

// The constructor leaves the row's default speed, kept within a u16, and the row's strength truncated; every other
// value zero
TEST(SavedWorldWriteLivingParts, TheConstructorsPartsAreTheRowsSpeedAndStrength)
{
	GLivingInfo info {};
	info.speedGroup.speedDefault = SpeedState::MetresSec_1;
	info.strength = 2.75f;
	const auto parts = LivingConstructorParts(info);
	EXPECT_EQ(parts.move.speed, static_cast<uint16_t>(static_cast<uint32_t>(SpeedState::MetresSec_1)));
	EXPECT_EQ(parts.living.firstWord, 2u);
	EXPECT_EQ(parts.mobile, gamesave::MobilePart {});
	auto rest = parts;
	rest.move.speed = 0;
	rest.living.firstWord = 0;
	EXPECT_EQ(rest, LivingParts {});
}

// A villager made in play is written from its components: its counter, speed, angle, goal and step, its states, birth
// turn, status, flock, creche walk, specular colour, dance and the reactions it did; what openblack does not hold
// comes from the constructor's parts
TEST(SavedWorldWriteLivingParts, AVillagerMadeInPlayIsWrittenFromItsComponents)
{
	ecs::Registry registry;
	const auto writers = NamingWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);

	const auto entity = registry.Create();
	auto& villager = registry.Assign<components::Villager>(entity);
	villager.birthTurn = 1500;
	villager.status = components::Villager::k_StatusSkeleton;
	villager.flock = MakeNamed(registry);
	villager.flockOrder = 2;
	villager.crecheWalk = 0x00030004;
	registry.Assign<components::Poisoned>(entity);
	auto& action = registry.Assign<components::LivingAction>(entity, VillagerStates::MoveToPos, uint16_t {321});
	action.turnsSinceStateChange = 0x0102;
	auto& wallHug = registry.Assign<components::WallHug>(entity);
	wallHug.speed = map_coords::ToMetres(int32_t {983});
	wallHug.gameAngle = 0x400;
	wallHug.goal = {12.5f, 20.0f};
	wallHug.step = {100, -200};
	const auto footpath = MakeNamed(registry);
	registry.Assign<components::LivingFootpath>(
	    entity, components::LivingFootpath {.next = entt::null, .footpath = footpath, .node = 3, .reverse = true});
	registry.Assign<components::SpecularColour>(entity, components::SpecularColour {.colour = {10, 20, 30}});
	auto& memory = registry.Assign<components::ReactionRecords>(entity);
	memory.records[0] = {.type = 4, .turn = 50};
	memory.records[1] = {.type = 7, .turn = 60};
	memory.count = 2;

	GLivingInfo info {};
	info.speedGroup.speedDefault = SpeedState::MetresSec_0_5;
	info.strength = 3.0f;
	const auto written = LivingPartsOf(context, entity, nullptr, LivingConstructorParts(info));
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(written->mobile.mobileWord, 0);
	EXPECT_EQ(written->move.turnsToStateChange, 321);
	EXPECT_EQ(written->move.speed, 983);
	EXPECT_EQ(written->move.angle, 0x400);
	EXPECT_EQ(written->move.moveState, 0);
	EXPECT_EQ(written->move.goal.x, map_coords::ToFixed(12.5f));
	EXPECT_EQ(written->move.goal.z, map_coords::ToFixed(20.0f));
	EXPECT_EQ(written->move.step.x, 100);
	EXPECT_EQ(written->move.step.z, -200);
	EXPECT_EQ(written->move.footpath.number, 0u);
	EXPECT_EQ(written->living.action[0], static_cast<uint8_t>(VillagerStates::MoveToPos));
	EXPECT_EQ(written->living.action[4], 0x02);
	EXPECT_EQ(written->living.action[5], 0x01);
	EXPECT_EQ(written->living.firstWord, 3u);
	EXPECT_EQ(written->living.birthTurn, 1500u);
	EXPECT_EQ(written->living.status, components::Villager::k_StatusSkeleton | 0x0002 | 0x0008);
	EXPECT_NE(written->living.footpath.number, 0u);
	EXPECT_EQ(written->living.footpathAgain, written->living.footpath);
	EXPECT_NE(written->living.footpathNode.number, 0u);
	EXPECT_NE(written->living.flock.number, 0u);
	EXPECT_EQ(written->living.secondByte, 2);
	EXPECT_EQ(written->living.secondWord, 0x00030004u);
	EXPECT_EQ(written->living.specular, (std::array<uint8_t, 4> {30, 20, 10, 0xFF}));
	EXPECT_EQ(written->living.danceGroup.number, 0u);
	ASSERT_EQ(written->living.reactionsDone.size(), 2u);
	EXPECT_EQ(written->living.reactionsDone[0], (gamesave::ReactionDone {.reaction = 4, .turn = 50}));
	EXPECT_EQ(written->living.reactionsDone[1], (gamesave::ReactionDone {.reaction = 7, .turn = 60}));
}

// An animal is written from its brain: its move state, counter, speed, angle, target, goal with its altitude, step,
// states, reaction and its object, birth turn and status; its flock and its place there from the animal
TEST(SavedWorldWriteLivingParts, AnAnimalIsWrittenFromItsBrain)
{
	ecs::Registry registry;
	const auto writers = NamingWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);

	const auto entity = registry.Create();
	auto& brain = registry.Assign<components::AnimalBrain>(entity);
	brain.topState = 3;
	brain.finalState = 4;
	brain.previousState = 5;
	brain.turnsSinceStateChange = 9;
	brain.speed = 164;
	brain.angle = 0x7FF;
	brain.step = {5, 6};
	brain.goal = {1.0f, 2.0f};
	brain.goalAltitude = 4.5f;
	brain.moveState = 0x04;
	brain.counter = -3;
	brain.status = 0x0031;
	brain.target = MakeNamed(registry);
	brain.predator = MakeNamed(registry);
	brain.reaction = 17;
	brain.birthTurn = -3000;
	auto& animal = registry.Assign<components::Animal>(entity);
	animal.flock = MakeNamed(registry);
	animal.flockOrder = 5;

	auto kept = KeptParts();
	const auto written = LivingPartsOf(context, entity, &kept, LivingParts {});
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(written->move.moveState, 0x04);
	EXPECT_EQ(written->move.turnsToStateChange, -3);
	EXPECT_EQ(written->move.speed, 164);
	EXPECT_EQ(written->move.angle, 0x7FF);
	EXPECT_NE(written->move.target.number, 0u);
	EXPECT_EQ(written->move.goal.altitude, 4.5f);
	EXPECT_EQ(written->move.step.x, 5);
	EXPECT_EQ(written->move.step.z, 6);
	EXPECT_EQ(written->living.action[0], 3);
	EXPECT_EQ(written->living.action[1], 4);
	EXPECT_EQ(written->living.action[2], 5);
	EXPECT_EQ(written->living.action[4], 9);
	EXPECT_NE(written->living.reaction.number, 0u);
	EXPECT_NE(written->living.otherObject.number, 0u);
	EXPECT_EQ(written->living.birthTurn, std::bit_cast<uint32_t>(int32_t {-3000}));
	// dying and the land of its last landing are the brain's; the other bits as kept
	EXPECT_EQ(written->living.status, 0x0104 | 0x0031);
	EXPECT_NE(written->living.flock.number, 0u);
	EXPECT_EQ(written->living.secondByte, 5);
}

// What openblack does not hold stays as the load kept it: the moving thing's word, the byte among the action's, the
// step's height, the strength, the unread byte, the status bits openblack does not keep, the creche walk of an animal
// and its place in its last dance
TEST(SavedWorldWriteLivingParts, WhatOpenblackDoesNotHoldStaysAsKept)
{
	ecs::Registry registry;
	const auto writers = NamingWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto entity = registry.Create();
	registry.Assign<components::AnimalBrain>(entity);

	const auto kept = KeptParts();
	const auto written = LivingPartsOf(context, entity, &kept, LivingParts {});
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(written->mobile.mobileWord, 0x0102);
	EXPECT_EQ(written->living.action[3], 0x5A);
	EXPECT_EQ(written->move.step.altitude, 0.25f);
	EXPECT_EQ(written->living.firstWord, 1234u);
	EXPECT_EQ(written->living.firstByte, 0x77);
	EXPECT_EQ(written->living.status & 0x0104, 0x0104);
	EXPECT_EQ(written->living.secondWord, 0x00020003u);
	EXPECT_EQ(written->living.thirdByte, 6);

	// an entity openblack holds nothing of is written as kept
	const auto other = registry.Create();
	const auto same = LivingPartsOf(context, other, &kept, LivingParts {});
	ASSERT_TRUE(same.has_value()) << context.Failure();
	EXPECT_EQ(*same, kept);
}

// A goal openblack holds in metres keeps the kept coordinates while it is their place
TEST(SavedWorldWriteLivingParts, AGoalAtTheKeptPlaceKeepsItsCoordinates)
{
	ecs::Registry registry;
	const auto writers = NamingWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	gamesave::WallHugPart kept {.goal = {.x = 123457, .z = -98765, .altitude = 2.0f}};
	const WallHugHeld at {.goal = glm::vec2(map_coords::ToMetres(kept.goal.x), map_coords::ToMetres(kept.goal.z))};
	const auto same = WallHugPartOf(context, registry.Create(), &kept, {}, at);
	ASSERT_TRUE(same.has_value());
	EXPECT_EQ(same->goal, kept.goal);
	const WallHugHeld moved {.goal = glm::vec2(5.0f, 6.0f)};
	const auto elsewhere = WallHugPartOf(context, registry.Create(), &kept, {}, moved);
	ASSERT_TRUE(elsewhere.has_value());
	EXPECT_EQ(elsewhere->goal.x, map_coords::ToFixed(5.0f));
	EXPECT_EQ(elsewhere->goal.z, map_coords::ToFixed(6.0f));
	EXPECT_EQ(elsewhere->goal.altitude, 2.0f);
}

// A value no source gives stops the write with a message naming the object: the birth turn of a living thing made in
// play that openblack does not hold, a circle no load kept, a step through with no turns to its rebuild, a moving
// object made in play without its object
TEST(SavedWorldWriteLivingParts, AValueWithNoSourceFailsNamingTheObject)
{
	ecs::Registry registry;
	const auto writers = NamingWriters();
	const WorldSource source {.registry = registry};
	{
		WriteContext context(writers, source);
		EXPECT_FALSE(LivingPartsOf(context, registry.Create(), nullptr, LivingParts {}).has_value());
		EXPECT_NE(context.Failure().find("birth turn"), std::string::npos) << context.Failure();
		EXPECT_NE(context.Failure().find("entity"), std::string::npos) << context.Failure();
	}
	{
		WriteContext context(writers, source);
		const WallHugHeld circling {.moveState = gamesave::k_FirstCircleState};
		EXPECT_FALSE(WallHugPartOf(context, registry.Create(), nullptr, {}, circling).has_value());
		EXPECT_NE(context.Failure().find("circle"), std::string::npos) << context.Failure();
	}
	{
		WriteContext context(writers, source);
		const WallHugHeld stepping {.moveState = gamesave::k_StepThroughState};
		EXPECT_FALSE(WallHugPartOf(context, registry.Create(), nullptr, {}, stepping).has_value());
		EXPECT_NE(context.Failure().find("steps through"), std::string::npos) << context.Failure();
	}
	{
		WriteContext context(writers, source);
		EXPECT_FALSE(MobileObjectPartOf(context, registry.Create(), nullptr, std::nullopt).has_value());
		EXPECT_NE(context.Failure().find("made in play"), std::string::npos) << context.Failure();
		WriteContext made(writers, source);
		const auto part = MobileObjectPartOf(made, registry.Create(), nullptr, MakeNamed(registry));
		ASSERT_TRUE(part.has_value());
		EXPECT_NE(part->object, 0u);
		EXPECT_EQ(part->xAngle, 0.0f);
		EXPECT_EQ(part->zAngle, 0.0f);
		EXPECT_EQ(part->path, 0u);
	}
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. Every villager and
// animal of the frozen Land 1 autosave, made into an entity holding what its record says, writes back its record's
// parts: the same values, and an object named wherever the record names one
TEST(SavedWorldWriteLivingParts, TheFrozenLandOneAutosavesVillagersAndAnimalsWriteBackTheirParts)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	ecs::Registry registry;
	const auto writers = NamingWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);

	size_t villagers = 0;
	size_t animals = 0;
	const auto check = [&context](entt::entity entity, const LivingParts& kept) {
		const auto written = LivingPartsOf(context, entity, &kept, LivingParts {});
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_EQ(written->mobile, kept.mobile);
		EXPECT_EQ(WithoutNumbers(written->move, kept.move), Unnumbered(kept.move));
		EXPECT_EQ(WithoutNumbers(written->living, kept.living), Unnumbered(kept.living));
	};
	for (const auto& object : world.objects)
	{
		if (!object.has_value())
		{
			continue;
		}
		if (const auto* villager = std::get_if<gamesave::VillagerRecord>(&*object))
		{
			const LivingParts kept {
			    .mobile = villager->mobile, .move = villager->values.move, .living = villager->values.living};
			check(HoldVillager(registry, kept.mobile, kept.move, kept.living), kept);
			++villagers;
			continue;
		}
		std::visit(
		    [&](const auto& record) {
			    if constexpr (requires {
				                  record.animal;
				                  record.move;
				                  record.living;
			                  })
			    {
				    const LivingParts kept {.mobile = record.mobile, .move = record.move, .living = record.living};
				    check(HoldAnimal(registry, kept.move, kept.living), kept);
				    ++animals;
			    }
		    },
		    *object);
	}
	EXPECT_GT(villagers, 0u);
	EXPECT_GT(animals, 0u);
}
