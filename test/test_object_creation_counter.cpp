/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creation counter's numbers for the town centres' spell icons: each icon takes, as it is made, the next of the
// numbers its town's spells took for their icons, the oldest first

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using openblack::Locator;
using openblack::ecs::systems::ObjectCreationIndexSystem;

// A town's two spells before its centre take their numbers as the centre is made; a third after it takes the next one
// as it is added; the icons take them in that order, and no more than were taken
TEST(ObjectCreationCounter, ATownsIconsTakeTheNumbersItsSpellsTook)
{
	ObjectCreationIndexSystem counter;
	counter.OnLoadMap(); // the game's first land: from 2
	counter.AddTownSpell(4, "heal");
	counter.AddTownSpell(4, "water");
	EXPECT_EQ(counter.Next(), 2u);
	counter.OnTownCentre(4); // 3 and 4
	EXPECT_EQ(counter.Next(), 5u);
	counter.AddTownSpell(4, "food"); // 6
	counter.AddTownSpell(4, "food"); // the same seed again: no icon
	EXPECT_EQ(counter.Peek(), 7u);
	EXPECT_EQ(counter.TakeTownIcon(4), 3u);
	EXPECT_EQ(counter.TakeTownIcon(4), 4u);
	EXPECT_EQ(counter.TakeTownIcon(4), 6u);
	EXPECT_FALSE(counter.TakeTownIcon(4).has_value());
	EXPECT_FALSE(counter.TakeTownIcon(5).has_value());
}

// An icon made once its town's counted numbers are all taken (a town given a magic in play, or an icon made since a
// load) takes the counter's next number as it is made, as every object does
TEST(ObjectCreationCounter, AnIconBeyondItsTownsCountedOnesTakesTheCountersNextNumber)
{
	const openblack::test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const openblack::test::RestoreService<Locator::objectCreationIndexSystem> restoreIndex;
	auto& registry = Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
	auto& counter =
	    static_cast<ObjectCreationIndexSystem&>(Locator::objectCreationIndexSystem::emplace<ObjectCreationIndexSystem>());
	counter.OnLoadMap(); // from 2
	counter.AddTownSpell(4, "heal");
	counter.OnTownCentre(4); // 2
	const auto counted = registry.Create();
	openblack::ecs::object_index::AssignTownIcon(counted, 4);
	const auto given = registry.Create();
	openblack::ecs::object_index::AssignTownIcon(given, 4);
	const auto none = registry.Create();
	openblack::ecs::object_index::AssignTownIcon(none, 5);
	EXPECT_EQ(openblack::ecs::object_index::Of(counted), 2);
	EXPECT_EQ(openblack::ecs::object_index::Of(given), 3);
	EXPECT_EQ(openblack::ecs::object_index::Of(none), 4);
	EXPECT_EQ(counter.Peek(), 5u);
}
