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
// a power-up only once grown, the body able to pay), what it casts, going near, getting away from and turning to face
// what it casts at, and the search for a clear area to stand in, as the game does them

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "3D/CreatureBody.h"
#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "Creature/CreatureCastAgenda.h"
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
#include "ECS/Components/CreatureClearAreaReadout.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "creature/CreatureSystemWorld.h"
#include "support/FireFakes.h"
#include "support/MagicFakes.h"
#include "support/MapFakes.h"
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
	/// The stops on its way, each also counted as a stop
	int brakes {0};
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
	void BrakeToStop(entt::entity creature) override
	{
		++brakes;
		Stop(creature);
	}
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
	// a walk brakes to a stop
	EXPECT_EQ(_moves->stops, 1);
	EXPECT_EQ(_moves->brakes, 1);
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
	// it stops, a walk braking, and turns first
	EXPECT_EQ(_moves->stops, 1);
	EXPECT_EQ(_moves->brakes, 1);
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

// After a fizzle it is embarrassed (73), then on a draw of two coming up 0 sad as well (56), and sadder by a draw of up to
// 0.3 drawn after it; with any other draw it is only embarrassed and draws no amount, as the original
// (docs/wiki/creature/casting.md#the-fizzle-shown)
TEST(CreatureFizzleReaction, EmbarrassedThenOneTimeInTwoSadAndSadderByUpToThreeTenths)
{
	EXPECT_EQ(creature_mind::k_FizzleEmbarrassed, 73u);
	std::vector<std::string> draws;
	const auto random = [&draws](uint32_t lots) {
		draws.push_back("rand " + std::to_string(lots));
		return 0u;
	};
	const auto randomFloat = [&draws](float most) {
		draws.push_back("float " + std::to_string(std::bit_cast<uint32_t>(most)));
		return 0.125f;
	};
	const auto sad = creature_mind::SadAfterFizzle(random, true, randomFloat);
	ASSERT_TRUE(sad.has_value());
	EXPECT_EQ(sad->animation, 56u);
	ASSERT_TRUE(sad->sadness.has_value());
	EXPECT_EQ(*sad->sadness, 0.125f);
	// The float 0.3 bit for bit, drawn after the draw of two
	EXPECT_EQ(draws, (std::vector<std::string> {"rand 2", "float " + std::to_string(0x3E99999Au)}));

	draws.clear();
	const auto notSad = creature_mind::SadAfterFizzle(
	    [&draws](uint32_t lots) {
		    draws.push_back("rand " + std::to_string(lots));
		    return 1u;
	    },
	    true, randomFloat);
	EXPECT_FALSE(notSad.has_value());
	EXPECT_EQ(draws, (std::vector<std::string> {"rand 2"}));
}

namespace
{
/// The map's cells as the test files them: the things that stay put and the things that move, by cell
class FiledCells final: public ecs::MapInterface
{
public:
	using Lists = std::map<std::pair<uint16_t, uint16_t>, std::vector<entt::entity>>;
	Lists fixed;
	Lists mobile;

	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const CellId& cellId) const override
	{
		return In(fixed, cellId);
	}
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& pos) const override
	{
		return In(fixed, GetGridCell(pos));
	}
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const CellId& cellId) const override
	{
		return In(mobile, cellId);
	}
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& pos) const override
	{
		return In(mobile, GetGridCell(pos));
	}
	[[nodiscard]] std::vector<entt::entity> GetAllInCell(glm::ivec2 /*cell*/) const override { return {}; }
	void Sync() override {}
	void Refile(entt::entity /*entity*/) override {}

private:
	[[nodiscard]] static std::span<const entt::entity> In(const Lists& lists, const CellId& cellId)
	{
		const auto found = lists.find({cellId.x, cellId.y});
		return found != lists.end() ? std::span<const entt::entity>(found->second) : std::span<const entt::entity> {};
	}
};

/// A creature's search for a clear area on 64 x 64 cells of land, with the things in the cells as the test files them
class CreatureClearAreaTest: public ::testing::Test
{
protected:
	static constexpr int32_t k_LandCells = 64;

	void SetUp() override
	{
		_cells = &static_cast<FiledCells&>(Locator::entitiesMap::emplace<FiledCells>());
		Locator::fireSystem::emplace<test::FakeFires>();
		// no thing has a model's shape or box unless the test gives it one
		_shapes = &static_cast<test::FakeMapShapeProvider&>(Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>());
		_shapes->meshShape = [](entt::entity, ecs::map_collide::Shape&, float&) { return false; };
		_boxes = &static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
		_boxes->meshBox = [](entt::id_type) { return std::optional<AxisAlignedBoundingBox> {}; };
		auto& land = Locator::landAvoidSystem::value().GetState();
		land.size = k_LandCells;
		land.avoid.assign(static_cast<size_t>(k_LandCells * k_LandCells), land_avoid::k_Land);
	}

	static void NotLand(int32_t x, int32_t z)
	{
		Locator::landAvoidSystem::value().GetState().avoid.at(static_cast<size_t>(z * k_LandCells + x)) = land_avoid::k_Avoid;
	}

	static entt::entity Make(glm::vec3 position)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	static entt::entity MakeCreature(glm::vec3 position)
	{
		const auto creature = Make(position);
		test::creature_world::World::Registry().Assign<Creature>(creature).species = CreatureType::GiantApe;
		return creature;
	}

	// put back after the registry, which the entities may reach as they go
	const test::RestoreService<Locator::entitiesMap> _restoreCells;
	const test::RestoreService<Locator::fireSystem> _restoreFires;
	const test::RestoreService<Locator::mapShapeProvider> _restoreShapes;
	const test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::creature_world::World _world;
	FiledCells* _cells {nullptr};
	test::FakeMapShapeProvider* _shapes {nullptr};
	test::FakeMeshBoxProvider* _boxes {nullptr};
};
} // namespace

TEST_F(CreatureClearAreaTest, ACellIsKeptFromBeingClearByTheFirstThingThatBlocksIt)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = MakeCreature({305.0f, 0.0f, 305.0f});
	const auto temple = Make({305.0f, 0.0f, 305.0f});
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_ONE;
	const auto villager = Make({315.0f, 0.0f, 315.0f});
	registry.Assign<Villager>(villager);
	_cells->fixed[{30, 30}] = {temple};
	_cells->mobile[{31, 31}] = {villager};
	const auto& world = std::as_const(registry);

	// a temple's heart takes its cell outright
	const auto heart = mind_detail::BlockerOf(world, creature, 30, 30);
	ASSERT_TRUE(heart.has_value());
	EXPECT_EQ(heart->why, ClearCellBlock::TempleHeart);
	EXPECT_EQ(heart->by, temple);
	EXPECT_TRUE(mind_detail::CellBlocked(world, creature, 30, 30));
	// a villager only when nothing living may stand in the area
	EXPECT_FALSE(mind_detail::BlockerOf(world, creature, 31, 31).has_value());
	const auto living = mind_detail::BlockerOf(world, creature, 31, 31, true);
	ASSERT_TRUE(living.has_value());
	EXPECT_EQ(living->why, ClearCellBlock::Living);
	EXPECT_EQ(living->by, villager);
	// an empty cell, and none off the map
	EXPECT_FALSE(mind_detail::BlockerOf(world, creature, 29, 30, true).has_value());
	EXPECT_FALSE(mind_detail::BlockerOf(world, creature, -1, 30, true).has_value());
}

TEST_F(CreatureClearAreaTest, TheDebugReadoutKeepsTheLastSearchAndWhatKeptEachCellFromBeingClear)
{
	auto& registry = test::creature_world::World::Registry();
	const auto watched = MakeCreature({305.0f, 0.0f, 305.0f});
	const auto unwatched = MakeCreature({305.0f, 0.0f, 305.0f});
	registry.AssignOrReplaceState<CreatureClearAreaReadout>(watched);
	const auto temple = Make({305.0f, 0.0f, 305.0f});
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_ONE;
	const auto villager = Make({315.0f, 0.0f, 315.0f});
	registry.Assign<Villager>(villager);
	_cells->fixed[{30, 30}] = {temple};
	_cells->mobile[{31, 31}] = {villager};
	NotLand(29, 30);
	const auto& world = std::as_const(registry);

	// 15 wide: a square of two cells whose disc holds only its first cell, so the nearest open cell's square. The cell
	// of the point has the temple and the one west of it is not land: the cell south of it is taken
	const auto found = mind_detail::FindClearArea(world, watched, {305.0f, 305.0f}, 15.0f, true);
	ASSERT_TRUE(found.has_value());
	EXPECT_FLOAT_EQ(found->x, 307.5f);
	EXPECT_FLOAT_EQ(found->y, 297.5f);
	// the same search without the readout finds the same, and keeps nothing
	EXPECT_EQ(mind_detail::FindClearArea(world, unwatched, {305.0f, 305.0f}, 15.0f, true), found);
	EXPECT_FALSE(registry.AllOf<CreatureClearAreaReadout>(unwatched));

	const auto& last = registry.Get<CreatureClearAreaReadout>(watched).last;
	ASSERT_TRUE(last.has_value());
	EXPECT_FLOAT_EQ(last->width, 15.0f);
	EXPECT_EQ(last->point, glm::vec2(305.0f, 305.0f));
	EXPECT_TRUE(last->nothingLiving);
	EXPECT_EQ(last->found, found);
	EXPECT_EQ(last->result, found);
	// the cells not clear, in the order they were looked at: row by row
	ASSERT_EQ(last->blocked.size(), 3u);
	EXPECT_EQ(last->blocked[0].cell, glm::ivec2(29, 30));
	EXPECT_EQ(last->blocked[0].blocker.why, ClearCellBlock::NotLand);
	EXPECT_EQ(last->blocked[0].blocker.by, entt::entity {entt::null});
	EXPECT_EQ(last->blocked[1].cell, glm::ivec2(30, 30));
	EXPECT_EQ(last->blocked[1].blocker.why, ClearCellBlock::TempleHeart);
	EXPECT_EQ(last->blocked[1].blocker.by, temple);
	EXPECT_EQ(last->blocked[2].cell, glm::ivec2(31, 31));
	EXPECT_EQ(last->blocked[2].blocker.why, ClearCellBlock::Living);
	EXPECT_EQ(last->blocked[2].blocker.by, villager);
}

TEST_F(CreatureClearAreaTest, OnlyWhatTheMapGivesCollideDataKeepsACellClearByItsShape)
{
	// every thing that stays put, put in the map, has a shape of radius 3 where it stands; a mobile object's model is 10
	// tall, taller than a tenth of the creature, which must go round it
	_shapes->meshShape = [](entt::entity object, ecs::map_collide::Shape& shape, float& reach) {
		const auto& at = test::creature_world::World::Registry().Get<const Transform>(object);
		shape = ecs::map_collide::Shape {glm::vec2(at.position.x, at.position.z), 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	};
	_boxes->meshBox = [](entt::id_type) {
		return std::optional(AxisAlignedBoundingBox {.minima = {-3.0f, 0.0f, -3.0f}, .maxima = {3.0f, 10.0f, 3.0f}});
	};
	auto& registry = test::creature_world::World::Registry();
	const auto creature = MakeCreature({305.0f, 0.0f, 305.0f});
	const auto field = Make({305.0f, 0.0f, 305.0f});
	registry.Assign<Field>(field).town = -1;
	ecs::map_cells::InsertMapObject(field);
	const auto mobile = Make({325.0f, 0.0f, 305.0f});
	registry.Assign<MobileObject>(mobile);
	registry.Assign<Mesh>(mobile, Mesh {.id = 1, .submeshId = 0, .bbSubmeshId = 0});
	ecs::map_cells::InsertMapObject(mobile);
	_cells->fixed[{30, 30}] = {field};
	_cells->mobile[{32, 30}] = {mobile};
	const auto& world = std::as_const(registry);

	// the field keeps its cell clear by the shape the map built for it
	ASSERT_NE(ecs::map_cells::CollideDataOf(field), nullptr);
	const auto byField = mind_detail::BlockerOf(world, creature, 30, 30);
	ASSERT_TRUE(byField.has_value());
	EXPECT_EQ(byField->why, ClearCellBlock::Shape);
	EXPECT_EQ(byField->by, field);
	// the mobile object is one the creature must go round, with a model, but the map gives it no collide data, so its
	// cell stays clear
	ASSERT_TRUE(ecs::route_plan_world::CreatureMustAvoid(mobile, creature));
	EXPECT_EQ(ecs::map_cells::CollideDataOf(mobile), nullptr);
	EXPECT_FALSE(mind_detail::CellBlocked(world, creature, 32, 30));
}
