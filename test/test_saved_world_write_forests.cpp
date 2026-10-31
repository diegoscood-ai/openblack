/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forests', the trees', the big forests', the rocks' and the other mobile statics' records written from what
// openblack holds, with every object's parts: a tree and a rock made in play are written from their state, and the
// frozen Land 1 autosave, its forests, trees, big forests, rocks and mobile statics made as the game's load makes them,
// writes back byte for byte (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <DanceFile.h>
#include <EffectRecords.h>
#include <ForestRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <VillageRecords.h>
#include <VillagerRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "FileSystem/FileSystemInterface.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::SavedRecord;
using gamesave::TreeRecord;

namespace
{
class ForestWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
		RegisterForestWriters(_writers);
		RegisterRiverAndMistWriters(_writers);
		RegisterMobileStaticWriters(_writers);
		RegisterFireWriters(_writers);
		for (auto& row : Info().tree)
		{
			if (!GameData())
			{
				row.normal = MeshId::TreeBeech;
			}
			LoadMesh(row.normal, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 3.0f, 1.0f));
		}
		for (const auto& row : Info().bigForest)
		{
			LoadMesh(row.meshId, glm::vec3(-10.0f, 0.0f, -10.0f), glm::vec3(10.0f, 3.0f, 10.0f));
		}
		for (const auto& row : Info().mobileStatic)
		{
			LoadMesh(row.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 2.0f, 1.0f));
		}
	}

	ClassWriters _writers;

private:
	test::RestoreService<Locator::forestSystem> _restoreForests;
};

/// The index of the first field two records differ in, in their FieldsOf order; -1 when they agree
template <typename Record>
int FirstDifferentField(const Record& written, const Record& saved)
{
	const auto ours = Record::FieldsOf(written);
	const auto theirs = Record::FieldsOf(saved);
	return [&]<size_t... I>(std::index_sequence<I...>) {
		int found = -1;
		static_cast<void>(((std::get<I>(ours) == std::get<I>(theirs) ? false : (found = static_cast<int>(I), true)) || ...));
		return found;
	}(std::make_index_sequence<std::tuple_size_v<decltype(ours)>> {});
}

/// A frozen profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / relative, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

// A tree made in play is written from its state: its kind's row, its place, creation number, life, size and angle, its
// turns to its next growth step, the size it grows to, its forest and its wind slot in its flags word
TEST_P(ForestWriteWorld, ATreeMadeInPlayIsWrittenFromItsState)
{
	const auto forest = Locator::forestSystem::value().Create(7, glm::vec3(2500.0f, k_Ground, 2500.0f));
	const auto entity = ecs::archetypes::TreeArchetype::Create(forest, glm::vec3(2510.0f, k_Ground, 2520.0f), TreeInfo::Beech,
	                                                           false, 0.3f, 1.25f, 0.5f, {.growing = true, .counter = 9});
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(TreeRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value());
	const auto& record = std::get<TreeRecord>(*written);
	EXPECT_EQ(record.base.object.info, ecs::saved_world_load::TreeInfoRowOf(TreeInfo::Beech));
	EXPECT_EQ(record.base.position.position.x, map_coords::ToFixed(2510.0f));
	EXPECT_EQ(record.base.position.position.z, map_coords::ToFixed(2520.0f));
	EXPECT_EQ(record.base.object.coords, record.base.position.position);
	EXPECT_EQ(record.base.position.position.altitude, 0.0f);
	EXPECT_EQ(record.base.object.creationIndex, Registry().Get<const ecs::components::ObjectCreationIndex>(entity).value);
	EXPECT_EQ(record.base.object.life, 1.0f);
	EXPECT_EQ(record.base.object.scale, 0.5f);
	EXPECT_EQ(record.base.object.heading, 0.3f);
	EXPECT_EQ(record.base.object.fire, 0u);
	EXPECT_EQ(record.firstValue, 9);
	EXPECT_EQ(record.secondValue, 1.25f);
	EXPECT_NE(record.forest.number, 0u);
	EXPECT_EQ(record.thirdValue, Registry().Get<const ecs::components::Tree>(entity).windSlot << 2);
	// in the map, counted as fixed
	EXPECT_NE(record.base.position.flags & position_flags::k_InMap, 0);
	EXPECT_EQ(record.base.position.flags & position_flags::k_InPhysics, 0);
}

// A rock made in play is written as a rock, from its state: its kind's row, its place and altitude, its X, Y and Z
// angles and size, made whole, its player, and the two values a rock is made with; a mobile static that is no rock is
// written as a mobile static, with a script's built percentage as it stands
TEST_P(ForestWriteWorld, ARockMadeInPlayIsWrittenFromItsState)
{
	auto& rows = Info().mobileStatic;
	rows.at(static_cast<size_t>(MobileStaticInfo::Rock)).mobileType = static_cast<MobileStaticInfo>(2);
	rows.at(static_cast<size_t>(MobileStaticInfo::CeltFenceShort)).mobileType = static_cast<MobileStaticInfo>(0);
	auto& registry = Registry();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_TWO;
	const auto rock = ecs::archetypes::MobileStaticArchetype::Create(glm::vec3(2510.0f, k_Ground, 2520.0f),
	                                                                 MobileStaticInfo::Rock, 1.5f, 0.25f, 0.5f, 0.75f, 2.0f);
	registry.Get<ecs::components::MobileStatic>(rock).owner = PlayerNames::PLAYER_TWO;
	const auto other = ecs::archetypes::MobileStaticArchetype::Create(
	    glm::vec3(2530.0f, k_Ground, 2520.0f), MobileStaticInfo::CeltFenceShort, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(registry, rock), std::vector<uint32_t> {gamesave::RockRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(registry, other), std::vector<uint32_t> {gamesave::MobileStaticRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::RockRecord::k_Type))(context, rock);
	ASSERT_TRUE(written.has_value());
	const auto& record = std::get<gamesave::RockRecord>(*written);
	EXPECT_EQ(record.base.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Rock));
	EXPECT_EQ(record.base.object.info, 1935u);
	EXPECT_EQ(record.base.position.position.x, map_coords::ToFixed(2510.0f));
	EXPECT_EQ(record.base.position.position.z, map_coords::ToFixed(2520.0f));
	EXPECT_FLOAT_EQ(record.base.position.position.altitude, 1.5f);
	// its target is where it stands, its height over the land too
	EXPECT_EQ(record.base.object.coords, record.base.position.position);
	EXPECT_EQ(record.base.object.heading, 0.5f);
	EXPECT_EQ(record.base.object.scale, 2.0f);
	EXPECT_EQ(std::bit_cast<float>(record.mobileStatic.words[0]), 0.25f);
	EXPECT_EQ(std::bit_cast<float>(record.mobileStatic.words[1]), 0.75f);
	EXPECT_EQ(record.mobileStatic.player, 0u);
	EXPECT_EQ(record.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(record.base.multi.percentBuilt, 0.0f);
	EXPECT_EQ(record.words[0], 0u);
	EXPECT_EQ(std::bit_cast<float>(record.words[1]), -1.0f);
	EXPECT_NE(record.object.number, 0u);
	EXPECT_EQ(record.object.number, context.Reference(player, gamesave::k_PlayerType));
	registry.Assign<ecs::components::StaticBuiltPercentage>(other, 0.5f);
	const auto plain = (*_writers.WriterOf(gamesave::MobileStaticRecord::k_Type))(context, other);
	ASSERT_TRUE(plain.has_value());
	EXPECT_EQ(std::get<gamesave::MobileStaticRecord>(*plain).base.multi.percentBuilt, 0.5f);
}

// A fire is written from its state, keyed by its handle: its charring, the tags, its temperatures, what it burns, its
// player, its flags, the next fire of its group and its root, and the list holds the fires newest first; a fire made
// again from that record burns the same object at the same values
TEST_P(ForestWriteWorld, AFireIsWrittenFromItsStateAndMadeAgainFromItsRecord)
{
	auto& registry = Registry();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_ONE;
	const auto first = ecs::archetypes::MobileStaticArchetype::Create(glm::vec3(2510.0f, k_Ground, 2520.0f),
	                                                                  MobileStaticInfo::Rock, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	const auto second = ecs::archetypes::MobileStaticArchetype::Create(glm::vec3(2530.0f, k_Ground, 2520.0f),
	                                                                   MobileStaticInfo::Rock, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	auto* root = ecs::fire::Restore({.object = first,
	                                 .player = PlayerNames::PLAYER_ONE,
	                                 .temperature = 503.092f,
	                                 .previous = 499.874f,
	                                 .charring = 0.25f,
	                                 .tag = 0,
	                                 .flags = 0x04});
	auto* other = ecs::fire::Restore({.object = second, .temperature = 98.5f, .previous = 98.5f});
	ASSERT_NE(root, nullptr);
	ASSERT_NE(other, nullptr);
	EXPECT_EQ(ecs::fire::Restore({.object = first}), nullptr); // one fire an object
	root->next = other;
	other->root = root;
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(context.Keys(gamesave::FireEffectRecord::k_Type), (std::vector<uint64_t> {other->id, root->id}));
	const auto written = (*_writers.SourceWriterOf(gamesave::FireEffectRecord::k_Type))(context, root->id);
	ASSERT_TRUE(written.has_value());
	const auto& record = std::get<gamesave::FireEffectRecord>(*written);
	EXPECT_EQ(std::bit_cast<float>(record.word), 0.25f);
	EXPECT_EQ(std::bit_cast<float>(record.words[0]), 503.092f);
	EXPECT_EQ(std::bit_cast<float>(record.words[1]), 499.874f);
	EXPECT_EQ(record.objects[0].number, context.Reference(first));
	EXPECT_EQ(record.objects[1].number, 0u);
	EXPECT_EQ(record.objects[2].number, context.Reference(player, gamesave::k_PlayerType));
	EXPECT_EQ(record.objects[3].number, 0u);
	EXPECT_EQ(record.secondByte, 0x04);
	EXPECT_EQ(record.others[0].number,
	          context.Reference(SourceKey {.type = gamesave::FireEffectRecord::k_Type, .id = other->id}));
	EXPECT_EQ(record.others[1].number,
	          context.Reference(SourceKey {.type = gamesave::FireEffectRecord::k_Type, .id = root->id}));
	EXPECT_TRUE(record.burning.numbers.empty());

	// made again from its record, of the object that number names, alone in its group
	Fires().Reset();
	const auto ids = ecs::saved_world_load::MakeFires(
	    {{.number = 7, .record = record, .player = PlayerNames::PLAYER_ONE}}, {3, 5},
	    [&](gamesave::ObjectNumber number) { return number == record.objects[0].number ? first : entt::entity {entt::null}; });
	ASSERT_EQ(ids.size(), 1u);
	auto* made = Fires().FindById(ids[0]);
	ASSERT_NE(made, nullptr);
	EXPECT_EQ(made->object, first);
	EXPECT_EQ(made->temperature, 503.092f);
	EXPECT_EQ(made->previous, 499.874f);
	EXPECT_EQ(made->charring, 0.25f);
	EXPECT_EQ(made->flags, 0x04);
	EXPECT_EQ(made->player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(made->root, made);
	EXPECT_EQ(made->next, nullptr);
	EXPECT_EQ(Fires().ProcessTag(), 3);
	EXPECT_EQ(Fires().CreateTag(), 5);
}

// A villager made in play is written from its state: its row, its flags from what holds each bit, its food and check
// turn, no carried object until its first clip, the move state it was made with and its constructor's counter, and no
// states' values while it is just made; one made in play that has moved holds no move state, and its write fails
TEST_P(ForestWriteWorld, AVillagerMadeInPlayIsWrittenFromItsState)
{
	auto& registry = Registry();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Transform>(entity, glm::vec3(2510.0f, k_Ground, 2520.0f), glm::mat3(1.0f),
	                                            glm::vec3(1.0f));
	auto& villager = registry.Assign<ecs::components::Villager>(entity);
	villager.infoRow = VillagerInfo::NorseHousewifeFemale;
	villager.flags = ecs::components::Villager::k_FlagChild;
	villager.woken = true;
	villager.food = 0.75f;
	villager.lastCheckTurn = 5;
	villager.birthTurn = 3;
	villager.town = entt::null;
	villager.abode = entt::null;
	villager.mother = entt::null;
	registry.Assign<ecs::components::AtHome>(entity).beenToBed = true;
	registry.Assign<ecs::components::LivingAction>(entity, VillagerStates::Created, uint16_t {321});
	registry.Assign<ecs::components::WallHug>(entity);
	RegisterVillagerWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(registry, entity), std::vector<uint32_t> {gamesave::VillagerRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::VillagerRecord>(*written);
	EXPECT_EQ(record.object.info, 2247u - static_cast<uint32_t>(VillagerInfo::NorseHousewifeFemale));
	EXPECT_EQ(record.values.villager.flags, 0x8 | 0x1 | 0x4 | 0x2000);
	EXPECT_EQ(record.values.villager.food, 0.75f);
	EXPECT_EQ(record.values.villager.lastCheckTurn, 5);
	EXPECT_EQ(record.values.villager.carriedObjectType, 0);
	EXPECT_EQ(record.values.villager.home.number, 0u);
	EXPECT_EQ(record.values.move.moveState, 0);
	EXPECT_EQ(record.values.move.turnsToStateChange, 321);
	EXPECT_EQ(record.values.living.action[0], 85);
	EXPECT_TRUE(record.values.topState.empty());
	EXPECT_TRUE(record.values.finalState.empty());

	// made in play, moved on to another state with no move state tag: not held, and the write fails
	registry.Get<ecs::components::LivingAction>(entity).states[0] = static_cast<uint8_t>(VillagerStates::MoveToPos);
	WriteContext moved(_writers, source);
	EXPECT_FALSE((*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(moved, entity).has_value());
	EXPECT_FALSE(moved.Failure().empty());
}

// Integration test: needs the original game's data; skipped without it. A frozen save's worship site (17/3) is made in
// its saved slot of its citadel, with its towns, food pot, chants and totem, and its worshippers once they are made
TEST_P(ForestWriteWorld, AFrozenSavesWorshipSiteIsMadeInItsSlotWithItsTownsAndWorshippers)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "17" / "3" / "17.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen saves are the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	namespace load = ecs::saved_world_load;
	const auto plan = load::PlanOf(world);
	ASSERT_FALSE(plan.worshipSites.empty());
	// the sites' model, whose special points place their totems: the citadel's, as a site takes it with no heart model
	auto& files = Locator::filesystem::value();
	Locator::resources::value().GetMeshes().Load(
	    entt::hashed_string("temple/B_WORSHIP_l3d"), resources::L3DLoader::FromDiskTag {},
	    files.FindPath(files.GetPath<filesystem::Path::Citadel>() / "OutsideMeshes" / "B_WORSHIP_l3d.zzz"));
	const load::Makers makers {.town = load::MakeTownEntity,
	                           .tree = load::MakeTreeEntity,
	                           .bigForest = load::MakeBigForestEntity,
	                           .pile = load::MakePileEntity,
	                           .citadelHeart = load::MakeCitadelHeartEntity,
	                           .worshipSite = load::MakeWorshipSiteEntity,
	                           .villager = load::MakeVillagerEntity};
	LoadedObjects loaded;
	std::ignore = load::Apply(plan, makers, &loaded);
	auto& registry = Registry();
	size_t sites = 0;
	registry.Each<const ecs::components::LoadedRecord<gamesave::WorshipSiteRecord>>([&registry, &sites](entt::entity entity,
	                                                                                                    const auto& kept) {
		const auto& record = kept.record;
		const auto& site = registry.Get<const ecs::components::WorshipSite>(entity);
		EXPECT_EQ(site.slot, record.slot);
		EXPECT_EQ(registry.Get<const ecs::components::CitadelWorship>(site.citadel).sites.at(site.slot), entity);
		EXPECT_EQ(site.towns.size(), record.towns.numbers.size());
		EXPECT_EQ(site.battery, record.chants[0]);
		EXPECT_EQ(site.sacrificed, record.chants[1]);
		EXPECT_NE(site.totem, entt::entity {entt::null});
		// its totem and need signs with their saved creation numbers and records
		const auto* totem = registry.TryGet<const ecs::components::LoadedRecord<gamesave::WorshipTotemRecord>>(site.totem);
		ASSERT_NE(totem, nullptr);
		EXPECT_EQ(registry.Get<const ecs::components::ObjectCreationIndex>(site.totem).value,
		          totem->record.part.base.object.creationIndex);
		// and its own build state as saved
		const auto& altarBuild = registry.Get<const ecs::components::CitadelPartBuild>(site.totem);
		EXPECT_EQ(altarBuild.percentBuilt, totem->record.part.base.multi.percentBuilt);
		EXPECT_EQ(altarBuild.buildFlags, totem->record.part.base.multi.fixedFlags);
		for (const auto sign : site.needsSigns)
		{
			const auto* saved = registry.TryGet<const ecs::components::LoadedRecord<gamesave::ShowNeedsVisualsRecord>>(sign);
			ASSERT_NE(saved, nullptr);
			EXPECT_EQ(registry.Get<const ecs::components::ObjectCreationIndex>(sign).value, saved->record.object.creationIndex);
			EXPECT_EQ(registry.Get<const ecs::components::ShowNeedsVisuals>(sign).desire,
			          std::bit_cast<float>(saved->record.lastWord));
		}
		EXPECT_LE(site.villagers.size(), record.worshipping.numbers.size());
		// its spell icons, in its list's order, each of the site with its saved creation number
		ASSERT_EQ(site.icons.size(), record.icons.numbers.size());
		for (const auto icon : site.icons)
		{
			EXPECT_EQ(registry.Get<const ecs::components::WorshipSpellIcon>(icon).site, entity);
			const auto& saved = registry.Get<const ecs::components::LoadedRecord<gamesave::WorshipSpellIconRecord>>(icon);
			EXPECT_EQ(registry.Get<const ecs::components::ObjectCreationIndex>(icon).value,
			          saved.record.icon.base.object.creationIndex);
		}
		++sites;
	});
	EXPECT_EQ(sites, plan.worshipSites.size());
}

// Integration test: needs the original game's data; skipped without it. A villager of the frozen Land 1 autosave put in
// a dance's state (in a dance, 5) with its two words is made, as nothing of those states reads them, and is written back
// with them as loaded; one dead is made with its cause and written back with it; in a state whose values openblack
// does not load (building, 39) it is not made
TEST_P(ForestWriteWorld, AVillagerInADancesStateIsMadeAndWrittenBackWithItsWords)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const gamesave::VillagerRecord* found = nullptr;
	gamesave::ObjectNumber number = 0;
	for (size_t i = 0; i < world.objects.size() && found == nullptr; ++i)
	{
		if (world.objects[i].has_value())
		{
			found = std::get_if<gamesave::VillagerRecord>(&*world.objects[i]);
			number = static_cast<gamesave::ObjectNumber>(i + 1);
		}
	}
	ASSERT_NE(found, nullptr);
	auto record = *found;
	record.values.living.action[0] = 5;
	gamesave::StateValue first {.size = 4};
	gamesave::StateValue second {.size = 4};
	first.bytes[0] = 0x14;
	second.bytes[0] = 0x93;
	second.bytes[1] = 0x01;
	record.values.topState = {first, second};
	namespace load = ecs::saved_world_load;
	const load::PlannedVillager planned {
	    .number = number, .type = load::VillagerTypeOfInfo(record.object.info).value_or(VillagerInfo::None), .record = record};
	const auto entity = load::MakeVillagerEntity(planned, std::nullopt);
	ASSERT_NE(entity, entt::entity {entt::null});
	auto& registry = Registry();
	EXPECT_EQ(registry.Get<const ecs::components::LivingAction>(entity).states[0], 5);
	if ((record.position.flags & position_flags::k_DrawListed) != 0)
	{
		registry.AssignOrReplace<ecs::components::DrawListed>(entity);
	}
	RegisterVillagerWriters(_writers);
	LoadedObjects loaded;
	loaded.try_emplace(number, LoadedObject {.entity = entity, .type = gamesave::VillagerRecord::k_Type});
	const WorldSource source {.registry = registry, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*written).values.topState, record.values.topState);
	// one that entered a dance's state in play writes the 0 and 0 it is made with
	registry.Get<ecs::components::LivingAction>(entity).states[0] = 193;
	WriteContext entered(_writers, source);
	const auto inPlay = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(entered, entity);
	ASSERT_TRUE(inPlay.has_value()) << entered.Failure();
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*inPlay).values.topState,
	          (std::vector<gamesave::StateValue> {{.size = 4}, {.size = 4}}));

	record.values.living.action[0] = 39;
	const load::PlannedVillager building {.number = number, .type = planned.type, .record = record};
	EXPECT_EQ(load::MakeVillagerEntity(building, std::nullopt), entt::entity {entt::null});

	// dead (13), of the cause its state's byte names: made with it, and written back with it
	record.values.living.action[0] = 13;
	gamesave::StateValue cause {.size = 1};
	cause.bytes[0] = static_cast<uint8_t>(DeathReason::Starving);
	record.values.topState = {cause};
	const load::PlannedVillager dead {.number = number, .type = planned.type, .record = record};
	const auto deadEntity = load::MakeVillagerEntity(dead, std::nullopt);
	ASSERT_NE(deadEntity, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<const ecs::components::Villager>(deadEntity).deathReason, DeathReason::Starving);
	WriteContext deadContext(_writers, source);
	const auto deadWritten = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(deadContext, deadEntity);
	ASSERT_TRUE(deadWritten.has_value()) << deadContext.Failure();
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*deadWritten).values.topState, record.values.topState);

	// felling a tree: made, and written with the tree it holds as its target
	uint8_t felling = 0;
	while (gamesave::VillagerStateSaveOf(felling) != gamesave::VillagerStateSave::Foresting)
	{
		++felling;
	}
	record.values.living.action[0] = felling;
	gamesave::StateValue target {.size = gamesave::k_StateReference};
	record.values.topState = {target};
	const load::PlannedVillager forester {.number = number, .type = planned.type, .record = record};
	const auto foresterEntity = load::MakeVillagerEntity(forester, std::nullopt);
	ASSERT_NE(foresterEntity, entt::entity {entt::null});
	const auto forest = Locator::forestSystem::value().Create(7, glm::vec3(2500.0f, k_Ground, 2500.0f));
	const auto tree = ecs::archetypes::TreeArchetype::Create(forest, glm::vec3(2510.0f, k_Ground, 2520.0f), TreeInfo::Beech,
	                                                         false, 0.0f, 1.0f, 1.0f, {.growing = false, .counter = 0});
	registry.Get<ecs::components::Villager>(foresterEntity).targetThing = tree;
	WriteContext foresterContext(_writers, source);
	const auto foresterWritten = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(foresterContext, foresterEntity);
	ASSERT_TRUE(foresterWritten.has_value()) << foresterContext.Failure();
	const auto& saved = std::get<gamesave::VillagerRecord>(*foresterWritten).values.topState;
	ASSERT_EQ(saved.size(), 1u);
	EXPECT_EQ(saved[0].number, foresterContext.Reference(tree));
	EXPECT_NE(saved[0].number, 0u);
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 1 autosave's 55 villagers are
// made again: each of its row, in its saved states, with its saved food and birth, its counter drawn again from 1 to
// 20 as its states load nothing, in its home's villagers; and each written back is its record, but for the counter
TEST_P(ForestWriteWorld, TheFrozenLandOneAutosavesVillagersAreMadeAndWrittenBack)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	namespace load = ecs::saved_world_load;
	// the towns and the buildings keep their records, so that a villager's home and town are named as saved
	auto& lookup = Registry();
	const auto keeping = [&lookup]<typename Record>(const Record& record) {
		const auto entity = lookup.Create();
		lookup.Assign<ecs::components::LoadedRecord<Record>>(entity, record);
		return entity;
	};
	const auto keepingOne = [&keeping](const auto& variant) {
		return std::visit([&keeping](const auto& record) { return keeping(record); }, variant);
	};
	const load::Makers makers {
	    .town = [&](const load::PlannedTown& town) { return keeping(town.record); },
	    .tree = load::MakeTreeEntity,
	    .bigForest = load::MakeBigForestEntity,
	    .storagePit = [&](const load::PlannedStoragePit& pit, uint32_t /*town*/,
	                      const ecs::archetypes::AbodeArchetype::StoragePitPiles& /*piles*/) { return keeping(pit.record); },
	    .abode = [&](const load::PlannedAbode& abode, uint32_t /*town*/) { return keepingOne(abode.record); },
	    .villager = load::MakeVillagerEntity};
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), makers, &loaded);
	size_t made = 0;
	for (const auto& entry : census)
	{
		if (entry.type == gamesave::VillagerRecord::k_Type || entry.type == gamesave::SpecialVillagerRecord::k_Type)
		{
			EXPECT_EQ(entry.made, entry.saved) << entry.type;
			made += entry.made;
		}
	}
	EXPECT_EQ(made, 55u);
	RegisterVillagerWriters(_writers);
	auto& registry = Registry();
	const WorldSource source {.registry = registry, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto check = [&]<typename Record>(entt::entity entity, const Record& saved) {
		// a load clears the draw-list mark and the next drawn frame lists it again; no frame is drawn here
		if ((saved.position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.AssignOrReplace<ecs::components::DrawListed>(entity);
		}
		const auto& action = registry.Get<const ecs::components::LivingAction>(entity);
		EXPECT_EQ(action.states[0], saved.values.living.action[0]);
		EXPECT_GE(action.turnsUntilStateChange, 1);
		EXPECT_LE(action.turnsUntilStateChange, 20);
		const auto& villager = registry.Get<const ecs::components::Villager>(entity);
		EXPECT_EQ(villager.food, saved.values.villager.food);
		EXPECT_EQ(villager.birthTurn, saved.values.living.birthTurn);
		const auto written = (*_writers.WriterOf(Record::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		auto expected = saved;
		expected.values.move.turnsToStateChange = std::bit_cast<int16_t>(action.turnsUntilStateChange);
		EXPECT_EQ(std::get<Record>(*written), expected);
	};
	registry.Each<const ecs::components::LoadedRecord<gamesave::VillagerRecord>>(
	    [&check](entt::entity entity, const auto& kept) { check(entity, kept.record); });
	registry.Each<const ecs::components::LoadedRecord<gamesave::SpecialVillagerRecord>>(
	    [&check](entt::entity entity, const auto& kept) { check(entity, kept.record); });
}

// A dance made in play that has not started is written from its state: its place, clock, groups and their order, its
// speeds, the camera as made, its type, owner, state and times and the dances' label; each group by its dance and
// number, as a living dancing in it names it; a dance that has started, or an untitled group, is not held and fails
TEST_P(ForestWriteWorld, ADanceMadeInPlayIsWrittenWithItsGroups)
{
	auto& registry = Registry();
	const auto entity = registry.Create();
	auto& dance = registry.Assign<ecs::components::Dance>(entity);
	dance.type = static_cast<DanceInfo>(15);
	dance.place = {.x = 0x0A000000, .z = 0x0B000000, .altitude = 0.0f};
	dance.speed = 0.25f;
	dance.rate = 0.8f;
	dance.clock = 12.0f;
	dance.loopLength = 1;
	dance.duration = 900;
	dance.madeByScript = true;
	dance.groups.all.resize(2);
	dance.groups.all[0].name = "G1";
	dance.groups.all[0].shape = 3;
	dance.groups.all[0].radius = 12.5f;
	dance.groups.all[1].name = "G2";
	dance.groups.all[1].parent = 0;
	dance.groups.all[0].children = {1};
	dance.groups.shared = {0, 1};
	dance.groups.roundLength = 2;
	RegisterDanceWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::DanceRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::DanceRecord>(*written);
	EXPECT_EQ(record.type, 15u);
	EXPECT_EQ(record.group.time, 12.0f);
	EXPECT_EQ(record.group.length, 1u);
	EXPECT_EQ(record.group.editorValues, (std::array<uint32_t, 2> {30, 0}));
	EXPECT_EQ(record.group.danceGroups.numbers.size(), 2u);
	EXPECT_TRUE(record.group.fixedGroups.numbers.empty());
	EXPECT_EQ(record.group.proportionGroups.numbers, record.group.danceGroups.numbers);
	EXPECT_EQ(record.group.denominator, 2u);
	EXPECT_EQ(record.group.speed, 0.8f);
	EXPECT_EQ(record.group.flags, (std::array<int32_t, 4> {0, 1, 0, 0}));
	EXPECT_EQ(record.group.cameraTravelTime, 2.0f);
	EXPECT_EQ(record.worshipSpeed, 0.25f);
	EXPECT_EQ(record.words, (std::array<uint32_t, 6> {1, 900, 0, 0, 0, 0}));
	EXPECT_EQ(std::string(reinterpret_cast<const char*>(record.label.data())), "begin");
	// its groups, by its dance and number, as a dancer names its group
	const SourceKey second {.type = gamesave::DanceGroupRecord::k_Type, .id = PartId(entity, 1)};
	EXPECT_EQ(record.group.danceGroups.numbers[1], context.Reference(second));
	const auto group = (*_writers.SourceWriterOf(gamesave::DanceGroupRecord::k_Type))(context, second.id);
	ASSERT_TRUE(group.has_value()) << context.Failure();
	const auto& part = std::get<gamesave::DanceGroupRecord>(*group);
	EXPECT_EQ(part.words[0], 1u);
	EXPECT_EQ(part.other.number, record.group.danceGroups.numbers[0]);
	EXPECT_EQ(part.secondInt, 1311);
	EXPECT_EQ(std::string(part.text.data()), "G2");
	EXPECT_EQ(context.Keys(gamesave::DanceGroupRecord::k_Type), (std::vector<uint64_t> {PartId(entity, 0), PartId(entity, 1)}));

	// started, or with an untitled group: not held
	dance.groups.all[1].name.clear();
	WriteContext untitled(_writers, source);
	EXPECT_FALSE((*_writers.SourceWriterOf(gamesave::DanceGroupRecord::k_Type))(untitled, second.id).has_value());
	dance.startTurn = 40;
	WriteContext started(_writers, source);
	EXPECT_FALSE((*_writers.WriterOf(gamesave::DanceRecord::k_Type))(started, entity).has_value());
	EXPECT_FALSE(started.Failure().empty());
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 1 save of a blaze holds one
// dance, of the Ritual (type 15), waiting with its two groups: a dance entity holding what its record says, with its
// file read from the game's data, writes back its record, its groups', its key frames' and their actions' as saved
TEST_P(ForestWriteWorld, AFrozenSavesDanceWritesBackItsRecordsAsSaved)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "14" / "1" / "14.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen saves are the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	constexpr gamesave::ObjectNumber k_Dance = 1858;
	ASSERT_TRUE(world.objects.at(k_Dance - 1).has_value());
	const auto& saved = std::get<gamesave::DanceRecord>(*world.objects.at(k_Dance - 1));
	const auto recordOf = [&world](gamesave::ObjectNumber number) -> const gamesave::SavedRecord& {
		return *world.objects.at(number - 1);
	};
	auto& registry = Registry();
	LoadedObjects loaded;
	// an object it names, keeping its record, by its number
	const auto keeping = [&](gamesave::ObjectNumber number) {
		if (number == 0)
		{
			return entt::entity {entt::null};
		}
		const auto entity = registry.Create();
		const auto& record = recordOf(number);
		// the classes a dance names here: its target, a mobile static, and its dancers, villagers
		uint32_t type = 0;
		if (const auto* statics = std::get_if<gamesave::MobileStaticRecord>(&record))
		{
			registry.Assign<ecs::components::LoadedRecord<gamesave::MobileStaticRecord>>(entity, *statics);
			type = gamesave::MobileStaticRecord::k_Type;
		}
		else if (const auto* villager = std::get_if<gamesave::VillagerRecord>(&record))
		{
			registry.Assign<ecs::components::LoadedRecord<gamesave::VillagerRecord>>(entity, *villager);
			type = gamesave::VillagerRecord::k_Type;
		}
		else if (const auto* special = std::get_if<gamesave::SpecialVillagerRecord>(&record))
		{
			registry.Assign<ecs::components::LoadedRecord<gamesave::SpecialVillagerRecord>>(entity, *special);
			type = gamesave::SpecialVillagerRecord::k_Type;
		}
		EXPECT_NE(type, 0u) << "object " << number << " of a class this test does not keep";
		loaded.try_emplace(number, LoadedObject {.entity = entity, .type = type});
		return entity;
	};

	const auto entity = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::DanceRecord>>(entity, saved);
	loaded.try_emplace(k_Dance, LoadedObject {.entity = entity, .type = gamesave::DanceRecord::k_Type});
	auto& dance = registry.Assign<ecs::components::Dance>(entity);
	dance.type = static_cast<DanceInfo>(saved.type);
	dance.place = {.x = saved.group.position.position.x,
	               .z = saved.group.position.position.z,
	               .altitude = saved.group.position.position.altitude};
	dance.clock = saved.group.time;
	dance.loopLength = saved.group.length;
	dance.dancers = saved.group.dancers;
	dance.groups.round = saved.group.proportionCounter;
	dance.groups.roundLength = saved.group.denominator;
	dance.rate = saved.group.speed;
	dance.dancingRate = saved.group.lastSpeed;
	dance.madeByScript = saved.group.flags[1] != 0;
	dance.speed = saved.worshipSpeed;
	dance.owner = keeping(saved.target.number);
	dance.state = saved.words[0] == 0 ? ecs::components::Dance::State::Dancing : ecs::components::Dance::State::Stopped;
	dance.duration = saved.words[1];
	dance.startTurn = saved.words[2];
	if (saved.words[3] != 0)
	{
		dance.firstDancerTurn = saved.words[4];
	}
	dance.onTheirWay = saved.words[5];
	// its file, from the game's data
	const auto& info = Info().dance.at(saved.type);
	std::string name(info.fileName.data(), strnlen(info.fileName.data(), info.fileName.size()));
	std::ranges::replace(name, '\\', '/');
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	std::ifstream stream(std::filesystem::path(game) / name, std::ios::binary);
	ASSERT_TRUE(stream) << name;
	const std::vector<uint8_t> fileBytes(std::istreambuf_iterator<char>(stream), {});
	auto file = std::make_shared<dance::DanceFile>();
	file->Open(fileBytes);
	dance.file = file;

	// its groups, each as its record says, by its number in the dance
	const auto& numbers = saved.group.danceGroups.numbers;
	const auto indexOf = [&numbers](gamesave::ObjectNumber number) {
		return static_cast<size_t>(std::ranges::find(numbers, number) - numbers.begin());
	};
	for (size_t index = 0; index < numbers.size(); ++index)
	{
		const auto& record = std::get<gamesave::DanceGroupRecord>(recordOf(numbers[index]));
		loaded.try_emplace(numbers[index], LoadedObject {.type = gamesave::DanceGroupRecord::k_Type,
		                                                 .id = PartId(entity, static_cast<uint32_t>(index))});
		auto& group = dance.groups.all.emplace_back();
		group.name = std::string(record.text.data(), strnlen(record.text.data(), record.text.size()));
		for (const auto member : record.members.numbers)
		{
			group.dancers.push_back(keeping(member));
		}
		group.flag = record.words[1] != 0;
		group.formation = record.words[2];
		group.shape = record.words[3];
		std::memcpy(&group.offset, record.eightBytes.data(), record.eightBytes.size());
		group.radius = std::bit_cast<float>(record.secondWords[0]);
		group.rotation = std::bit_cast<float>(record.secondWords[2]);
		std::array<uint32_t, 4> move {};
		std::memcpy(move.data(), record.sixteenBytes.data(), record.sixteenBytes.size());
		group.move = {.action = move[0], .first = move[1], .second = move[2], .end = move[3]};
		group.spin = record.value;
		if (record.other.number != 0)
		{
			group.parent = indexOf(record.other.number);
		}
		group.indexInParent = record.byte;
		group.limitedDancers = record.thirdWord;
		group.moved = record.firstInt != 0;
		group.danceType = record.danceType;
		group.inFormation = record.fourthWord;
		std::array<uint32_t, 3> proportion {};
		std::memcpy(proportion.data(), record.proportion.data(), record.proportion.size());
		group.limited = proportion[0] != 0;
		group.quota = proportion[1];
		group.weight = proportion[2];
		for (const auto child : record.stack.values)
		{
			group.children.push_back(child);
		}
		group.sexes = record.villagerType;
	}
	for (const auto number : saved.group.fixedGroups.numbers)
	{
		dance.groups.limited.push_back(indexOf(number));
	}
	for (const auto number : saved.group.proportionGroups.numbers)
	{
		dance.groups.shared.push_back(indexOf(number));
	}
	// its key frames and their actions, by their places in its file
	const auto& frames = saved.group.keyFrames.numbers;
	for (size_t frame = 0; frame < frames.size(); ++frame)
	{
		loaded.try_emplace(frames[frame], LoadedObject {.type = gamesave::DanceKeyFrameRecord::k_Type,
		                                                .id = PartId(entity, static_cast<uint32_t>(frame))});
		const auto& actions = std::get<gamesave::DanceKeyFrameRecord>(recordOf(frames[frame])).actions.numbers;
		for (size_t action = 0; action < actions.size(); ++action)
		{
			loaded.try_emplace(actions[action],
			                   LoadedObject {.type = gamesave::DanceKeyActionRecord::k_Type,
			                                 .id = PartId(entity, static_cast<uint32_t>((frame << 16U) | action))});
		}
	}

	RegisterDanceWriters(_writers);
	const WorldSource source {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::DanceRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& writtenDance = std::get<gamesave::DanceRecord>(*written);
	EXPECT_EQ(writtenDance.thing, saved.thing);
	EXPECT_EQ(FirstDifferentField(writtenDance.group, saved.group), -1);
	EXPECT_EQ(FirstDifferentField(writtenDance, saved), -1);
	for (const auto number : numbers)
	{
		const auto target = context.TargetOf(number);
		ASSERT_TRUE(target.has_value() && target->id.has_value()) << number;
		const auto group = (*_writers.SourceWriterOf(gamesave::DanceGroupRecord::k_Type))(context, *target->id);
		ASSERT_TRUE(group.has_value()) << context.Failure();
		const auto& writtenGroup = std::get<gamesave::DanceGroupRecord>(*group);
		const auto& savedGroup = std::get<gamesave::DanceGroupRecord>(recordOf(number));
		EXPECT_EQ(writtenGroup.thing, savedGroup.thing) << "group " << number;
		EXPECT_EQ(FirstDifferentField(writtenGroup, savedGroup), -1) << "group " << number;
	}
	ASSERT_EQ(file->keyFrames.size(), frames.size());
	for (size_t frame = 0; frame < frames.size(); ++frame)
	{
		const auto keyFrame = (*_writers.SourceWriterOf(gamesave::DanceKeyFrameRecord::k_Type))(
		    context, PartId(entity, static_cast<uint32_t>(frame)));
		ASSERT_TRUE(keyFrame.has_value());
		const auto& savedFrame = std::get<gamesave::DanceKeyFrameRecord>(recordOf(frames[frame]));
		// the stream's running sum is put by the file's writer, not the record's
		EXPECT_EQ(FirstDifferentField(std::get<gamesave::DanceKeyFrameRecord>(*keyFrame), savedFrame), -1)
		    << "key frame " << frame;
		for (size_t action = 0; action < savedFrame.actions.numbers.size(); ++action)
		{
			const auto written = (*_writers.SourceWriterOf(gamesave::DanceKeyActionRecord::k_Type))(
			    context, PartId(entity, static_cast<uint32_t>((frame << 16U) | action)));
			ASSERT_TRUE(written.has_value());
			const auto& writtenAction = std::get<gamesave::DanceKeyActionRecord>(*written);
			const auto& savedAction = std::get<gamesave::DanceKeyActionRecord>(recordOf(savedFrame.actions.numbers[action]));
			EXPECT_EQ(writtenAction.words.values, savedAction.words.values) << "key frame " << frame << " action " << action;
			EXPECT_EQ(FirstDifferentField(writtenAction, savedAction), -1) << "key frame " << frame << " action " << action;
		}
	}
}

namespace
{
/// A frozen save made as the game's load makes the classes these writers write, every other object keeping its
/// record, and written back by these writers and the game object's: compared with the original's file byte for byte;
/// of each class, how many records its writer wrote
std::map<uint32_t, size_t> WriteBackCounts(ClassWriters& writers, const std::vector<uint8_t>& bytes)
{
	const auto world = gamesave::ReadWorld(bytes);
	EXPECT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto state = ecs::saved_game_load::StateOf(world);
	if (!state.has_value())
	{
		ADD_FAILURE() << "no game state";
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto keeping = [&registry]<typename Record>(const Record& record) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::LoadedRecord<Record>>(entity, record);
		return entity;
	};
	const auto keepingOne = [&keeping](const auto& variant) {
		return std::visit([&keeping](const auto& record) { return keeping(record); }, variant);
	};
	namespace load = ecs::saved_world_load;
	const load::Makers makers {
	    .footpath = [&](const load::PlannedFootpath& footpath) { return keeping(footpath.record); },
	    .footpathLink = [&](const load::PlannedFootpathLink& link,
	                        const std::vector<entt::entity>& /*footpaths*/) { return keeping(link.record); },
	    .town = [&](const load::PlannedTown& town) { return keeping(town.record); },
	    .tree = load::MakeTreeEntity,
	    .bigForest = load::MakeBigForestEntity,
	    .pile = [&](const load::PlannedPile& pile) { return keepingOne(pile.record); },
	    .storagePit = [&](const load::PlannedStoragePit& pit, uint32_t /*town*/,
	                      const ecs::archetypes::AbodeArchetype::StoragePitPiles& /*piles*/) { return keeping(pit.record); },
	    .citadelHeart =
	        [&](const load::PlannedCitadelHeart& heart, entt::entity citadel) {
		        const auto entity = keeping(heart.record);
		        if (citadel == entt::null)
		        {
			        registry.Assign<ecs::components::LoadedRecord<gamesave::CitadelRecord>>(entity, heart.citadel);
		        }
		        return entity;
	        },
	    .abode = [&](const load::PlannedAbode& abode, uint32_t /*town*/) { return keepingOne(abode.record); },
	    .desireFlag = [&](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return keeping(flag.record); },
	    .mobileStatic = load::MakeMobileStaticEntity,
	    .stream = load::MakeStreamEntity,
	    .mist = load::MakeMistEntity,
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	    .fires = load::MakeFires,
	};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), makers, &loaded);
	// a load clears every object's draw-list mark and the next drawn frame lists the objects in view again; this test
	// draws no frame, so the objects the save had listed are listed as that frame would
	registry.Each<const ecs::components::LoadedRecord<TreeRecord>>([&registry](entt::entity entity, const auto& kept) {
		if ((kept.record.base.position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.Assign<ecs::components::DrawListed>(entity);
		}
	});
	registry.Each<const ecs::components::LoadedRecord<gamesave::BigForestRecord>>(
	    [&registry](entt::entity entity, const auto& kept) {
		    if ((kept.record.base.position.flags & position_flags::k_DrawListed) != 0)
		    {
			    registry.Assign<ecs::components::DrawListed>(entity);
		    }
	    });
	const auto listedAsKept = [&registry](entt::entity entity, const auto& kept) {
		if ((kept.base.position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.AssignOrReplace<ecs::components::DrawListed>(entity);
		}
	};
	registry.Each<const ecs::components::LoadedRecord<gamesave::RockRecord>>(
	    [&listedAsKept](entt::entity entity, const auto& kept) { listedAsKept(entity, kept.record); });
	registry.Each<const ecs::components::LoadedRecord<gamesave::MobileStaticRecord>>(
	    [&listedAsKept](entt::entity entity, const auto& kept) { listedAsKept(entity, kept.record); });
	// the creation counter as the save left it
	ecs::object_index::Restore(state->createdObjects);

	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	EXPECT_TRUE(written.Complete()) << written.failure;
	std::map<uint32_t, size_t> byWriter;
	for (const auto& record : written.records)
	{
		if (record.source == RecordSource::Writer)
		{
			++byWriter[record.type];
		}
	}
	EXPECT_EQ(written.bytes.size(), bytes.size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	if (written.bytes.size() == bytes.size())
	{
		const auto difference =
		    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes.begin() + k_Counter, bytes.end());
		EXPECT_TRUE(difference.first == written.bytes.end())
		    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
	}
	return byWriter;
}
} // namespace

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave with its forests, trees, big forests, rocks, mobile statics, rivers and mists made as the game's load makes
// them, and every other object keeping its record, written back: the original's file byte for byte, every tree, big
// forest, forest, rock and mobile static by its writer
TEST_P(ForestWriteWorld, TheFrozenLandOneAutosaveWithItsForestsAndRocksMadeWritesBackByteForByte)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	auto counts = WriteBackCounts(_writers, *bytes);
	EXPECT_GT(counts[TreeRecord::k_Type] + counts[gamesave::BigForestRecord::k_Type] + counts[gamesave::ForestRecord::k_Type],
	          1000u);
	EXPECT_EQ(counts[gamesave::RockRecord::k_Type], 132u);
	EXPECT_EQ(counts[gamesave::MobileStaticRecord::k_Type], 109u);
}

// Integration test: needs the original game's data; skipped without it. A frozen Land 1 save of a blaze, its 13 fires
// in one group made again on what they burn, written back byte for byte, every fire made by its writer
TEST_P(ForestWriteWorld, AFrozenSaveOfABlazeWritesBackItsFiresByteForByte)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "14" / "1" / "14.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen saves are the game's data";
	}
	auto counts = WriteBackCounts(_writers, *bytes);
	// one of the 13 burns an object this load does not make: it is not made, and is written as it was loaded
	EXPECT_EQ(counts[gamesave::FireEffectRecord::k_Type], 12u);
}

INSTANTIATE_TEST_SUITE_P(Data, ForestWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
