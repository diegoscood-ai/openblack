/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The street lanterns' writer: an object of the row it was made with, whether it is a country lantern and the dark
// flag; a loaded lantern made again and written back as read (docs/wiki/engine/saved-games.md#writing-the-world-file,
// the street lantern)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <tuple>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Audio/Services/LanternSounds.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "Locator.h"
#include "support/FireFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The game's registry and map cells, which the place's flags ask whether the lantern is in the map, and the fires,
/// which the object's part asks for the fire it burns with
class StreetLanternWriteTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		Locator::fireSystem::emplace<test::FakeFires>();
		// the creation numbers a lantern's making takes
		Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::objectCreationIndexSystem> _restoreIndex;
};

entt::entity Lantern(ecs::Registry& registry, glm::vec3 position, MobileStaticInfo info, uint32_t creationIndex)
{
	const auto entity = registry.Create();
	registry.Assign<components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::ObjectCreationIndex>(entity, components::ObjectCreationIndex {.value = creationIndex});
	registry.Assign<components::StreetLantern>(entity, info != MobileStaticInfo::StreetLantern, info);
	return entity;
}

gamesave::StreetLanternRecord Written(const ecs::Registry& registry, entt::entity lantern, bool dark)
{
	ClassWriters writers;
	RegisterStreetLanternWriters(writers, [dark] { return dark; });
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ(writers.ClaimsOf(registry, lantern), std::vector {gamesave::StreetLanternRecord::k_Type});
	const auto written = (*writers.WriterOf(gamesave::StreetLanternRecord::k_Type))(context, lantern);
	EXPECT_TRUE(written.has_value()) << context.Failure();
	return written.has_value() ? std::get<gamesave::StreetLanternRecord>(*written) : gamesave::StreetLanternRecord {};
}
} // namespace

// A town lantern is written where it stands, of the street lantern's row, its target its own place, with its creation
// number; its word 0 and the dark flag as the game has it
TEST_F(StreetLanternWriteTest, ATownLanternIsWrittenOfItsRowAtItsPlace)
{
	auto& registry = Reg();
	const auto lantern = Lantern(registry, glm::vec3(1234.5f, 17.0f, 987.25f), MobileStaticInfo::StreetLantern, 42);
	const auto record = Written(registry, lantern, true);
	EXPECT_EQ(record.position.position.x, map_coords::ToFixed(1234.5f));
	EXPECT_EQ(record.position.position.z, map_coords::ToFixed(987.25f));
	EXPECT_EQ(record.object.coords, record.position.position);
	EXPECT_EQ(record.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::StreetLantern));
	EXPECT_EQ(record.object.creationIndex, 42u);
	EXPECT_EQ(record.word, 0u);
	EXPECT_EQ(record.mark, 1);
}

// A country lantern, made of another row, has its word 1; in daylight the flag is 0
TEST_F(StreetLanternWriteTest, ACountryLanternHasItsWordSet)
{
	auto& registry = Reg();
	const auto lantern = Lantern(registry, glm::vec3(100.0f, 0.0f, 200.0f), MobileStaticInfo::Bonfire, 7);
	const auto record = Written(registry, lantern, false);
	EXPECT_EQ(record.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Bonfire));
	EXPECT_EQ(record.word, 1u);
	EXPECT_EQ(record.mark, 0);
}

// A street lantern of the save is made again on the land at its place, of its row, a country lantern as its word says
// (here a town lantern's row saved with its word set), with its creation number; its flag puts the lanterns' sounds on;
// it is written back as it was read
TEST_F(StreetLanternWriteTest, ASavedLanternIsMadeAgainAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	auto& registry = Reg();
	audio::lantern_sounds::Clear();
	gamesave::StreetLanternRecord saved;
	saved.position.position = {.x = map_coords::ToFixed(1234.5f), .z = map_coords::ToFixed(987.25f), .altitude = 0.0f};
	// in the map and counting as fixed, as a Land 1 save's lanterns are
	saved.position.flags = static_cast<uint16_t>(position_flags::k_InMap | position_flags::k_CountsAsFixed);
	saved.object.info = load::MobileStaticInfoRowOf(MobileStaticInfo::StreetLantern);
	saved.object.coords = saved.position.position;
	saved.object.creationIndex = 42;
	saved.object.scale = 1.0f;
	saved.object.life = 1.0f;
	saved.word = 1;
	saved.mark = 1;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(saved);
	world.loadOrder = {2};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.streetLantern = load::MakeStreetLanternEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto entity = loaded.at(2).entity;
	ASSERT_TRUE(entity != entt::null);
	const auto& lantern = registry.Get<const components::StreetLantern>(entity);
	EXPECT_TRUE(lantern.country);
	EXPECT_TRUE(lantern.info == MobileStaticInfo::StreetLantern);
	EXPECT_EQ(ecs::object_index::Of(entity), 42);
	EXPECT_TRUE(audio::lantern_sounds::IsOn());

	ClassWriters writers;
	RegisterStreetLanternWriters(writers, audio::lantern_sounds::IsOn);
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(gamesave::StreetLanternRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::StreetLanternRecord>(*written);
	EXPECT_TRUE(gamesave::StreetLanternRecord::FieldsOf(record) == gamesave::StreetLanternRecord::FieldsOf(saved));
	audio::lantern_sounds::Clear();
}
