/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which objects put their footprint and their shadow into the land's textures, as an object's change reads them to
// mark the blocks under it, on a registry of the test's own, as the original
// (docs/wiki/world/terrain.md#who-asks-for-its-footprint)

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/LandPrintMarks.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::land_print_marks::HasFootprintBit;
using openblack::ecs::land_print_marks::TextureBitsOf;

namespace
{
/// A fixed object of the map with nothing else on it
entt::entity FixedThing(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Fixed>(entity, glm::vec2(0.0f), 1.0f);
	return entity;
}

/// A building as AbodeArchetype::Create leaves it: whole, or not built with the footprint set after its insert
entt::entity Building(ecs::Registry& registry, bool madeNotBuilt)
{
	const auto entity = FixedThing(registry);
	auto& abode = registry.Assign<Abode>(entity);
	if (madeNotBuilt)
	{
		abode.buildFlags = Abode::k_UnderConstruction;
		abode.percentBuilt = 0.0f;
		abode.footprintOnTexture = true;
	}
	return entity;
}
} // namespace

// The original's creations give the footprint bit to a field, an animated static, a worship site and the citadel's
// heart, whatever they are, as the original (docs/wiki/world/terrain.md#who-asks-for-its-footprint)
TEST(LandPrintMarks, TheClassesMadeWithTheirFootprint)
{
	ecs::Registry registry;
	const auto field = FixedThing(registry);
	registry.Assign<Abode>(field);
	registry.Assign<Field>(field);
	const auto gate = FixedThing(registry);
	registry.Assign<AnimatedStatic>(gate, AnimatedStatic {.type = AnimatedStaticInfo::NorseGate});
	const auto cave = FixedThing(registry);
	registry.Assign<AnimatedStatic>(cave, AnimatedStatic {.type = AnimatedStaticInfo::PiperCaveEntrance});
	const auto site = registry.Create();
	registry.Assign<WorshipSite>(site);
	const auto heart = registry.Create();
	registry.Assign<CitadelHeart>(heart);
	EXPECT_TRUE(HasFootprintBit(registry, field));
	EXPECT_TRUE(HasFootprintBit(registry, gate));
	EXPECT_TRUE(HasFootprintBit(registry, cave));
	EXPECT_TRUE(HasFootprintBit(registry, site));
	EXPECT_TRUE(HasFootprintBit(registry, heart));
}

// A building gets the bit only when it is made not built, and Built never clears it; a building made whole, a feature
// (also one whose percent built is lowered later) and a tree never get it, whatever their meshes hold, as the original
// (docs/wiki/world/terrain.md#who-asks-for-its-footprint)
TEST(LandPrintMarks, ABuildingHasItsFootprintOnlyWhenMadeNotBuilt)
{
	ecs::Registry registry;
	const auto whole = Building(registry, false);
	const auto site = Building(registry, true);
	const auto finished = Building(registry, true);
	registry.Get<Abode>(finished).buildFlags = Abode::k_Built;
	const auto feature = FixedThing(registry);
	registry.Assign<Feature>(feature).percentBuilt = 0.5f;
	const auto tree = FixedThing(registry);
	registry.Assign<Tree>(tree);
	EXPECT_FALSE(HasFootprintBit(registry, whole));
	EXPECT_TRUE(HasFootprintBit(registry, site));
	EXPECT_TRUE(HasFootprintBit(registry, finished));
	EXPECT_FALSE(HasFootprintBit(registry, feature));
	EXPECT_FALSE(HasFootprintBit(registry, tree));
}

// A building made not built: while it is being made (its first insert in the map) its shadow is on and its footprint
// off, as every fixed object's; then the footprint only; once built both, as the original
// (docs/wiki/world/terrain.md#who-asks-for-its-footprint)
TEST(LandPrintMarks, ABuildingsBitsFromItsMakingToBuilt)
{
	ecs::Registry registry;
	const auto building = FixedThing(registry);
	auto& abode = registry.Assign<Abode>(building);
	abode.buildFlags = Abode::k_UnderConstruction;
	abode.percentBuilt = 0.0f;
	auto bits = TextureBitsOf(registry, building);
	EXPECT_TRUE(bits.shadow);
	EXPECT_FALSE(bits.footprint);

	registry.Get<Abode>(building).footprintOnTexture = true;
	bits = TextureBitsOf(registry, building);
	EXPECT_FALSE(bits.shadow);
	EXPECT_TRUE(bits.footprint);

	registry.Get<Abode>(building).buildFlags = Abode::k_Built;
	bits = TextureBitsOf(registry, building);
	EXPECT_TRUE(bits.shadow);
	EXPECT_TRUE(bits.footprint);
}

// The others: a building made whole has its shadow only, a tree too; an animated static and a field their footprint
// only, as the original (docs/wiki/world/terrain.md#who-asks-for-its-footprint)
TEST(LandPrintMarks, TheOthersBits)
{
	ecs::Registry registry;
	const auto whole = Building(registry, false);
	const auto tree = FixedThing(registry);
	registry.Assign<Tree>(tree);
	const auto plinth = FixedThing(registry);
	registry.Assign<AnimatedStatic>(plinth, AnimatedStatic {.type = AnimatedStaticInfo::GateStonePlinth});
	const auto field = FixedThing(registry);
	registry.Assign<Abode>(field);
	registry.Assign<Field>(field);

	const auto wholeBits = TextureBitsOf(registry, whole);
	EXPECT_TRUE(wholeBits.shadow);
	EXPECT_FALSE(wholeBits.footprint);
	const auto treeBits = TextureBitsOf(registry, tree);
	EXPECT_TRUE(treeBits.shadow);
	EXPECT_FALSE(treeBits.footprint);
	const auto plinthBits = TextureBitsOf(registry, plinth);
	EXPECT_FALSE(plinthBits.shadow);
	EXPECT_TRUE(plinthBits.footprint);
	const auto fieldBits = TextureBitsOf(registry, field);
	EXPECT_FALSE(fieldBits.shadow);
	EXPECT_TRUE(fieldBits.footprint);
}

// A building site is no fixed object of the map and has no 3D object, so it has no footprint bit: the one the
// creation gives when not built is the building's it puts up; the fixed classes that are always built (a mobile static,
// a big forest, a fish farm, a dead tree) have none either, as the original
// (docs/wiki/world/terrain.md#who-asks-for-its-footprint)
TEST(LandPrintMarks, ABuildingSiteAndTheAlwaysBuiltHaveNoFootprint)
{
	ecs::Registry registry;
	const auto building = Building(registry, true);
	const auto site = registry.Create();
	registry.Assign<BuildingSite>(site).root = building;
	const auto forest = FixedThing(registry);
	registry.Assign<BigForest>(forest);
	const auto dead = FixedThing(registry);
	registry.Assign<DeadTree>(dead, DeadTree {TreeInfo::Beech});
	EXPECT_FALSE(HasFootprintBit(registry, site));
	EXPECT_FALSE(TextureBitsOf(registry, site).shadow);
	EXPECT_TRUE(HasFootprintBit(registry, building));
	EXPECT_FALSE(HasFootprintBit(registry, forest));
	EXPECT_FALSE(HasFootprintBit(registry, dead));
}
