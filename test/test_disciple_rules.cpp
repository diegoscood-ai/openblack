/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Alignment.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Effects/Alignment.h"

using namespace openblack;
using namespace openblack::disciple;

namespace
{
using enum VillagerDisciple;

constexpr auto k_Me = PlayerNames::PLAYER_ONE;
constexpr auto k_Them = PlayerNames::PLAYER_TWO;

/// One of my adults held in my hand
HeldFacts MyAdult()
{
	return {.hand = k_Me, .villager = k_Me, .sexuallyActive = true, .male = true};
}

/// A built, whole building of one of my towns
NearFacts MyBuilding(NearKind kind)
{
	return {.kind = kind, .owner = k_Me, .hasTown = true, .heldVillagersTown = true};
}

/// The same of their town
NearFacts TheirBuilding(NearKind kind)
{
	return {.kind = kind, .owner = k_Them, .hasTown = true};
}
} // namespace

TEST(DiscipleRules, InterestFallsAwayToNothingBySixMetres)
{
	// Touching, a thing draws as much as its kind
	EXPECT_NEAR(Interest(0.0f, 1.0f), 1.0f, 1e-3f);
	// Overlapping counts as touching
	EXPECT_FLOAT_EQ(Interest(-2.0f, 1.0f), Interest(0.0f, 1.0f));
	EXPECT_LT(Interest(3.0f, 1.0f), Interest(1.0f, 1.0f));
	// At six metres and beyond, too little to look at
	EXPECT_LT(Interest(6.0f, 1.5f), k_LeastInterest);
	EXPECT_LT(Interest(20.0f, 1.5f), k_LeastInterest);
	// A kind that draws villagers more draws them more at the same gap
	EXPECT_GT(Interest(1.0f, 1.5f), Interest(1.0f, 0.9f));
}

TEST(DiscipleRules, MostInterestingFirstTiesInTheOrderFound)
{
	const std::array<float, 5> interests {0.5f, 0.9f, 0.0001f, 0.9f, 0.25f};
	EXPECT_EQ(OrderByInterest(interests), (std::vector<size_t> {1, 3, 0, 4}));
}

TEST(DiscipleRules, FieldsMakeFarmersOrBuilders)
{
	auto field = MyBuilding(NearKind::Field);
	EXPECT_EQ(Offered(field, MyAdult()), Farmer);
	field.built = false;
	EXPECT_EQ(Offered(field, MyAdult()), Builder);
	// Anyone's field farms, but only the player's own people
	EXPECT_EQ(Offered(TheirBuilding(NearKind::Field), MyAdult()), Farmer);
	EXPECT_EQ(Offered(MyBuilding(NearKind::Field), {.hand = k_Me, .villager = k_Them}), None);
}

TEST(DiscipleRules, TreesForestsAndFishFarms)
{
	EXPECT_EQ(Offered({.kind = NearKind::Tree}, MyAdult()), Forester);
	EXPECT_EQ(Offered({.kind = NearKind::Forest}, MyAdult()), Forester);
	EXPECT_EQ(Offered({.kind = NearKind::FishFarm}, MyAdult()), Fisherman);
	EXPECT_EQ(Offered({.kind = NearKind::Tree}, {.hand = k_Me, .villager = std::nullopt}), None);
}

TEST(DiscipleRules, OwnBuildingsAreBuiltUpAndOthersPreachedTo)
{
	auto house = MyBuilding(NearKind::Building);
	EXPECT_EQ(Offered(house, MyAdult()), None);
	house.repaired = false;
	EXPECT_EQ(Offered(house, MyAdult()), Builder);
	EXPECT_EQ(Offered(TheirBuilding(NearKind::Building), MyAdult()), Missionary);
	// A neutral town is another player's as much as an enemy's
	EXPECT_EQ(Offered({.kind = NearKind::Building, .owner = PlayerNames::NEUTRAL, .hasTown = true}, MyAdult()), Missionary);
	// A building of no town, or of a town no one can live in, offers nothing
	EXPECT_EQ(Offered({.kind = NearKind::Building, .owner = PlayerNames::NEUTRAL}, MyAdult()), None);
	auto empty = TheirBuilding(NearKind::Building);
	empty.townHabitable = false;
	EXPECT_EQ(Offered(empty, MyAdult()), None);
}

TEST(DiscipleRules, StoragePitsTradeWorkshopsCraft)
{
	EXPECT_EQ(Offered(TheirBuilding(NearKind::StoragePit), MyAdult()), Trader);
	EXPECT_EQ(Offered(MyBuilding(NearKind::StoragePit), MyAdult()), None);
	EXPECT_EQ(Offered(MyBuilding(NearKind::Workshop), MyAdult()), Craftsman);
	EXPECT_EQ(Offered(TheirBuilding(NearKind::Workshop), MyAdult()), Missionary);
	auto unfinished = MyBuilding(NearKind::Workshop);
	unfinished.built = false;
	EXPECT_EQ(Offered(unfinished, MyAdult()), Builder);
}

TEST(DiscipleRules, HousesWithRoomTakeInOthers)
{
	auto house = MyBuilding(NearKind::House);
	house.roomForAdults = true;
	EXPECT_EQ(Offered(house, MyAdult()), ChangeHouse);
	house.heldVillagersHome = true;
	EXPECT_EQ(Offered(house, MyAdult()), None);
	house.heldVillagersHome = false;
	house.roomForAdults = false;
	EXPECT_EQ(Offered(house, MyAdult()), None);
	// Even another player's villager moves into one of the player's houses
	house.roomForAdults = true;
	EXPECT_EQ(Offered(house, {.hand = k_Me, .villager = k_Them}), ChangeHouse);
}

TEST(DiscipleRules, AnotherTownsCentreTakesInWhenThereIsRoom)
{
	auto centre = MyBuilding(NearKind::TownCentre);
	EXPECT_EQ(Offered(centre, MyAdult()), None);
	centre.heldVillagersTown = false;
	EXPECT_EQ(Offered(centre, MyAdult()), None);
	centre.townHasRoom = true;
	EXPECT_EQ(Offered(centre, MyAdult()), ChangeHouse);
	// Another player's centre is preached to
	EXPECT_EQ(Offered(TheirBuilding(NearKind::TownCentre), MyAdult()), Missionary);
}

TEST(DiscipleRules, OwnWorshipSiteMakesWorshippers)
{
	auto site = MyBuilding(NearKind::WorshipSite);
	EXPECT_EQ(Offered(site, MyAdult()), None);
	site.heldVillagersWorshipSite = true;
	EXPECT_EQ(Offered(site, MyAdult()), Worship);
	site.built = false;
	EXPECT_EQ(Offered(site, MyAdult()), Builder);
}

TEST(DiscipleRules, BreedersByOneOfTheOtherSex)
{
	NearFacts woman {.kind = NearKind::Villager,
	                 .owner = k_Me,
	                 .villagerFunctional = true,
	                 .villagerSexuallyActive = true,
	                 .villagerMale = false};
	EXPECT_EQ(Offered(woman, MyAdult()), Breeder);
	auto man = woman;
	man.villagerMale = true;
	EXPECT_EQ(Offered(man, MyAdult()), None);
	auto old = woman;
	old.villagerSexuallyActive = false;
	EXPECT_EQ(Offered(old, MyAdult()), None);
	auto theirs = woman;
	theirs.owner = k_Them;
	EXPECT_EQ(Offered(theirs, MyAdult()), None);
}

TEST(DiscipleRules, FirstOfferTakenAndOldWorkKeptWithNone)
{
	const std::array<NearFacts, 3> ordered {MyBuilding(NearKind::Building), NearFacts {.kind = NearKind::Tree},
	                                        MyBuilding(NearKind::Field)};
	const auto choice = ChooseWhileHeld(ordered, MyAdult(), None);
	EXPECT_EQ(choice.type, Forester);
	EXPECT_EQ(choice.from, 1u);
	const auto nothing = ChooseWhileHeld(std::span(ordered).first(1), MyAdult(), Builder);
	EXPECT_EQ(nothing.type, Builder);
	EXPECT_FALSE(nothing.from.has_value());
}

TEST(DiscipleRules, ChildrenOnlyWorship)
{
	auto site = MyBuilding(NearKind::WorshipSite);
	site.heldVillagersWorshipSite = true;
	const std::array<NearFacts, 2> ordered {MyBuilding(NearKind::Field), site};
	auto child = MyAdult();
	child.child = true;
	EXPECT_EQ(ChooseWhileHeld(ordered, child, None).type, Worship);
	EXPECT_EQ(ChooseWhileHeld(std::span(ordered).first(1), child, None).type, None);
	// An adult a script holds takes up nothing, not even worship
	auto held = MyAdult();
	held.otherwiseRefused = true;
	EXPECT_EQ(ChooseWhileHeld(ordered, held, None).type, None);
	// An adult free to choose takes the field
	EXPECT_EQ(ChooseWhileHeld(ordered, MyAdult(), None).type, Farmer);
}

TEST(DiscipleRules, MissionariesPickedUpStopPreaching)
{
	EXPECT_EQ(AtPickUp(Missionary), None);
	EXPECT_EQ(AtPickUp(Farmer), Farmer);
}

TEST(DiscipleRules, Symbols)
{
	EXPECT_EQ(SymbolCell(Missionary), 32u);
	EXPECT_EQ(SymbolCell(Fisherman), 38u);
	EXPECT_EQ(SymbolCell(ChangeHouse), 40u);
	EXPECT_EQ(SymbolCell(Trader), SymbolCell(Protection));
	EXPECT_FALSE(SymbolCell(Worship).has_value());
}

TEST(DiscipleRules, TheSymbolIsYellow)
{
	EXPECT_EQ(k_SymbolColour, 0xFFFFFF00u);
	EXPECT_EQ(TintOf(k_SymbolColour), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
	// The bytes are alpha, red, green, blue: the glow's half-bright orange reads as it is drawn
	EXPECT_EQ(TintOf(0x80C18119u), glm::vec4(193.0f / 255.0f, 129.0f / 255.0f, 25.0f / 255.0f, 128.0f / 255.0f));
}

TEST(DiscipleRules, GlowAndSymbolStandOverTheDrawnVillager)
{
	// The glow is over the villager where it is drawn this frame, raised by its size; nothing else moves it
	const glm::vec3 drawn {1234.5f, 17.25f, -88.125f};
	const float size = 0.75f;
	const auto glow = GlowPoint(drawn, size);
	EXPECT_EQ(glow.x, drawn.x);
	EXPECT_EQ(glow.y, 18.0f);
	EXPECT_EQ(glow.z, drawn.z);
	// The symbol is two and a half sizes over the glow, added to the glow's height as it stands
	const auto symbol = SymbolPoint(glow, size);
	EXPECT_EQ(symbol.x, drawn.x);
	EXPECT_EQ(symbol.y, 19.875f);
	EXPECT_EQ(symbol.z, drawn.z);
	// The rise goes on the glow's rounded height, which can land a step lower than one sum of both rises from the villager
	const auto rounded = GlowPoint({0.0f, 29.292348861694336f, 0.0f}, 1.3f);
	EXPECT_EQ(rounded.y, 30.592348098754883f);
	EXPECT_EQ(SymbolPoint(rounded, 1.3f).y, 33.84234619140625f);
}

TEST(DiscipleRules, AlignmentFollowsWhatTheTownWants)
{
	EXPECT_FLOAT_EQ(AlignmentForMaking(0.8f, 0.2f, 0.6f, 0.05f), 0.05f);
	EXPECT_FLOAT_EQ(AlignmentForMaking(0.1f, 0.2f, 0.6f, 0.05f), -0.05f);
	EXPECT_FLOAT_EQ(AlignmentForMaking(0.4f, 0.2f, 0.6f, 0.05f), 0.0f);
	EXPECT_FLOAT_EQ(AlignmentForMaking(std::nullopt, 0.2f, 0.6f, 0.05f), 0.0f);
	// Leaning good, more good counts for less and evil for more
	const ecs::components::Alignment good {.value = 0.5f};
	const ecs::components::Alignment evil {.value = -0.5f};
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(good, 0.1f), 0.075f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(good, -0.1f), -0.125f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(evil, -0.1f), -0.075f);
}
