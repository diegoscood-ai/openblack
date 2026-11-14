/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The building sites written from their entities, in the original's order, and the game's list of them newest first
// by the order they were made; a loaded standard site made again on its building and written back as read
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the building sites)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <variant>
#include <vector>

#include <BuildingRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The writers of the sites and the game's lists, with a class for the buildings and villagers the sites name
ClassWriters SiteWriters()
{
	ClassWriters writers;
	RegisterBuildingSiteWriters(writers);
	RegisterGameListWriters(writers);
	writers.Register(
	    gamesave::AbodeRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return !registry.AllOf<components::BuildingSite>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	return writers;
}
} // namespace

// A standard site is written as the original writes it: its building, its builders and scaffolds the head first, an
// empty list, a 0 word, its ring as 128 points in metres, the builders' count, whether it repairs, the forced desire,
// the life it measures the repair by, then its wood pile; a workshop's site the same without the pile
TEST(SavedWorldWriteBuildingSites, ASiteIsWrittenInTheOriginalsOrder)
{
	ecs::Registry registry;
	const auto writers = SiteWriters();
	const auto building = registry.Create();
	const auto builder = registry.Create();
	const auto scaffold = registry.Create();
	const auto pile = registry.Create();
	const auto entity = registry.Create();
	auto& site = registry.Assign<components::BuildingSite>(entity);
	site.root = building;
	site.builders = {builder};
	site.scaffolds = {scaffold};
	site.ring.at(0) = glm::vec3(1.0f, 2.0f, 3.0f);
	site.ring.at(127) = glm::vec3(4.0f, 5.0f, 6.0f);
	site.builderCount = 1;
	site.isRepairSite = true;
	site.desireBoost = 2.5f;
	site.repairBase = 0.9f;
	site.woodPile = pile;
	site.creationStamp = 1;
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ(writers.ClaimsOf(registry, entity), std::vector {gamesave::StandardBuildingSiteRecord::k_Type});
	const auto written = (*writers.WriterOf(gamesave::StandardBuildingSiteRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::StandardBuildingSiteRecord>(*written);
	EXPECT_EQ(record.rootBuilding.number, context.Reference(building));
	EXPECT_EQ(record.workers.numbers, std::vector {context.Reference(builder)});
	EXPECT_EQ(record.scaffolds.numbers, std::vector {context.Reference(scaffold)});
	EXPECT_TRUE(record.third.numbers.empty());
	EXPECT_EQ(record.word, 0u);
	std::array<float, 3> first {};
	std::memcpy(first.data(), record.positions.data(), sizeof(first));
	EXPECT_EQ(first, (std::array {1.0f, 2.0f, 3.0f}));
	std::array<float, 3> last {};
	std::memcpy(last.data(), record.positions.data() + 127 * 12, sizeof(last));
	EXPECT_EQ(last, (std::array {4.0f, 5.0f, 6.0f}));
	EXPECT_EQ(record.secondWord, 1u);
	EXPECT_EQ(record.thirdWord, 1u);
	EXPECT_EQ(record.forcedDesire, 2.5f);
	EXPECT_EQ(record.life, 0.9f);
	EXPECT_EQ(record.woodPile.number, context.Reference(pile));

	site.kind = components::BuildingSite::Kind::Workshop;
	EXPECT_EQ(writers.ClaimsOf(registry, entity), std::vector {gamesave::WorkshopBuildingSiteRecord::k_Type});
	const auto workshop = (*writers.WriterOf(gamesave::WorkshopBuildingSiteRecord::k_Type))(context, entity);
	ASSERT_TRUE(workshop.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::WorkshopBuildingSiteRecord>(*workshop).life, 0.9f);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// The game's list of building sites runs newest first by the order they were made; a citadel's site is a class of its
// own, written with its six piles by their slots
TEST(SavedWorldWriteBuildingSites, TheListRunsNewestFirstAndACitadelsSiteIsWrittenWithItsPiles)
{
	ecs::Registry registry;
	const auto writers = SiteWriters();
	const auto site = [&registry](uint32_t stamp) {
		const auto entity = registry.Create();
		registry.Assign<components::BuildingSite>(entity).creationStamp = stamp;
		return entity;
	};
	const auto second = site(2);
	const auto first = site(1);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::BuildingSites))(context),
	          (std::vector {context.Reference(second), context.Reference(first)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	const auto pile = registry.Create();
	registry.Assign<components::CitadelBuildingSite>(first).piles.at(2) = pile;
	EXPECT_EQ(writers.ClaimsOf(registry, first), std::vector {gamesave::CitadelBuildingSiteRecord::k_Type});
	const auto written = (*writers.WriterOf(gamesave::CitadelBuildingSiteRecord::k_Type))(context, first);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::CitadelBuildingSiteRecord>(*written).piles.numbers,
	          (std::vector<gamesave::ObjectNumber> {0, 0, context.Reference(pile), 0, 0, 0}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A standard site of the save is made again on its building (made already), with its ring, builders' count, repair
// mark, forced desire and repair base as saved, and goes into its town's list; it is written back as it was read
TEST(SavedWorldWriteBuildingSites, ASavedSiteIsMadeOnItsBuildingAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	// the forests' lists, which a loaded town's forests go into
	test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	gamesave::AbodeRecord abode;
	abode.base.object.info = load::AbodeInfoRowOf(AbodeInfo::CelticHut);
	gamesave::StandardBuildingSiteRecord saved;
	saved.rootBuilding.number = 3;
	const std::array<float, 3> first {1.0f, 2.0f, 3.0f};
	std::memcpy(saved.positions.data(), first.data(), sizeof(first));
	saved.secondWord = 2;
	saved.thirdWord = 1;
	saved.forcedDesire = 2.5f;
	saved.life = 0.9f;
	gamesave::TownRecord town;
	town.buildingSites.numbers = {2};
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(saved);
	world.objects.emplace_back(abode);
	world.objects.emplace_back(town);
	world.loadOrder = {3, 2, 4};
	const auto keeping = [&registry](const auto& record) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::LoadedRecord<std::decay_t<decltype(record)>>>(entity, record);
		return entity;
	};
	entt::entity building = entt::null;
	entt::entity madeTown = entt::null;
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world),
	                                load::Makers {
	                                    .town =
	                                        [&](const load::PlannedTown& planned) {
		                                        madeTown = keeping(planned.record);
		                                        registry.Assign<components::Town>(madeTown);
		                                        return madeTown;
	                                        },
	                                    .abode =
	                                        [&](const load::PlannedAbode& planned, uint32_t /*town*/) {
		                                        building = std::visit(keeping, planned.record);
		                                        return building;
	                                        },
	                                    .buildingSite = load::MakeBuildingSiteEntity,
	                                },
	                                &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto entity = loaded.at(2).entity;
	const auto* site = registry.TryGet<const components::BuildingSite>(entity);
	ASSERT_NE(site, nullptr);
	EXPECT_TRUE(site->root == building);
	EXPECT_EQ(site->ring.at(0), glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(site->builderCount, 2);
	EXPECT_TRUE(site->isRepairSite);
	EXPECT_EQ(site->desireBoost, 2.5f);
	EXPECT_EQ(site->repairBase, 0.9f);
	ASSERT_TRUE(madeTown != entt::null);
	EXPECT_EQ(registry.Get<const components::Town>(madeTown).buildingSites, std::vector {entity});
	const auto line = std::ranges::find(census, gamesave::StandardBuildingSiteRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 1u);

	const auto writers = SiteWriters();
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(gamesave::StandardBuildingSiteRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_TRUE(std::get<gamesave::StandardBuildingSiteRecord>(*written) == saved);
}

// A workshop's site and a citadel's site of the save are made again on their buildings (made already) as a standard
// one is: the workshop's with no wood pile of its own, the citadel's with its six piles by their slots; each is
// written back as it was read
TEST(SavedWorldWriteBuildingSites, ASavedWorkshopsAndCitadelsSitesAreMadeAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	gamesave::AbodeRecord workshop;
	workshop.base.object.info = load::AbodeInfoRowOf(AbodeInfo::CelticHut);
	gamesave::AbodeRecord heart = workshop;
	heart.base.object.creationIndex = 1;
	gamesave::WoodPileRecord pile;
	pile.base.object.info = load::PotInfoRowOf(PotInfo::WoodPile_1);
	gamesave::WorkshopBuildingSiteRecord workshopSite;
	workshopSite.rootBuilding.number = 4;
	workshopSite.secondWord = 1;
	workshopSite.forcedDesire = 1.5f;
	workshopSite.life = 0.5f;
	gamesave::CitadelBuildingSiteRecord citadelSite;
	citadelSite.rootBuilding.number = 5;
	const std::array<float, 3> first {7.0f, 8.0f, 9.0f};
	std::memcpy(citadelSite.positions.data(), first.data(), sizeof(first));
	citadelSite.thirdWord = 1;
	citadelSite.life = 0.25f;
	citadelSite.piles.numbers = {0, 0, 6, 0, 0, 0};
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(workshopSite);
	world.objects.emplace_back(citadelSite);
	world.objects.emplace_back(workshop);
	world.objects.emplace_back(heart);
	world.objects.emplace_back(pile);
	world.loadOrder = {4, 2, 5, 6, 3, 1};
	const auto keeping = [&registry](const auto& record) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::LoadedRecord<std::decay_t<decltype(record)>>>(entity, record);
		return entity;
	};
	LoadedObjects loaded;
	const auto census = load::Apply(
	    load::PlanOf(world),
	    load::Makers {
	        .pile = [&](const load::PlannedPile& planned) { return std::visit(keeping, planned.record); },
	        .abode = [&](const load::PlannedAbode& planned, uint32_t /*town*/) { return std::visit(keeping, planned.record); },
	        .buildingSite = load::MakeBuildingSiteEntity,
	    },
	    &loaded);
	ASSERT_TRUE(loaded.contains(2));
	ASSERT_TRUE(loaded.contains(3));
	ASSERT_TRUE(loaded.contains(6));
	const auto ofWorkshop = loaded.at(2).entity;
	const auto ofCitadel = loaded.at(3).entity;
	const auto& made = registry.Get<const components::BuildingSite>(ofWorkshop);
	EXPECT_EQ(made.kind, components::BuildingSite::Kind::Workshop);
	EXPECT_TRUE(made.root == loaded.at(4).entity);
	EXPECT_TRUE(made.woodPile == entt::null);
	EXPECT_EQ(made.builderCount, 1);
	EXPECT_EQ(made.desireBoost, 1.5f);
	const auto& citadel = registry.Get<const components::BuildingSite>(ofCitadel);
	EXPECT_TRUE(citadel.root == loaded.at(5).entity);
	EXPECT_EQ(citadel.ring.at(0), glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_TRUE(citadel.isRepairSite);
	const auto& piles = registry.Get<const components::CitadelBuildingSite>(ofCitadel).piles;
	EXPECT_TRUE(piles.at(2) == loaded.at(6).entity);
	EXPECT_TRUE(piles.at(0) == entt::null);
	for (const auto type : {gamesave::WorkshopBuildingSiteRecord::k_Type, gamesave::CitadelBuildingSiteRecord::k_Type})
	{
		const auto line = std::ranges::find(census, type, &load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, 1u) << type;
	}

	const auto writers = SiteWriters();
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	EXPECT_EQ(writers.ClaimsOf(registry, ofWorkshop), std::vector {gamesave::WorkshopBuildingSiteRecord::k_Type});
	EXPECT_EQ(writers.ClaimsOf(registry, ofCitadel), std::vector {gamesave::CitadelBuildingSiteRecord::k_Type});
	const auto writtenWorkshop = (*writers.WriterOf(gamesave::WorkshopBuildingSiteRecord::k_Type))(context, ofWorkshop);
	ASSERT_TRUE(writtenWorkshop.has_value()) << context.Failure();
	EXPECT_TRUE(std::get<gamesave::WorkshopBuildingSiteRecord>(*writtenWorkshop) == workshopSite);
	const auto writtenCitadel = (*writers.WriterOf(gamesave::CitadelBuildingSiteRecord::k_Type))(context, ofCitadel);
	ASSERT_TRUE(writtenCitadel.has_value()) << context.Failure();
	// its pile by the number the writers give it: the writers here have no class for a pile, so it is numbered as one
	// made since
	auto expected = citadelSite;
	expected.piles.numbers.at(2) = context.Reference(loaded.at(6).entity);
	EXPECT_TRUE(std::get<gamesave::CitadelBuildingSiteRecord>(*writtenCitadel) == expected);
}
