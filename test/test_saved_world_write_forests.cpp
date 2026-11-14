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
#include <array>
#include <bit>
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
#include <GameTailRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <VillageRecords.h>
#include <VillagerRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "3D/CameraEdits.h"
#include "3D/MapCoords.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptMemory.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/ShepherdWork.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/LivingFootpath.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/WalkStateOf.h"
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
		for (const auto& row : Info().feature)
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

// A forest made in play is written as the game makes one: its container at its centre in map units with no height and
// no flags, of no row and no owner, no footpath link, its id, its trees and the game's two words
TEST_P(ForestWriteWorld, AForestMadeInPlayIsWrittenAsTheGameMakesOne)
{
	auto& forests = Locator::forestSystem::value();
	const auto forest = forests.Create(7, glm::vec3(2500.0f, k_Ground, 2500.0f));
	const auto tree = ecs::archetypes::TreeArchetype::Create(forest, glm::vec3(2510.0f, k_Ground, 2520.0f), TreeInfo::Beech,
	                                                         false, 0.3f, 1.25f, 1.25f, {.growing = false});
	const WorldSource source {.registry = Registry()};
	WriteContext context(_writers, source);
	const auto* writer = _writers.SourceWriterOf(gamesave::ForestRecord::k_Type);
	ASSERT_NE(writer, nullptr);
	const auto written = (*writer)(context, forest);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::ForestRecord>(*written);
	EXPECT_EQ(record.container.position.position.x, map_coords::ToFixed(2500.0f));
	EXPECT_EQ(record.container.position.position.z, map_coords::ToFixed(2500.0f));
	EXPECT_EQ(record.container.position.position.altitude, 0.0f);
	EXPECT_EQ(record.container.position.flags, 0u);
	EXPECT_EQ(record.container.info, 0xFFFFFFFFu);
	EXPECT_EQ(record.container.owner.number, 0u);
	EXPECT_EQ(record.footpathLink.number, 0u);
	EXPECT_EQ(record.id, forests.All().at(forest).id);
	EXPECT_EQ(record.trees.numbers, (std::vector {context.Reference(tree)}));
	EXPECT_TRUE(record.otherTrees.numbers.empty());
	EXPECT_EQ(record.secondGameWord, forests.NextId());
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

// A dead tree made in play is written as a dead tree of the dead tree's rock row: its player, the two values a rock is
// made with, the row of the tree it was and its wood multiplier; one a forester felled is a felled tree; a bonfire a
// bonfire of its own row with its player
TEST_P(ForestWriteWorld, ADeadTreeAFelledTreeAndABonfireMadeInPlayAreWrittenFromTheirState)
{
	auto& registry = Registry();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_ONE;
	const auto dead = ecs::archetypes::DeadTreeArchetype::Create(glm::vec3(2510.0f, k_Ground, 2520.0f), TreeInfo::Birch,
	                                                             PlayerNames::PLAYER_ONE, 1.0f, 0.0f, 0.5f, 0.0f);
	registry.Get<ecs::components::DeadTree>(dead).woodValueMultiplier = 1.5f;
	const auto felled = ecs::archetypes::DeadTreeArchetype::Create(glm::vec3(2530.0f, k_Ground, 2520.0f), TreeInfo::Cedar,
	                                                               PlayerNames::PLAYER_ONE, 1.0f, 0.0f, 0.0f, 0.0f);
	registry.Assign<ecs::components::FelledTree>(felled);
	// a bonfire's own making also lights its spot visual: a mobile static of its row is all the writer reads
	const auto bonfire = ecs::archetypes::MobileStaticArchetype::Create(
	    glm::vec3(2550.0f, k_Ground, 2520.0f), MobileStaticInfo::CeltFenceShort, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	registry.Get<ecs::components::MobileStatic>(bonfire).type = MobileStaticInfo::Bonfire;
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(registry, dead), std::vector<uint32_t> {gamesave::DeadTreeRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(registry, felled), std::vector<uint32_t> {gamesave::FelledTreeRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(registry, bonfire), std::vector<uint32_t> {gamesave::BonfireRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::DeadTreeRecord::k_Type))(context, dead);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::DeadTreeRecord>(*written);
	EXPECT_EQ(record.base.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::DeadTree));
	EXPECT_EQ(record.info, ecs::saved_world_load::TreeInfoRowOf(TreeInfo::Birch));
	EXPECT_EQ(std::bit_cast<float>(record.word), 1.5f);
	EXPECT_EQ(record.words[0], 0u);
	EXPECT_EQ(std::bit_cast<float>(record.words[1]), -1.0f);
	EXPECT_EQ(record.object.number, context.Reference(player, gamesave::k_PlayerType));
	EXPECT_EQ(record.base.multi.fixedFlags, 0x08);
	const auto log = (*_writers.WriterOf(gamesave::FelledTreeRecord::k_Type))(context, felled);
	ASSERT_TRUE(log.has_value()) << context.Failure();
	EXPECT_EQ(std::get<gamesave::FelledTreeRecord>(*log).info, ecs::saved_world_load::TreeInfoRowOf(TreeInfo::Cedar));
	const auto fire = (*_writers.WriterOf(gamesave::BonfireRecord::k_Type))(context, bonfire);
	ASSERT_TRUE(fire.has_value()) << context.Failure();
	const auto& bonfireRecord = std::get<gamesave::BonfireRecord>(*fire);
	EXPECT_EQ(bonfireRecord.base.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Bonfire));
	EXPECT_EQ(bonfireRecord.word, 0u);
	EXPECT_EQ(std::bit_cast<float>(bonfireRecord.words[1]), -1.0f);
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

// A fire's reaction is written by the reactions' writer while it lasts and as none once it has ended; one its load
// named and did not make again stays as kept. The load gives each made fire the reaction its record names, made again
TEST_P(ForestWriteWorld, AFiresReactionIsWrittenWhileItLastsAndGivenBackByTheLoad)
{
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto rock = ecs::archetypes::MobileStaticArchetype::Create(glm::vec3(2510.0f, k_Ground, 2520.0f),
	                                                                 MobileStaticInfo::Rock, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	auto* fire = ecs::fire::Restore({.object = rock, .temperature = 503.0f, .previous = 500.0f});
	ASSERT_NE(fire, nullptr);
	fire->reaction = reactions.Add({.initiator = rock, .type = openblack::Reaction::ReactToFire});
	const auto write = [this, &fire](const WorldSource& source) {
		WriteContext context(_writers, source);
		const auto written = (*_writers.SourceWriterOf(gamesave::FireEffectRecord::k_Type))(context, fire->id);
		EXPECT_TRUE(written.has_value());
		const auto number = written.has_value() ? std::get<gamesave::FireEffectRecord>(*written).objects[3].number : 0;
		return std::pair(number, context.Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = fire->reaction}));
	};
	{
		const auto [named, reaction] = write({.registry = Registry()});
		EXPECT_EQ(named, reaction);
	}
	// ended: none
	reactions.Remove(fire->reaction);
	EXPECT_EQ(write({.registry = Registry()}).first, 0u);

	// a loaded fire whose kept record names reaction 40: not made again, as kept; made again and ended, none
	fire->reaction = 0;
	gamesave::WorldRead kept;
	kept.objects.resize(50);
	gamesave::FireEffectRecord record;
	record.objects[3].number = 40;
	kept.objects[44] = record;
	const LoadedObjects loaded {{45, {.type = gamesave::FireEffectRecord::k_Type, .id = fire->id}}};
	EXPECT_EQ(write({.registry = Registry(), .kept = &kept, .loaded = loaded}).first, 40u);
	auto madeAgain = loaded;
	madeAgain.try_emplace(40, LoadedObject {.type = gamesave::ReactionRecord::k_Type, .id = 999});
	EXPECT_EQ(write({.registry = Registry(), .kept = &kept, .loaded = madeAgain}).first, 0u);

	// the load: the fire numbered 45 gets the reaction its record names (40), made again as a new id
	const auto again = reactions.Add({.initiator = rock, .type = openblack::Reaction::ReactToFire});
	const std::vector<ecs::saved_world_load::PlannedFire> fires {{.number = 45, .record = record}};
	ecs::saved_world_load::GiveFiresTheirReactions(fires, {{45, fire->id}}, {{40, again}});
	EXPECT_EQ(fire->reaction, again);
}

// Fires of a group whose root and one middle fire were not made (their objects were not): the made ones keep the
// saved chain's order with the first of them as their root, a whole group stays as saved, and merging the two groups
// afterwards ends with one chain that holds each fire once
TEST_P(ForestWriteWorld, LoadedFireGroupsStayWholeWhenSomeOfTheirFiresAreNotMade)
{
	std::array<entt::entity, 7> objects {};
	for (size_t i = 1; i < objects.size(); ++i)
	{
		const glm::vec3 position(2500.0f + 10.0f * static_cast<float>(i), k_Ground, 2520.0f);
		objects.at(i) =
		    ecs::archetypes::MobileStaticArchetype::Create(position, MobileStaticInfo::Rock, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	}
	// fire n burns object n; group 1 -> 2 -> 3 -> 4 with root 1, group 5 -> 6 with root 5; 1 and 3 are not made
	const auto planned = [](gamesave::ObjectNumber number, gamesave::ObjectNumber next, gamesave::ObjectNumber root) {
		ecs::saved_world_load::PlannedFire fire {.number = 100 + number};
		fire.record.objects[0].number = number;
		fire.record.others[0].number = next == 0 ? 0 : 100 + next;
		fire.record.others[1].number = 100 + root;
		return fire;
	};
	const std::vector<ecs::saved_world_load::PlannedFire> fires {planned(1, 2, 1), planned(2, 3, 1), planned(3, 4, 1),
	                                                             planned(4, 0, 1), planned(5, 6, 5), planned(6, 0, 5)};
	Fires().Reset();
	const auto ids = ecs::saved_world_load::MakeFires(fires, {0, 0}, [&](gamesave::ObjectNumber number) {
		return number == 1 || number == 3 || number >= objects.size() ? entt::entity {entt::null} : objects.at(number);
	});
	ASSERT_EQ(ids.size(), fires.size());
	EXPECT_EQ(ids[0], 0u);
	EXPECT_EQ(ids[2], 0u);
	auto* second = Fires().FindById(ids[1]);
	auto* fourth = Fires().FindById(ids[3]);
	auto* fifth = Fires().FindById(ids[4]);
	auto* sixth = Fires().FindById(ids[5]);
	ASSERT_NE(second, nullptr);
	ASSERT_NE(fourth, nullptr);
	ASSERT_NE(fifth, nullptr);
	ASSERT_NE(sixth, nullptr);
	EXPECT_EQ(second->root, second);
	EXPECT_EQ(second->next, fourth);
	EXPECT_EQ(fourth->root, second);
	EXPECT_EQ(fourth->next, nullptr);
	EXPECT_EQ(fifth->root, fifth);
	EXPECT_EQ(fifth->next, sixth);
	EXPECT_EQ(sixth->root, fifth);
	EXPECT_EQ(sixth->next, nullptr);

	// joined, then joined again (already one group, so nothing changes): the chain from the root ends, each fire once
	ecs::fire::AddToFireGroup(*second, *fifth);
	ecs::fire::AddToFireGroup(*fourth, *sixth);
	std::vector<const ecs::fire::FireEffect*> chain;
	for (const auto* member = second->root; member != nullptr && chain.size() <= fires.size(); member = member->next)
	{
		chain.push_back(member);
		EXPECT_EQ(member->root, second);
	}
	EXPECT_EQ(chain, (std::vector<const ecs::fire::FireEffect*> {second, fifth, sixth, fourth}));
}

// A villager made in play is written from its state: its row, its flags from what holds each bit, its food and check
// turn, no carried object until its first clip, the move state it was made with and its constructor's counter, and no
// states' values while it is just made; moved on to another state without a walk it keeps the 0 it was made with, and
// once a walk's tags are dropped it keeps the walk's last state, as the game does
// A villager whose home, town or mother is gone names none there, as a deleted abode puts its villagers out, a deleted
// mother orphans her children and a deleted town lets its villagers go
TEST_P(ForestWriteWorld, AVillagerNamesNoHomeTownOrMotherThatIsGone)
{
	auto& registry = Registry();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Transform>(entity, glm::vec3(2510.0f, k_Ground, 2520.0f), glm::mat3(1.0f),
	                                            glm::vec3(1.0f));
	auto& villager = registry.Assign<ecs::components::Villager>(entity);
	villager.infoRow = VillagerInfo::NorseHousewifeFemale;
	villager.birthTurn = 3;
	const auto home = registry.Create();
	const auto town = registry.Create();
	const auto mother = registry.Create();
	villager.abode = home;
	villager.town = town;
	villager.mother = mother;
	registry.Destroy(home);
	registry.Destroy(town);
	registry.Destroy(mother);
	registry.Assign<ecs::components::LivingAction>(entity, VillagerStates::Created, uint16_t {0});
	registry.Assign<ecs::components::WallHug>(entity);
	RegisterVillagerWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& fields = std::get<gamesave::VillagerRecord>(*written).values.villager;
	EXPECT_EQ(fields.home.number, 0u);
	EXPECT_EQ(fields.town.number, 0u);
	EXPECT_EQ(fields.mother.number, 0u);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

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

	// moved on to another state without a walk: the 0 it was made with
	registry.Get<ecs::components::LivingAction>(entity).states[0] = static_cast<uint8_t>(VillagerStates::MoveToPos);
	WriteContext moved(_writers, source);
	const auto still = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(moved, entity);
	ASSERT_TRUE(still.has_value()) << moved.Failure();
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*still).values.move.moveState, 0);

	// a walk that ended on its final step, its tags dropped at the arrival: the final step's state stays
	registry.Assign<ecs::components::MoveStateFinalStepTag>(entity, ecs::components::MoveStateClockwise::Undefined,
	                                                        glm::vec2(2510.0f, 2520.0f));
	ecs::wall_hug::DropWalkTags<ecs::components::MoveStateFinalStepTag, ecs::components::MoveStateArrivedTag>(registry, entity);
	EXPECT_FALSE(registry.AnyOf<ecs::components::MoveStateFinalStepTag>(entity));
	WriteContext walked(_writers, source);
	const auto after = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(walked, entity);
	ASSERT_TRUE(after.has_value()) << walked.Failure();
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*after).values.move.moveState, 4);

	// with a script memory, its living part names its script's reminder, written from the memory and read back the same
	auto& memory = registry.AssignOrReplace<ecs::components::ScriptMemory>(entity);
	memory.destination = {.x = 0x12340, .z = 0x23450, .altitude = 1.5f};
	memory.arguments = ecs::components::ScriptStateArguments {.place = {100.0f, 200.0f}, .number = 6.0f};
	memory.clipAndPlays = ecs::components::ScriptAnimation {.clip = static_cast<AnimId>(12), .playsLeft = 2};
	memory.stateClip = 4;
	memory.state = VillagerStates::InScript;
	WriteContext remembering(_writers, source);
	const auto withMemory = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(remembering, entity);
	ASSERT_TRUE(withMemory.has_value()) << remembering.Failure();
	const auto key = SourceKey {.type = gamesave::ScriptReminderRecord::k_Type, .id = PartId(entity, 0)};
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*withMemory).values.living.scriptReminder.number, remembering.Reference(key));
	EXPECT_EQ(remembering.Keys(gamesave::ScriptReminderRecord::k_Type), std::vector<uint64_t> {key.id});
	const auto reminder = (*_writers.SourceWriterOf(gamesave::ScriptReminderRecord::k_Type))(remembering, key.id);
	ASSERT_TRUE(reminder.has_value()) << remembering.Failure();
	const auto read = ecs::saved_world_load::ScriptMemoryOf(std::get<gamesave::ScriptReminderRecord>(*reminder), entt::null);
	EXPECT_EQ(read.destination, memory.destination);
	ASSERT_TRUE(read.arguments.has_value());
	EXPECT_EQ(read.arguments->place, memory.arguments->place);
	EXPECT_EQ(read.arguments->number, 6.0f);
	ASSERT_TRUE(read.clipAndPlays.has_value());
	EXPECT_EQ(read.clipAndPlays->clip, static_cast<AnimId>(12));
	EXPECT_EQ(read.clipAndPlays->playsLeft, 2u);
	EXPECT_EQ(read.stateClip, 4);
	EXPECT_EQ(read.state, VillagerStates::InScript);
}

// A villager a script made keeps its name hidden through a save: its thing part is written with the hidden-name mark,
// one with its name shown without it, and a load gives the mark back only to the first
TEST_P(ForestWriteWorld, AVillagerAScriptMadeIsWrittenAndLoadedWithItsNameHidden)
{
	namespace load = ecs::saved_world_load;
	constexpr uint8_t k_NameHiddenMark = 0x40;
	auto& registry = Registry();
	const auto make = [&registry]() {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Transform>(entity, glm::vec3(2510.0f, k_Ground, 2520.0f), glm::mat3(1.0f),
		                                            glm::vec3(1.0f));
		auto& villager = registry.Assign<ecs::components::Villager>(entity);
		villager.infoRow = VillagerInfo::NorseHousewifeFemale;
		villager.town = entt::null;
		villager.abode = entt::null;
		villager.mother = entt::null;
		registry.Assign<ecs::components::LivingAction>(entity, VillagerStates::Created, uint16_t {10});
		registry.Assign<ecs::components::WallHug>(entity);
		return entity;
	};
	const auto made = make();
	registry.Assign<ecs::components::NameHidden>(made);
	const auto named = make();
	RegisterVillagerWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	for (const auto entity : {made, named})
	{
		const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		world.objects.emplace_back(std::get<gamesave::VillagerRecord>(*written));
	}
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*world.objects.at(1)).thing.flags, k_NameHiddenMark);
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*world.objects.at(2)).thing.flags, 0);

	world.loadOrder = {2, 3, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.villager = load::MakeVillagerEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	ASSERT_TRUE(loaded.contains(3));
	EXPECT_TRUE(registry.AllOf<ecs::components::NameHidden>(loaded.at(2).entity));
	EXPECT_FALSE(registry.AllOf<ecs::components::NameHidden>(loaded.at(3).entity));
}

// A villager farming or in a script's state is written with its states' values and a load makes it again with them: a
// farmer its work point in its field and its field, one in a script's state the place, the number and the pair the
// script gave it, a shepherd its turns to wait and the state it goes on to
TEST_P(ForestWriteWorld, AFarmerAndAVillagerInAScriptsStateAreMadeAgainWithTheirStatesValues)
{
	namespace load = ecs::saved_world_load;
	auto& registry = Registry();
	const auto make = [&registry](VillagerStates top, VillagerStates final) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Transform>(entity, glm::vec3(2510.0f, k_Ground, 2520.0f), glm::mat3(1.0f),
		                                            glm::vec3(1.0f));
		auto& villager = registry.Assign<ecs::components::Villager>(entity);
		villager.infoRow = VillagerInfo::NorseHousewifeFemale;
		villager.town = entt::null;
		villager.abode = entt::null;
		villager.mother = entt::null;
		registry.Assign<ecs::components::LivingAction>(entity, top, uint16_t {10});
		registry.Get<ecs::components::LivingAction>(entity).states[1] = static_cast<uint8_t>(final);
		registry.Assign<ecs::components::WallHug>(entity);
		return entity;
	};
	const auto farmer = make(VillagerStates::FarmerArrivesAtFarm, VillagerStates::FarmerPlantsCrop);
	registry.Get<ecs::components::Villager>(farmer).workPos = {.x = 1000, .z = 2000, .altitude = 0.5f};
	const auto scripted = make(VillagerStates::InScript, VillagerStates::MoveToPos);
	registry.Assign<ecs::components::ScriptStateArguments>(scripted, glm::vec2(2512.0f, 2522.0f), 3.5f);
	registry.Assign<ecs::components::ScriptAnimation>(scripted, static_cast<AnimId>(7), uint32_t {2});
	const auto shepherd = make(VillagerStates::ShepherdLookForFlock, VillagerStates::ShepherdMoveFlockBack);
	registry.Assign<ecs::components::ShepherdWork>(shepherd, int16_t {5}, VillagerStates::ShepherdMoveFlockToFood);
	const auto builder = make(VillagerStates::MoveToPos, VillagerStates::ArrivesAtBuildingSite);
	registry.Get<ecs::components::Villager>(builder).buildPosIndex = 3;
	RegisterVillagerWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	for (const auto entity : {farmer, scripted, shepherd, builder})
	{
		const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		auto record = std::get<gamesave::VillagerRecord>(*written);
		for (auto* values : {&record.values.topState, &record.values.finalState})
		{
			for (auto& value : *values)
			{
				value.number = 0;
			}
		}
		world.objects.emplace_back(record);
	}
	// one more, walking straight with a thing's circle ahead: made, its circle the thing's once the thing is made (here
	// none is, so it holds none)
	auto circling = std::get<gamesave::VillagerRecord>(*world.objects.at(2));
	circling.values.move.moveState = gamesave::k_FirstCircleState;
	circling.values.move.circle = gamesave::CircleHug {.turnsToObject = 3, .object = {.number = 99}, .index = -1};
	world.objects.emplace_back(circling);
	// and one in a reaction: made, its reaction the one made again from its record (here none is)
	auto reacting = std::get<gamesave::VillagerRecord>(*world.objects.at(2));
	reacting.values.living.reaction = {.number = 98};
	world.objects.emplace_back(reacting);
	// and a shepherd walking to a thing: made, walking to the thing its walk names once that is made
	auto following = std::get<gamesave::VillagerRecord>(*world.objects.at(2));
	following.values.living.action[0] = static_cast<uint8_t>(VillagerStates::MoveToObject);
	following.values.living.action[1] = static_cast<uint8_t>(VillagerStates::ShepherdDecideWhatToDoWithFlock);
	following.values.topState = {};
	following.values.finalState = {};
	following.values.move.target = {.number = 3};
	world.objects.emplace_back(following);
	world.loadOrder = {2, 3, 4, 5, 6, 7, 8, 1};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.villager = load::MakeVillagerEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(5));
	EXPECT_EQ(registry.Get<ecs::components::Villager>(loaded.at(5).entity).buildPosIndex, 3);
	ASSERT_TRUE(loaded.contains(6));
	EXPECT_FALSE(registry.AllOf<ecs::components::WallHugObjectReference>(loaded.at(6).entity));
	ASSERT_TRUE(loaded.contains(7));
	EXPECT_FALSE(registry.AllOf<ecs::components::LivingReaction>(loaded.at(7).entity));
	ASSERT_TRUE(loaded.contains(8));
	ASSERT_TRUE(loaded.contains(3));
	EXPECT_EQ(registry.Get<ecs::components::Villager>(loaded.at(8).entity).walkTarget, loaded.at(3).entity);
	EXPECT_EQ(registry.Get<ecs::components::Villager>(loaded.at(7).entity).walkTarget, entt::entity {entt::null});
	ASSERT_TRUE(loaded.contains(4));
	const auto shepherdAgain = loaded.at(4).entity;
	ASSERT_TRUE(registry.AllOf<ecs::components::ShepherdWork>(shepherdAgain));
	EXPECT_EQ(registry.Get<ecs::components::ShepherdWork>(shepherdAgain).waitTurns, 5);
	EXPECT_EQ(registry.Get<ecs::components::ShepherdWork>(shepherdAgain).pending, VillagerStates::ShepherdMoveFlockToFood);
	ASSERT_TRUE(loaded.contains(2));
	ASSERT_TRUE(loaded.contains(3));
	const auto farmerAgain = loaded.at(2).entity;
	const auto& work = registry.Get<ecs::components::Villager>(farmerAgain).workPos;
	EXPECT_EQ(work.x, 1000);
	EXPECT_EQ(work.z, 2000);
	EXPECT_EQ(work.altitude, 0.5f);
	const auto scriptedAgain = loaded.at(3).entity;
	ASSERT_TRUE(registry.AllOf<ecs::components::ScriptStateArguments>(scriptedAgain));
	EXPECT_NEAR(registry.Get<ecs::components::ScriptStateArguments>(scriptedAgain).place.x, 2512.0f, 1e-2f);
	EXPECT_NEAR(registry.Get<ecs::components::ScriptStateArguments>(scriptedAgain).place.y, 2522.0f, 1e-2f);
	EXPECT_EQ(registry.Get<ecs::components::ScriptStateArguments>(scriptedAgain).number, 3.5f);
	ASSERT_TRUE(registry.AllOf<ecs::components::ScriptAnimation>(scriptedAgain));
	EXPECT_EQ(registry.Get<ecs::components::ScriptAnimation>(scriptedAgain).clip, static_cast<AnimId>(7));
	EXPECT_EQ(registry.Get<ecs::components::ScriptAnimation>(scriptedAgain).playsLeft, 2u);

	// a breeder and a villager going round a fire are made again too: the breeder's place to wander about, and the
	// place the other was going to before the fire, in whole map units (their partner and fire once those are made;
	// here none is)
	const auto whole = [](int32_t x, int32_t z) {
		gamesave::StateValue value {.size = 8};
		std::memcpy(value.bytes.data(), &x, sizeof(x));
		std::memcpy(value.bytes.data() + sizeof(x), &z, sizeof(z));
		return value;
	};
	auto breeder = std::get<gamesave::VillagerRecord>(*world.objects.at(2));
	breeder.values.living.action[0] = static_cast<uint8_t>(VillagerStates::BreederDisciple);
	breeder.values.living.action[1] = 0;
	breeder.values.topState = {gamesave::StateValue {.size = gamesave::k_StateReference}, whole(1300, 2400)};
	breeder.values.finalState = {};
	auto byFire = breeder;
	byFire.values.living.action[0] = static_cast<uint8_t>(VillagerStates::MoveAroundFire);
	byFire.values.topState = {whole(6553600, 13107200), gamesave::StateValue {.size = gamesave::k_StateReference}};
	// and one in the hand, with the disciple it was at the pick-up, which it becomes again as it lands
	auto held = breeder;
	held.values.living.action[0] = static_cast<uint8_t>(VillagerStates::InHand);
	held.values.topState = {gamesave::StateValue {.size = 4, .bytes = {3}}};
	gamesave::WorldRead second;
	second.objects.emplace_back(gamesave::GameHead {});
	second.objects.emplace_back(breeder);
	second.objects.emplace_back(byFire);
	second.objects.emplace_back(held);
	second.loadOrder = {2, 3, 4, 1};
	LoadedObjects again;
	std::ignore = load::Apply(load::PlanOf(second), load::Makers {.villager = load::MakeVillagerEntity}, &again);
	ASSERT_TRUE(again.contains(2));
	const auto& area = registry.Get<ecs::components::Villager>(again.at(2).entity).wanderArea;
	EXPECT_EQ(area.x, 1300);
	EXPECT_EQ(area.z, 2400);
	ASSERT_TRUE(again.contains(3));
	ASSERT_TRUE(registry.AllOf<ecs::components::VillagerFireState>(again.at(3).entity));
	EXPECT_EQ(registry.Get<ecs::components::VillagerFireState>(again.at(3).entity).savedDestination,
	          glm::vec2(1000.0f, 2000.0f));
	ASSERT_TRUE(again.contains(4));
	EXPECT_EQ(registry.Get<ecs::components::Villager>(again.at(4).entity).discipleOnPickUp, 3u);

	// one walking a script's track is made with its walk, as its data path keeps it, while the track is there; one
	// whose track is not is left out
	auto walker = breeder;
	walker.values.living.action[0] = static_cast<uint8_t>(VillagerStates::MoveAlongPath);
	walker.values.topState = {};
	walker.values.living.dataPath = {.number = 3};
	const gamesave::DataPathRecord path {
	    .trackNumber = 20, .endPercentage = 0.75f, .reverse = 1, .frame = 500.0f, .framesPerTurn = 100.0f, .last = 3264.0f};
	gamesave::WorldRead third;
	third.objects.emplace_back(gamesave::GameHead {});
	third.objects.emplace_back(walker);
	third.objects.emplace_back(path);
	third.loadOrder = {2, 3, 1};
	LoadedObjects walked;
	std::ignore = load::Apply(load::PlanOf(third), load::Makers {.villager = load::MakeVillagerEntity}, &walked);
	if (camera_edits::FindTrack(20) == nullptr)
	{
		EXPECT_FALSE(walked.contains(2));
		return;
	}
	ASSERT_TRUE(walked.contains(2));
	ASSERT_TRUE(registry.AllOf<ecs::components::LivingWalkPath>(walked.at(2).entity));
	const auto& walk = registry.Get<ecs::components::LivingWalkPath>(walked.at(2).entity);
	EXPECT_EQ(walk.trackNumber, 20);
	EXPECT_EQ(walk.path.walk.to, 0.75f);
	EXPECT_TRUE(walk.path.walk.forward);
	EXPECT_EQ(walk.path.walk.current, 500.0f);
	EXPECT_EQ(walk.path.walk.step, 100.0f);
	EXPECT_EQ(walk.path.speed, 3264);
}

// A feature, an animated static and a script's highlight made in play are written from their state: their rows (the
// original's new Land 1 save has its Aztec statues at 2063, its Norse gate at 1986, its did-you-know signs at 2087),
// made whole, an animated static with no gate stones and closed, a highlight with its words and the thing's mark all
// the original's highlights carry
TEST_P(ForestWriteWorld, AFeatureAnAnimatedStaticAndAHighlightMadeInPlayAreWrittenFromTheirState)
{
	using namespace ecs::components;
	auto& registry = Registry();
	const auto made = [&registry](glm::vec3 at, uint32_t creation) {
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ObjectCreationIndex>(entity, ObjectCreationIndex {.value = creation});
		return entity;
	};
	const auto feature = made(glm::vec3(2510.0f, k_Ground, 2520.0f), 1999);
	registry.Assign<Feature>(feature, Feature {.type = FeatureInfo::AztcStatue});
	const auto gate = made(glm::vec3(2530.0f, k_Ground, 2520.0f), 306);
	registry.Assign<AnimatedStatic>(gate, AnimatedStatic {.type = AnimatedStaticInfo::NorseGate});
	const auto sign = made(glm::vec3(2550.0f, k_Ground, 2520.0f), 2029);
	auto& highlight = registry.Assign<ScriptHighlight>(sign);
	highlight.infoIndex = 1;
	highlight.scriptId = 4184;
	highlight.drawHeight = 0.0f;
	highlight.category = DykCategory::VillageLife;
	RegisterLandFeatureWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);

	const auto writtenFeature = (*_writers.WriterOf(gamesave::FeatureRecord::k_Type))(context, feature);
	ASSERT_TRUE(writtenFeature.has_value()) << context.Failure();
	const auto& featureRecord = std::get<gamesave::FeatureRecord>(*writtenFeature);
	EXPECT_EQ(featureRecord.base.object.info, 2063u);
	EXPECT_EQ(featureRecord.base.object.creationIndex, 1999u);
	EXPECT_EQ(featureRecord.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(featureRecord.base.multi.percentBuilt, 1.0f);

	const auto writtenGate = (*_writers.WriterOf(gamesave::AnimatedStaticRecord::k_Type))(context, gate);
	ASSERT_TRUE(writtenGate.has_value()) << context.Failure();
	const auto& gateRecord = std::get<gamesave::AnimatedStaticRecord>(*writtenGate);
	EXPECT_EQ(gateRecord.base.object.info, 1986u);
	EXPECT_EQ(gateRecord.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(gateRecord.frames.values, (std::vector<uint32_t> {0, 0, 0}));
	EXPECT_EQ(gateRecord.word, 0u);

	const auto writtenSign = (*_writers.WriterOf(gamesave::ScriptHighlightRecord::k_Type))(context, sign);
	ASSERT_TRUE(writtenSign.has_value()) << context.Failure();
	const auto& signRecord = std::get<gamesave::ScriptHighlightRecord>(*writtenSign);
	EXPECT_EQ(signRecord.thing.flags, 0x40);
	EXPECT_EQ(signRecord.base.object.info, 2087u);
	EXPECT_EQ(signRecord.base.object.creationIndex, 2029u);
	EXPECT_EQ(signRecord.words, (std::array<uint32_t, 6> {0, 0, 1, 0, 4184, 2}));

	// a plinth's slots are written as the models of the stones laid in them, 0 for an empty one
	registry.Get<AnimatedStatic>(gate).gateStones = {MeshId::ObjectGateTotemApe, MeshId::ObjectGateTotemTiger, std::nullopt};
	WriteContext stones(_writers, source);
	const auto writtenStones = (*_writers.WriterOf(gamesave::AnimatedStaticRecord::k_Type))(stones, gate);
	ASSERT_TRUE(writtenStones.has_value()) << stones.Failure();
	EXPECT_EQ(std::get<gamesave::AnimatedStaticRecord>(*writtenStones).frames.values, (std::vector<uint32_t> {362, 366, 0}));
}

// An animated static of the save is made of its row at its place, angle and size, with its open state and creation
// number, and is written back with them
TEST_P(ForestWriteWorld, ASavedAnimatedStaticIsMadeAndWrittenBackAsRead)
{
	if (GameData())
	{
		GTEST_SKIP() << "with the game's data the static's row stands on its shipped model, which the tests do not load";
	}
	namespace load = ecs::saved_world_load;
	gamesave::AnimatedStaticRecord saved;
	saved.base.position.position = {.x = map_coords::ToFixed(2550.0f), .z = map_coords::ToFixed(2520.0f), .altitude = 0.0f};
	saved.base.object.info = 1986;
	saved.base.object.coords = saved.base.position.position;
	saved.base.object.creationIndex = 2031;
	saved.base.object.heading = 0.5f;
	saved.base.object.scale = 1.0f;
	saved.base.object.life = 1.0f;
	saved.frames.values.assign(3, 0);
	saved.word = 1;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(saved);
	world.loadOrder = {2};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.animatedStatic = load::MakeAnimatedStaticEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto entity = loaded.at(2).entity;
	ASSERT_TRUE(entity != entt::null);
	const auto& still = Registry().Get<const ecs::components::AnimatedStatic>(entity);
	EXPECT_TRUE(still.type == AnimatedStaticInfo::NorseGate);
	EXPECT_EQ(still.openState, 1);
	EXPECT_EQ(ecs::object_index::Of(entity), 2031);

	RegisterLandFeatureWriters(_writers);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::AnimatedStaticRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::AnimatedStaticRecord>(*written);
	EXPECT_EQ(record.word, saved.word);
	EXPECT_EQ(record.base.object.info, saved.base.object.info);
	EXPECT_EQ(record.base.object.creationIndex, saved.base.object.creationIndex);
	EXPECT_EQ(record.base.position.position.x, saved.base.position.position.x);
	EXPECT_EQ(record.base.position.position.z, saved.base.position.position.z);
}

namespace
{
/// A plinth's record made again through the load's animated static maker, alone in a world file of its own, and the
/// entity made of it; the world is kept for the writer
struct LoadedPlinth
{
	gamesave::WorldRead world;
	LoadedObjects loaded;
	entt::entity entity {entt::null};
};
LoadedPlinth LoadPlinth(const gamesave::AnimatedStaticRecord& saved)
{
	namespace load = ecs::saved_world_load;
	LoadedPlinth plinth;
	plinth.world.objects.emplace_back(gamesave::GameHead {});
	plinth.world.objects.emplace_back(saved);
	plinth.world.loadOrder = {2};
	std::ignore = load::Apply(load::PlanOf(plinth.world), load::Makers {.animatedStatic = load::MakeAnimatedStaticEntity},
	                          &plinth.loaded);
	if (plinth.loaded.contains(2))
	{
		plinth.entity = plinth.loaded.at(2).entity;
	}
	return plinth;
}
} // namespace

// A plinth of the save holding gate stones is made with each stone laid again in its slot, by the model its slot keeps,
// worth what those stones are worth, and is written back with the same slots; an empty slot stays empty and is written
// as 0
TEST_P(ForestWriteWorld, ASavedPlinthsGateStonesAreLaidAgainAndWrittenBackAsRead)
{
	const auto& row = Info().animatedStatic.at(static_cast<size_t>(AnimatedStaticInfo::GateStonePlinth));
	LoadMesh(row.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 2.0f, 1.0f));
	gamesave::AnimatedStaticRecord saved;
	saved.base.position.position = {.x = map_coords::ToFixed(2550.0f), .z = map_coords::ToFixed(2520.0f), .altitude = 0.0f};
	saved.base.object.info = 1985;
	saved.base.object.coords = saved.base.position.position;
	saved.base.object.creationIndex = 2032;
	saved.base.object.scale = 1.0f;
	saved.base.object.life = 1.0f;
	saved.frames.values = {366, 362, 0};
	auto plinth = LoadPlinth(saved);
	ASSERT_TRUE(plinth.entity != entt::null);
	const auto& still = Registry().Get<const ecs::components::AnimatedStatic>(plinth.entity);
	EXPECT_TRUE(still.type == AnimatedStaticInfo::GateStonePlinth);
	EXPECT_EQ(still.gateStones,
	          (animated_static::GateStones {MeshId::ObjectGateTotemTiger, MeshId::ObjectGateTotemApe, std::nullopt}));
	EXPECT_EQ(animated_static::GateStoneValue(still.gateStones), 3u);

	RegisterLandFeatureWriters(_writers);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &plinth.world, .loaded = plinth.loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::AnimatedStaticRecord::k_Type))(context, plinth.entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::AnimatedStaticRecord>(*written);
	EXPECT_EQ(record.frames.values, saved.frames.values);
	EXPECT_EQ(record.word, saved.word);
	EXPECT_EQ(record.base.object.info, saved.base.object.info);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// saves of a profile that laid the three gate stones keep the plinth's slots as the tiger's, the ape's and the cow's
// models, open; each plinth is made with the three stones in those slots, worth 7 to the scripts, and is written back
// with the same slots and open state
TEST_P(ForestWriteWorld, TheFrozenLandOneSavesPlinthsComeBackWithTheirGateStones)
{
	if (!GameData())
	{
		GTEST_SKIP() << "the frozen saves are the game's data";
	}
	const auto& row = Info().animatedStatic.at(static_cast<size_t>(AnimatedStaticInfo::GateStonePlinth));
	LoadMesh(row.meshId, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 2.0f, 1.0f));
	RegisterLandFeatureWriters(_writers);
	size_t plinths = 0;
	for (const auto* slot : {"15/1/15.sav", "16/1/16.sav", "20/20.sav"})
	{
		const auto bytes = FrozenSave(std::filesystem::path("_s_i_s_i") / "Saved Games" / slot);
		if (!bytes.has_value())
		{
			continue;
		}
		const auto world = gamesave::ReadWorld(*bytes);
		EXPECT_EQ(world.stop, gamesave::k_ReadToTheEnd) << slot;
		for (const auto& object : world.objects)
		{
			const auto* saved = object.has_value() ? std::get_if<gamesave::AnimatedStaticRecord>(&*object) : nullptr;
			if (saved == nullptr || std::ranges::all_of(saved->frames.values, [](uint32_t value) { return value == 0; }))
			{
				continue;
			}
			++plinths;
			EXPECT_EQ(saved->frames.values, (std::vector<uint32_t> {366, 362, 364})) << slot;
			EXPECT_EQ(saved->word, 1u) << slot;
			auto plinth = LoadPlinth(*saved);
			ASSERT_TRUE(plinth.entity != entt::null) << slot;
			const auto& still = Registry().Get<const ecs::components::AnimatedStatic>(plinth.entity);
			EXPECT_EQ(still.gateStones, (animated_static::GateStones {MeshId::ObjectGateTotemTiger, MeshId::ObjectGateTotemApe,
			                                                          MeshId::ObjectGateTotemCow}))
			    << slot;
			EXPECT_EQ(animated_static::GateStoneValue(still.gateStones), 7u) << slot;
			EXPECT_EQ(still.openState, 1) << slot;
			const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &plinth.world, .loaded = plinth.loaded};
			WriteContext context(_writers, source);
			const auto written = (*_writers.WriterOf(gamesave::AnimatedStaticRecord::k_Type))(context, plinth.entity);
			ASSERT_TRUE(written.has_value()) << context.Failure();
			const auto& record = std::get<gamesave::AnimatedStaticRecord>(*written);
			EXPECT_EQ(record.frames.values, saved->frames.values) << slot;
			EXPECT_EQ(record.word, saved->word) << slot;
		}
	}
	if (plinths == 0)
	{
		GTEST_SKIP() << "no such save";
	}
	EXPECT_EQ(plinths, 3u);
}

// A script's marker of the save is made as a script makes it, at its place and height, with its hidden name, and is
// written back as it was read
TEST_P(ForestWriteWorld, ASavedScriptMarkerIsMadeAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	gamesave::ScriptMarkerRecord saved;
	saved.thing.flags = 0x40;
	saved.position.position = {.x = map_coords::ToFixed(2550.0f), .z = map_coords::ToFixed(2520.0f), .altitude = 0.0f};
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::ThingsWithPosition)).numbers = {2};
	world.objects.emplace_back(game);
	world.objects.emplace_back(saved);
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.marker = load::MakeScriptMarkerEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto entity = loaded.at(2).entity;
	ASSERT_TRUE(entity != entt::null);
	EXPECT_TRUE(Registry().AllOf<ecs::components::ScriptMarker>(entity));
	EXPECT_TRUE(Registry().AllOf<ecs::components::NameHidden>(entity));
	EXPECT_EQ(loaded.at(2).type, gamesave::ScriptMarkerRecord::k_Type);

	RegisterMarkerWriters(_writers);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::ScriptMarkerRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::ScriptMarkerRecord>(*written);
	EXPECT_EQ(record.position.position.x, saved.position.position.x);
	EXPECT_EQ(record.position.position.z, saved.position.position.z);
	EXPECT_NEAR(record.position.position.altitude, saved.position.position.altitude, 1e-3f);
	EXPECT_EQ(record.thing.flags, saved.thing.flags);
}

// A script's timer of the save is made as a script makes one, set on the turn and for the turns it was set, with its
// hidden name and its script's marks, and is written back as it was read; one made in play is written with no place,
// its script's marks, its hidden name and how it was set
TEST_P(ForestWriteWorld, AScriptTimerIsMadeAndWrittenBackAsReadAndOneMadeInPlayFromItsState)
{
	namespace load = ecs::saved_world_load;
	gamesave::ScriptTimerRecord saved;
	saved.thing.flags = 0x40;
	saved.position.flags = position_flags::k_InScript | position_flags::k_ControlledByScript;
	saved.words = {1234, 300};
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(saved);
	world.loadOrder = {2};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.timer = load::MakeScriptTimerEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto entity = loaded.at(2).entity;
	ASSERT_TRUE(entity != entt::null);
	const auto& timer = Registry().Get<ecs::components::ScriptTimer>(entity);
	EXPECT_EQ(timer.timer.setTurn, 1234u);
	EXPECT_EQ(timer.timer.turns, 300);
	EXPECT_TRUE(Registry().AllOf<ecs::components::NameHidden>(entity));
	EXPECT_TRUE((Registry().AllOf<ecs::components::InScript, ecs::components::ScriptControlled>(entity)));
	EXPECT_EQ(loaded.at(2).type, gamesave::ScriptTimerRecord::k_Type);

	RegisterMarkerWriters(_writers);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(Registry(), entity), std::vector {gamesave::ScriptTimerRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::ScriptTimerRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_TRUE(std::get<gamesave::ScriptTimerRecord>(*written) == saved);

	const auto made = Registry().Create();
	Registry().Assign<ecs::components::ScriptTimer>(made, ecs::components::ScriptTimer {.timer = {.setTurn = 50, .turns = 75}});
	Registry().Assign<ecs::components::NameHidden>(made);
	Registry().Assign<ecs::components::InScript>(made);
	Registry().Assign<ecs::components::ScriptControlled>(made);
	const auto writtenMade = (*_writers.WriterOf(gamesave::ScriptTimerRecord::k_Type))(context, made);
	ASSERT_TRUE(writtenMade.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::ScriptTimerRecord>(*writtenMade);
	EXPECT_EQ(record.thing.flags, 0x40);
	EXPECT_EQ(record.position.position, gamesave::MapCoords {});
	EXPECT_EQ(record.position.flags, position_flags::k_InScript | position_flags::k_ControlledByScript);
	EXPECT_EQ(record.words, (std::array<uint32_t, 2> {50, 75}));
}

// Integration test: needs the game's data; skipped without it. Every script timer of a profile's Land 1 save with its
// temple built is a thing with no place, a hidden name, and its script's marks
TEST_P(ForestWriteWorld, TheProfilesLandOneTimersHaveNoPlaceAndTheirScriptsMarks)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!GameData() || game == nullptr)
	{
		GTEST_SKIP() << "the profile's save is the game's data";
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / "_J_a_h_o_v_i_a" / "Saved Games" / "16" / "1" / "16.sav",
	                   std::ios::binary);
	if (!file)
	{
		GTEST_SKIP() << "no such save";
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
	const auto world = gamesave::ReadWorld(bytes);
	size_t timers = 0;
	for (const auto& object : world.objects)
	{
		const auto* timer = object.has_value() ? std::get_if<gamesave::ScriptTimerRecord>(&*object) : nullptr;
		if (timer == nullptr)
		{
			continue;
		}
		++timers;
		EXPECT_EQ(timer->thing.flags, 0x40);
		EXPECT_EQ(timer->position.position, gamesave::MapCoords {});
		EXPECT_EQ(timer->position.flags, position_flags::k_InScript | position_flags::k_ControlledByScript);
	}
	EXPECT_EQ(timers, 9u);
}

// A script's highlight of the save is made as a script makes it, of its row, with its script id, its set height, its
// category and its marks, and is written back as it was read
TEST_P(ForestWriteWorld, ASavedHighlightIsMadeAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	gamesave::ScriptHighlightRecord saved;
	saved.thing.flags = 0x40;
	saved.base.position.position = {.x = map_coords::ToFixed(2550.0f), .z = map_coords::ToFixed(2520.0f), .altitude = 0.0f};
	saved.base.position.flags = 0x8001;
	saved.base.object.info = 2087;
	saved.base.object.coords = saved.base.position.position;
	saved.base.object.creationIndex = 2029;
	saved.words = {0, 0, 1, 0, 4184, 2};
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::ScriptHighlights)).numbers = {2};
	world.objects.emplace_back(game);
	world.objects.emplace_back(saved);
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), load::Makers {.highlight = load::MakeHighlightEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	const auto entity = loaded.at(2).entity;
	ASSERT_TRUE(entity != entt::null);
	const auto& highlight = Registry().Get<const ecs::components::ScriptHighlight>(entity);
	EXPECT_EQ(highlight.infoIndex, 1u);
	EXPECT_EQ(highlight.scriptId, 4184u);
	EXPECT_TRUE(highlight.drawHeight == std::optional<float> {0.0f});
	EXPECT_EQ(highlight.category, DykCategory::VillageLife);
	EXPECT_EQ(ecs::object_index::Of(entity), 2029);
	EXPECT_TRUE(Registry().AllOf<ecs::components::NameHidden>(entity));

	RegisterLandFeatureWriters(_writers);
	const WorldSource source {.registry = Registry(), .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::ScriptHighlightRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::ScriptHighlightRecord>(*written);
	EXPECT_EQ(gamesave::ScriptHighlightRecord::FieldsOf(record), gamesave::ScriptHighlightRecord::FieldsOf(saved));
	EXPECT_EQ(record.thing.flags, saved.thing.flags);
}

// A singing stone's base a script made is written as an object of no other class: its row, its place and height, its
// target where it was made, and the thing's marks of a hidden name
TEST_P(ForestWriteWorld, ASingingStonesBaseIsWrittenAsAnObjectOfNoOtherClass)
{
	auto& registry = Registry();
	const auto base = ecs::archetypes::MobileStaticArchetype::Create(
	    glm::vec3(2510.0f, k_Ground, 2520.0f), MobileStaticInfo::SingingStoneBase, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	ASSERT_TRUE(base != entt::null);
	registry.Assign<ecs::components::NameHidden>(base);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(registry, base), std::vector {gamesave::BaseOnlyRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::BaseOnlyRecord::k_Type))(context, base);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::BaseOnlyRecord>(*written);
	EXPECT_EQ(record.thing.flags, 0x40);
	EXPECT_EQ(record.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::SingingStoneBase));
	EXPECT_EQ(record.position.position.x, map_coords::ToFixed(2510.0f));
	EXPECT_EQ(record.object.coords, record.position.position);

	// a load of it makes the base again, its name hidden, at its height
	namespace load = ecs::saved_world_load;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(record);
	world.loadOrder = {2, 1};
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.stoneBases.size(), 1u);
	const auto again = load::MakeStoneBaseEntity(plan.stoneBases[0]);
	ASSERT_TRUE(registry.Valid(again));
	EXPECT_EQ(registry.Get<ecs::components::MobileStatic>(again).type, MobileStaticInfo::SingingStoneBase);
	EXPECT_TRUE(registry.AllOf<ecs::components::NameHidden>(again));
}

// A teleport's stone made in play is written as the original's stone: an object of several cells of the teleport's
// row, made whole, at a hundredth of its scale, with no owner, then the reaction it spreads while it lasts and its
// player; its player's list names its stones, the newest first
TEST_P(ForestWriteWorld, ATeleportsStoneMadeInPlayIsWrittenWithItsReactionAndPlayer)
{
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = Registry();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_TWO;
	const auto stoneAt = [&registry](glm::vec3 position, uint32_t serial) {
		const auto entity = registry.Create();
		ecs::object_index::Assign(entity);
		registry.Assign<ecs::components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& stone = registry.Assign<ecs::components::TeleportStone>(entity);
		stone.player = PlayerNames::PLAYER_TWO;
		stone.serial = serial;
		return entity;
	};
	const auto older = stoneAt(glm::vec3(2510.0f, k_Ground, 2520.0f), 1);
	const auto newer = stoneAt(glm::vec3(2530.0f, k_Ground, 2520.0f), 2);
	auto& stone = registry.Get<ecs::components::TeleportStone>(older);
	stone.reaction = reactions.Add({.initiator = older, .type = openblack::Reaction::ReactToTeleport});
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(registry, older), std::vector<uint32_t> {gamesave::MagicTeleportRecord::k_Type});
	const auto written = (*_writers.WriterOf(gamesave::MagicTeleportRecord::k_Type))(context, older);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::MagicTeleportRecord>(*written);
	EXPECT_EQ(record.base.object.info, ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Teleport));
	EXPECT_EQ(record.base.position.position.x, map_coords::ToFixed(2510.0f));
	EXPECT_EQ(record.base.object.creationIndex, static_cast<uint32_t>(ecs::object_index::Of(older)));
	EXPECT_EQ(record.base.object.scale, 0.01f);
	EXPECT_EQ(record.base.multi.fixedFlags, 0x08);
	EXPECT_EQ(record.mobileStatic.player, 0u);
	EXPECT_EQ(record.reaction.number,
	          context.Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = stone.reaction}));
	EXPECT_NE(record.reaction.number, 0u);
	EXPECT_EQ(record.spell.number, 0u);
	EXPECT_EQ(record.player.number, PlayerReference(context, PlayerNames::PLAYER_TWO));
	EXPECT_NE(record.player.number, 0u);
	// its reaction gone: none
	reactions.Remove(stone.reaction);
	const auto ended = (*_writers.WriterOf(gamesave::MagicTeleportRecord::k_Type))(context, older);
	ASSERT_TRUE(ended.has_value());
	EXPECT_EQ(std::get<gamesave::MagicTeleportRecord>(*ended).reaction.number, 0u);
	// the player's list: the newest first, another player's none
	EXPECT_EQ(ecs::saved_players::ObjectsOf(registry, PlayerNames::PLAYER_TWO).teleports,
	          (std::vector<entt::entity> {newer, older}));
	EXPECT_TRUE(ecs::saved_players::ObjectsOf(registry, PlayerNames::PLAYER_ONE).teleports.empty());
}

// A feature of the save is made again of its row at its place, angle and size, built as far as saved, with its creation
// number (a script's feature a load did not make left the script naming nothing)
TEST_P(ForestWriteWorld, ASavedFeatureIsMadeAgainFromItsRecord)
{
	namespace load = ecs::saved_world_load;
	auto& registry = Registry();
	const auto feature =
	    ecs::archetypes::FeatureArchetype::Create(glm::vec3(2510.0f, k_Ground, 2520.0f), FeatureInfo::AztcStatue, 0.5f, 1.5f);
	registry.Get<ecs::components::Feature>(feature).percentBuilt = 0.25f;
	RegisterLandFeatureWriters(_writers);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(gamesave::FeatureRecord::k_Type))(context, feature);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(load::FeatureTypeOfInfo(std::get<gamesave::FeatureRecord>(*written).base.object.info), FeatureInfo::AztcStatue);
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(*written);
	world.loadOrder = {2, 1};
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.features.size(), 1u);
	const auto again = load::MakeFeatureEntity(plan.features[0]);
	ASSERT_TRUE(registry.Valid(again));
	EXPECT_EQ(registry.Get<ecs::components::Feature>(again).type, FeatureInfo::AztcStatue);
	EXPECT_EQ(registry.Get<ecs::components::Feature>(again).percentBuilt, 0.25f);
	EXPECT_EQ(registry.Get<ecs::components::ObjectCreationIndex>(again).value,
	          registry.Get<ecs::components::ObjectCreationIndex>(feature).value);
}

// A dead and a felled tree of the save are made again of their tree rows and player, at their angles, with their wood
// multiplier; the felled one felled
TEST_P(ForestWriteWorld, ASavedDeadAndFelledTreeAreMadeAgainFromTheirRecords)
{
	namespace load = ecs::saved_world_load;
	auto& registry = Registry();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_ONE;
	const auto dead = ecs::archetypes::DeadTreeArchetype::Create(glm::vec3(2510.0f, k_Ground, 2520.0f), TreeInfo::Birch,
	                                                             PlayerNames::PLAYER_ONE, 1.0f, 0.0f, 0.5f, 0.0f);
	registry.Get<ecs::components::DeadTree>(dead).woodValueMultiplier = 1.5f;
	const auto felled = ecs::archetypes::DeadTreeArchetype::Create(glm::vec3(2530.0f, k_Ground, 2520.0f), TreeInfo::Cedar,
	                                                               PlayerNames::NEUTRAL, 1.0f, 0.0f, 0.0f, 0.0f);
	registry.Assign<ecs::components::FelledTree>(felled);
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	const auto deadRecord = (*_writers.WriterOf(gamesave::DeadTreeRecord::k_Type))(context, dead);
	const auto felledRecord = (*_writers.WriterOf(gamesave::FelledTreeRecord::k_Type))(context, felled);
	ASSERT_TRUE(deadRecord.has_value() && felledRecord.has_value()) << context.Failure();
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(*deadRecord);
	world.objects.emplace_back(*felledRecord);
	world.loadOrder = {2, 3, 1};
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.deadTrees.size(), 2u);
	const auto deadAgain = load::MakeDeadTreeEntity(plan.deadTrees[0]);
	const auto felledAgain = load::MakeDeadTreeEntity(plan.deadTrees[1]);
	ASSERT_TRUE(registry.Valid(deadAgain) && registry.Valid(felledAgain));
	EXPECT_EQ(registry.Get<ecs::components::DeadTree>(deadAgain).type, TreeInfo::Birch);
	EXPECT_EQ(registry.Get<ecs::components::DeadTree>(deadAgain).woodValueMultiplier, 1.5f);
	EXPECT_FALSE(registry.AllOf<ecs::components::FelledTree>(deadAgain));
	EXPECT_EQ(registry.Get<ecs::components::DeadTree>(felledAgain).type, TreeInfo::Cedar);
	EXPECT_TRUE(registry.AllOf<ecs::components::FelledTree>(felledAgain));
}

// Dropping a walker's move state tags keeps the state they gave, which the game keeps after the walk; with no tag
// nothing is kept, and a state kept before stays
TEST_P(ForestWriteWorld, DroppingAWalksTagsKeepsTheWalksState)
{
	using namespace ecs::components;
	auto& registry = Registry();
	const auto walker = registry.Create();
	registry.Assign<MoveStateStepThroughTag>(walker, MoveStateClockwise::Undefined, glm::vec2(0.0f));
	ecs::wall_hug::DropWalkTags<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                            MoveStateFinalStepTag, MoveStateArrivedTag>(registry, walker);
	EXPECT_FALSE(ecs::wall_hug::WalkStateOf(registry, walker).has_value());
	ASSERT_TRUE(registry.AllOf<WalkStateAfterWalk>(walker));
	EXPECT_EQ(registry.Get<const WalkStateAfterWalk>(walker).state,
	          static_cast<uint8_t>(ecs::wall_hug::WalkState::StepThrough));
	ecs::wall_hug::DropWalkTags<MoveStateFinalStepTag, MoveStateArrivedTag>(registry, walker);
	EXPECT_EQ(registry.Get<const WalkStateAfterWalk>(walker).state,
	          static_cast<uint8_t>(ecs::wall_hug::WalkState::StepThrough));
	const auto never = registry.Create();
	ecs::wall_hug::DropWalkTags<MoveStateFinalStepTag, MoveStateArrivedTag>(registry, never);
	EXPECT_FALSE(registry.AllOf<WalkStateAfterWalk>(never));
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
	// its mana path's effect is planned with its particles (interface 5030, manager 5031)
	const auto withPath = std::ranges::find_if(plan.worshipSites, [](const auto& site) { return site.manaPath.has_value(); });
	ASSERT_NE(withPath, plan.worshipSites.end());
	EXPECT_EQ(withPath->manaPath->interfaceNumber, 5030u);
	EXPECT_EQ(withPath->manaPath->managerNumber, 5031u);
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

	// clearing an area for a push, whose values a load does not put back yet (openblack never enters it), is not made
	record.values.living.action[0] = 185;
	const load::PlannedVillager breeding {.number = number, .type = planned.type, .record = record};
	EXPECT_EQ(load::MakeVillagerEntity(breeding, std::nullopt), entt::entity {entt::null});

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

// Integration test: needs the original game's data; skipped without it. A villager of the frozen Land 1 autosave that
// did two reactions is made with them and their turns, oldest first, and is written back with them as loaded
TEST_P(ForestWriteWorld, AVillagerThatDidReactionsIsMadeWithThemAndWrittenBack)
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
	// in a dance's state with its two words, which the load makes
	record.values.living.action[0] = 5;
	record.values.topState = {gamesave::StateValue {.size = 4}, gamesave::StateValue {.size = 4}};
	record.values.living.reactionsDone = {{.reaction = 3, .turn = 400}, {.reaction = 7, .turn = 512}};
	namespace load = ecs::saved_world_load;
	const load::PlannedVillager planned {
	    .number = number, .type = load::VillagerTypeOfInfo(record.object.info).value_or(VillagerInfo::None), .record = record};
	const auto entity = load::MakeVillagerEntity(planned, std::nullopt);
	ASSERT_NE(entity, entt::entity {entt::null});
	auto& registry = Registry();
	const auto& memory = registry.Get<const ecs::components::ReactionRecords>(entity);
	ASSERT_EQ(memory.count, 2);
	EXPECT_EQ(memory.records[0].type, 3);
	EXPECT_EQ(memory.records[0].turn, 400u);
	EXPECT_EQ(memory.records[1].type, 7);
	EXPECT_EQ(memory.records[1].turn, 512u);
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
	EXPECT_EQ(std::get<gamesave::VillagerRecord>(*written).values.living.reactionsDone, record.values.living.reactionsDone);
}
// A loaded villager's walk is made as its record holds it: stepping through to its goal with the turns to its next
// aim, walking straight with no obstacle ahead, and its last step, each written back with its state and values
TEST_P(ForestWriteWorld, ALoadedVillagersWalkIsMadeInItsStateAndWrittenBack)
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
	namespace load = ecs::saved_world_load;
	RegisterVillagerWriters(_writers);
	auto& registry = Registry();
	const auto roundTrip = [&](gamesave::VillagerRecord record) {
		record.values.living.action[0] = 5;
		record.values.topState = {gamesave::StateValue {.size = 4}, gamesave::StateValue {.size = 4}};
		const load::PlannedVillager planned {.number = number,
		                                     .type = load::VillagerTypeOfInfo(record.object.info).value_or(VillagerInfo::None),
		                                     .record = record};
		const auto entity = load::MakeVillagerEntity(planned, std::nullopt);
		EXPECT_NE(entity, entt::entity {entt::null});
		if (entity == entt::null)
		{
			return entity;
		}
		if ((record.position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.AssignOrReplace<ecs::components::DrawListed>(entity);
		}
		LoadedObjects loaded;
		loaded.try_emplace(number, LoadedObject {.entity = entity, .type = gamesave::VillagerRecord::k_Type});
		const WorldSource source {.registry = registry, .kept = &world, .loaded = loaded};
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
		EXPECT_TRUE(written.has_value()) << context.Failure();
		if (written.has_value())
		{
			const auto& move = std::get<gamesave::VillagerRecord>(*written).values.move;
			EXPECT_EQ(move.moveState, record.values.move.moveState);
			EXPECT_EQ(move.turnsToStepRebuild, record.values.move.turnsToStepRebuild);
			EXPECT_EQ(move.circle, record.values.move.circle);
		}
		return entity;
	};
	auto stepping = *found;
	stepping.values.move.moveState = 0xB;
	stepping.values.move.circle.reset();
	stepping.values.move.turnsToStepRebuild = 3;
	const auto stepper = roundTrip(stepping);
	ASSERT_NE(stepper, entt::entity {entt::null});
	EXPECT_TRUE(registry.AllOf<ecs::components::MoveStateStepThroughTag>(stepper));
	EXPECT_EQ(registry.Get<const ecs::components::WallHug>(stepper).turnsUntilStepRebuild, 3);

	auto straight = *found;
	straight.values.move.moveState = 0xC;
	straight.values.move.turnsToStepRebuild.reset();
	straight.values.move.circle = gamesave::CircleHug {.turnsToObject = 0xFF};
	const auto walker = roundTrip(straight);
	ASSERT_NE(walker, entt::entity {entt::null});
	EXPECT_TRUE(registry.AllOf<ecs::components::MoveStateLinearTag>(walker));
	EXPECT_FALSE(registry.AllOf<ecs::components::WallHugObjectReference>(walker));

	// a straight walk that let go of its circle keeps its turns to it and its entry distance, as the walk does
	auto letGo = *found;
	letGo.values.move.moveState = 0xD;
	letGo.values.move.turnsToStepRebuild.reset();
	letGo.values.move.circle = gamesave::CircleHug {.turnsToObject = 6, .entryDistance = 90};
	const auto lettingGo = roundTrip(letGo);
	ASSERT_NE(lettingGo, entt::entity {entt::null});
	EXPECT_FALSE(registry.AllOf<ecs::components::WallHugObjectReference>(lettingGo));
	EXPECT_EQ(registry.Get<const ecs::components::WallHug>(lettingGo).turnsToObj, 6);
	EXPECT_EQ(registry.Get<const ecs::components::WallHug>(lettingGo).entryDistance, 90u);

	// going round water with no end in reach, it still holds the water's circle
	auto aroundWater = *found;
	aroundWater.values.move.moveState = 0xF;
	aroundWater.values.move.turnsToStepRebuild.reset();
	aroundWater.values.move.circle = gamesave::CircleHug {
	    .turnsToObject = 0xFF,
	    .entryDistance = 500,
	    .index = 1,
	    .coords = {.x = map_coords::ToFixed(2015.0f), .z = map_coords::ToFixed(2005.0f), .altitude = 0.0f}};
	const auto rounder = roundTrip(aroundWater);
	ASSERT_NE(rounder, entt::entity {entt::null});
	EXPECT_TRUE(registry.AllOf<ecs::components::MoveStateOrbitTag>(rounder));
	ASSERT_TRUE(registry.AllOf<ecs::components::WallHugObjectReference>(rounder));
	EXPECT_TRUE(registry.Get<const ecs::components::WallHugObjectReference>(rounder).landscape);
	EXPECT_EQ(registry.Get<const ecs::components::WallHug>(rounder).turnsToObj, 0xFF);

	auto last = *found;
	last.values.move.moveState = 0x4;
	last.values.move.circle.reset();
	last.values.move.turnsToStepRebuild.reset();
	const auto finisher = roundTrip(last);
	ASSERT_NE(finisher, entt::entity {entt::null});
	EXPECT_TRUE(registry.AllOf<ecs::components::MoveStateFinalStepTag>(finisher));

	// one going round a thing it holds is made too, its circle the thing's once the thing is made
	auto round = *found;
	round.values.move.moveState = 0xF;
	round.values.living.action[0] = 5;
	round.values.topState = {gamesave::StateValue {.size = 4}, gamesave::StateValue {.size = 4}};
	round.values.move.circle = gamesave::CircleHug {.turnsToObject = 2, .object = {7}, .index = -1};
	const load::PlannedVillager roundPlanned {
	    .number = number, .type = load::VillagerTypeOfInfo(round.object.info).value_or(VillagerInfo::None), .record = round};
	EXPECT_NE(load::MakeVillagerEntity(roundPlanned, std::nullopt), entt::entity {entt::null});
}

// Integration test: needs the game's data; skipped without it. The villagers walking on a footpath in the first save
// that holds one are made on it, at their node and their way along it, with the footpath they take next, and written
// back with them as saved
TEST_P(ForestWriteWorld, TheFirstSavedFootpathWalkersAreMadeOnTheirFootpaths)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!GameData() || game == nullptr)
	{
		GTEST_SKIP() << "the saves are the game's data";
	}
	// the first world file, of the profiles' and the original's own saves, with a villager on a footpath
	const auto onAFootpath = [](const gamesave::WorldRead& read) {
		return std::ranges::any_of(read.objects, [](const auto& object) {
			const auto* villager = object.has_value() ? std::get_if<gamesave::VillagerRecord>(&*object) : nullptr;
			return villager != nullptr && villager->values.living.footpath.number != 0;
		});
	};
	std::optional<gamesave::WorldRead> chosen;
	for (const auto& root :
	     {std::filesystem::path(game) / "Profiles", std::filesystem::path(game).parent_path() / "original_saves"})
	{
		if (chosen.has_value() || !std::filesystem::exists(root))
		{
			continue;
		}
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
		{
			const auto name = entry.path().filename().string();
			if (!entry.is_regular_file() || entry.path().extension() != ".sav" || name.starts_with("Script_"))
			{
				continue;
			}
			std::ifstream file(entry.path(), std::ios::binary);
			const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
			auto read = gamesave::ReadWorld(bytes);
			if (read.stop == gamesave::k_ReadToTheEnd && onAFootpath(read))
			{
				chosen = std::move(read);
				break;
			}
		}
	}
	if (!chosen.has_value())
	{
		GTEST_SKIP() << "no save holds a villager on a footpath";
	}
	const auto& world = *chosen;
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	namespace load = ecs::saved_world_load;
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
	    .footpath = load::MakeFootpathEntity,
	    .footpathLink = [&](const load::PlannedFootpathLink& link,
	                        const std::vector<entt::entity>& /*footpaths*/) { return keeping(link.record); },
	    .town = [&](const load::PlannedTown& town) { return keeping(town.record); },
	    .tree = [&](const load::PlannedTree& tree, uint32_t /*forest*/) { return keeping(tree.record); },
	    .bigForest = [&](const load::PlannedBigForest& big, uint32_t /*forest*/) { return keeping(big.record); },
	    .pile = [&](const load::PlannedPile& pile) { return keepingOne(pile.record); },
	    .storagePit = [&](const load::PlannedStoragePit& pit, uint32_t /*town*/,
	                      const ecs::archetypes::AbodeArchetype::StoragePitPiles& /*piles*/) { return keeping(pit.record); },
	    .citadelHeart = [&](const load::PlannedCitadelHeart& heart, entt::entity /*citadel*/) { return keeping(heart.record); },
	    .abode = [&](const load::PlannedAbode& abode, uint32_t /*town*/) { return keepingOne(abode.record); },
	    .desireFlag = [&](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return keeping(flag.record); },
	    .stream = [&](const load::PlannedStream& stream) { return keeping(stream.record); },
	    .mist = [&](const load::PlannedMist& mist) { return keeping(mist.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	    .villager = load::MakeVillagerEntity};
	LoadedObjects loaded;
	std::ignore = load::Apply(load::PlanOf(world), makers, &loaded);
	RegisterVillagerWriters(_writers);
	RegisterFootpathWriters(_writers);
	auto& registry = Registry();
	const WorldSource source {.registry = registry, .kept = &world, .loaded = loaded};
	WriteContext context(_writers, source);
	size_t walkers = 0;
	std::vector<std::pair<entt::entity, gamesave::VillagerRecord>> onFootpaths;
	registry.Each<const ecs::components::LoadedRecord<gamesave::VillagerRecord>>(
	    [&onFootpaths](entt::entity entity, const auto& kept) {
		    if (kept.record.values.living.footpath.number != 0)
		    {
			    onFootpaths.emplace_back(entity, kept.record);
		    }
	    });
	for (const auto& [entity, saved] : onFootpaths)
	{
		++walkers;
		const auto* onFootpath = registry.TryGet<const ecs::components::LivingFootpath>(entity);
		ASSERT_NE(onFootpath, nullptr);
		EXPECT_NE(onFootpath->footpath, entt::entity {entt::null});
		if ((saved.position.flags & position_flags::k_DrawListed) != 0)
		{
			registry.AssignOrReplace<ecs::components::DrawListed>(entity);
		}
		const auto written = (*_writers.WriterOf(gamesave::VillagerRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		const auto& record = std::get<gamesave::VillagerRecord>(*written);
		EXPECT_EQ(record.values.living.footpath, saved.values.living.footpath);
		EXPECT_EQ(record.values.living.footpathNode, saved.values.living.footpathNode);
		EXPECT_EQ(record.values.living.status, saved.values.living.status);
		EXPECT_EQ(record.values.move.footpath, saved.values.move.footpath);
		EXPECT_EQ(record.values.move.moveState, saved.values.move.moveState);
	}
	EXPECT_GT(walkers, 0u);
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
// speeds, the camera as it holds it, its type, owner, state and times and the dances' label; each group by its dance
// and number, as a living dancing in it names it, with its speed and centres as measured; an untitled group is not
// held and fails
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

	// started: its camera and its groups' speed and centres as it holds them; an untitled group is not held
	dance.startTurn = 40;
	dance.useCamera = true;
	dance.cameraCalled = true;
	dance.cameraTravelTime = std::bit_cast<uint32_t>(3.5f);
	dance.cameraNearTurns = 120;
	WriteContext started(_writers, source);
	const auto watched = (*_writers.WriterOf(gamesave::DanceRecord::k_Type))(started, entity);
	ASSERT_TRUE(watched.has_value()) << started.Failure();
	const auto& camera = std::get<gamesave::DanceRecord>(*watched).group;
	EXPECT_EQ(camera.flags, (std::array<int32_t, 4> {1, 1, 1, 0}));
	EXPECT_EQ(camera.cameraTravelTime, 3.5f);
	EXPECT_EQ(camera.cameraNearTurns, 120u);
	dance.groups.all[1].speed = 400;
	dance.groups.all[1].hasLastCentre = true;
	dance.groups.all[1].centre = {.x = 7, .z = 8, .altitude = 1.0f};
	dance.groups.all[1].lastCentre = {.x = 5, .z = 6, .altitude = 1.0f};
	const auto measured = (*_writers.SourceWriterOf(gamesave::DanceGroupRecord::k_Type))(started, second.id);
	ASSERT_TRUE(measured.has_value()) << started.Failure();
	const auto& moving = std::get<gamesave::DanceGroupRecord>(*measured);
	EXPECT_EQ(moving.secondInt, 400);
	EXPECT_EQ(moving.thirdInt, 1);
	EXPECT_EQ(moving.firstPosition, (gamesave::MapCoords {.x = 7, .z = 8, .altitude = 1.0f}));
	EXPECT_EQ(moving.secondPosition, (gamesave::MapCoords {.x = 5, .z = 6, .altitude = 1.0f}));
	dance.startTurn = 0;
	dance.groups.all[1].name.clear();
	WriteContext untitled(_writers, source);
	EXPECT_FALSE((*_writers.SourceWriterOf(gamesave::DanceGroupRecord::k_Type))(untitled, second.id).has_value());
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
	dance.useCamera = saved.group.flags[0] != 0;
	dance.cameraCalled = saved.group.flags[2] != 0;
	dance.cameraTravelTime = std::bit_cast<uint32_t>(saved.group.cameraTravelTime);
	dance.cameraNearTurns = saved.group.cameraNearTurns;
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
		group.speed = record.secondInt;
		group.hasLastCentre = record.thirdInt != 0;
		group.centre = {.x = record.firstPosition.x, .z = record.firstPosition.z, .altitude = record.firstPosition.altitude};
		group.lastCentre = {
		    .x = record.secondPosition.x, .z = record.secondPosition.z, .altitude = record.secondPosition.altitude};
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
	registry.Each<const ecs::components::LoadedRecord<gamesave::BonfireRecord>>(
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
	// every one of the 13 burns an object the load makes, so each is made again and written by its writer
	EXPECT_EQ(counts[gamesave::FireEffectRecord::k_Type], 13u);
}

INSTANTIATE_TEST_SUITE_P(Data, ForestWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
