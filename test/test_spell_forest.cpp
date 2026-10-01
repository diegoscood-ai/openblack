/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forest miracle (M4b): SpellForest's spiral of trees, their target scales, how many trees it wants, its upkeep,
// GetMaxObjectsToCreate and AdjustSpellSeedPos (Magic/Spells/SpellForest).

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellForest.h"

using namespace openblack;
using namespace openblack::magic;

/// fn_00725790: all the trees while the spell has strength, none without
TEST(SpellForest, treesWanted)
{
	EXPECT_EQ(spell_forest::TreesWanted(1.0f, -1, 18), 18);
	EXPECT_EQ(spell_forest::TreesWanted(0.001f, 18, 18), 18);
	EXPECT_EQ(spell_forest::TreesWanted(0.0f, 18, 18), 0);
	EXPECT_EQ(spell_forest::TreesWanted(-1.0f, 18, 18), 0);
	EXPECT_EQ(spell_forest::TreesWanted(0.5f, 7, 18), 7); // a script's SetMaxObjectsToCreate
}

/// SpellForest::SpellEvent 0x725830: from 2 m at the centre to 11 m, 1.30769 turns per tree
TEST(SpellForest, spiral)
{
	const auto first = spell_forest::SpiralOffset(0, 18);
	EXPECT_NEAR(first.x, 2.0f, 1e-5f);
	EXPECT_NEAR(first.y, 0.0f, 1e-5f);
	const auto second = spell_forest::SpiralOffset(1, 18);
	EXPECT_NEAR(glm::length(second), 5.041239f, 1e-4f);
	EXPECT_NEAR(second.x, -3.773422f, 1e-4f);
	EXPECT_NEAR(second.y, 3.342960f, 1e-4f);
	const auto sixth = spell_forest::SpiralOffset(5, 18);
	EXPECT_NEAR(sixth.x, 7.415667f, 1e-4f);
	EXPECT_NEAR(sixth.y, -3.892027f, 1e-4f);
	const auto last = spell_forest::SpiralOffset(17, 18);
	EXPECT_NEAR(glm::length(last), 11.0f, 1e-4f);
	EXPECT_NEAR(last.x, -10.680362f, 1e-4f);
	EXPECT_NEAR(last.y, -2.632464f, 1e-4f);
	// every tree between the two radii, farther out as i grows
	float previous = 0.0f;
	for (int i = 0; i < 18; ++i)
	{
		const float r = glm::length(spell_forest::SpiralOffset(i, 18));
		EXPECT_GE(r, spell_forest::k_InnerRadius - 1e-4f);
		EXPECT_LE(r, spell_forest::k_ForestRadius + 1e-4f);
		EXPECT_GT(r, previous);
		previous = r;
	}
	// one tree: f = 0 (step 1), at 2 m
	EXPECT_NEAR(spell_forest::SpiralOffset(0, 1).x, 2.0f, 1e-5f);
}

/// The MapCoords the trees go to: ftol(x * 6553.6) (a truncation) back to metres
TEST(SpellForest, mapCoords)
{
	const auto point = spell_forest::ToMapCoords(glm::vec2(1790.0f, 2625.123f));
	EXPECT_FLOAT_EQ(point.x, 1790.0f);
	// a step of 10 / 65536 m, down (the float 2625.123 is 2625.12305)
	EXPECT_LE(static_cast<double>(point.y), 2625.123047);
	EXPECT_GT(static_cast<double>(point.y), 2625.123047 - 10.0 / 65536.0);
}

/// fn_007255C0: 1 - 0.5 x distance / 11 (0.91 for the first tree at 2 m, 0.5 at the rim)
TEST(SpellForest, targetScale)
{
	EXPECT_FLOAT_EQ(spell_forest::TargetScale(0.0f), 1.0f);
	EXPECT_NEAR(spell_forest::TargetScale(2.0f), 0.909091f, 1e-5f);
	EXPECT_FLOAT_EQ(spell_forest::TargetScale(11.0f), 0.5f);
}

/// SpellForest::GetMaxObjectsToCreate 0x7256F0 and HasEnoughChantsAndLifeForRecast 0x725730 (> 0)
TEST(SpellForest, maxObjectsToCreate)
{
	EXPECT_EQ(spell_forest::MaxObjectsToCreate(18, 18, false, 0, false), 18); // not landed yet
	EXPECT_EQ(spell_forest::MaxObjectsToCreate(18, 18, true, 11, true), 11);  // the trees it has
	EXPECT_EQ(spell_forest::MaxObjectsToCreate(18, 18, false, 0, true), 0);   // its forest is gone
	EXPECT_EQ(spell_forest::MaxObjectsToCreate(5, 18, true, 11, true), 5);    // +0xF4 caps it
}

/// SpellForest::CalculateCostToMaintain 0x7259E0: 5 + 1 per tree (NATURE costPerGameTurn / costPerEvent)
TEST(SpellForest, costToMaintain)
{
	EXPECT_FLOAT_EQ(spell_forest::CostToMaintain(5.0f, 1.0f, 0), 5.0f);
	EXPECT_FLOAT_EQ(spell_forest::CostToMaintain(5.0f, 1.0f, 18), 23.0f);
}

/// SpellForest::AdjustSpellSeedPos 0x725750
TEST(SpellForest, adjustSpellSeedPos)
{
	EXPECT_FLOAT_EQ(spell_forest::AdjustSpellSeedAltitude(false, 7.0f, 3.0f), -5.0f);
	EXPECT_FLOAT_EQ(spell_forest::AdjustSpellSeedAltitude(true, 7.0f, 3.0f), 7.0f);
	EXPECT_FLOAT_EQ(spell_forest::AdjustSpellSeedAltitude(true, 7.0f, 9.0f), 9.0f);
}

/// fn_00728FC0 (the seed's draw 0x729020 and ProcessFromSpell 0x728F70): the NATURE seed follows its spell only while it
/// is linked to a worship icon (+0x5C) and its spell (if any) is open; a cast-in-hand or kept-in-hand seed never does
TEST(SpellForest, seedFollowsSpell)
{
	// seed_table.md: NATURE is HAND_POSITION, isKeptInHand 0, seedFollowsSpell 1
	const auto setInfo = [](SpellCastType castType, uint32_t kept) {
		auto info = std::make_unique<InfoConstants>();
		auto& nature = info->spellSeed.at(static_cast<size_t>(SpellSeedType::Nature));
		nature.castType = castType;
		nature.isKeptInHand = kept;
		nature.seedFollowsSpell = 1;
		Locator::infoConstants::reset(info.release());
	};
	setInfo(SpellCastType::SpellCastHandPosition, 0);
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	ecs::components::SpellSeed seed;
	seed.seedType = SpellSeedType::Nature;
	// a loose seed (a one-shot orb, CreateSpellIntoHand): no icon, not drawn
	EXPECT_FALSE(magic::seed::FollowsSpell(seed));
	seed.icon = registry.Create();
	EXPECT_TRUE(magic::seed::FollowsSpell(seed)); // no spell yet: a NULL +0x60 goes on (0x728FF6)
	const auto spell = registry.Create();
	registry.Assign<ecs::components::Spell>(spell);
	seed.spell = spell;
	EXPECT_TRUE(magic::seed::FollowsSpell(seed));
	registry.Get<ecs::components::Spell>(spell).closedDown = true; // +0x40
	EXPECT_FALSE(magic::seed::FollowsSpell(seed));
	registry.Get<ecs::components::Spell>(spell).closedDown = false;
	setInfo(SpellCastType::SpellCastHandPosition, 1); // kept in hand (0x729840)
	EXPECT_FALSE(magic::seed::FollowsSpell(seed));
	setInfo(SpellCastType::SpellCastInHand, 0); // cast in hand (0x729820)
	EXPECT_FALSE(magic::seed::FollowsSpell(seed));
	Locator::entitiesRegistry::reset();
	Locator::infoConstants::reset();
}

/// With OPENBLACK_GAME_PATH set to the install: the real NATURE row (resources.md section 0.2) and the terrain
/// materials' magic tree types
TEST(SpellForest, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto* forest = magic::GetMagicInfoAs<GMagicForestInfo>(*info, MagicType::Forest);
	ASSERT_NE(forest, nullptr);
	EXPECT_EQ(forest->finalNoTrees, 18u);
	EXPECT_FLOAT_EQ(forest->startLife, 0.1f);
	EXPECT_FLOAT_EQ(forest->growSpeed, 0.01f);
	EXPECT_FLOAT_EQ(forest->decaySpeed, 0.05f);
	EXPECT_FLOAT_EQ(forest->woodValueMultiplier, 0.25f);
	const auto& effect = magic::GetMagicEffectInfo(*info, MagicType::Forest);
	EXPECT_FLOAT_EQ(spell_forest::CostToMaintain(effect.costPerGameTurn, effect.costPerEvent, 18), 23.0f);
	EXPECT_FLOAT_EQ(effect.initialChants, 10000.0f);
	EXPECT_FLOAT_EQ(effect.timerWhenOneShot, 120.0f);
	EXPECT_FLOAT_EQ(effect.timerWhenPlayerCasting, -1.0f);
	// MATERIAL_TYPE_GRASS (18), like most: Beech, Birch, Cedar, Cedar
	const auto& grass = info->terrainMaterial[18].magicTreeTypes;
	EXPECT_EQ(grass[0], TreeInfo::Beech);
	EXPECT_EQ(grass[1], TreeInfo::Birch);
	EXPECT_EQ(grass[2], TreeInfo::Cedar);
	EXPECT_EQ(grass[3], TreeInfo::Cedar);
	// 1 (DEEP_WATER, what GetTerrainMaterial gives for a material 0) and sand: palms; snow (27): conifers
	EXPECT_EQ(info->terrainMaterial[1].magicTreeTypes[0], TreeInfo::Palm);
	EXPECT_EQ(info->terrainMaterial[7].magicTreeTypes[3], TreeInfo::PalmC);
	EXPECT_EQ(info->terrainMaterial[27].magicTreeTypes[1], TreeInfo::ConiferA);
}
