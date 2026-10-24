/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The crossing file's lines (ECS/VortexSaveLines.h; raffclar's cases, PR #382): each kind of thing a way out of a land
// takes is written as the land-script command that makes it again, placed relative to the vortex, in the land scripts'
// own words

#include <cmath>

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/VortexSaveLines.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::vortex::crossing;
namespace lines = openblack::ecs::vortex_save_lines;

namespace
{
map_coords::MapCoords Offset(float x, float z, float altitude = 0.0f)
{
	map_coords::MapCoords coords;
	coords.x = map_coords::ToFixed(x);
	coords.z = map_coords::ToFixed(z);
	coords.altitude = altitude;
	return coords;
}

lines::World FakeWorld()
{
	return {
	    .turn = 0,
	    .villagerRow = [](entt::entity) { return 7; },
	    .potResource = [](PotInfo) { return 1; },
	    .life = [](entt::entity) { return 0.5f; },
	    .player = [](entt::entity) { return std::optional<PlayerNames> {}; },
	    .ofAStoragePit = [](entt::entity) { return false; },
	    .magicName = [](SpellSeedType, int) { return std::string("MAGIC_TYPE_TELEPORT"); },
	    .placeOf = [](entt::entity) { return map_coords::MapCoords {}; },
	    .lyingOnTheLand = [](entt::entity) { return true; },
	};
}

class VortexSaveLines: public ::testing::Test
{
protected:
	entt::entity Thing()
	{
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return thing;
	}

	std::optional<Line> LineOf(entt::entity thing, const map_coords::MapCoords& place)
	{
		return lines::LineFor(registry, thing, place, k_Vortex, FakeWorld());
	}

	const map_coords::MapCoords k_Vortex = Offset(100.0f, 200.0f);
	ecs::Registry registry;
};
} // namespace

TEST_F(VortexSaveLines, AVillagerOfATribeIsWrittenByItsTribeAndJob)
{
	const auto villager = Thing();
	auto& data = registry.Assign<Villager>(villager);
	data.tribe = Tribe::CELTIC;
	data.number = VillagerNumber::Farmer;
	data.town = entt::null;
	data.birthTurn = 0;
	const auto line = LineOf(villager, Offset(110.0f, 220.0f));
	ASSERT_TRUE(line.has_value());
	const auto* written = std::get_if<VillagerLine>(&*line);
	ASSERT_NE(written, nullptr);
	// it left its town as it was taken
	EXPECT_EQ(written->town, -1);
	EXPECT_EQ(written->villagerType, "CELTIC_FARMER");
	EXPECT_EQ(lines::Text(*line), "CREATE_TOWN_VILLAGER(-1, \"10.00,20.00\", CELTIC_FARMER, 0)\n");
}

TEST_F(VortexSaveLines, AVillagerOfNoTribeIsWrittenByItsRow)
{
	const auto villager = Thing();
	auto& data = registry.Assign<Villager>(villager);
	data.tribe = Tribe::NONE;
	data.town = entt::null;
	const auto line = LineOf(villager, Offset(100.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line), "CREATE_SPECIAL_TOWN_VILLAGER(-1, \"0.00,0.00\", 7, 0)\n");
}

TEST_F(VortexSaveLines, AnAnimalNamesNoFlockOnceItHasLeftIt)
{
	const auto cow = Thing();
	auto& data = registry.Assign<Animal>(cow);
	data.type = AnimalInfo::Cow;
	data.age = 2;
	data.flock = entt::null;
	const auto line = LineOf(cow, Offset(95.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line),
	          "CREATE_NEW_ANIMAL(\"-5.00,0.00\", " + std::to_string(static_cast<int>(AnimalInfo::Cow)) + ", -1, -1, 2)\n");
}

TEST_F(VortexSaveLines, APotIsWrittenWithWhatItHolds)
{
	const auto pot = Thing();
	auto& data = registry.Assign<Pot>(pot);
	data.type = PotInfo::HandFood;
	data.amount = 1000;
	const auto line = LineOf(pot, Offset(110.0f, 190.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line),
	          "CREATE_POT(\"10.00,-10.00\", " + std::to_string(static_cast<int>(PotInfo::HandFood)) + ", 1, 1000)\n");
}

TEST_F(VortexSaveLines, AStorageSPitsPileStaysWithIt)
{
	const auto pot = Thing();
	registry.Assign<Pot>(pot).type = PotInfo::HandFood;
	auto world = FakeWorld();
	world.ofAStoragePit = [](entt::entity) { return true; };
	EXPECT_FALSE(lines::LineFor(registry, pot, Offset(100.0f, 200.0f), k_Vortex, world).has_value());
}

TEST_F(VortexSaveLines, AStaticIsWrittenAtItsHeightAboveTheLand)
{
	const auto rock = Thing();
	registry.Assign<MobileStatic>(rock).type = MobileStaticInfo::Rock;
	registry.Get<Transform>(rock).scale = glm::vec3(2.0f);
	const auto line = LineOf(rock, Offset(100.0f, 200.0f, 0.5f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line), "CREATE_MOBILE_STATIC(\"0.00,0.00\", " +
	                                  std::to_string(static_cast<int>(MobileStaticInfo::Rock)) +
	                                  ", 0.500000, 0.000000, 0.000000, 0.000000, 2.000000)\n");
}

TEST_F(VortexSaveLines, APieceKnockedOffABuildingIsWrittenAsARock)
{
	const auto piece = Thing();
	registry.Assign<Fragment>(piece);
	const auto line = LineOf(piece, Offset(102.0f, 200.0f, 0.25f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line), "CREATE_MOBILE_STATIC(\"2.00,0.00\", " +
	                                  std::to_string(static_cast<int>(MobileStaticInfo::Rock)) +
	                                  ", 0.250000, 0.000000, 0.000000, 0.000000, 1.000000)\n");
}

TEST_F(VortexSaveLines, AScaffoldIsWrittenWithItsTownAsAScaffold)
{
	const auto town = registry.Create();
	registry.Assign<Town>(town).id = 4;
	const auto scaffold = Thing();
	registry.Assign<MobileObject>(scaffold).type = MobileObjectInfo::OldScaffold;
	registry.Assign<Scaffold>(scaffold).town = town;
	const auto line = LineOf(scaffold, Offset(101.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line), "CREATE_SCAFFOLD(4, \"1.00,0.00\", 0, 0, 1000)\n");
	// one of no town names none
	registry.Get<Scaffold>(scaffold).town = entt::null;
	const auto alone = LineOf(scaffold, Offset(100.0f, 200.0f));
	ASSERT_TRUE(alone.has_value());
	EXPECT_EQ(lines::Text(*alone), "CREATE_SCAFFOLD(-1, \"0.00,0.00\", 0, 0, 1000)\n");
}

TEST_F(VortexSaveLines, ATownIsNamedByTheNumberTheScriptsFindItBy)
{
	const auto town = registry.Create();
	auto& data = registry.Assign<Town>(town);
	data.id = 4;
	// a town a scaffold founded is found by the founded towns' number, not by its own
	data.scriptId = 0xABA52u;
	const auto scaffold = Thing();
	registry.Assign<Scaffold>(scaffold).town = town;
	const auto line = LineOf(scaffold, Offset(100.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(std::get<ScaffoldLine>(*line).town, 0xABA52);
}

TEST_F(VortexSaveLines, AThingLyingAboutIsWrittenInThousandths)
{
	EXPECT_EQ(lines::Text(MobileObjectLine {.offset = Offset(0.0f, 0.0f), .row = 6, .yAngle = 1.2345f, .scale = 1.0f}),
	          "CREATE_MOBILEOBJECT(\"0.00,0.00\", 6, 1234, 1000)\n");
}

TEST_F(VortexSaveLines, ATreeAndADeadTreeAreWritten)
{
	EXPECT_EQ(lines::Text(TreeLine {.offset = Offset(0.0f, 0.0f), .row = 4, .yAngle = 1.5f, .scale = 0.8f, .maxScale = 1.0f}),
	          "CREATE_NEW_TREE(-1, \"0.00,0.00\", 4, 0, 1.500000, 0.800000, 1.000000)\n");
	EXPECT_EQ(lines::Text(
	              DeadTreeLine {.offset = Offset(0.0f, 0.0f), .player = "PLAYER_ONE", .row = 2, .life = 1.0f, .yAngle = 0.5f}),
	          "CREATE_DEAD_TREE(\"0.00,0.00\", \"PLAYER_ONE\", 2, 1.000000, 0.000000, 0.500000, 0.000000)\n");
}

TEST_F(VortexSaveLines, AnythingElseIsNotWritten)
{
	EXPECT_FALSE(LineOf(Thing(), Offset(100.0f, 200.0f)).has_value());
	EXPECT_FALSE(LineOf(registry.Create(), Offset(100.0f, 200.0f)).has_value());
}

TEST_F(VortexSaveLines, APlayersArtefactStaysOneOfNoTown)
{
	const auto rock = Thing();
	registry.Assign<MobileStatic>(rock).type = MobileStaticInfo::Rock;
	auto& artefact = registry.Assign<TownArtefact>(rock);
	artefact.player = PlayerNames::PLAYER_ONE;
	artefact.value = 2.5f;
	const auto line = LineOf(rock, Offset(100.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line), "CREATE_MOBILE_STATIC(\"0.00,0.00\", " +
	                                  std::to_string(static_cast<int>(MobileStaticInfo::Rock)) +
	                                  ", 0.000000, 0.000000, 0.000000, 0.000000, 1.000000)\n"
	                                  "MAKE_LAST_OBJECT_ARTIFACT(-1, \"PLAYER_ONE\", 2.500000)\n");
	// nobody's artefact is a plain static
	registry.Get<TownArtefact>(rock).player.reset();
	const auto plain = LineOf(rock, Offset(100.0f, 200.0f));
	ASSERT_TRUE(plain.has_value());
	EXPECT_FALSE(std::get<MobileStaticLine>(*plain).artefact.has_value());
}

TEST_F(VortexSaveLines, AGlobeIsWrittenByItsMiracle)
{
	const auto globe = Thing();
	registry.Assign<OneOffSpellSeed>(globe);
	const auto line = LineOf(globe, Offset(110.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(lines::Text(*line), "CREATE_ONE_SHOT_SPELL_PU(\"10.00,0.00\", \"MAGIC_TYPE_TELEPORT\")\n");
}

TEST_F(VortexSaveLines, StaticsOfAKindLyingJustThereCountAsWrittenWithIt)
{
	const auto rock = Thing();
	registry.Assign<MobileStatic>(rock).type = MobileStaticInfo::Rock;
	const auto fellow = Thing();
	registry.Assign<MobileStatic>(fellow).type = MobileStaticInfo::Rock;
	const auto other = Thing();
	registry.Assign<MobileStatic>(other).type = MobileStaticInfo::Bonfire;
	const auto away = Thing();
	registry.Assign<MobileStatic>(away).type = MobileStaticInfo::Rock;
	auto world = FakeWorld();
	world.placeOf = [away](entt::entity thing) { return thing == away ? Offset(5.0f, 0.0f) : Offset(0.0f, 0.0f); };
	EXPECT_EQ(lines::IdenticalStatics(registry, rock, world), std::vector<entt::entity> {fellow});
	// not one that is not lying on the land
	world.lyingOnTheLand = [fellow](entt::entity thing) { return thing != fellow; };
	EXPECT_TRUE(lines::IdenticalStatics(registry, rock, world).empty());
}

TEST_F(VortexSaveLines, ThingsLyingAboutOfAKindJustThereCountAsWrittenWithIt)
{
	const auto barrel = Thing();
	registry.Assign<MobileObject>(barrel).type = MobileObjectInfo::EgyptBarrel;
	const auto fellow = Thing();
	registry.Assign<MobileObject>(fellow).type = MobileObjectInfo::EgyptBarrel;
	const auto other = Thing();
	registry.Assign<MobileObject>(other).type = MobileObjectInfo::EgyptCart;
	const auto higher = Thing();
	registry.Assign<MobileObject>(higher).type = MobileObjectInfo::EgyptBarrel;
	// a scaffold of the same row is no fellow: it has its own line
	const auto scaffold = Thing();
	registry.Assign<MobileObject>(scaffold).type = MobileObjectInfo::EgyptBarrel;
	registry.Assign<Scaffold>(scaffold);
	auto world = FakeWorld();
	world.placeOf = [higher](entt::entity thing) { return thing == higher ? Offset(0.0f, 0.0f, 1.0f) : Offset(0.0f, 0.0f); };
	EXPECT_EQ(lines::IdenticalMobileObjects(registry, barrel, world), std::vector<entt::entity> {fellow});
	EXPECT_TRUE(lines::IdenticalMobileObjects(registry, scaffold, world).empty());
	// not one that is not lying on the land
	world.lyingOnTheLand = [fellow](entt::entity thing) { return thing != fellow; };
	EXPECT_TRUE(lines::IdenticalMobileObjects(registry, barrel, world).empty());
}

TEST_F(VortexSaveLines, ALeaningStaticIsWrittenWithTheAnglesItKeeps)
{
	// a rock that leans: its turn is made from its angles about y, then x, then z
	const auto rock = Thing();
	registry.Assign<MobileStatic>(rock).type = MobileStaticInfo::Rock;
	const glm::vec3 kept {0.3f, 1.2f, -0.4f};
	registry.Get<Transform>(rock).rotation = affine::RotationYXZ(kept.y, kept.x, kept.z);
	ecs::placed_turn::KeepAngles(registry, rock, kept);
	const auto line = LineOf(rock, Offset(100.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	const auto& written = std::get<MobileStaticLine>(*line);
	EXPECT_EQ(written.xAngle, kept.x);
	EXPECT_EQ(written.yAngle, kept.y);
	EXPECT_EQ(written.zAngle, kept.z);
	// the angles taken apart from its turn in the other order are not the ones it keeps
	const auto decomposed = openblack::script::property_rules::PlacedAngles(registry.Get<Transform>(rock).rotation);
	EXPECT_GT(std::abs(decomposed.x - kept.x) + std::abs(decomposed.y - kept.y) + std::abs(decomposed.z - kept.z), 0.01f);
}

TEST_F(VortexSaveLines, ADeadTreeIsWrittenAsThePlayersItIs)
{
	const auto dead = Thing();
	registry.Assign<DeadTree>(dead).owner = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(lines::OwnerOf(registry, dead), PlayerNames::PLAYER_TWO);
	// the line names the player the game finds for it
	auto world = FakeWorld();
	world.player = [this](entt::entity thing) { return lines::OwnerOf(registry, thing); };
	const auto line = lines::LineFor(registry, dead, Offset(100.0f, 200.0f), k_Vortex, world);
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(std::get<DeadTreeLine>(*line).player, "PLAYER_TWO");
	// a felled tree or a dropped log is the neutral player's
	registry.Get<DeadTree>(dead).owner = PlayerNames::NEUTRAL;
	EXPECT_EQ(lines::OwnerOf(registry, dead), PlayerNames::NEUTRAL);
	// anything else belongs to nobody
	const auto rock = Thing();
	registry.Assign<MobileStatic>(rock).type = MobileStaticInfo::Rock;
	EXPECT_FALSE(lines::OwnerOf(registry, rock).has_value());
	EXPECT_FALSE(lines::OwnerOf(registry, entt::null).has_value());
}

TEST_F(VortexSaveLines, ALeaningDeadTreeIsWrittenWithTheAnglesItKeeps)
{
	const auto dead = Thing();
	registry.Assign<DeadTree>(dead);
	const glm::vec3 kept {-0.25f, 2.5f, 0.6f};
	registry.Get<Transform>(dead).rotation = affine::RotationYXZ(kept.y, kept.x, kept.z);
	ecs::placed_turn::KeepAngles(registry, dead, kept);
	const auto line = LineOf(dead, Offset(100.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	const auto& written = std::get<DeadTreeLine>(*line);
	EXPECT_EQ(written.xAngle, kept.x);
	EXPECT_EQ(written.yAngle, kept.y);
	EXPECT_EQ(written.zAngle, kept.z);
}

TEST_F(VortexSaveLines, AnUprightThingIsWrittenWithItsAngleAsItWasGivenNotWrapped)
{
	// 4 radians is past a half turn: it is written as it was given, not brought back to -2.28
	constexpr float k_Angle = 4.0f;
	const auto tree = Thing();
	registry.Assign<Tree>(tree);
	registry.Get<Transform>(tree).rotation = affine::AngleY(k_Angle);
	ecs::placed_turn::KeepAngles(registry, tree, {0.0f, k_Angle, 0.0f});
	const auto treeLine = LineOf(tree, Offset(100.0f, 200.0f));
	ASSERT_TRUE(treeLine.has_value());
	EXPECT_EQ(std::get<TreeLine>(*treeLine).yAngle, k_Angle);

	const auto scaffold = Thing();
	registry.Assign<MobileObject>(scaffold).type = MobileObjectInfo::OldScaffold;
	registry.Assign<Scaffold>(scaffold).town = entt::null;
	registry.Get<Transform>(scaffold).rotation = affine::AngleY(k_Angle);
	ecs::placed_turn::KeepAngles(registry, scaffold, {0.0f, k_Angle, 0.0f});
	const auto scaffoldLine = LineOf(scaffold, Offset(100.0f, 200.0f));
	ASSERT_TRUE(scaffoldLine.has_value());
	EXPECT_EQ(lines::Text(*scaffoldLine), "CREATE_SCAFFOLD(-1, \"0.00,0.00\", 0, 4000, 1000)\n");
}

TEST_F(VortexSaveLines, AThingTurnedSinceItKeptItsAnglesIsWrittenWithTheAnglesOfItsTurn)
{
	// its kept angles were kept with another turn: they are no longer its own, so the angles come from the turn now
	const auto barrel = Thing();
	registry.Assign<MobileObject>(barrel).type = MobileObjectInfo::EgyptBarrel;
	ecs::placed_turn::KeepAngles(registry, barrel, {0.0f, 3.0f, 0.0f});
	const auto turn = affine::AngleY(1.0f);
	registry.Get<Transform>(barrel).rotation = turn;
	const auto line = LineOf(barrel, Offset(100.0f, 200.0f));
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(std::get<MobileObjectLine>(*line).yAngle, ecs::placed_turn::AnglesOfTurn(turn, true).y);
}
