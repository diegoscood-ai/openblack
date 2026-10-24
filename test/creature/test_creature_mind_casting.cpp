/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature mind's casting, on a registry of its own with a fake of the creatures' movement: the miracle an action
// casts, how often it has been seen, whether the creature may try it (half the sightings, nothing of nothing failing,
// a power-up only once grown, the body able to pay), what it casts, and going near, getting away from and turning to
// face what it casts at, as the game does them

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureSpellCasting.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "GameClock.h"
#include "creature/CreatureSystemWorld.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_mind::Movement;
using creature_mind::SubMove;

namespace
{
/// The creatures' movement: what it was asked, answering as the test says
class FakeMoves final: public CreatureLocomotionSystemInterface
{
public:
	struct Asked
	{
		glm::vec2 point;
		float minDistance;
		float maxDistance;
	};
	std::vector<Asked> moves;
	MoveResult answer {MoveResult::Started};
	bool moving {false};
	bool turns {true};
	int stops {0};
	int turnsAsked {0};

	void ProcessTurn() override {}
	void Update(float) override {}
	void AnimationAt(entt::entity, float, CreatureAnimationInputs&) const override {}
	MoveResult MoveTo(entt::entity, glm::vec2 point, Pace, float minDistance, float maxDistance) override
	{
		moves.push_back({.point = point, .minDistance = minDistance, .maxDistance = maxDistance});
		return answer;
	}
	MoveResult WalkBack(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override
	{
		++turnsAsked;
		return turns;
	}
	void Stop(entt::entity) override { ++stops; }
	void Place(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return moving; }
	[[nodiscard]] bool IsValidPosition(glm::vec2, float) const override { return true; }
};

constexpr uint32_t k_Lightning = 4;
constexpr uint32_t k_CastAction = 45;
constexpr uint32_t k_PowerUpAction = 99;

/// The miracles as the casting mind sees them: what a creature may cast at, and what it casts and lets go of
class FakeMagic final: public test::InertMagicSystem
{
public:
	bool CastByCreature(entt::entity /*creature*/, MagicType type, entt::entity /*target*/) override
	{
		cast.push_back(type);
		return castWorks;
	}
	bool CastByCreatureAt(entt::entity /*creature*/, MagicType type, glm::vec3 place) override
	{
		cast.push_back(type);
		places.push_back(place);
		return castWorks;
	}
	void ReleaseCreatureCast(entt::entity /*creature*/) override { ++releases; }
	[[nodiscard]] bool CanCreatureCastAt(MagicType /*type*/, entt::entity /*target*/) override { return castableAt; }
	[[nodiscard]] bool CanCreatureCastAtPlace(MagicType /*type*/, glm::vec3 /*place*/) override { return castableAt; }

	bool castableAt {true};
	bool castWorks {true};
	std::vector<MagicType> cast;
	std::vector<glm::vec3> places;
	uint32_t releases {0};
};

/// The advisor, keeping the messages raised
class FakeHelp final: public ecs::systems::CreatureHelpSystemInterface
{
public:
	void Raise(entt::entity creature, const creature_help::Message& message) override
	{
		raised.emplace_back(creature, message);
	}
	void ProcessTurn() override {}
	void PromptKnown(entt::entity /*creature*/, creature_help::Prompt /*prompt*/) override {}
	void Fainting(entt::entity /*creature*/) override {}
	void SetHelpOn(bool /*on*/) override {}
	[[nodiscard]] bool IsHelpOn() const override { return true; }
	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

	std::vector<std::pair<entt::entity, creature_help::Message>> raised;

private:
	std::vector<Told> _told;
};

class CreatureMindCastingTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_moves = &static_cast<FakeMoves&>(Locator::creatureLocomotionSystem::emplace<FakeMoves>());
		auto& info = _world.Info();
		info.creatureAction.at(k_CastAction).magicType = k_Lightning;
		info.creatureAction.at(k_PowerUpAction).magicType = k_Lightning;
		// the miracle costs a tenth of the energy of a grown creature of strength one half
		auto& species = info.creature.at(creature::InfoRow(CreatureType::GiantApe));
		species.chantsPerEnergy = 36000.0f;
		species.spellEnergyFloor = 0.005f;
		species.spellSizeFactor = 1.0f;
		info.magicEffect.at(k_Lightning).costToCreate = 9000.0f;
		_tables.miracles.resize(42);
		_tables.miracles.at(k_Lightning).timesToSee = 6;
	}

	/// A creature of the Giant Ape at a point, with a mind that has seen the lightning so often
	entt::entity MakeCreature(glm::vec3 position, uint32_t seen)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Creature>(entity).species = CreatureType::GiantApe;
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureLocomotion>(entity);
		registry.Assign<CreatureNeeds>(entity).needs.energy = 1.0f;
		auto& mind = registry.Assign<CreatureMindState>(entity);
		mind.learnt.emplace();
		mind.learnt->knowledge.miraclesSeen.resize(42);
		mind.learnt->knowledge.miraclesKnown.resize(42);
		mind.learnt->knowledge.miraclesSeen.at(k_Lightning).count = seen;
		return entity;
	}

	static entt::entity MakeThing(glm::vec3 position)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	static Movement MoveOf(Movement::Kind kind, entt::entity target, float keep)
	{
		return {.kind = kind, .object = entt::to_integral(target), .maxDistance = keep};
	}

	[[nodiscard]] float Needed() const
	{
		return creature_watching::TimesNeeded(
		    _tables.miracles.at(k_Lightning).timesToSee,
		    creature_mind_tables::MiracleMultiplier(creature::InfoRow(CreatureType::GiantApe)));
	}

	// put back after the registry, which the entities may reach as they go
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreMoves;
	test::creature_world::World _world;
	creature_mind_tables::Tables _tables;
	FakeMoves* _moves {nullptr};
};
} // namespace

TEST_F(CreatureMindCastingTest, AnActionCastsTheMiracleOfItsRowFromTheFirstToTheLastACreatureCasts)
{
	auto& info = _world.Info();
	EXPECT_EQ(mind_detail::CastMagicOf(k_CastAction), k_Lightning);
	info.creatureAction.at(1).magicType = 41;
	EXPECT_EQ(mind_detail::CastMagicOf(1), 41u);
	info.creatureAction.at(2).magicType = 42;
	EXPECT_FALSE(mind_detail::CastMagicOf(2).has_value());
	// none for an action that casts nothing, or past the table
	EXPECT_FALSE(mind_detail::CastMagicOf(3).has_value());
	EXPECT_FALSE(mind_detail::CastMagicOf(static_cast<uint32_t>(info.creatureAction.size())).has_value());
}

TEST_F(CreatureMindCastingTest, TheWorkshopsWoodIsNeverTriedAsItsRowCastsNoMiracle)
{
	// the game's table gives the row of wood by a workshop no miracle, so its executor is never weighed
	constexpr uint32_t k_WoodByWorkshop = 302;
	auto& info = _world.Info();
	info.creatureAction.at(k_WoodByWorkshop).magicType = 0;
	_tables.actions.resize(k_WoodByWorkshop + 1);
	_tables.actions.at(k_WoodByWorkshop).name = "CastMagicWoodByWorkshop";
	auto& registry = test::creature_world::World::Registry();
	// however often it has seen the miracles
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	EXPECT_FALSE(mind_detail::CastMagicOf(k_WoodByWorkshop).has_value());
	EXPECT_FALSE(mind_detail::MayCast(creature, registry.Get<CreatureMindState>(creature), &_tables, k_WoodByWorkshop, false));
	EXPECT_FALSE(mind_detail::CastInfoFor(creature, k_WoodByWorkshop).has_value());
	// but a plan forced on it casts wood
	const auto cast = mind_detail::CastInfoFor(creature, k_WoodByWorkshop, &_tables);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->magicType, static_cast<uint32_t>(MagicType::Wood));
}

TEST_F(CreatureMindCastingTest, TheSightingsAreAgainstTheTimesTheSpeciesMustSeeTheMiracle)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 3);
	const auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	const auto sightings = mind_detail::MiracleSightings(mind, _tables, CreatureType::GiantApe, MagicType::LightningBolt);
	ASSERT_TRUE(sightings.has_value());
	EXPECT_FLOAT_EQ(sightings->first, 3.0f);
	EXPECT_FLOAT_EQ(sightings->second, Needed());
	// nothing without its learning
	CreatureMindState fresh;
	EXPECT_FALSE(mind_detail::MiracleSightings(fresh, _tables, CreatureType::GiantApe, MagicType::LightningBolt).has_value());
}

TEST_F(CreatureMindCastingTest, ItMayTryAMiracleSeenHalfTheTimesItNeeds)
{
	const auto half = static_cast<uint32_t>(std::ceil(Needed() * 0.5f));
	const auto ready = MakeCreature({100.0f, 0.0f, 100.0f}, half);
	const auto short1 = MakeCreature({100.0f, 0.0f, 100.0f}, half - 1);
	const auto& registry = test::creature_world::World::Registry();
	EXPECT_TRUE(mind_detail::MayCast(ready, registry.Get<CreatureMindState>(ready), &_tables, k_CastAction, false));
	EXPECT_FALSE(mind_detail::MayCast(short1, registry.Get<CreatureMindState>(short1), &_tables, k_CastAction, false));
	// not without the tables, nor for an action that casts nothing
	EXPECT_FALSE(mind_detail::MayCast(ready, registry.Get<CreatureMindState>(ready), nullptr, k_CastAction, false));
	EXPECT_FALSE(mind_detail::MayCast(ready, registry.Get<CreatureMindState>(ready), &_tables, 3, false));
}

TEST_F(CreatureMindCastingTest, NothingSeenOfAMiracleNeedingNoSightingsIsNotEnough)
{
	_tables.miracles.at(k_Lightning).timesToSee = 0;
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	EXPECT_FALSE(mind_detail::MayCast(creature, mind, &_tables, k_CastAction, false));
}

TEST_F(CreatureMindCastingTest, APowerUpOnlyOnceGrownPastItsSeventhStage)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	mind.developmentPhase = 7;
	EXPECT_FALSE(mind_detail::MayCast(creature, mind, &_tables, k_PowerUpAction, true));
	EXPECT_TRUE(mind_detail::MayCast(creature, mind, &_tables, k_CastAction, false));
	mind.developmentPhase = 8;
	EXPECT_TRUE(mind_detail::MayCast(creature, mind, &_tables, k_PowerUpAction, true));
}

TEST_F(CreatureMindCastingTest, NotWhenTheBodyCantPayForIt)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	auto& registry = test::creature_world::World::Registry();
	const auto& mind = registry.Get<CreatureMindState>(creature);
	// 0.85 less the 0.1 the miracle would cost
	registry.Get<CreatureNeeds>(creature).needs.exhaustion = 0.74f;
	EXPECT_TRUE(mind_detail::MayCast(creature, mind, &_tables, k_CastAction, false));
	registry.Get<CreatureNeeds>(creature).needs.exhaustion = 0.75f;
	EXPECT_FALSE(mind_detail::MayCast(creature, mind, &_tables, k_CastAction, false));
}

TEST_F(CreatureMindCastingTest, ToAmuseItNeedsASpellOnCreaturesThatItKnowsAndCanPayFor)
{
	constexpr uint32_t k_Amusing = 297;
	constexpr uint32_t k_Freeze = 26;
	constexpr uint32_t k_Itchy = 41;
	_tables.actions.resize(k_Amusing + 1);
	_tables.actions.at(k_Amusing).name = "CastAmusingSpellOnCreature";
	// freeze costs all of the energy of a grown creature of strength one half, itchy nothing
	_world.Info().magicEffect.at(k_Freeze).costToCreate = 90000.0f;
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	// knowing no spell on creatures, it can't
	EXPECT_FALSE(mind_detail::MayCast(creature, mind, &_tables, k_Amusing, false));
	EXPECT_TRUE(mind_detail::AmusingSpells(creature, mind, _tables).empty());
	// known, however seldom seen; the one it can't pay for is left out
	mind.learnt->knowledge.miraclesKnown.at(k_Freeze) = true;
	mind.learnt->knowledge.miraclesKnown.at(k_Itchy) = true;
	EXPECT_TRUE(mind_detail::MayCast(creature, mind, &_tables, k_Amusing, false));
	const auto spells = mind_detail::AmusingSpells(creature, mind, _tables);
	ASSERT_EQ(spells.size(), 1u);
	EXPECT_EQ(spells.front().magicType, k_Itchy);
	// the row casts no miracle of its own: it is given the spells to pick from, with the tables only
	const auto cast = mind_detail::CastInfoFor(creature, k_Amusing, &_tables);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->magicType, 0u);
	ASSERT_EQ(cast->amusingSpells.size(), 1u);
	EXPECT_EQ(cast->amusingSpells.front().magicType, k_Itchy);
	EXPECT_FALSE(mind_detail::CastInfoFor(creature, k_Amusing).has_value());
}

TEST_F(CreatureMindCastingTest, ATryLetsGoOfWhatItHeldThenCasts)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	const auto target = MakeThing({120.0f, 0.0f, 100.0f});
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	FakeMagic magic;
	EXPECT_TRUE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
	EXPECT_EQ(magic.releases, 1u);
	ASSERT_EQ(magic.cast.size(), 1u);
	EXPECT_EQ(magic.cast.front(), MagicType::LightningBolt);
	// cast, a try is only seen as the player's miracles are
	EXPECT_EQ(mind.learnt->knowledge.miraclesSeen.at(k_Lightning).count, 100u);
}

TEST_F(CreatureMindCastingTest, ATryShortOfAllButOneSightingFizzlesAndCountsAsOneMore)
{
	const auto short1 = static_cast<uint32_t>(std::ceil(Needed())) - 2;
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, short1);
	const auto target = MakeThing({120.0f, 0.0f, 100.0f});
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.desires.emplace();
	const auto before = mind.desires->desires;
	FakeMagic magic;
	EXPECT_FALSE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
	EXPECT_TRUE(magic.cast.empty());
	EXPECT_EQ(magic.releases, 1u);
	EXPECT_EQ(mind.learnt->knowledge.miraclesSeen.at(k_Lightning).count, short1 + 1);
	const auto& fizzle = registry.Get<CreatureCasting>(creature).fizzle;
	ASSERT_TRUE(fizzle.has_value());
	EXPECT_EQ(fizzle->target, target);
	EXPECT_EQ(fizzle->magicType, MagicType::LightningBolt);
	// a fizzle doesn't make it hungry
	for (size_t d = 0; d < before.size(); ++d)
	{
		EXPECT_EQ(mind.desires->desires.at(d).value, before.at(d).value) << d;
	}
}

TEST_F(CreatureMindCastingTest, WithoutTheEnergyOrWhereItCantCastItWantsFoodAboveAll)
{
	auto& registry = test::creature_world::World::Registry();
	const auto target = MakeThing({120.0f, 0.0f, 100.0f});
	const float floor = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).desireFloor;
	const auto hungry = [&](CreatureMindState& mind) {
		return (*mind.desires)[creature_desires::Desire::Hunger].value ==
		           (*mind.desires)[creature_desires::Desire::Hunger].max &&
		       (*mind.desires)[creature_desires::Desire::Tiredness].value == floor;
	};
	// too tired to pay for it
	{
		const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
		registry.Get<CreatureNeeds>(creature).needs.exhaustion = 0.75f;
		auto& mind = registry.Get<CreatureMindState>(creature);
		mind.desires.emplace();
		(*mind.desires)[creature_desires::Desire::Hunger].suppressedTurns = 30;
		FakeMagic magic;
		EXPECT_FALSE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
		EXPECT_TRUE(magic.cast.empty());
		EXPECT_TRUE(hungry(mind));
		EXPECT_EQ((*mind.desires)[creature_desires::Desire::Hunger].suppressedTurns, 0u);
	}
	// at something the miracle can't be cast at
	{
		const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
		auto& mind = registry.Get<CreatureMindState>(creature);
		mind.desires.emplace();
		FakeMagic magic;
		magic.castableAt = false;
		EXPECT_FALSE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
		EXPECT_TRUE(magic.cast.empty());
		EXPECT_TRUE(hungry(mind));
	}
	// refused for any other reason, it is not hungry for it
	{
		const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
		auto& mind = registry.Get<CreatureMindState>(creature);
		mind.desires.emplace();
		FakeMagic magic;
		magic.castWorks = false;
		EXPECT_FALSE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
		EXPECT_EQ(magic.cast.size(), 1u);
		EXPECT_FALSE(hungry(mind));
	}
}

TEST_F(CreatureMindCastingTest, InAFightATryPaysStaminaAndNoneWithoutEnough)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	const auto target = MakeThing({120.0f, 0.0f, 100.0f});
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.desires.emplace();
	auto& stamina = registry.Assign<CreatureFighting>(creature).fighter.stamina;
	const float cost = creature_spell_casting::StaminaCost(_world.Info().magicEffect.at(k_Lightning).costToCreate);
	stamina = cost * 0.5f;
	FakeMagic magic;
	EXPECT_FALSE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
	EXPECT_TRUE(magic.cast.empty());
	EXPECT_FLOAT_EQ(stamina, cost * 0.5f);
	EXPECT_NE((*mind.desires)[creature_desires::Desire::Hunger].value, (*mind.desires)[creature_desires::Desire::Hunger].max);
	stamina = 1.0f;
	EXPECT_TRUE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
	EXPECT_FLOAT_EQ(stamina, std::clamp(1.0f - cost, 0.0f, 1.0f));
}

TEST_F(CreatureMindCastingTest, AFizzleAtAThingOrAPlaceTellsThePlayerTheCreatureMessedTheMiracleUp)
{
	const test::RestoreService<Locator::creatureHelpSystem> restoreHelp;
	auto& help = static_cast<FakeHelp&>(Locator::creatureHelpSystem::emplace<FakeHelp>());
	const auto short1 = static_cast<uint32_t>(std::ceil(Needed())) - 2;
	auto& registry = test::creature_world::World::Registry();
	FakeMagic magic;
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, short1 - 1);
	auto& mind = registry.Get<CreatureMindState>(creature);
	EXPECT_FALSE(
	    mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, MakeThing({120.0f, 0.0f, 100.0f})));
	EXPECT_FALSE(mind_detail::TryMiracleAt(creature, mind, _tables, magic, MagicType::LightningBolt, {130.0f, 2.0f, 110.0f}));
	ASSERT_EQ(help.raised.size(), 2u);
	for (const auto& [about, message] : help.raised)
	{
		EXPECT_EQ(about, creature);
		EXPECT_EQ(message.kind, creature_help::Kind::MessedUpAction);
		EXPECT_EQ(message.parameter, 1u);
		EXPECT_EQ(message.thing, entt::entity {entt::null});
		EXPECT_FALSE(message.forced);
	}
	// a try that succeeds tells nothing
	mind.learnt->knowledge.miraclesSeen.at(k_Lightning).count = 100;
	EXPECT_TRUE(mind_detail::TryMiracleAt(creature, mind, _tables, magic, MagicType::LightningBolt, {130.0f, 2.0f, 110.0f}));
	EXPECT_EQ(help.raised.size(), 2u);
}

TEST_F(CreatureMindCastingTest, ATryAtAPlaceCastsThereAndCostsNoStaminaInAFight)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& stamina = registry.Assign<CreatureFighting>(creature).fighter.stamina;
	stamina = 0.0f;
	FakeMagic magic;
	const glm::vec3 place {130.0f, 2.0f, 110.0f};
	EXPECT_TRUE(mind_detail::TryMiracleAt(creature, mind, _tables, magic, MagicType::LightningBolt, place));
	EXPECT_EQ(magic.releases, 1u);
	ASSERT_EQ(magic.places.size(), 1u);
	EXPECT_EQ(magic.places.front(), place);
	EXPECT_FLOAT_EQ(stamina, 0.0f);
}

TEST_F(CreatureMindCastingTest, AFizzledTryAtAPlaceIsShownAboutTheThingOfTheLastFizzleAtAThing)
{
	const auto short1 = static_cast<uint32_t>(std::ceil(Needed())) - 2;
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 place {130.0f, 2.0f, 110.0f};
	FakeMagic magic;
	// with no fizzle at a thing before, it is about nothing
	{
		const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, short1);
		auto& mind = registry.Get<CreatureMindState>(creature);
		EXPECT_FALSE(mind_detail::TryMiracleAt(creature, mind, _tables, magic, MagicType::LightningBolt, place));
		const auto& fizzle = registry.Get<CreatureCasting>(creature).fizzle;
		ASSERT_TRUE(fizzle.has_value());
		EXPECT_EQ(fizzle->target, entt::entity {entt::null});
		EXPECT_EQ(mind.learnt->knowledge.miraclesSeen.at(k_Lightning).count, short1 + 1);
		// it fizzles before it lets go of what it held
		EXPECT_EQ(magic.releases, 0u);
	}
	// after one at a thing, about that thing
	{
		const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, short1 - 1);
		const auto target = MakeThing({120.0f, 0.0f, 100.0f});
		auto& mind = registry.Get<CreatureMindState>(creature);
		EXPECT_FALSE(mind_detail::TryMiracle(creature, mind, _tables, magic, MagicType::LightningBolt, target));
		auto& casting = registry.Get<CreatureCasting>(creature);
		casting.fizzle.reset();
		EXPECT_FALSE(mind_detail::TryMiracleAt(creature, mind, _tables, magic, MagicType::LightningBolt, place));
		ASSERT_TRUE(casting.fizzle.has_value());
		EXPECT_EQ(casting.fizzle->target, target);
		EXPECT_EQ(casting.lastFizzleTarget, target);
	}
	EXPECT_TRUE(magic.cast.empty());
}

TEST_F(CreatureMindCastingTest, AtAPlaceTheMiracleCantGoToItWantsFoodAboveAll)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 100);
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.desires.emplace();
	FakeMagic magic;
	magic.castableAt = false;
	EXPECT_FALSE(mind_detail::TryMiracleAt(creature, mind, _tables, magic, MagicType::LightningBolt, {130.0f, 2.0f, 110.0f}));
	EXPECT_TRUE(magic.cast.empty());
	EXPECT_EQ((*mind.desires)[creature_desires::Desire::Hunger].value, (*mind.desires)[creature_desires::Desire::Hunger].max);
}

TEST_F(CreatureMindCastingTest, ItCastsTheActionsMiracleAtItsOwnHeight)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto cast = mind_detail::CastInfoFor(creature, k_CastAction);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->magicType, k_Lightning);
	EXPECT_EQ(cast->gesture, 0u);
	EXPECT_FLOAT_EQ(cast->height, object_measures::Height(test::creature_world::World::Registry(), creature));
	EXPECT_FALSE(mind_detail::CastInfoFor(creature, 3).has_value());
}

TEST_F(CreatureMindCastingTest, GoingNearItIsSentOnEveryTurnAndKeepsGoingWhileItCantSetOff)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto target = MakeThing({200.0f, 0.0f, 100.0f});
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::GoNearObject, target, 50.0f), 0.0f);
	EXPECT_TRUE(_moves->moves.empty());
	_moves->moving = true;
	for (int turn = 0; turn < 3; ++turn)
	{
		mind_detail::StepSubMove(creature, false);
	}
	ASSERT_EQ(_moves->moves.size(), 3u);
	EXPECT_EQ(_moves->moves.back().point, glm::vec2(200.0f, 100.0f));
	// its furthest is the distance to keep more than what it keeps clear of
	EXPECT_GE(_moves->moves.back().maxDistance, 50.0f);
	// a walk that can't start now is no failure
	_moves->answer = CreatureLocomotionSystemInterface::MoveResult::Busy;
	mind_detail::StepSubMove(creature, false);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
	// nor is playing an action, while which it isn't sent on
	mind_detail::StepSubMove(creature, true);
	EXPECT_EQ(_moves->moves.size(), 4u);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
}

TEST_F(CreatureMindCastingTest, GoingNearFailsOnlyWhereItCantGo)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto target = MakeThing({200.0f, 0.0f, 100.0f});
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::GoNearObject, target, 50.0f), 0.0f);
	_moves->answer = CreatureLocomotionSystemInterface::MoveResult::InvalidDestination;
	mind_detail::StepSubMove(creature, false);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Failed);
}

TEST_F(CreatureMindCastingTest, GoingNearItStopsOnceNearEnough)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto target = MakeThing({140.0f, 0.0f, 100.0f});
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::GoNearObject, target, 50.0f), 0.0f);
	_moves->moving = true;
	mind_detail::StepSubMove(creature, false);
	EXPECT_EQ(_moves->stops, 1);
	EXPECT_TRUE(_moves->moves.empty());
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Done);
}

TEST_F(CreatureMindCastingTest, AThingWalkedUpToByTheWalkAloneEndsItOnlyOnceInTheMap)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto target = MakeThing({300.0f, 0.0f, 100.0f});
	test::creature_world::World::Registry().Assign<Abode>(target);
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::GoNearObject, target, 10.0f), 0.0f);
	// sent off at once
	EXPECT_EQ(_moves->moves.size(), 1u);
	// standing still, but the building is not in the map: it is sent on
	mind_detail::StepSubMove(creature, false);
	EXPECT_EQ(_moves->moves.size(), 2u);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
}

TEST_F(CreatureMindCastingTest, GettingAwayItIsDoneOnceFarEnoughAndFailsWithNowhereClear)
{
	const auto far = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto near = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto from = MakeThing({130.0f, 0.0f, 100.0f});
	mind_detail::StartSubMove(far, MoveOf(Movement::Kind::GetAwayFromObject, from, 20.0f), 0.0f);
	EXPECT_EQ(mind_detail::SubMoveOf(far), SubMove::Done);
	// with no land to stand on there is no clear area to go to
	mind_detail::StartSubMove(near, MoveOf(Movement::Kind::GetAwayFromObject, from, 40.0f), 0.0f);
	EXPECT_EQ(mind_detail::SubMoveOf(near), SubMove::Failed);
	EXPECT_TRUE(_moves->moves.empty());
}

TEST_F(CreatureMindCastingTest, GettingAwayWaitsWhilePlayingAnAction)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto from = MakeThing({130.0f, 0.0f, 100.0f});
	auto& registry = test::creature_world::World::Registry();
	// done at once, then asked to get further away while it plays an action
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::GetAwayFromObject, from, 20.0f), 0.0f);
	registry.Get<CreatureCasting>(creature).outcome = CreatureCasting::Outcome::Running;
	registry.Get<CreatureCasting>(creature).phase = 0;
	registry.Get<CreatureCasting>(creature).keep = 40.0f;
	mind_detail::StepSubMove(creature, true);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
}

TEST_F(CreatureMindCastingTest, FacingItHoldsStillTheStepsSecondsInWholeTurns)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto target = MakeThing({100.0f, 0.0f, 150.0f});
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureLocomotion>(creature).heading = creature_locomotion::HeadingOf({0.0f, 50.0f});
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::TurnToFaceObject, target, 0.0f), 0.3f);
	// it stops and turns first
	EXPECT_EQ(_moves->stops, 1);
	EXPECT_EQ(_moves->turnsAsked, 1);
	const auto hold = game_clock::TicksForSeconds(0.3f);
	ASSERT_GT(hold, 0);
	// facing it, it holds still that many turns, then is done
	for (int32_t turn = 0; turn <= hold; ++turn)
	{
		mind_detail::StepSubMove(creature, false);
		EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running) << turn;
	}
	mind_detail::StepSubMove(creature, false);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Done);
	EXPECT_EQ(_moves->turnsAsked, 1);
}

TEST_F(CreatureMindCastingTest, FacingWaitsWhileTheBodyIsBusyAndTurnsAgainWhenNotFacing)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	const auto target = MakeThing({100.0f, 0.0f, 150.0f});
	auto& registry = test::creature_world::World::Registry();
	// facing the other way
	registry.Get<CreatureLocomotion>(creature).heading = creature_locomotion::HeadingOf({0.0f, -50.0f});
	mind_detail::StartSubMove(creature, MoveOf(Movement::Kind::TurnToFaceObject, target, 0.0f), 0.3f);
	EXPECT_EQ(_moves->turnsAsked, 1);
	mind_detail::StepSubMove(creature, true);
	EXPECT_EQ(_moves->turnsAsked, 1);
	mind_detail::StepSubMove(creature, false);
	EXPECT_EQ(_moves->turnsAsked, 2);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
}

TEST_F(CreatureMindCastingTest, NoMoveIsRunningAndAGoneTargetIsFacedAtOnce)
{
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
	mind_detail::StartSubMove(creature, {.kind = Movement::Kind::TurnToFaceObject}, 0.3f);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Done);
	// a movement that is no sub-move leaves nothing running
	mind_detail::StartSubMove(creature, {.kind = Movement::Kind::ToPoint}, 0.0f);
	EXPECT_EQ(mind_detail::SubMoveOf(creature), SubMove::Running);
	EXPECT_EQ(test::creature_world::World::Registry().Get<CreatureCasting>(creature).move, CreatureCasting::Move::None);
}

TEST_F(CreatureMindCastingTest, ToImpressItWeighsEveryMiracleThatImpressesAndPicksOneItKnows)
{
	// The game's row of the impressive spell casts no miracle of its own
	constexpr uint32_t k_Impress = 40;
	constexpr uint32_t k_Heal = 10;
	constexpr uint32_t k_Storm = 16;
	auto& info = _world.Info();
	info.creatureAction.at(k_Impress).magicType = 0;
	for (const auto magic : {k_Heal, k_Storm})
	{
		info.magicEffect.at(magic).costToCreate = 9000.0f;
		_tables.miracles.at(magic).timesToSee = 6;
	}
	_tables.actions.resize(k_Impress + 1);
	_tables.actions.at(k_Impress).name = "CastImpressiveSpell";
	auto& registry = test::creature_world::World::Registry();
	const auto creature = MakeCreature({100.0f, 0.0f, 100.0f}, 0);
	auto& mind = registry.Get<CreatureMindState>(creature);
	// Knowing none, it may not
	EXPECT_FALSE(mind_detail::MayCast(creature, mind, &_tables, k_Impress, false));
	// Knowing the heal and seen often enough, it may
	auto& knowledge = mind.learnt->knowledge;
	knowledge.miraclesKnown.at(k_Heal) = true;
	knowledge.miraclesSeen.at(k_Heal).count = 100;
	EXPECT_TRUE(mind_detail::MayCast(creature, mind, &_tables, k_Impress, false));
	// It casts the miracle it knows; knowing two that were cast as long ago, the later in the list
	auto cast = mind_detail::CastInfoFor(creature, k_Impress, &_tables);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->magicType, k_Heal);
	knowledge.miraclesKnown.at(k_Storm) = true;
	registry.Context().impressiveMiracleCastTurn = {};
	cast = mind_detail::CastInfoFor(creature, k_Impress, &_tables);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->magicType, k_Storm);
	// Without the tables it can't tell the row impresses, and the row casts nothing
	EXPECT_FALSE(mind_detail::CastInfoFor(creature, k_Impress).has_value());
}
