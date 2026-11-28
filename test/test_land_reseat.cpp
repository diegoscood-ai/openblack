/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::land_reseat: after the vortex or the temple flattens the land, the villagers, animals and wood and food piles
// standing on it keep their height above the new ground; everything else stays. Fake grounds, no game data

#include <array>
#include <bit>
#include <memory>
#include <utility>

#include <EDTFile.h>
#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "3D/CameraTrack.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandMovedUnder.h"
#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LandReseat.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::ecs::land_reseat::ReseatedHeight;

namespace
{
/// A walk along a straight two-point track, for the things that only need to be walking one
WalkPath AStraightWalk()
{
	auto track = std::make_shared<edt::EDTTrack>();
	for (auto* way : {&track->position, &track->focus})
	{
		way->points = {std::array {0.0f, 0.0f, 0.0f}, std::array {10.0f, 0.0f, 0.0f}};
		way->handles = {{std::array {10.0f / 3.0f, 0.0f, 0.0f}, std::array {20.0f / 3.0f, 0.0f, 0.0f}}};
		way->times = {0.0f, 1000.0f};
		way->speeds = {10.0f, 10.0f};
		way->duration = 1000;
	}
	auto walk = camera_track::StartWalk(*track, true, 0.0f, 1.0f);
	return {.track = std::move(track), .walk = std::move(walk)};
}

/// The box of corners (10, 10) .. (14, 14): the ground in cells 9 .. 14 reads it
constexpr glm::ivec2 k_Min {10, 10};
constexpr glm::ivec2 k_Max {14, 14};

float FlatGround(glm::vec2)
{
	return 20.0f;
}

/// The land raised by 5 m in cells 10 .. 13, as after a flattening; cells 9 and 14 read the box but kept their ground
float RaisedGround(glm::vec2 xz)
{
	const bool inside = xz.x >= 100.0f && xz.x < 140.0f && xz.y >= 100.0f && xz.y < 140.0f;
	return inside ? 25.0f : 20.0f;
}

bool NeverOff(entt::entity)
{
	return false;
}

entt::entity Make(Registry& registry, glm::vec3 position)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}

entt::entity MakeVillager(Registry& registry, glm::vec3 position)
{
	const auto entity = Make(registry, position);
	registry.Assign<Villager>(entity);
	return entity;
}

entt::entity MakePile(Registry& registry, glm::vec3 position, float sink)
{
	const auto entity = Make(registry, position);
	registry.Assign<Pot>(entity);
	auto& pileSink = registry.Assign<PileSink>(entity, position.y);
	pileSink.offset.SetPosition(sink);
	registry.Get<Transform>(entity).position.y = position.y + sink;
	return entity;
}

/// Records on the flat ground, then re-seats on the raised one
size_t Flatten(Registry& registry)
{
	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, NeverOff);
	return land_reseat::Reseat(registry, grounds, RaisedGround);
}
} // namespace

TEST(LandReseat, UnchangedGroundGivesNothing)
{
	EXPECT_FALSE(ReseatedHeight(12.5f, 10.0f, 10.0f).has_value());
	// bitwise: -0 and +0 are different grounds
	EXPECT_TRUE(ReseatedHeight(1.0f, 0.0f, -0.0f).has_value());
}

TEST(LandReseat, KeepsTheHeightAboveTheGround)
{
	EXPECT_EQ(ReseatedHeight(10.0f, 10.0f, 15.0f), 15.0f); // raised, on the ground
	EXPECT_EQ(ReseatedHeight(12.0f, 10.0f, 4.0f), 6.0f);   // dropped, 2 m above it
}

TEST(LandReseat, TakesTheHeightAboveTheGroundFirst)
{
	// the height above the ground is taken first, then added to the new ground: (y - before) + after, not
	// (y + after) - before, which rounds differently
	const float y = 0.1f;
	const float before = 1000.3f;
	const float after = 1000.7f;
	const float aboveGround = y - before;
	const auto reseated = ReseatedHeight(y, before, after);
	ASSERT_TRUE(reseated.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(*reseated), std::bit_cast<uint32_t>(after + aboveGround));
}

TEST(LandReseat, GroundReadsTheCornersOfItsCell)
{
	// a point in cell 9 reads corners 9 and 10, in cell 14 corners 14 and 15
	EXPECT_TRUE(land_reseat::GroundReadsCorners({95.0f, 120.0f}, k_Min, k_Max));
	EXPECT_TRUE(land_reseat::GroundReadsCorners({145.0f, 145.0f}, k_Min, k_Max));
	EXPECT_FALSE(land_reseat::GroundReadsCorners({85.0f, 120.0f}, k_Min, k_Max));
	EXPECT_FALSE(land_reseat::GroundReadsCorners({120.0f, 155.0f}, k_Min, k_Max));
}

TEST(LandReseat, StandingVillagerAndAnimalFollowTheGround)
{
	Registry registry;
	const auto villager = MakeVillager(registry, {120.0f, 20.0f, 120.0f});
	const auto bird = Make(registry, {121.0f, 30.0f, 121.0f}); // 10 m above the ground
	registry.Assign<Animal>(bird);
	registry.Assign<AnimalBrain>(bird);

	EXPECT_EQ(Flatten(registry), 2u);
	EXPECT_EQ(registry.Get<Transform>(villager).position, glm::vec3(120.0f, 25.0f, 120.0f));
	EXPECT_EQ(registry.Get<Transform>(bird).position, glm::vec3(121.0f, 35.0f, 121.0f));
}

TEST(LandReseat, PileMovesItsSinkBase)
{
	Registry registry;
	const auto pile = MakePile(registry, {120.0f, 20.0f, 120.0f}, -0.5f);

	EXPECT_EQ(Flatten(registry), 1u);
	EXPECT_EQ(registry.Get<PileSink>(pile).baseY, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(pile).position.y, 24.5f);
}

TEST(LandReseat, StaticsStay)
{
	Registry registry;
	const auto abode = Make(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<Abode>(abode);
	const auto tree = Make(registry, {121.0f, 20.0f, 121.0f});
	registry.Assign<Tree>(tree);
	const auto pot = Make(registry, {122.0f, 20.0f, 122.0f}); // a pot at rest has no sink offset
	registry.Assign<Pot>(pot);

	EXPECT_EQ(Flatten(registry), 0u);
	EXPECT_EQ(registry.Get<Transform>(abode).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(tree).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(pot).position.y, 20.0f);
}

// A static is drawn where it was placed, but its next placing (a scale, an angle, a growth) makes it again at its
// height over the land under it then: the land change keeps where it stood and the ground it stood on
TEST(LandReseat, AStaticTakesTheNewGroundAtItsNextPlacing)
{
	Registry registry;
	const auto tree = Make(registry, {121.0f, 20.5f, 121.0f});
	registry.Assign<Tree>(tree);
	const auto outside = Make(registry, {300.0f, 20.0f, 300.0f});
	registry.Assign<Tree>(outside);

	EXPECT_EQ(Flatten(registry), 0u);
	ASSERT_TRUE(registry.AllOf<LandMovedUnder>(tree));
	EXPECT_EQ(registry.Get<LandMovedUnder>(tree).groundBefore, 20.0f);
	EXPECT_FALSE(registry.AllOf<LandMovedUnder>(outside));

	EXPECT_TRUE(land_reseat::PlaceOnTheLandNow(registry, tree, RaisedGround));
	EXPECT_EQ(registry.Get<Transform>(tree).position.y, 25.5f);
	EXPECT_FALSE(registry.AllOf<LandMovedUnder>(tree));
	// placed again with nothing changed under it: it stays
	EXPECT_FALSE(land_reseat::PlaceOnTheLandNow(registry, tree, RaisedGround));
	EXPECT_EQ(registry.Get<Transform>(tree).position.y, 25.5f);
}

// Two land changes before its next placing: its height is over the ground it was first put on
TEST(LandReseat, AStaticKeepsTheGroundItWasPutOnThroughTwoChanges)
{
	Registry registry;
	const auto abode = Make(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<Abode>(abode);

	EXPECT_EQ(Flatten(registry), 0u);
	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, RaisedGround, NeverOff);
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, [](glm::vec2) { return 30.0f; }), 0u);
	EXPECT_EQ(registry.Get<LandMovedUnder>(abode).groundBefore, 20.0f);

	EXPECT_TRUE(land_reseat::PlaceOnTheLandNow(registry, abode, [](glm::vec2) { return 30.0f; }));
	EXPECT_EQ(registry.Get<Transform>(abode).position.y, 30.0f);
}

// A static moved since the land changed was placed on the land where it went: its next placing leaves it there
TEST(LandReseat, AStaticMovedSinceIsLeftWhereItWent)
{
	Registry registry;
	const auto rock = Make(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<MobileStatic>(rock);

	EXPECT_EQ(Flatten(registry), 0u);
	registry.Get<Transform>(rock).position = {150.0f, 22.0f, 150.0f};
	EXPECT_FALSE(land_reseat::PlaceOnTheLandNow(registry, rock, RaisedGround));
	EXPECT_EQ(registry.Get<Transform>(rock).position.y, 22.0f);
	EXPECT_FALSE(registry.AllOf<LandMovedUnder>(rock));
}

// A miracle dispenser and a teleport stone stay where they stand, but their hand points are at their map place, its
// altitude the one they were put at: over the new ground once the land changed under them
TEST(LandReseat, ADispenserAndAStoneKeepTheAltitudeTheyWerePutAt)
{
	Registry registry;
	const auto dispenser = Make(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<SpellDispenser>(dispenser);
	const auto stone = Make(registry, {121.0f, 21.5f, 121.0f});
	registry.Assign<TeleportStone>(stone);
	EXPECT_FALSE(land_reseat::KeptAltitude(registry, stone, {121.0f, 21.5f, 121.0f}).has_value());

	EXPECT_EQ(Flatten(registry), 0u);
	EXPECT_EQ(registry.Get<Transform>(dispenser).position.y, 20.0f);
	EXPECT_EQ(land_reseat::KeptAltitude(registry, dispenser, {120.0f, 20.0f, 120.0f}), 0.0f);
	EXPECT_EQ(land_reseat::KeptAltitude(registry, stone, {121.0f, 21.5f, 121.0f}), 1.5f);
	// moved since: placed on the land where it went
	EXPECT_FALSE(land_reseat::KeptAltitude(registry, stone, {130.0f, 21.5f, 121.0f}).has_value());
}

// A town's totem is drawn on the ground at its plinth's point every frame: the plinth, its icon and the second totem
// follow the new ground, and it rises from its rest height there
TEST(LandReseat, ATownsTotemFollowsTheGroundWithItsIconAndItsSecondTotem)
{
	Registry registry;
	const auto plinth = Make(registry, {120.0f, 21.0f, 120.0f}); // risen 1 m with its worship
	const auto icon = Make(registry, {120.0f, 24.0f, 120.0f});
	const auto ghost = Make(registry, {120.0f, 20.5f, 120.0f});
	const auto ghostIcon = Make(registry, {120.0f, 23.5f, 120.0f});
	auto& totem = registry.Assign<VillageTotem>(plinth);
	totem.icon = icon;
	totem.ghost = ghost;
	totem.ghostIcon = ghostIcon;
	totem.restY = 20.0f;
	totem.rise = 1.0f;

	EXPECT_EQ(Flatten(registry), 1u);
	EXPECT_EQ(registry.Get<Transform>(plinth).position.y, 26.0f);
	EXPECT_EQ(registry.Get<VillageTotem>(plinth).restY, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(icon).position.y, 29.0f);
	EXPECT_EQ(registry.Get<Transform>(ghost).position.y, 25.5f);
	EXPECT_EQ(registry.Get<Transform>(ghostIcon).position.y, 28.5f);
}

// A spell icon drawn as built is drawn on the ground at its point every frame, and follows the new ground; one whose
// worship site is going up is drawn as a building where it was placed, and stays
TEST(LandReseat, ABuiltSpellIconFollowsTheGroundAndOneGoingUpStays)
{
	Registry registry;
	const auto townIcon = Make(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<SpellIcon>(townIcon);
	const auto builtSite = Make(registry, {125.0f, 20.0f, 125.0f});
	registry.Assign<CitadelPartBuild>(builtSite);
	const auto siteIcon = Make(registry, {121.0f, 20.0f, 121.0f});
	registry.Assign<SpellIcon>(siteIcon);
	registry.Assign<WorshipSpellIcon>(siteIcon).site = builtSite;
	const auto goingUp = Make(registry, {126.0f, 20.0f, 126.0f});
	registry.Assign<CitadelPartBuild>(
	    goingUp, CitadelPartBuild {.buildFlags = CitadelPartBuild::k_UnderConstruction, .percentBuilt = 0.5f});
	const auto unbuiltIcon = Make(registry, {122.0f, 20.0f, 122.0f});
	registry.Assign<SpellIcon>(unbuiltIcon);
	registry.Assign<WorshipSpellIcon>(unbuiltIcon).site = goingUp;

	EXPECT_EQ(Flatten(registry), 2u);
	EXPECT_EQ(registry.Get<Transform>(townIcon).position.y, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(siteIcon).position.y, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(unbuiltIcon).position.y, 20.0f);
	// once its site is built it is drawn on the ground: it takes the new ground then
	EXPECT_TRUE(land_reseat::PlaceOnTheLandNow(registry, unbuiltIcon, RaisedGround));
	EXPECT_EQ(registry.Get<Transform>(unbuiltIcon).position.y, 25.0f);
}

// A fragment lying on the land is drawn from its map place, on the ground under it, every frame: it follows the new
// ground; one in flight is left to its flight
TEST(LandReseat, AFragmentAtRestFollowsTheGround)
{
	Registry registry;
	const auto lying = Make(registry, {120.0f, 20.5f, 120.0f});
	registry.Assign<Fragment>(lying);
	const auto flying = Make(registry, {121.0f, 30.0f, 121.0f});
	registry.Assign<Fragment>(flying);

	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround,
	                                                [flying](entt::entity thing) { return thing == flying; });
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 1u);
	EXPECT_EQ(registry.Get<Transform>(lying).position.y, 25.5f);
	EXPECT_EQ(registry.Get<Transform>(flying).position.y, 30.0f);
}

TEST(LandReseat, ACreatureKeepsItsHeightUntilItsNextStep)
{
	// Unlike the villagers, a creature's body keeps the height its walk gave it: its next step takes the new ground's
	Registry registry;
	const auto creature = Make(registry, {120.0f, 21.0f, 120.0f});
	registry.Assign<Creature>(creature);
	registry.Assign<DrawPosition>(creature).turnStart = {119.0f, 21.0f, 119.0f};
	auto& walk = registry.Assign<CreatureLocomotion>(creature);
	walk.fromPosition = {119.0f, 21.0f, 119.0f};
	walk.toPosition = {120.0f, 21.0f, 120.0f};

	EXPECT_EQ(Flatten(registry), 0u);
	EXPECT_EQ(registry.Get<Transform>(creature).position, glm::vec3(120.0f, 21.0f, 120.0f));
	EXPECT_EQ(registry.Get<DrawPosition>(creature).turnStart, glm::vec3(119.0f, 21.0f, 119.0f));
	EXPECT_EQ(registry.Get<CreatureLocomotion>(creature).toPosition.y, 21.0f);
}

TEST(LandReseat, WalkersFollowTheGround)
{
	Registry registry;
	const auto pathWalker = MakeVillager(registry, {120.0f, 20.0f, 120.0f});
	registry.Assign<MoveStateLinearTag>(pathWalker);
	const auto trackWalker = MakeVillager(registry, {121.0f, 20.0f, 121.0f});
	registry.Assign<LivingWalkPath>(trackWalker, LivingWalkPath {.path = AStraightWalk()});
	const auto movingAnimal = Make(registry, {122.0f, 20.0f, 122.0f});
	registry.Assign<Animal>(movingAnimal);
	registry.Assign<AnimalBrain>(movingAnimal).movedLastTurn = 0.3f;

	EXPECT_EQ(Flatten(registry), 3u);
	EXPECT_EQ(registry.Get<Transform>(pathWalker).position.y, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(trackWalker).position.y, 25.0f);
	EXPECT_EQ(registry.Get<Transform>(movingAnimal).position.y, 25.0f);
}

TEST(LandReseat, ThingsOffTheGroundAreLeft)
{
	Registry registry;
	const auto carried = MakeVillager(registry, {122.0f, 20.0f, 122.0f});
	registry.Assign<CarriedByTornado>(carried);
	const auto gone = MakeVillager(registry, {123.0f, 20.0f, 123.0f});
	registry.Assign<Unavailable>(gone);
	const auto held = MakeVillager(registry, {124.0f, 20.0f, 124.0f});

	const auto grounds =
	    land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, [held](entt::entity thing) { return thing == held; });
	EXPECT_TRUE(grounds.empty());
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 0u);
	EXPECT_EQ(registry.Get<Transform>(carried).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(gone).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(held).position.y, 20.0f);
}

TEST(LandReseat, ThingsOutsideTheBoxOrOnUnchangedGroundStay)
{
	Registry registry;
	const auto far = MakeVillager(registry, {300.0f, 20.0f, 300.0f});
	// in a cell that reads the box, but whose ground did not change
	const auto edge = MakeVillager(registry, {145.0f, 20.0f, 120.0f});

	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, NeverOff);
	ASSERT_EQ(grounds.size(), 1u);
	EXPECT_EQ(grounds.front().thing, edge);
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 0u);
	EXPECT_EQ(registry.Get<Transform>(far).position.y, 20.0f);
	EXPECT_EQ(registry.Get<Transform>(edge).position.y, 20.0f);
}

TEST(LandReseat, ThingsThatWentAwayAreSkipped)
{
	Registry registry;
	const auto villager = MakeVillager(registry, {120.0f, 20.0f, 120.0f});
	const auto grounds = land_reseat::RecordGrounds(registry, k_Min, k_Max, FlatGround, NeverOff);
	registry.Destroy(villager);
	EXPECT_EQ(land_reseat::Reseat(registry, grounds, RaisedGround), 0u);
}
