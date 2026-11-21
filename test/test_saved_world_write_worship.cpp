/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship sites written: a site made in play with its altar, what the altar shows of the site's needs and the three
// signs, and the original's own Land 2 save, whose sites, altars and signs hold the values the writer gives one made in
// play (docs/wiki/engine/saved-games.md#writing-the-world-file, the worship sites)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <gtest/gtest.h>

#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/ShowNeeds.h"
#include "Worship/WorshipSite.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::CitadelHeartRecord;
using gamesave::CitadelRecord;
using gamesave::SavedRecord;
using gamesave::ShowNeedsRecord;
using gamesave::ShowNeedsVisualsRecord;
using gamesave::WorshipSiteRecord;
using gamesave::WorshipSpellIconRecord;
using gamesave::WorshipTotemRecord;
namespace components = openblack::ecs::components;

namespace
{
/// The rows of the worship sites, the tribes and the signs, each table counted down from its first row
constexpr uint32_t k_FirstWorshipSiteRow = 2405;
constexpr uint32_t k_FirstTribeRow = 742;
constexpr uint32_t k_FirstShowNeedsRow = 1855;

class WorshipWriteWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		RegisterCitadelHeartWriters(_writers);
		RegisterWorshipSiteWriters(_writers);
	}

	/// A site of the tribe in the citadel's slot, its altar where the site stands and the altar's three signs, made as
	/// the game makes them, with no dance and no food pot
	struct Made
	{
		entt::entity heart {entt::null};
		entt::entity site {entt::null};
		entt::entity totem {entt::null};
	};
	Made MakeSite(Tribe tribe, int slot)
	{
		const glm::vec3 at(1500.0f, k_Ground, 1500.0f);
		Made made;
		made.heart = ecs::archetypes::CitadelArchetype::CreateHeart(at, PlayerNames::PLAYER_ONE, entt::null, 0.0f, 1.0f, 1.0f,
		                                                            false, false);
		made.site = worship::site::CreateInSlot(made.heart, tribe, slot, 0.5f);
		auto& registry = Registry();
		registry.AssignOrReplace<components::CitadelPartBuild>(made.site);
		made.totem = registry.Create();
		ecs::object_index::Assign(made.totem);
		const glm::vec3 totemAt(1510.0f, k_Ground + 4.0f, 1490.0f);
		registry.Assign<components::Transform>(made.totem, totemAt, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<components::WorshipTotem>(made.totem, made.site);
		registry.Assign<components::CitadelPartBuild>(made.totem);
		auto& site = registry.Get<components::WorshipSite>(made.site);
		site.totem = made.totem;
		for (uint8_t row = 0; row < ecs::show_needs::k_AltarSigns; ++row)
		{
			site.needsSigns.at(row) = ecs::show_needs::Create(totemAt, made.totem, row);
		}
		return made;
	}

	ClassWriters _writers;
};

template <typename Record>
const Record& Written(const std::optional<SavedRecord>& written)
{
	return std::get<Record>(*written);
}

/// The original's own save of Land 2, or nothing without it (local reference data beside the game's folder)
std::optional<std::vector<uint8_t>> OriginalLandTwoSave()
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game).parent_path() / "original_saves" / "land2_resumed_save60" / "slot20" /
	                       "20.sav",
	                   std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

// A site made in play is written with its altar, what the altar shows and its signs: the site of its tribe's rows, at
// the citadel's origin with no height, its row's influence and its citadel, its slot, its altar and what the altar
// shows, and the values a new site starts with; the altar of the site's row at its own place with no height, the same
// influence and citadel, no specular colour and its site; what the altar shows names the altar and its three signs in
// their order; each sign of its row, at the altar's place with no height, its own scale 1, what the altar shows and its
// desire
TEST_P(WorshipWriteWorld, ASiteMadeInPlayIsWrittenWithItsAltarAndSigns)
{
	const auto made = MakeSite(Tribe::GREEK, 2);
	ASSERT_TRUE(made.heart != entt::null && made.site != entt::null);
	auto& registry = Registry();
	auto& site = registry.Get<components::WorshipSite>(made.site);
	site.battery = 12.5f;
	site.villagersAtSite = 3;
	site.foodOnTheWay = 4;
	registry.Get<components::ShowNeedsVisuals>(site.needsSigns[1]).desire = 0.75f;
	const WorldSource source {.registry = registry};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(source.registry, made.site), std::vector {WorshipSiteRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(source.registry, made.totem), std::vector {WorshipTotemRecord::k_Type});
	EXPECT_EQ(_writers.ClaimsOf(source.registry, site.needsSigns[0]), std::vector {ShowNeedsVisualsRecord::k_Type});
	const auto siteNumber = context.Reference(made.site);
	const auto totemNumber = context.Reference(made.totem);
	const auto needsNumber = context.Reference(made.totem, ShowNeedsRecord::k_Type);
	const auto citadelNumber = context.Reference(made.heart, CitadelRecord::k_Type);
	const auto row = k_FirstWorshipSiteRow - static_cast<uint32_t>(Tribe::GREEK);
	const auto influence = Info().worshipSite.at(static_cast<size_t>(Tribe::GREEK)).influence;

	const auto writtenSite = (*_writers.WriterOf(WorshipSiteRecord::k_Type))(context, made.site);
	ASSERT_TRUE(writtenSite.has_value()) << context.Failure();
	const auto& siteRecord = Written<WorshipSiteRecord>(writtenSite);
	EXPECT_EQ(siteRecord.part.base.object.info, row);
	EXPECT_EQ(siteRecord.part.base.position.position.altitude, 0.0f);
	EXPECT_EQ(siteRecord.part.base.object.creationIndex, static_cast<uint32_t>(ecs::object_index::Of(made.site)));
	EXPECT_EQ(siteRecord.part.influence, influence);
	EXPECT_EQ(siteRecord.part.citadel.number, citadelNumber);
	EXPECT_EQ(siteRecord.part.other.number, 0u);
	EXPECT_EQ(siteRecord.tribeInfo, k_FirstTribeRow - static_cast<uint32_t>(Tribe::GREEK));
	EXPECT_EQ(siteRecord.showNeeds.number, needsNumber);
	EXPECT_EQ(siteRecord.dance.number, 0u);
	EXPECT_EQ(siteRecord.foodPot.number, 0u);
	EXPECT_EQ(siteRecord.words, (std::array<uint32_t, 2> {}));
	EXPECT_EQ(siteRecord.byte, 0);
	EXPECT_EQ(siteRecord.worshippers, 3u);
	EXPECT_EQ(siteRecord.word, 0u);
	EXPECT_EQ(siteRecord.cheatSpellSeed, 0u);
	EXPECT_EQ(siteRecord.resourcesOnTheWay.values, (std::vector<uint32_t> {4, 0}));
	EXPECT_EQ(siteRecord.chants[0], 12.5f);
	EXPECT_EQ(siteRecord.slot, 2);
	EXPECT_EQ(siteRecord.totem.number, totemNumber);
	EXPECT_EQ(siteRecord.manaPath.number, 0u);

	const auto writtenTotem = (*_writers.WriterOf(WorshipTotemRecord::k_Type))(context, made.totem);
	ASSERT_TRUE(writtenTotem.has_value()) << context.Failure();
	const auto& totem = Written<WorshipTotemRecord>(writtenTotem);
	EXPECT_EQ(totem.part.base.object.info, row);
	EXPECT_EQ(totem.part.base.position.position.altitude, 0.0f);
	EXPECT_NE(totem.part.base.position.position.x, siteRecord.part.base.position.position.x);
	EXPECT_EQ(totem.part.base.object.creationIndex, siteRecord.part.base.object.creationIndex + 1);
	EXPECT_EQ(totem.part.influence, influence);
	EXPECT_EQ(totem.part.citadel.number, citadelNumber);
	EXPECT_EQ(totem.part.other.number, 0u);
	EXPECT_EQ(totem.specularColour, 0u);
	EXPECT_EQ(totem.site.number, siteNumber);

	const auto writtenNeeds = (*_writers.WriterOf(ShowNeedsRecord::k_Type))(context, made.totem);
	ASSERT_TRUE(writtenNeeds.has_value()) << context.Failure();
	const auto& needs = Written<ShowNeedsRecord>(writtenNeeds);
	EXPECT_EQ(needs.object.number, totemNumber);
	ASSERT_EQ(needs.visuals.numbers.size(), 3u);
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(needs.visuals.numbers[i], context.Reference(site.needsSigns.at(i))) << i;
		const auto writtenSign = (*_writers.WriterOf(ShowNeedsVisualsRecord::k_Type))(context, site.needsSigns.at(i));
		ASSERT_TRUE(writtenSign.has_value()) << context.Failure();
		const auto& sign = Written<ShowNeedsVisualsRecord>(writtenSign);
		EXPECT_EQ(sign.position.position, totem.part.base.position.position) << i;
		EXPECT_EQ(sign.position.flags, position_flags::k_CountsAsFixed) << i;
		EXPECT_EQ(sign.object.info, k_FirstShowNeedsRow - i) << i;
		EXPECT_EQ(sign.object.coords, sign.position.position) << i;
		EXPECT_EQ(sign.object.creationIndex, totem.part.base.object.creationIndex + 1 + i) << i;
		EXPECT_EQ(sign.object.life, 1.0f);
		EXPECT_EQ(sign.object.scale, 1.0f);
		EXPECT_EQ(sign.object.heading, 0.0f);
		EXPECT_EQ(sign.words[0], 0u);
		EXPECT_EQ(std::bit_cast<float>(sign.words[1]), 1.0f);
		EXPECT_EQ(sign.other.number, needsNumber);
		EXPECT_EQ(std::bit_cast<float>(sign.lastWord), i == 1 ? 0.75f : 0.0f);
	}
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A loaded site is written as its load kept it, with the spell icons it holds now in its list's order: an icon deleted
// since the load has left the site's list as it went, so the site does not name it (the save was refused for such an
// icon, which the kept record still named)
TEST_P(WorshipWriteWorld, ALoadedSiteNamesOnlyTheSpellIconsItHoldsNow)
{
	const auto made = MakeSite(Tribe::GREEK, 2);
	ASSERT_TRUE(made.heart != entt::null && made.site != entt::null);
	auto& registry = Registry();
	// three icons as the load makes them, each with the record it kept, the head of the list first
	std::vector<entt::entity> icons;
	for (int16_t slot = 10; slot < 13; ++slot)
	{
		const auto icon = registry.Create();
		auto& ofSite = registry.Assign<components::WorshipSpellIcon>(icon);
		ofSite.site = made.site;
		ofSite.slot = slot;
		registry.Assign<components::LoadedRecord<WorshipSpellIconRecord>>(icon, WorshipSpellIconRecord {});
		icons.push_back(icon);
	}
	auto& site = registry.Get<components::WorshipSite>(made.site);
	site.icons = icons;
	WorshipSiteRecord loaded;
	loaded.slot = 2;
	loaded.chants[0] = 7.5f;
	loaded.icons.numbers = {101, 102, 103};
	registry.Assign<components::LoadedRecord<WorshipSiteRecord>>(made.site, loaded);
	const WorldSource source {.registry = registry,
	                          .loaded = {{101, LoadedObject {.entity = icons[0], .type = WorshipSpellIconRecord::k_Type}},
	                                     {102, LoadedObject {.entity = icons[1], .type = WorshipSpellIconRecord::k_Type}},
	                                     {103, LoadedObject {.entity = icons[2], .type = WorshipSpellIconRecord::k_Type}}}};

	// right after the load: the record as kept
	{
		WriteContext context(_writers, source);
		const auto written = (*_writers.WriterOf(WorshipSiteRecord::k_Type))(context, made.site);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		EXPECT_TRUE(Written<WorshipSiteRecord>(written) == loaded);
		EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	}

	// the middle icon deleted: out of the site's list, then gone
	std::erase(site.icons, icons[1]);
	registry.Destroy(icons[1]);
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(WorshipSiteRecord::k_Type))(context, made.site);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = Written<WorshipSiteRecord>(written);
	EXPECT_EQ(record.icons.numbers, (std::vector<gamesave::ObjectNumber> {101, 103}));
	EXPECT_EQ(record.slot, 2);
	EXPECT_EQ(record.chants[0], 7.5f);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A loaded site is written without the worshippers gone since its load, its count down by one for each, as a villager
// comes off its site's worship list as it is deleted; its dance and its totem none once gone, as its links go with them
TEST_P(WorshipWriteWorld, ALoadedSiteIsWrittenWithoutTheWorshippersGoneSinceItsLoad)
{
	const auto made = MakeSite(Tribe::GREEK, 2);
	ASSERT_TRUE(made.heart != entt::null && made.site != entt::null);
	auto& registry = Registry();
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	const auto dance = registry.Create();
	WorshipSiteRecord loaded;
	loaded.worshipping.numbers = {40, 41};
	loaded.worshippers = 2;
	loaded.dance.number = 42;
	registry.Assign<components::LoadedRecord<WorshipSiteRecord>>(made.site, loaded);
	const WorldSource source {.registry = registry,
	                          .loaded = {{40, LoadedObject {.entity = alive, .type = gamesave::VillagerRecord::k_Type}},
	                                     {41, LoadedObject {.entity = dead, .type = gamesave::VillagerRecord::k_Type}},
	                                     {42, LoadedObject {.entity = dance, .type = gamesave::DanceRecord::k_Type}}}};
	registry.Destroy(dead);
	registry.Destroy(dance);
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(WorshipSiteRecord::k_Type))(context, made.site);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = Written<WorshipSiteRecord>(written);
	EXPECT_EQ(record.worshipping.numbers, (std::vector<gamesave::ObjectNumber> {40}));
	EXPECT_EQ(record.worshippers, 1u);
	EXPECT_EQ(record.dance.number, 0u);
}

// Integration test: needs the original game's data and the original's own save of Land 2 (local reference data beside
// the game's folder); skipped without them. Each of its worship sites, altars and signs holds what the writer gives one
// made in play: the site and its altar of the row its tribe's row names, with no height and their row's influence, the
// object after the citadel 0; the site's two words, byte and word, and the cheat's seed 0, food then wood on their way;
// the altar of its site and citadel with no specular colour; what it shows names the altar and three signs of the
// first three rows, which count as fixed only, at the altar's place with no height, whole, at scale 1, turned 0, with
// the creation numbers after the altar's, their word 0, their own scale 1 and what the altar shows
TEST_P(WorshipWriteWorld, TheOriginalsLandTwoSitesHoldWhatANewSiteStartsWith)
{
	const auto bytes = OriginalLandTwoSave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the original's save of Land 2 is local reference data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto recordAt = [&world](gamesave::ObjectNumber number) -> const SavedRecord* {
		return number != 0 && number <= world.objects.size() && world.objects.at(number - 1).has_value()
		           ? &*world.objects.at(number - 1)
		           : nullptr;
	};
	size_t sites = 0;
	for (size_t index = 0; index < world.objects.size(); ++index)
	{
		const auto* site =
		    world.objects.at(index).has_value() ? std::get_if<WorshipSiteRecord>(&*world.objects.at(index)) : nullptr;
		if (site == nullptr)
		{
			continue;
		}
		++sites;
		const auto number = static_cast<gamesave::ObjectNumber>(index + 1);
		const auto tribe = k_FirstTribeRow - site->tribeInfo;
		const auto row = k_FirstWorshipSiteRow - tribe;
		EXPECT_EQ(site->part.base.object.info, row) << number;
		EXPECT_EQ(site->part.base.position.position.altitude, 0.0f) << number;
		EXPECT_EQ(site->part.influence, Info().worshipSite.at(tribe).influence) << number;
		EXPECT_EQ(site->part.other.number, 0u) << number;
		EXPECT_EQ(site->words, (std::array<uint32_t, 2> {})) << number;
		EXPECT_EQ(site->byte, 0) << number;
		EXPECT_EQ(site->word, 0u) << number;
		EXPECT_EQ(site->cheatSpellSeed, 0u) << number;
		EXPECT_EQ(site->resourcesOnTheWay.values.size(), 2u) << number;
		const auto* citadel = recordAt(site->part.citadel.number);
		ASSERT_NE(citadel, nullptr) << number;
		ASSERT_TRUE(std::holds_alternative<CitadelRecord>(*citadel)) << number;
		const auto* heart = recordAt(std::get<CitadelRecord>(*citadel).heart.number);
		ASSERT_NE(heart, nullptr) << number;
		EXPECT_EQ(site->part.base.position.position, std::get<CitadelHeartRecord>(*heart).base.position.position) << number;

		const auto* totemRead = recordAt(site->totem.number);
		ASSERT_NE(totemRead, nullptr) << number;
		const auto& totem = std::get<WorshipTotemRecord>(*totemRead);
		EXPECT_EQ(totem.part.base.object.info, row) << number;
		EXPECT_EQ(totem.part.base.position.position.altitude, 0.0f) << number;
		EXPECT_EQ(totem.part.influence, site->part.influence) << number;
		EXPECT_EQ(totem.part.citadel.number, site->part.citadel.number) << number;
		EXPECT_EQ(totem.part.other.number, 0u) << number;
		EXPECT_EQ(totem.specularColour, 0u) << number;
		EXPECT_EQ(totem.site.number, number) << number;
		EXPECT_EQ(totem.part.base.object.heading, site->part.base.object.heading) << number;

		const auto* needsRead = recordAt(site->showNeeds.number);
		ASSERT_NE(needsRead, nullptr) << number;
		const auto& needs = std::get<ShowNeedsRecord>(*needsRead);
		EXPECT_EQ(needs.object.number, site->totem.number) << number;
		ASSERT_EQ(needs.visuals.numbers.size(), 3u) << number;
		for (size_t i = 0; i < 3; ++i)
		{
			const auto* signRead = recordAt(needs.visuals.numbers[i]);
			ASSERT_NE(signRead, nullptr) << number;
			const auto& sign = std::get<ShowNeedsVisualsRecord>(*signRead);
			EXPECT_EQ(sign.position.position, totem.part.base.position.position) << number << " " << i;
			EXPECT_EQ(sign.position.flags, position_flags::k_CountsAsFixed) << number << " " << i;
			EXPECT_EQ(sign.object.info, k_FirstShowNeedsRow - i) << number << " " << i;
			EXPECT_EQ(sign.object.coords, sign.position.position) << number << " " << i;
			EXPECT_EQ(sign.object.creationIndex, totem.part.base.object.creationIndex + 1 + i) << number << " " << i;
			EXPECT_EQ(sign.object.life, 1.0f) << number << " " << i;
			EXPECT_EQ(sign.object.scale, 1.0f) << number << " " << i;
			EXPECT_EQ(sign.object.heading, 0.0f) << number << " " << i;
			EXPECT_FALSE(sign.object.physics.has_value()) << number << " " << i;
			EXPECT_EQ(sign.words[0], 0u) << number << " " << i;
			EXPECT_EQ(std::bit_cast<float>(sign.words[1]), 1.0f) << number << " " << i;
			EXPECT_EQ(sign.other.number, site->showNeeds.number) << number << " " << i;
		}
	}
	EXPECT_EQ(sites, 4u);
}

// Integration test: as above. Each workshop of the original's Land 2 save names its wood pile (a magic wood) and its
// sign of the wood it wants: a sign of the workshop's wood row at the workshop's place and height, counting as
// fixed only, its own scale 1, belonging to the workshop; and its three scaffold slots
TEST_P(WorshipWriteWorld, TheOriginalsLandTwoWorkshopsNameTheirPileAndTheirSign)
{
	const auto bytes = OriginalLandTwoSave();
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the original's save of Land 2 is local reference data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto recordAt = [&world](gamesave::ObjectNumber number) -> const SavedRecord* {
		return number != 0 && number <= world.objects.size() && world.objects.at(number - 1).has_value()
		           ? &*world.objects.at(number - 1)
		           : nullptr;
	};
	size_t workshops = 0;
	for (size_t index = 0; index < world.objects.size(); ++index)
	{
		const auto* workshop =
		    world.objects.at(index).has_value() ? std::get_if<gamesave::WorkshopRecord>(&*world.objects.at(index)) : nullptr;
		if (workshop == nullptr)
		{
			continue;
		}
		++workshops;
		const auto number = static_cast<gamesave::ObjectNumber>(index + 1);
		EXPECT_EQ(workshop->bytes.values.size(), 3u) << number;
		const auto* pile = recordAt(workshop->first.number);
		ASSERT_NE(pile, nullptr) << number;
		EXPECT_TRUE(std::holds_alternative<gamesave::MagicWoodRecord>(*pile)) << number;
		const auto* signRead = recordAt(workshop->second.number);
		ASSERT_NE(signRead, nullptr) << number;
		const auto& sign = std::get<ShowNeedsVisualsRecord>(*signRead);
		EXPECT_EQ(sign.position.position, workshop->base.position.position) << number;
		EXPECT_EQ(sign.position.flags, position_flags::k_CountsAsFixed) << number;
		EXPECT_EQ(sign.object.info, k_FirstShowNeedsRow - 3) << number;
		EXPECT_EQ(sign.object.coords, sign.position.position) << number;
		EXPECT_EQ(sign.words[0], 0u) << number;
		EXPECT_EQ(std::bit_cast<float>(sign.words[1]), 1.0f) << number;
		EXPECT_EQ(sign.other.number, number) << number;
	}
	EXPECT_GT(workshops, 0u);
}

INSTANTIATE_TEST_SUITE_P(Data, WorshipWriteWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
