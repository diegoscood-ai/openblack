/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a land's load does to the map script's globals: the game only puts the interact desire back; the land number
// and the influence multipliers stay as the last land left them until the new land's script sets them. And what it does
// to the things the scripts' variables hold: the variables keep them, and a thing of the land before is never found
// again on the new land, even where a new thing takes its place

#include <cstdint>

#include <algorithm>
#include <array>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Registry.h"

using openblack::ecs::components::MapScriptGlobals;
using openblack::ecs::components::ResetForLandLoad;

TEST(LandLoadGlobals, TheGameStartsWithNoLandNumberAndBothMultipliersAtOne)
{
	const MapScriptGlobals globals;
	EXPECT_EQ(globals.landNumber, 0);
	EXPECT_EQ(globals.townInfluenceMultiplier, 1.0f);
	EXPECT_EQ(globals.playerInfluenceMultiplier, 1.0f);
	EXPECT_EQ(globals.interactDesire, 0.0f);
}

TEST(LandLoadGlobals, ALandsLoadKeepsTheLastLandsNumberAndMultipliers)
{
	// Land 3 as its script leaves it: SET_LAND_NUMBER(3), SET_TOWN_INFLUENCE_MULTIPLIER(0.5)
	MapScriptGlobals globals;
	globals.landNumber = 3;
	globals.townInfluenceMultiplier = 0.5f;
	globals.playerInfluenceMultiplier = 0.75f;

	ResetForLandLoad(globals);

	// until the next land's script sets its own, they are still Land 3's
	EXPECT_EQ(globals.landNumber, 3);
	EXPECT_EQ(globals.townInfluenceMultiplier, 0.5f);
	EXPECT_EQ(globals.playerInfluenceMultiplier, 0.75f);
}

TEST(LandLoadGlobals, ALandsLoadPutsTheInteractDesireBack)
{
	MapScriptGlobals globals;
	globals.interactDesire = 0.25f;
	globals.fireFlySpellRewardProbability.at(4) = 0.5f;

	ResetForLandLoad(globals);

	EXPECT_EQ(globals.interactDesire, 0.0f);
	// the firefly rewards are not reset between lands either
	EXPECT_EQ(globals.fireFlySpellRewardProbability.at(4), 0.5f);
}

TEST(LandLoadThings, AThingOfTheLastLandIsNotFoundOnTheNewLand)
{
	// A script variable holds a thing as the number the natives give it; the land's load empties the registry
	openblack::ecs::Registry registry;
	std::array<uint32_t, 3> held {};
	for (auto& variable : held)
	{
		variable = entt::to_integral(registry.Create());
	}

	registry.Reset();
	// the new land's things take the same places in the registry
	std::array<entt::entity, 3> made {};
	for (auto& thing : made)
	{
		thing = registry.Create();
	}

	for (const uint32_t variable : held)
	{
		const auto old = static_cast<entt::entity>(variable);
		EXPECT_FALSE(registry.Valid(old)) << variable;
		// one of the new things has the old thing's place, but not its number
		EXPECT_TRUE(std::ranges::any_of(made, [old](entt::entity thing) {
			return entt::to_entity(thing) == entt::to_entity(old);
		})) << variable;
		EXPECT_TRUE(std::ranges::none_of(made, [variable](entt::entity thing) { return entt::to_integral(thing) == variable; }))
		    << variable;
	}
	EXPECT_TRUE(std::ranges::all_of(made, [&registry](entt::entity thing) { return registry.Valid(thing); }));
}
