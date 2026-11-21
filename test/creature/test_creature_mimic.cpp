/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the game tells the players' creatures (ECS/CreatureMimic.h): the deeds published with their player, object,
// place and miracle, the town needs shared only with a player, the handler that passes the deeds on to the minds, the
// minds' own filter (only the player's own creature watches, and only it draws), and two of the game's sites: a
// field watered by a player's miracle and the deed numbers the game's table is read by

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/sparse_set.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLastInteraction.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/LastDropped.h"
#include "ECS/Life.h"
#include "ECS/ObjectResources.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Spells/SpellWater.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::creature_watching::Deed;
using openblack::ecs::PhysicsEntry;
using openblack::ecs::events::CreatureEmpathyWithTownDesire;
using openblack::ecs::events::PlayerDeedForMimic;
namespace creature_mimic = openblack::ecs::creature_mimic;

namespace
{
/// The registry's storages, each with how many it holds, and the three random streams' seeds
struct Snapshot
{
	std::vector<std::pair<entt::id_type, size_t>> storages;
	uint32_t synced {0};
	uint32_t local {0};
	uint32_t crt {0};

	bool operator==(const Snapshot&) const = default;
};

Snapshot Take()
{
	Snapshot snapshot;
	std::as_const(test::creature_world::World::Registry())
	    .EachStorage([&snapshot](entt::id_type id, const entt::sparse_set& storage) {
		    snapshot.storages.emplace_back(id, storage.size());
	    });
	snapshot.synced = game_random::Current().synced;
	snapshot.local = game_random::Current().local;
	snapshot.crt = game_random::crt::Seed();
	return snapshot;
}

/// What is published, as the tests' own handlers hear it (the game's handler is added by the tests that want it)
struct Heard
{
	Heard()
	{
		auto& events = Locator::events::value();
		events.AddHandler<PlayerDeedForMimic>([this](const PlayerDeedForMimic& event) { deeds.push_back(event); });
		events.AddHandler<CreatureEmpathyWithTownDesire>(
		    [this](const CreatureEmpathyWithTownDesire& event) { empathies.push_back(event); });
	}
	Heard(const Heard&) = delete;
	Heard& operator=(const Heard&) = delete;

	std::vector<PlayerDeedForMimic> deeds;
	std::vector<CreatureEmpathyWithTownDesire> empathies;
};

entt::entity MakeThing(std::optional<glm::vec3> position)
{
	auto& registry = test::creature_world::World::Registry();
	const auto entity = registry.Create();
	if (position.has_value())
	{
		registry.Assign<Transform>(entity, *position, glm::mat3(1.0f), glm::vec3(1.0f));
	}
	return entity;
}

/// A registry, the tables and the events of their own
class CreatureMimicTest: public ::testing::Test
{
protected:
	test::creature_world::World _world;
	Heard _heard;
	std::vector<PlayerDeedForMimic>& deeds = _heard.deeds;
	std::vector<CreatureEmpathyWithTownDesire>& empathies = _heard.empathies;
};

/// The same with the world's lists too (the fires), for the game's own sites
class CreatureMimicSiteTest: public ::testing::Test
{
protected:
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
	Heard _heard;
	std::vector<PlayerDeedForMimic>& deeds = _heard.deeds;
};
} // namespace

TEST(CreatureDeeds, TheRowsOfTheGamesTable)
{
	EXPECT_EQ(static_cast<size_t>(Deed::PutFoodInWorshipSite), 0u);
	EXPECT_EQ(static_cast<size_t>(Deed::PlantTree), 11u);
	EXPECT_EQ(static_cast<size_t>(Deed::DamageByThrowingAt), 16u);
	EXPECT_EQ(static_cast<size_t>(Deed::ThrowInTheSea), 21u);
	EXPECT_EQ(static_cast<size_t>(Deed::CastWaterOnCrops), 33u);
	EXPECT_EQ(static_cast<size_t>(Deed::Sacrifice), 40u);
	EXPECT_EQ(static_cast<size_t>(Deed::PlayWithToy), 41u);
	EXPECT_EQ(static_cast<size_t>(Deed::StealWoodFromStoragePit), 45u);
	EXPECT_EQ(creature_watching::k_DeedCount, 46u);
	EXPECT_EQ(std::tuple_size_v<decltype(InfoConstants::creatureMimic)>, creature_watching::k_DeedCount);
}

TEST(CreatureMimicDecision, ABuildingHitCountsOnlyWithAPlayerAndItsOwnBodyFromTheHand)
{
	EXPECT_FALSE(creature_mimic::ShouldMimicBuildingHit(std::nullopt, PhysicsEntry::k_FromHand));
	EXPECT_FALSE(creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_ONE, 0));
	EXPECT_FALSE(creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_ONE,
	                                                    PhysicsEntry::k_Awake | PhysicsEntry::k_NoObjectCollision));
	EXPECT_TRUE(creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_ONE, PhysicsEntry::k_FromHand));
	EXPECT_TRUE(
	    creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_TWO, PhysicsEntry::k_Awake | PhysicsEntry::k_FromHand));
}

TEST_F(CreatureMimicTest, ADeedIsPublishedOnceWithItsPlayerObjectPlaceAndMiracle)
{
	const auto tree = MakeThing(glm::vec3(12.0f, 3.0f, -7.0f));
	creature_mimic::Consider(PlayerNames::PLAYER_TWO, Deed::PlantTree, tree);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.front().deed, Deed::PlantTree);
	EXPECT_EQ(deeds.front().object, tree);
	EXPECT_EQ(deeds.front().point, glm::vec3(12.0f, 3.0f, -7.0f));
	EXPECT_FALSE(deeds.front().magic.has_value());

	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::CastWaterOnCrops, tree, MagicType::Water);
	ASSERT_EQ(deeds.size(), 2u);
	ASSERT_TRUE(deeds.back().magic.has_value());
	EXPECT_EQ(*deeds.back().magic, MagicType::Water);
	EXPECT_TRUE(empathies.empty());
}

TEST_F(CreatureMimicTest, SomethingWithNoPlaceIsAtTheOrigin)
{
	const auto nowhere = MakeThing(std::nullopt);
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::DamageByThrowingAt, nowhere);
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::DamageByThrowingAt, entt::null);
	ASSERT_EQ(deeds.size(), 2u);
	EXPECT_EQ(deeds.at(0).point, glm::vec3(0.0f));
	EXPECT_EQ(deeds.at(1).point, glm::vec3(0.0f));
	EXPECT_TRUE(deeds.at(1).object == entt::null);
}

TEST_F(CreatureMimicTest, AVillagerInTheSeaReportsTheHandThatLastDroppedIt)
{
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_ONE);
	const auto thrown = MakeThing(glm::vec3(5.0f, 0.0f, 5.0f));
	registry.Assign<Villager>(thrown);
	ecs::last_dropped::RecordDrop(registry, PlayerNames::PLAYER_ONE, thrown);
	const auto walked = MakeThing(glm::vec3(6.0f, 0.0f, 6.0f));
	registry.Assign<Villager>(walked);
	// one that a thrown thing hit keeps the thrower's player as the last to have touched it, but no hand let go of it
	const auto struck = MakeThing(glm::vec3(7.0f, 0.0f, 7.0f));
	registry.Assign<Villager>(struck);
	registry.AssignState<VillagerLastInteraction>(struck, PlayerNames::PLAYER_ONE);

	creature_mimic::ConsiderThrownInTheSea(walked);
	creature_mimic::ConsiderThrownInTheSea(struck);
	EXPECT_TRUE(deeds.empty());
	creature_mimic::ConsiderThrownInTheSea(thrown);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, Deed::ThrowInTheSea);
	EXPECT_EQ(deeds.front().object, thrown);
	EXPECT_EQ(deeds.front().point, glm::vec3(5.0f, 0.0f, 5.0f));
}

TEST_F(CreatureMimicTest, ATownNeedIsSharedOnlyWithAPlayer)
{
	const auto villager = MakeThing(glm::vec3(1.0f, 2.0f, 3.0f));
	creature_mimic::EmpathiseWithTownDesire(std::nullopt, TownDesireInfo::ForFood, 0.5f, villager);
	EXPECT_TRUE(empathies.empty());

	creature_mimic::EmpathiseWithTownDesire(PlayerNames::PLAYER_ONE, TownDesireInfo::ForWood, 0.5f, villager);
	ASSERT_EQ(empathies.size(), 1u);
	EXPECT_EQ(empathies.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(empathies.front().desire, TownDesireInfo::ForWood);
	EXPECT_FLOAT_EQ(empathies.front().weight, 0.5f);
	EXPECT_EQ(empathies.front().point, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(deeds.empty());
}

TEST_F(CreatureMimicTest, AThrownVillagersLandingShowsItsThrowersCreatureItsGodKindOrCruel)
{
	// with its thrower's player only, the landing pose's desire and weight at where the villager came down, passed on to
	// the minds by the handler
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	test::creature_loop_fakes::CallLog log;
	Locator::creatureMindSystem::emplace<test::creature_loop_fakes::FakeMind>(log);
	creature_mimic::AddMimicEventHandlers(Locator::events::value());
	std::vector<ecs::events::CreatureEmpathyWithPlayerDesire> shown;
	Locator::events::value().AddHandler<ecs::events::CreatureEmpathyWithPlayerDesire>(
	    [&shown](const ecs::events::CreatureEmpathyWithPlayerDesire& event) { shown.push_back(event); });
	const auto villager = MakeThing(glm::vec3(1.0f, 2.0f, 3.0f));
	creature_mimic::EmpathiseWithThrownVillager(std::nullopt, CreatureDesires::Compassion, 0.1f, villager);
	EXPECT_TRUE(shown.empty());
	EXPECT_TRUE(log.empty());
	creature_mimic::EmpathiseWithThrownVillager(PlayerNames::PLAYER_TWO, CreatureDesires::Anger, 0.5f, villager);
	ASSERT_EQ(shown.size(), 1u);
	EXPECT_EQ(shown.front().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(shown.front().desire, CreatureDesires::Anger);
	EXPECT_FLOAT_EQ(shown.front().weight, 0.5f);
	EXPECT_EQ(shown.front().point, glm::vec3(1.0f, 2.0f, 3.0f));
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.at(0).name, "mind.EmpathiseWithPlayer");
	EXPECT_EQ(log.at(0).args, (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_TWO), 2.0f, 0.5f, 1.0f, 2.0f, 3.0f}));
	EXPECT_TRUE(deeds.empty());
	EXPECT_TRUE(empathies.empty());
}

TEST_F(CreatureMimicTest, TheHandlerPassesTheDeedOnToTheMinds)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	test::creature_loop_fakes::CallLog log;
	Locator::creatureMindSystem::emplace<test::creature_loop_fakes::FakeMind>(log);
	creature_mimic::AddMimicEventHandlers(Locator::events::value());
	const auto field = MakeThing(glm::vec3(4.0f, 0.5f, 8.0f));

	creature_mimic::Consider(PlayerNames::PLAYER_TWO, Deed::CastWaterOnCrops, field, MagicType::Water);
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::DamageByThrowingAt, entt::null);

	ASSERT_EQ(log.size(), 2u);
	EXPECT_EQ(log.at(0).name, "mind.PlayerDid");
	EXPECT_EQ(log.at(0).args, (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_TWO), 33.0f, 4.0f, 0.5f, 8.0f,
	                                               static_cast<float>(entt::to_integral(field))}));
	// no object: none passed on
	EXPECT_EQ(log.at(1).args,
	          (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_ONE), 16.0f, 0.0f, 0.0f, 0.0f, -1.0f}));
}

TEST_F(CreatureMimicTest, WithoutMindsTheHandlerDoesNothing)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	Locator::creatureMindSystem::reset();
	creature_mimic::AddMimicEventHandlers(Locator::events::value());
	const auto before = Take();
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::PlantTree, MakeThing(glm::vec3(1.0f)));
	EXPECT_EQ(deeds.size(), 1u);
	const auto after = Take();
	// nothing drawn: the thing made for the deed is the only change
	EXPECT_EQ(after.synced, before.synced);
	EXPECT_EQ(after.local, before.local);
	EXPECT_EQ(after.crt, before.crt);
}

TEST_F(CreatureMimicTest, WithNoCreatureTheMindsMakeNothingAndDrawNothing)
{
	const game_random::testing::ScopedState state;
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const auto villager = MakeThing(glm::vec3(10.0f, 0.0f, 10.0f));
	registry.Assign<Villager>(villager);
	const auto before = Take();
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), glm::vec3(10.0f, 0.0f, 10.0f), villager);
	EXPECT_EQ(Take(), before);
}

TEST_F(CreatureMimicTest, OnlyThePlayersOwnLeashableCreatureWatchesAndNothingIsDrawn)
{
	// a deed every creature that sees it considers, ranked 1
	_world.Info().creatureMimic.at(static_cast<size_t>(Deed::PlantTree)).chance = 1.0f;
	const game_random::testing::ScopedState state;
	size_t draws = 0;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&draws](float) {
		                                  ++draws;
		                                  return 0.0f;
	                                  });
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	// the local player's creature: a computer player's creature copies its player never
	const auto creature = test::creature_world::World::MakeCreature(at, PlayerNames::PLAYER_ONE);
	// its first turn sets up what it has learnt
	minds.ProcessTurn();
	draws = 0;
	const auto& learnt = registry.Get<CreatureMindState>(creature).learnt;
	ASSERT_TRUE(learnt.has_value());

	// another player's deed, and one the creature may not watch as it is not the one its player leashes
	minds.PlayerDid(PlayerNames::PLAYER_TWO, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	EXPECT_FALSE(learnt->mimicry.has_value());
	const auto& observed = registry.Get<CreatureMindState>(creature).observedDeed;
	EXPECT_FALSE(observed.has_value());
	registry.Get<Creature>(creature).leashable = true;
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	ASSERT_TRUE(learnt->mimicry.has_value());
	EXPECT_EQ(learnt->mimicry->rule, static_cast<size_t>(Deed::PlantTree));
	// the deed it started to copy is the last it saw
	ASSERT_TRUE(observed.has_value());
	EXPECT_EQ(observed->rule, static_cast<size_t>(Deed::PlantTree));
	EXPECT_FALSE(observed->object.has_value());
	EXPECT_EQ(observed->magic, 0u);
	// it starts by its rank alone: no chance is drawn
	EXPECT_EQ(draws, 0u);
}

// Noticing its god's deed raises the first source of the deed's desire by a tenth and its wish to follow its god by
// three tenths, each kept within 0..1, as the original's start of copying a seen deed (docs/wiki/creature/mimicry.md)
TEST_F(CreatureMimicTest, NoticingADeedStirsItsDesiresFirstSourceAndTheWishToFollowItsGod)
{
	constexpr uint32_t k_FirstSource = 7;
	constexpr uint32_t k_FollowPlayer = 54;
	auto& rule = _world.Info().creatureMimic.at(static_cast<size_t>(Deed::PlantTree));
	rule.chance = 1.0f;
	rule.desire = static_cast<uint32_t>(creature_desires::Desire::Compassion);
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	const auto creature = test::creature_world::World::MakeCreature(at, PlayerNames::PLAYER_ONE);
	registry.Get<Creature>(creature).leashable = true;
	// its first turn sets up what it has learnt and its desires
	minds.ProcessTurn();
	auto& desires = registry.Get<CreatureMindState>(creature).desires;
	ASSERT_TRUE(desires.has_value());
	for (auto& desire : desires->desires)
	{
		desire.sources.clear();
	}
	auto& compassion = (*desires)[creature_desires::Desire::Compassion].sources;
	compassion.push_back({.type = k_FirstSource, .value = 0.2f});
	compassion.push_back({.type = 8, .value = 0.2f});
	(*desires)[creature_desires::Desire::IdleWithPlayer].sources.push_back({.type = k_FollowPlayer, .value = 0.9f});

	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	ASSERT_TRUE(registry.Get<CreatureMindState>(creature).observedDeed.has_value());
	EXPECT_FLOAT_EQ(compassion.at(0).value, 0.2f + 0.1f);
	// only the first source is stirred
	EXPECT_FLOAT_EQ(compassion.at(1).value, 0.2f);
	// the wish to follow its god is kept within 0..1
	EXPECT_FLOAT_EQ((*desires)[creature_desires::Desire::IdleWithPlayer].sources.at(0).value, 1.0f);
}

// Its anger spares its own player's things from its making until it sets about copying its god's anger at one of them:
// a thing of another player, or one already destroyed, leaves it as it was; one taken out of play still counts as its
// player's (docs/wiki/creature/mimicry.md)
TEST_F(CreatureMimicTest, CopyingItsGodsAngerAtItsGodsOwnThingLetsItsAngerLooseOnThem)
{
	// a deed for its anger, and one for its compassion
	auto& angry = _world.Info().creatureMimic.at(static_cast<size_t>(Deed::DamageByThrowing));
	angry.chance = 1.0f;
	angry.desire = static_cast<uint32_t>(creature_desires::Desire::Anger);
	auto& kind = _world.Info().creatureMimic.at(static_cast<size_t>(Deed::PlantTree));
	kind.chance = 1.0f;
	kind.desire = static_cast<uint32_t>(creature_desires::Desire::Compassion);
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	const auto creature = test::creature_world::World::MakeCreature(at, PlayerNames::PLAYER_ONE);
	registry.Get<Creature>(creature).leashable = true;
	const auto& mind = registry.Get<CreatureMindState>(creature);
	EXPECT_TRUE(mind.angerSparesOwnPlayer);
	minds.ProcessTurn();
	ASSERT_TRUE(mind.learnt.has_value());
	const auto rock = [&registry, at](PlayerNames owner) {
		const auto thing = MakeThing(at);
		registry.Assign<MobileStatic>(thing, MobileStatic {.owner = owner});
		return thing;
	};
	const auto deedOn = [&](entt::entity thing, Deed deed = Deed::DamageByThrowing) {
		// each deed starts a copy afresh
		registry.Get<CreatureMindState>(creature).learnt->mimicry.reset();
		minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(deed), at, thing);
		EXPECT_TRUE(mind.learnt->mimicry.has_value());
	};

	deedOn(rock(PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(mind.angerSparesOwnPlayer);
	deedOn(rock(PlayerNames::NEUTRAL));
	EXPECT_TRUE(mind.angerSparesOwnPlayer);
	const auto gone = rock(PlayerNames::PLAYER_ONE);
	registry.Destroy(gone);
	deedOn(gone);
	EXPECT_TRUE(mind.angerSparesOwnPlayer);
	// a deed for another desire at its own player's thing
	deedOn(rock(PlayerNames::PLAYER_ONE), Deed::PlantTree);
	EXPECT_TRUE(mind.angerSparesOwnPlayer);
	// anger at its own player's thing, one taken out of play
	const auto marked = rock(PlayerNames::PLAYER_ONE);
	registry.Assign<Unavailable>(marked);
	deedOn(marked);
	EXPECT_FALSE(mind.angerSparesOwnPlayer);
	// and it stays so
	deedOn(rock(PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(mind.angerSparesOwnPlayer);
}

TEST_F(CreatureMimicSiteTest, AFieldWateredByAPlayersMiracleIsTheirDeed)
{
	auto& registry = test::creature_world::World::Registry();
	const auto field = MakeThing(glm::vec3(30.0f, 0.0f, 40.0f));
	registry.Assign<Field>(field).town = -1;
	const auto withPlayer = registry.Create();
	auto& cast = registry.Assign<Spell>(withPlayer);
	cast.magicType = MagicType::WaterPowerUpOne;
	cast.player = PlayerNames::PLAYER_ONE;
	cast.hasPlayer = true;
	const auto withoutPlayer = registry.Create();
	registry.Assign<Spell>(withoutPlayer).magicType = MagicType::Water;

	magic::water::ApplyWaterSpell(field, withoutPlayer);
	EXPECT_TRUE(deeds.empty());
	magic::water::ApplyWaterSpell(field, withPlayer);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, Deed::CastWaterOnCrops);
	EXPECT_EQ(deeds.front().object, field);
	EXPECT_EQ(deeds.front().point, glm::vec3(30.0f, 0.0f, 40.0f));
	// the plain water miracle, whichever water was cast
	ASSERT_TRUE(deeds.front().magic.has_value());
	EXPECT_EQ(*deeds.front().magic, MagicType::Water);
}

TEST_F(CreatureMimicSiteTest, AMiracleNearAnotherPlayersTownImpressesItUnlessItIsMeantToAnger)
{
	auto& registry = test::creature_world::World::Registry();
	auto& info = _world.Info();
	info.magicEffect.at(static_cast<size_t>(MagicType::Heal)).perceivedPlayerDesire[0] = CreatureDesires::Compassion;
	info.magicEffect.at(static_cast<size_t>(MagicType::LightningBolt)).perceivedPlayerDesire[0] = CreatureDesires::Anger;
	const auto town = MakeThing(glm::vec3(0.0f, 0.0f, 0.0f));
	registry.Assign<Town>(town).owner = PlayerNames::PLAYER_TWO;
	const auto near = MakeThing(glm::vec3(99.9f, 0.0f, 0.0f));
	const auto far = MakeThing(glm::vec3(100.0f, 0.0f, 0.0f));
	ecs::systems::GameMagicWorld world;

	// another player's town strictly within the reach is impressed, whatever the miracle reached
	world.PlayerAffected(near, MagicType::Heal, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.back().deed, Deed::ImpressWithMagic);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_ONE);
	// at the reach it is not near: the heal's own deed
	world.PlayerAffected(far, MagicType::Heal, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(deeds.size(), 2u);
	EXPECT_EQ(deeds.back().deed, Deed::Heal);
	// the player's own town impresses nothing
	world.PlayerAffected(near, MagicType::Heal, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(deeds.size(), 3u);
	EXPECT_EQ(deeds.back().deed, Deed::Heal);
	// a miracle meant to anger skips the town: the lightning's own deed
	world.PlayerAffected(near, MagicType::LightningBolt, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(deeds.size(), 4u);
	EXPECT_EQ(deeds.back().deed, Deed::DamageWithMagic);
}

TEST_F(CreatureMimicSiteTest, AFieldWateredIsCropsWateredAndNothingGoneIsReported)
{
	auto& registry = test::creature_world::World::Registry();
	const auto field = MakeThing(glm::vec3(30.0f, 0.0f, 40.0f));
	registry.Assign<Field>(field).town = -1;
	ecs::systems::GameMagicWorld world;
	world.PlayerAffected(field, MagicType::Water, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.back().deed, Deed::CastWaterOnCrops);
	EXPECT_EQ(deeds.back().object, field);
	registry.Destroy(field);
	world.PlayerAffected(field, MagicType::Water, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.size(), 1u);
}

TEST_F(CreatureMimicSiteTest, FoodAndWoodShowWhatTheyReached)
{
	auto& registry = test::creature_world::World::Registry();
	ecs::systems::GameMagicWorld world;
	const auto deedOf = [&](entt::entity object, MagicType type) {
		const auto before = deeds.size();
		world.PlayerAffected(object, type, PlayerNames::PLAYER_ONE);
		return deeds.size() > before ? std::optional(deeds.back().deed) : std::nullopt;
	};
	// a worship site, built or not
	const auto site = MakeThing(glm::vec3(10.0f, 0.0f, 0.0f));
	registry.Assign<WorshipSite>(site);
	auto& siteBuild = registry.Assign<CitadelPartBuild>(site);
	siteBuild.percentBuilt = 0.0f;
	EXPECT_EQ(deedOf(site, MagicType::Food), Deed::CastMagicFoodInWorshipSite);
	// a storage pit counts only once built
	const auto pit = MakeThing(glm::vec3(20.0f, 0.0f, 0.0f));
	registry.Assign<StoragePit>(pit);
	auto& pitAbode = registry.Assign<Abode>(pit);
	EXPECT_EQ(deedOf(pit, MagicType::Food), Deed::CastMagicFoodInStoragePit);
	EXPECT_EQ(deedOf(pit, MagicType::Wood), Deed::CastMagicWoodInStoragePit);
	pitAbode.buildFlags = Abode::k_UnderConstruction;
	pitAbode.percentBuilt = 0.5f;
	EXPECT_EQ(deedOf(pit, MagicType::Food), std::nullopt);
	// wood by a building going up, then by one damaged and not yet repaired, then by one whole
	const auto house = MakeThing(glm::vec3(30.0f, 0.0f, 0.0f));
	auto& houseAbode = registry.Assign<Abode>(house);
	houseAbode.percentBuilt = 0.5f;
	EXPECT_EQ(deedOf(house, MagicType::Wood), Deed::CastMagicWoodByBuildingSite);
	houseAbode.percentBuilt = 1.0f;
	houseAbode.buildFlags = Abode::k_Built | Abode::k_NotRepaired;
	ecs::life::SetLife(house, 0.5f);
	EXPECT_EQ(deedOf(house, MagicType::Wood), Deed::CastMagicWoodByBuildingSite);
	houseAbode.buildFlags = Abode::k_Built;
	EXPECT_EQ(deedOf(house, MagicType::Wood), std::nullopt);
}

TEST_F(CreatureMimicSiteTest, AResourceGivenByHandShowsTheDeedOfWhatTookIt)
{
	auto& registry = test::creature_world::World::Registry();
	const ecs::pot_resource::Dropper hand {.hasInterface = true, .player = PlayerNames::PLAYER_TWO};
	const auto deedOf = [&](entt::entity abode, ResourceType type, const ecs::pot_resource::Dropper& dropper) {
		const auto before = deeds.size();
		ecs::object_resources::ReportResourceDeed(abode, type, dropper);
		return deeds.size() > before ? std::optional(deeds.back().deed) : std::nullopt;
	};
	// food to a worship site, wood to a workshop
	const auto site = MakeThing(glm::vec3(10.0f, 0.0f, 0.0f));
	registry.Assign<WorshipSite>(site);
	EXPECT_EQ(deedOf(site, ResourceType::Food, hand), Deed::PutFoodInWorshipSite);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.back().object, site);
	EXPECT_EQ(deedOf(site, ResourceType::Wood, hand), std::nullopt);
	const auto workshop = MakeThing(glm::vec3(20.0f, 0.0f, 0.0f));
	registry.Assign<Workshop>(workshop);
	auto& workshopAbode = registry.Assign<Abode>(workshop);
	EXPECT_EQ(deedOf(workshop, ResourceType::Wood, hand), Deed::PutWoodInWorkshop);
	EXPECT_EQ(deedOf(workshop, ResourceType::Food, hand), std::nullopt);
	// wood to a building with a building site is the building site's deed, whatever the building
	workshopAbode.buildingSite = MakeThing(glm::vec3(20.0f, 0.0f, 0.0f));
	EXPECT_EQ(deedOf(workshop, ResourceType::Wood, hand), Deed::PutWoodInBuildingSite);
	const auto house = MakeThing(glm::vec3(30.0f, 0.0f, 0.0f));
	registry.Assign<Abode>(house).buildingSite = MakeThing(glm::vec3(30.0f, 0.0f, 0.0f));
	EXPECT_EQ(deedOf(house, ResourceType::Wood, hand), Deed::PutWoodInBuildingSite);
	EXPECT_EQ(deedOf(house, ResourceType::Food, hand), std::nullopt);
	// with no interface nothing is reported
	EXPECT_EQ(deedOf(site, ResourceType::Food, {}), std::nullopt);
}

TEST_F(CreatureMimicSiteTest, AThingPutDownNearThePlayersTempleIsTakenHome)
{
	auto& registry = test::creature_world::World::Registry();
	const auto temple = MakeThing(glm::vec3(100.0f, 0.0f, 100.0f));
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_ONE;
	const auto rival = MakeThing(glm::vec3(400.0f, 0.0f, 100.0f));
	registry.Assign<Temple>(rival).owner = PlayerNames::PLAYER_TWO;
	const auto near = MakeThing(glm::vec3(150.0f, 0.0f, 120.0f));
	creature_mimic::ConsiderLanding(near, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.back().deed, Deed::TakeObjectHome);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.back().object, near);
	// 80 m or more from the player's own temple, with no town about, it is nothing; another player's temple is not home
	creature_mimic::ConsiderLanding(MakeThing(glm::vec3(180.0f, 0.0f, 100.0f)), PlayerNames::PLAYER_ONE);
	creature_mimic::ConsiderLanding(MakeThing(glm::vec3(410.0f, 0.0f, 100.0f)), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.size(), 1u);
}

TEST_F(CreatureMimicSiteTest, AThingAHandLetGoOfIsReportedForThePlayerWhoseHandItWas)
{
	// a computer player's hand lets go of a thing near its own temple: taken home, for that player. A thing no hand let
	// go of is nothing
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_ONE);
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_TWO);
	const auto temple = MakeThing(glm::vec3(100.0f, 0.0f, 100.0f));
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_TWO;
	const auto near = MakeThing(glm::vec3(150.0f, 0.0f, 120.0f));
	creature_mimic::ConsiderObjectLands(near);
	EXPECT_TRUE(deeds.empty());
	ecs::last_dropped::RecordDrop(registry, PlayerNames::PLAYER_TWO, near);
	creature_mimic::ConsiderObjectLands(near);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.back().deed, Deed::TakeObjectHome);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.back().object, near);
	// thrown by the local hand after that: both interfaces keep it and the first player counts, whose temple is not
	// near, so nothing more
	ecs::last_dropped::MarkThrown(registry, PlayerNames::PLAYER_ONE, near);
	creature_mimic::ConsiderObjectLands(near);
	EXPECT_EQ(deeds.size(), 1u);
}

TEST_F(CreatureMimicSiteTest, AThingsEndOfFlightIsReportedForItsLastDroppersPlayerLastOnEveryPath)
{
	// the end of a thing's physics, last of all: a thing back on the map near its last dropper's temple is taken home,
	// one already marked for deletion (neither put back nor deleted) is still reported, and one no hand let go of is
	// nothing
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_ONE);
	const auto temple = MakeThing(glm::vec3(100.0f, 0.0f, 100.0f));
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_ONE;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(registry));
	const auto thrown = MakeThing(glm::vec3(150.0f, 0.0f, 120.0f));
	ecs::last_dropped::RecordDrop(registry, PlayerNames::PLAYER_ONE, thrown);
	EXPECT_TRUE(dynamics.EndPhysicsAsObject(thrown));
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.back().deed, Deed::TakeObjectHome);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.back().object, thrown);
	const auto marked = MakeThing(glm::vec3(140.0f, 0.0f, 110.0f));
	registry.Assign<Unavailable>(marked);
	ecs::last_dropped::RecordDrop(registry, PlayerNames::PLAYER_ONE, marked);
	EXPECT_FALSE(dynamics.EndPhysicsAsObject(marked));
	ASSERT_EQ(deeds.size(), 2u);
	EXPECT_EQ(deeds.back().deed, Deed::TakeObjectHome);
	EXPECT_EQ(deeds.back().object, marked);
	EXPECT_TRUE(dynamics.EndPhysicsAsObject(MakeThing(glm::vec3(150.0f, 0.0f, 130.0f))));
	EXPECT_EQ(deeds.size(), 2u);
	// one already destroyed when its end of flight comes is nothing, and nothing of it is read
	const auto gone = MakeThing(glm::vec3(145.0f, 0.0f, 115.0f));
	ecs::last_dropped::RecordDrop(registry, PlayerNames::PLAYER_ONE, gone);
	registry.Destroy(gone);
	creature_mimic::ConsiderObjectLands(gone);
	EXPECT_EQ(deeds.size(), 2u);
}

// A disciple landing on its first turn shows the deed of its type to the player whose hand let go of it last, whoever the
// local player is; one no hand let go of shows nothing
TEST_F(CreatureMimicSiteTest, ADiscipleLandingShowsTheDeedOfItsTypeToItsLastDroppersPlayer)
{
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_ONE);
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_TWO);
	const auto villager = MakeThing(glm::vec3(150.0f, 0.0f, 120.0f));
	registry.Assign<Villager>(villager).discipleType = static_cast<uint8_t>(VillagerDisciple::Farmer);
	ecs::villager::ShowDiscipleToCreature(villager);
	EXPECT_TRUE(deeds.empty());
	ecs::last_dropped::RecordDrop(registry, PlayerNames::PLAYER_TWO, villager);
	ecs::villager::ShowDiscipleToCreature(villager);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.back().deed, Deed::MakeDiscipleFarmer);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.back().object, villager);
}

// Noticing its god's deed done to a thing, it is made curious about the thing and plays the table's noticing action on
// it at its next decision once free, as the original forces it while it notices; a deed done to no thing is noticed
// with no action
// (docs/wiki/project/progress/creature/learning_by_observation.md)
TEST_F(CreatureMimicTest, NoticingADeedDoneToAThingForcesTheTablesNoticingActionOnItWithCuriosityAtItsNextDecision)
{
	constexpr uint32_t k_NoticeActionRow = 244;
	constexpr uint32_t k_StageActions = 9;
	auto& rule = _world.Info().creatureMimic.at(static_cast<size_t>(Deed::PlantTree));
	rule.chance = 1.0f;
	rule.noticeAction = k_NoticeActionRow;
	// the shipped table's count of actions per stage for planting a tree; with none, noticing would end at once
	rule.stageSteps = k_StageActions;
	std::ranges::copy(std::string_view("NoticeAction"), _world.Info().creatureAction.at(k_NoticeActionRow).name.begin());
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	const auto creature = test::creature_world::World::MakeCreature(at, PlayerNames::PLAYER_ONE);
	registry.Get<Creature>(creature).leashable = true;
	minds.ProcessTurn();
	const auto& mind = registry.Get<CreatureMindState>(creature);

	// a thing: what it was doing stops at once, and at its next decision the noticing action is forced on it for its
	// curiosity, about the thing
	const auto thing = MakeThing(glm::vec3(110.0f, 0.0f, 100.0f));
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, thing);
	EXPECT_TRUE(mind.noticedWith.empty());
	EXPECT_FALSE(mind.planActive);
	ASSERT_TRUE(mind.learnt->mimicry.has_value());
	EXPECT_EQ(mind.learnt->mimicry->object, entt::to_integral(thing));
	minds.ProcessTurn();
	EXPECT_EQ(mind.noticedWith, "NoticeAction");
	// it still notices until an action is finished
	ASSERT_TRUE(mind.learnt->mimicry.has_value());
	EXPECT_EQ(mind.learnt->mimicry->stage, creature_watching::MimicStage::Notice);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_NoticeActionRow);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
	EXPECT_EQ(mind.planner.current->object, entt::to_integral(thing));
	EXPECT_TRUE(mind.planner.current->forced);

	// a deed done to no thing, seen afresh: it starts to copy it, but plays no noticing action
	auto& again = registry.Get<CreatureMindState>(creature);
	again.learnt->mimicry.reset();
	again.noticedWith.clear();
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	ASSERT_TRUE(mind.learnt->mimicry.has_value());
	EXPECT_FALSE(mind.learnt->mimicry->object.has_value());
	minds.ProcessTurn();
	EXPECT_TRUE(mind.noticedWith.empty());

	// once copying has moved on from noticing, a notice still waiting is not played
	again.learnt->mimicry.reset();
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, thing);
	ASSERT_TRUE(mind.learnt->mimicry.has_value());
	again.learnt->mimicry->stage = creature_watching::MimicStage::CopyAction;
	minds.ProcessTurn();
	EXPECT_TRUE(mind.noticedWith.empty());
}

// Copying the action, each copy action it can do is forced in turn, first to the thing the deed was done to and then
// again to a thing like it, each in place of the one before, for the desire the deed's row names; after each one forced
// it stops there one time in two, by GameRand(2) (docs/wiki/creature/mimicry.md#the-stages-of-copying)
TEST_F(CreatureMimicTest, CopyingTheActionForcesEachCopyInTurnStoppingOneTimeInTwoAfterEach)
{
	constexpr uint32_t k_FirstCopy = 245;
	constexpr uint32_t k_SecondCopy = 246;
	constexpr uint32_t k_NoSkill = 6;
	auto& rule = _world.Info().creatureMimic.at(static_cast<size_t>(Deed::PlantTree));
	rule.chance = 1.0f;
	rule.copyActions[0] = k_FirstCopy;
	rule.copyActions[1] = k_SecondCopy;
	rule.desire = static_cast<uint32_t>(creature_desires::Desire::Curiosity);
	for (const auto row : {k_FirstCopy, k_SecondCopy})
	{
		// an action done to nothing that a creature can always do, needing no skill and no miracle
		auto& action = _world.Info().creatureAction.at(row);
		std::ranges::copy(std::string_view("Puke"), action.name.begin());
		action.desire = static_cast<uint32_t>(creature_desires::Desire::Hunger);
		action.skillsNeeded[0] = k_NoSkill;
		action.skillsNeeded[1] = k_NoSkill;
	}
	const game_random::testing::ScopedState state;
	uint32_t stopDraw = 1;
	game_random::testing::SetGameRand([&stopDraw](uint32_t range) { return range == 2 ? stopDraw : 0u; },
	                                  [](float) { return 0.0f; });
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	const auto creature = test::creature_world::World::MakeCreature(at, PlayerNames::PLAYER_ONE);
	registry.Get<Creature>(creature).leashable = true;
	minds.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto thing = MakeThing(glm::vec3(110.0f, 0.0f, 100.0f));

	// never stopped: all four copies are forced, the last one standing
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, thing);
	ASSERT_TRUE(mind.learnt->mimicry.has_value());
	mind.learnt->mimicry->stage = creature_watching::MimicStage::CopyAction;
	mind.learnt->mimicry->limit = 5;
	minds.ProcessTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_SecondCopy);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
	EXPECT_TRUE(mind.planActive);

	// stopped at the first draw: only the first copy is forced
	stopDraw = 0;
	mind.learnt->mimicry.reset();
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, thing);
	ASSERT_TRUE(mind.learnt->mimicry.has_value());
	mind.learnt->mimicry->stage = creature_watching::MimicStage::CopyAction;
	mind.learnt->mimicry->limit = 5;
	minds.ProcessTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_FirstCopy);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
}
