/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A villager of no tribe taken through a way out of a land and read back: the crossing file writes it as a
// CREATE_SPECIAL_TOWN_VILLAGER line by its row of the villagers' table, and reading the line makes the villager again
// from that row, where it stood relative to the vortex, as old as it was and in its town, in a small world of the game's
// own systems (docs/wiki/world/vortex.md#the-land-script-round-trip-of-vortextxt)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <variant>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerAge.h"
#include "ECS/VortexSave.h"
#include "ECS/VortexSaveLines.h"
#include "Enums.h"
#include "LHScriptX/Script.h"
#include "support/VillagerWorld.h"

using namespace openblack;
namespace lines = openblack::ecs::vortex_save_lines;
namespace vortex_save = openblack::ecs::vortex_save;

namespace
{
const glm::vec2 k_Vortex {1000.0f, 1200.0f};
const glm::vec2 k_At {1010.0f, 1220.0f};

class VortexVillagerWorld: public test::world::VillagerWorld
{
protected:
	void SetUp() override
	{
		VillagerWorld::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		// on synthetic data every row is a Celtic one: the hermit's is made one of no tribe
		Info().villager.at(static_cast<size_t>(VillagerInfo::Hermit)).tribeType = Tribe::NONE;
	}

	void TearDown() override
	{
		lhscriptx::Script::SetLastCreated(entt::null);
		VillagerWorld::TearDown();
	}

	/// The first row of the villagers' table of no tribe
	[[nodiscard]] std::optional<VillagerInfo> RowOfNoTribe() const
	{
		const auto& rows = Info().villager;
		const auto found = std::ranges::find(rows, Tribe::NONE, &GVillagerInfo::tribeType);
		if (found == rows.end())
		{
			return std::nullopt;
		}
		return static_cast<VillagerInfo>(std::distance(rows.begin(), found));
	}

	/// What the crossing file's lines read from the world: the villager's row as the scripts see it, and its place on the
	/// map where its body stands
	[[nodiscard]] static lines::World SaveLinesWorld()
	{
		return {
		    .turn = ecs::villager::CurrentTurn(),
		    .villagerRow = [](entt::entity villager) { return static_cast<int32_t>(ecs::script_type::SubtypeOf(villager)); },
		    .potResource = [](PotInfo) { return 0; },
		    .life = [](entt::entity) { return 1.0f; },
		    .player = [](entt::entity) { return std::optional<PlayerNames> {}; },
		    .ofAStoragePit = [](entt::entity) { return false; },
		    .magicName = [](SpellSeedType, int) { return std::string(); },
		    .placeOf =
		        [](entt::entity thing) {
			        const auto& position = Registry().Get<const ecs::components::Transform>(thing).position;
			        return map_coords::FromMetres(glm::vec2(position.x, position.z));
		        },
		    .lyingOnTheLand = [](entt::entity) { return true; },
		};
	}

	/// The villager's line, as the crossing file writes it
	[[nodiscard]] static std::optional<std::string> LineOf(entt::entity villager)
	{
		const auto world = SaveLinesWorld();
		const auto line =
		    lines::LineFor(Registry(), villager, world.placeOf(villager), map_coords::FromMetres(k_Vortex), world);
		if (!line.has_value() || !std::holds_alternative<vortex::crossing::SpecialVillagerLine>(*line))
		{
			return std::nullopt;
		}
		return lines::Text(*line);
	}

	/// What reading the line back at the vortex makes
	[[nodiscard]] static entt::entity ReadBack(const std::string& line)
	{
		auto reader = vortex_save::ReaderFrom(line);
		const auto read = vortex_save::ReadNext(reader, map_coords::FromMetres(k_Vortex));
		return read.value_or(entt::null);
	}
};
} // namespace

// A villager of no tribe in a town comes back from its line as a villager of the same row, where it stood relative to
// the vortex, as old as it was, in the town of the line's number
TEST_P(VortexVillagerWorld, AVillagerOfNoTribeIsReadBackFromItsLineIntoItsTown)
{
	const auto row = RowOfNoTribe();
	ASSERT_TRUE(row.has_value()) << "no villager row of no tribe";
	const auto town = ecs::archetypes::TownArchetype::Create(3, glm::vec3(k_At.x, k_Ground, k_At.y + 30.0f),
	                                                         PlayerNames::PLAYER_ONE, Tribe::CELTIC);
	const auto villager = Villager(*row, k_At, 30);
	ASSERT_TRUE(ecs::town_villagers::AddVillagerToTown(town, villager));
	const auto& written = Registry().Get<const ecs::components::Villager>(villager);
	const auto age = ecs::villager_age::AgeNow(written);

	const auto line = LineOf(villager);
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(*line, "CREATE_SPECIAL_TOWN_VILLAGER(3, \"10.00,20.00\", " + std::to_string(static_cast<int>(*row)) + ", " +
	                     std::to_string(age) + ")\n");

	const auto read = ReadBack(*line);
	ASSERT_NE(read, entt::entity {entt::null});
	ASSERT_NE(read, villager);
	const auto& made = Registry().Get<const ecs::components::Villager>(read);
	EXPECT_EQ(made.infoRow, *row);
	EXPECT_EQ(made.tribe, Tribe::NONE);
	EXPECT_EQ(made.town, town);
	const auto& at = Registry().Get<const ecs::components::Transform>(read).position;
	EXPECT_NEAR(at.x, k_At.x, 0.01f);
	EXPECT_NEAR(at.z, k_At.y, 0.01f);
	EXPECT_FLOAT_EQ(at.y, k_Ground);
	// a villager that drew a name is given its name's age instead
	if (!Registry().AllOf<ecs::components::NamedVillager>(read))
	{
		EXPECT_EQ(ecs::villager_age::AgeNow(made), age);
		// and written again, it gives the same line
		EXPECT_EQ(LineOf(read), line);
	}
}

// A villager of no tribe and no town, on a land with no town, comes back from its line in no town
TEST_P(VortexVillagerWorld, AVillagerOfNoTribeAndNoTownIsReadBackInNoTown)
{
	const auto row = RowOfNoTribe();
	ASSERT_TRUE(row.has_value()) << "no villager row of no tribe";
	const auto villager = Villager(*row, k_At, 30);
	const auto line = LineOf(villager);
	ASSERT_TRUE(line.has_value());
	EXPECT_TRUE(line->starts_with("CREATE_SPECIAL_TOWN_VILLAGER(-1, \"10.00,20.00\", "));

	const auto read = ReadBack(*line);
	ASSERT_NE(read, entt::entity {entt::null});
	ASSERT_NE(read, villager);
	const auto& made = Registry().Get<const ecs::components::Villager>(read);
	EXPECT_EQ(made.infoRow, *row);
	EXPECT_EQ(made.town, entt::entity {entt::null});
}

INSTANTIATE_TEST_SUITE_P(Data, VortexVillagerWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
