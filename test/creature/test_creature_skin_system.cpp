/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature skin system on a registry of its own: tattoos, wounds, blood and heals mark the skins to be painted
// again, the turns heal the marks, and the skins are painted only from mesh files the loaders read at start-up

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureSkinSystem;

namespace
{
class CreatureSkinSystemTest: public ::testing::Test
{
protected:
	test::creature_world::World _world;
	CreatureSkinSystem _system;
};
} // namespace

TEST_F(CreatureSkinSystemTest, EachChangeBumpsItsRevision)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();

	_system.SetTattoo(creature, 2, {.design = 3, .site = 1, .colour = glm::u8vec3(10, 20, 30)});
	EXPECT_EQ(registry.Get<CreatureTattoos>(creature).revision, 1u);
	EXPECT_EQ(registry.Get<CreatureTattoos>(creature).slots[2].design, 3);
	// the same tattoo again changes nothing; a slot past the eighth is not there
	_system.SetTattoo(creature, 2, {.design = 3, .site = 1, .colour = glm::u8vec3(10, 20, 30)});
	_system.SetTattoo(creature, creature_tattoo::k_SlotCount, {.design = 1, .site = 0});
	EXPECT_EQ(registry.Get<CreatureTattoos>(creature).revision, 1u);

	_system.AddWound(creature, {.u = 1, .v = 2, .skin = 0, .age = 0, .type = 1, .column = 2});
	_system.AddBlood(creature, {.u = 3, .v = 4, .skin = 1});
	const auto& marks = registry.Get<CreatureMarks>(creature);
	EXPECT_EQ(marks.revision, 2u);
	EXPECT_EQ(marks.marks.wounds.size(), 1u);
	EXPECT_EQ(marks.marks.blood.size(), 1u);
	// a heal that ages the marks a step paints them again
	_system.Heal(creature, creature_marks::k_CountsPerStep);
	EXPECT_EQ(registry.Get<CreatureMarks>(creature).revision, 3u);
	EXPECT_EQ(registry.Get<CreatureMarks>(creature).marks.wounds.front().age, 1);

	// something that is no creature has none of these
	const auto thing = registry.Create();
	_system.AddWound(thing, {});
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureMarks>(thing));
}

// A bleeding cut starts a trail in the first of the eight free; with all eight running a new cut starts none. A trail
// runs while younger than twelve seconds of the creature's own time, and is then free again
TEST(CreatureBloodTrails, StartInTheFirstFreeAndRunForTwelveSeconds)
{
	using namespace openblack::creature_marks;
	BloodTrails trails {};
	const MeshCut first {.subMesh = 1, .spot = {.primitive = 2, .corners = {3, 4, 5}, .u = 0.25f, .v = 0.5f}};
	StartTrail(trails, first);
	EXPECT_EQ(trails[0], (BloodTrail {.cut = first, .ms = 0}));
	EXPECT_EQ(trails[1].ms, k_TrailRunsMs);

	// a turn of 100 ms runs it, until it reaches twelve seconds
	auto& trail = trails[0];
	trail.ms = k_TrailRunsMs - 100;
	EXPECT_TRUE(RunTrail(trail, 100));
	EXPECT_EQ(trail.ms, k_TrailRunsMs);
	EXPECT_FALSE(RunTrail(trail, 100));
	EXPECT_EQ(trail.ms, k_TrailRunsMs);

	// the first free one is taken again; with every one running, a cut starts none
	for (auto& each : trails)
	{
		each.ms = 50;
	}
	trails[5].ms = k_TrailRunsMs;
	const MeshCut second {.subMesh = 0, .spot = {.primitive = 7}};
	StartTrail(trails, second);
	EXPECT_EQ(trails[5], (BloodTrail {.cut = second, .ms = 0}));
	const auto before = trails;
	StartTrail(trails, first);
	EXPECT_EQ(trails, before);
}

// The system starts a trail on a creature's marks; a running trail on a body with no mesh to run down stops at its
// first step and leaves no blood
TEST_F(CreatureSkinSystemTest, ABleedingCutStartsATrailThatStopsWithNoSkinToRunDown)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	_system.Bleed(creature, {.subMesh = 0, .spot = {.primitive = 1}});
	EXPECT_EQ(registry.Get<CreatureMarks>(creature).trails[0].ms, 0);

	_system.BleedTurn();
	const auto& marks = registry.Get<CreatureMarks>(creature);
	EXPECT_EQ(marks.trails[0].ms, creature_marks::k_TrailRunsMs);
	EXPECT_TRUE(marks.marks.blood.empty());
}

TEST_F(CreatureSkinSystemTest, TattoosPutOnTakeANewBlockOfPicturesAndPaintTheCreatureAgain)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto owner = registry.Get<const Creature>(creature).owner;
	// a creature with no block of pictures makes no masks, but is painted again
	_system.RemakeTattooMasks(creature, creature_tattoo::Sheet::Shipped);
	EXPECT_EQ(registry.Get<CreatureTattoos>(creature).revision, 1u);
	EXPECT_FALSE(registry.Get<CreatureTattoos>(creature).pictures.has_value());

	creature_tattoo::Slots slots {};
	slots[3] = {.design = 5, .site = 2, .colour = glm::u8vec3(1, 2, 3)};
	_system.PutOnTattoos(creature, slots);
	const auto& tattoos = registry.Get<CreatureTattoos>(creature);
	EXPECT_EQ(tattoos.slots, slots);
	EXPECT_EQ(tattoos.revision, 2u);
	// without own pictures the block is its head alone
	ASSERT_TRUE(tattoos.pictures.has_value());
	EXPECT_EQ(tattoos.pictures->size(), 0x28u);
	// the masks are made from the art, which this world has not loaded
	ASSERT_NE(_system.TattooMasks(owner), nullptr);
	EXPECT_FALSE(_system.TattooMasks(owner)->at(3).has_value());
}

TEST_F(CreatureSkinSystemTest, TheTurnsHealTheMarks)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.AddWound(creature, {.type = 1});
	auto& registry = test::creature_world::World::Registry();
	const auto revision = registry.Get<CreatureMarks>(creature).revision;
	// a count a turn: a step of healing every k_CountsPerStep turns
	for (uint32_t turn = 1; turn < creature_marks::k_CountsPerStep; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<CreatureMarks>(creature).revision, revision);
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureMarks>(creature).revision, revision + 1);
	EXPECT_EQ(registry.Get<CreatureMarks>(creature).marks.wounds.front().age, 1);
}

TEST_F(CreatureSkinSystemTest, WithoutThePreloadedFileNothingIsPaintedAndNoFileIsRead)
{
	test::ScopedDefaultFileSystem fileSystem;
	test::creature_block::Block block;
	block.clips = {test::creature_block::Clip {}};
	_world.LoadApeRig(block, {{"move", {"Cstand"}}});
	const auto creature = test::creature_world::World::MakeCreature();

	// with no file system at all, a skin that would be read from disk would fail here
	const test::RestoreService<Locator::filesystem> restoreFiles;
	Locator::filesystem::reset();
	_system.Update();

	const auto& skin = test::creature_world::World::Registry().Get<CreatureSkin>(creature);
	EXPECT_TRUE(skin.skins.empty());
	EXPECT_FALSE(skin.painted.has_value());
	EXPECT_EQ(skin.revision, 0u);
}
