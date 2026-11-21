/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A land script's position given to a command with its map units, as the script gave them, beside its metres, and
// the arena CREATE_ARENA makes keeping them with the order it was made in
// (docs/wiki/scripts/map-script-reader.md#the-map-script-reader-lhscriptx)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "LHScriptX/CommandSignature.h"
#include "LHScriptX/Script.h"
#include "LHScriptX/ScriptingBindingUtils.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::lhscriptx;

namespace
{
class ScriptArena: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::events::emplace<EventManager>();
		// the land's height is half the z of the point asked for
		Locator::terrainSystem::emplace<openblack::test::HeightFieldIsland>([](glm::vec2 xz) { return xz.y * 0.5f; });
	}

	void TearDown() override
	{
		Script::SetLastCreated(entt::null);
		Locator::terrainSystem::reset();
		Locator::events::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	/// The arenas the script made, in the order it made them
	static std::vector<ecs::components::Arena> Arenas()
	{
		std::vector<ecs::components::Arena> arenas;
		Locator::entitiesRegistry::value().Each<const ecs::components::Arena>(
		    [&arenas](entt::entity, const ecs::components::Arena& arena) { arenas.push_back(arena); });
		std::ranges::sort(arenas, {}, &ecs::components::Arena::number);
		return arenas;
	}
};
} // namespace

// A command that takes a ScriptPosition gets the position's metres and its map units; one that takes a vector gets
// the same metres as before
TEST(ScriptPositionArgument, ACommandTakesThePlaceInMetresAndTheScriptsMapUnits)
{
	ScriptCommandParameter parameter {2152.78f, 12.5f, 2690.82f};
	parameter.SetUnits({.x = 14108459, .z = 17634558, .altitude = 0.0f});
	const ScriptCommandParameters arguments {parameter};

	const auto position = GetParamValue<ScriptPosition>(arguments, 0);
	EXPECT_EQ(position.metres, glm::vec3(2152.78f, 12.5f, 2690.82f));
	EXPECT_EQ(position.units, (map_coords::MapCoords {.x = 14108459, .z = 17634558, .altitude = 0.0f}));
	EXPECT_EQ(GetParamValue<glm::vec3>(arguments, 0), glm::vec3(2152.78f, 12.5f, 2690.82f));
	EXPECT_EQ(GetParamType<ScriptPosition>(), ParameterType::Vector);
}

// CREATE_ARENA keeps the map units the script gave, its height above the land included (0 without a third field), and
// the order the land made its arenas in, from 1; its place in metres and its radius are as before. The first place is
// Land 1's first arena (14108459, 17634558 in the frozen Land 1 autosave)
TEST_F(ScriptArena, AnArenaKeepsTheScriptsMapUnitsAndTheOrderItWasMadeIn)
{
	Script script;
	script.Load("CREATE_ARENA(\"2152.78,2690.82\", 27.453003)\nCREATE_ARENA(\"10,20,3.5\", 5.0)\n", [] {});
	const auto arenas = Arenas();
	ASSERT_EQ(arenas.size(), 2u);
	EXPECT_EQ(arenas[0].at, (map_coords::MapCoords {.x = 14108459, .z = 17634558, .altitude = 0.0f}));
	EXPECT_EQ(arenas[0].number, 1u);
	EXPECT_FLOAT_EQ(arenas[0].radius, 27.453003f);
	EXPECT_EQ(arenas[0].position.x, map_coords::ToMetres(14108459));
	EXPECT_EQ(arenas[0].position.z, map_coords::ToMetres(17634558));
	EXPECT_EQ(arenas[1].at, (map_coords::MapCoords {.x = 65536, .z = 131072, .altitude = 3.5f}));
	EXPECT_EQ(arenas[1].number, 2u);
	EXPECT_FLOAT_EQ(arenas[1].position.y, 10.0f);
}
