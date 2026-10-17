/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fakes of the miracles own the real stores of the miracles' service
#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/DispenserRules.h"
#include "Magic/Spells/SpellResource.h"
#include "Magic/Spells/SpellWater.h"
#include "Particles/SpellLink.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
using openblack::ecs::effects::EffectValues;
using EventType = openblack::psys::SpellEventInfo::Type;

namespace
{
/// The miracles' rules over the tests' fake world: a registry, zeroed tables, the animals' shared state the spell classes
/// register with, and the inert miracles' service, whose world is a FakeMagicWorld, each put back as it was when the
/// test ends
class SpellBehaviours: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		_magic = &static_cast<test::InertMagicSystem&>(Locator::magicSystem::emplace<test::InertMagicSystem>());
		auto info = std::make_unique<InfoConstants>();
		_info = info.get();
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] test::FakeMagicWorld& World() const { return _magic->FakeWorld(); }
	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] InfoConstants& Info() const { return *_info; }
	[[nodiscard]] GMagicEffectInfo& Effect(MagicType type) const { return _info->magicEffect.at(static_cast<size_t>(type)); }

	/// A miracle the first player cast at (100, 0, 100), with its prayer power
	[[nodiscard]] static entt::entity MakeSpell(MagicType type, float prayerPower)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = type;
		component.spellClass = ClassOf(type);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		chants::SetChants(component, prayerPower);
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		return spell;
	}

private:
	// put back in reverse order: the tables, the miracles' service with its stores, the registry, then the animals'
	// shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
	test::InertMagicSystem* _magic {nullptr};
};

psys::SpellEventInfo EventAt(EventType type, glm::vec3 position, entt::entity target = entt::null)
{
	return {.type = type,
	        .position = position,
	        .velocity = glm::vec3(1.0f, 0.0f, 0.0f),
	        .strength = 1.0f,
	        .checkShields = false,
	        .target = target};
}
} // namespace

// Dispensers: the pure rules of a dispenser's timer and bubble, with our counting (the tick goes back to 0 only when
// a bubble is made)

TEST(DispenserRules, ADispenserCountsToItsPeriodWhileItsBubbleIsGoneAndAsksAgainUntilOneIsMade)
{
	DispenserTimer timer {.tick = 0, .period = 3, .active = false};
	// inactive, or without a magic: nothing counts
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
	timer.active = true;
	EXPECT_EQ(StepDispenser(timer, false, false, false), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 1u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 2u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::MakeOrb);
	EXPECT_EQ(timer.tick, 3u);
	// with no orb made the tick goes on, and every turn asks again
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::MakeOrb);
	EXPECT_EQ(timer.tick, 4u);
	// an orb made (the tick back to 0): it waits while the orb is on it
	timer.tick = 0;
	EXPECT_EQ(StepDispenser(timer, true, true, true), DispenserStep::Wait);
	EXPECT_EQ(StepDispenser(timer, true, true, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
	// taken: that turn only forgets it, the count starts on the next
	timer.tick = 2;
	EXPECT_EQ(StepDispenser(timer, true, false, true), DispenserStep::OrbTaken);
	EXPECT_EQ(timer.tick, 0u);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::MakeOrb);
	// inactive again: its orb is still forgotten when taken, but nothing counts
	timer.active = false;
	EXPECT_EQ(StepDispenser(timer, true, false, true), DispenserStep::OrbTaken);
	EXPECT_EQ(StepDispenser(timer, false, false, true), DispenserStep::Wait);
	EXPECT_EQ(timer.tick, 0u);
}

TEST(DispenserRules, TheOrbFloatsAboveItsDispenserAndStaysThereWithinHalfAMetreAcrossTheLand)
{
	const auto orb = OrbPosition({10.0f, 2.0f, 20.0f}, 5.0f);
	EXPECT_FLOAT_EQ(orb.x, 10.0f);
	EXPECT_FLOAT_EQ(orb.y, 2.0f + 5.0f * 1.2f);
	EXPECT_FLOAT_EQ(orb.z, 20.0f);
	// the height does not count, only the distance across the land, up to half a metre and the touch tolerance
	EXPECT_TRUE(OrbStillThere({10.3f, 99.0f, 20.0f}, orb));
	EXPECT_TRUE(OrbStillThere({10.5f, 8.0f, 20.0f}, orb));
	EXPECT_TRUE(OrbStillThere({10.5009f, 8.0f, 20.0f}, orb));
	EXPECT_FALSE(OrbStillThere({10.502f, 8.0f, 20.0f}, orb));
	EXPECT_FALSE(OrbStillThere({11.0f, 8.0f, 20.0f}, orb));
}

TEST(DispenserRules, ABubbleFacesAlongAnyDirection)
{
	for (const auto direction : {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 2.0f, -3.0f), glm::vec3(0.0f, -1.0f, 0.0f)})
	{
		const auto turn = FaceTowards(direction);
		const auto up = turn * glm::vec3(0.0f, 1.0f, 0.0f);
		EXPECT_NEAR(glm::dot(up, glm::normalize(direction)), 1.0f, 1e-5f);
		EXPECT_NEAR(glm::determinant(turn), 1.0f, 1e-5f);
	}
	EXPECT_TRUE(FaceTowards(glm::vec3(0.0f)) == glm::mat3(1.0f));
}

TEST(DispenserRules, EveryDispensableMiracleIsAMiracleOnce)
{
	const auto miracles = DispensableMiracles();
	EXPECT_EQ(miracles.size(), 34u);
	EXPECT_EQ(miracles.front(), MagicType::Fireball);
	EXPECT_EQ(std::ranges::count(miracles, MagicType::None), 0);
	for (const auto type : miracles)
	{
		EXPECT_EQ(std::ranges::count(miracles, type), 1) << static_cast<int>(type);
	}
}

TEST(DispenserRules, TheDispenserBuildingsAreThoseOfTheirTypeNorseFirst)
{
	// zeroed tables: no building is a dispenser
	const auto info = std::make_unique<InfoConstants>();
	EXPECT_TRUE(DispenserAbodes(*info).empty());
	EXPECT_FALSE(DefaultDispenserAbode({}).has_value());

	info->abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser)).abodeType = AbodeType::SpellDispenser;
	info->abode.at(static_cast<size_t>(AbodeInfo::NorseSpellDispenser)).abodeType = AbodeType::SpellDispenser;
	const std::vector<AbodeInfo> expected {AbodeInfo::CelticSpellDispenser, AbodeInfo::NorseSpellDispenser};
	const auto abodes = DispenserAbodes(*info);
	EXPECT_EQ(abodes, expected);
	EXPECT_EQ(DefaultDispenserAbode(abodes), AbodeInfo::NorseSpellDispenser);
	const std::vector<AbodeInfo> celtic {AbodeInfo::CelticSpellDispenser};
	EXPECT_EQ(DefaultDispenserAbode(celtic), AbodeInfo::CelticSpellDispenser);
}

// The cast rules, which ask the miracles' world for the map, the land and the influence

TEST_F(SpellBehaviours, TheCastRulesAskForLandAndInfluence)
{
	GMagicInfo info {};
	info.castRuleType = CastRuleType::OnLandInInfluence;
	World().wetBelowX = 50.0f;
	World().influence = false;
	const glm::vec3 land {100.0f, 0.0f, 100.0f};
	EXPECT_FALSE(cast_rules::CanCastRule(info, land, PlayerNames::PLAYER_ONE));
	World().influence = true;
	EXPECT_TRUE(cast_rules::CanCastRule(info, land, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(cast_rules::CanCastRule(info, {10.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(cast_rules::CanCastRule(info, {-5.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE));
	// anywhere is anywhere on the map, the water too
	info.castRuleType = CastRuleType::Anywhere;
	World().influence = false;
	EXPECT_TRUE(cast_rules::CanCastRule(info, {10.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(cast_rules::CanCastRule(info, {-5.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE));
	// the creature spells are cast on creatures, never at a point
	EXPECT_FALSE(cast_rules::CanCastAt(MagicType::CreatureSpellBig, land));
}

TEST_F(SpellBehaviours, LandForACastRuleIsACellWithoutWaterWhateverItsHeight)
{
	GMagicInfo info {};
	info.castRuleType = CastRuleType::OnLand;
	World().waterBelowZ = 50.0f;
	// above the sea but with water in its cell: not land
	EXPECT_TRUE(World().IsDryLand({100.0f, 0.0f, 10.0f}));
	EXPECT_FALSE(cast_rules::CanCastRule(info, {100.0f, 0.0f, 10.0f}, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(cast_rules::CanCastRule(info, {100.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE));
	// food and wood go on land by the same test
	EXPECT_FALSE(cast_rules::CanCastAt(MagicType::Food, {100.0f, 0.0f, 10.0f}));
	EXPECT_TRUE(cast_rules::CanCastAt(MagicType::Wood, {100.0f, 0.0f, 100.0f}));
}

// What the miracles do: the default effect of an event, through the world

TEST_F(SpellBehaviours, AnEventPaysItsCostAndActsOnItsTargetByTheMiraclesStrength)
{
	auto& bolt = Effect(MagicType::LightningBolt);
	bolt.costPerEvent = 2.0f;
	bolt.effectBurn = 800.0f;
	bolt.effectHit = 0.002f;
	bolt.radius = 1.0f;
	// the first player's tribal power of 1.5 counts for the bolt
	bolt.useTribalPowerMultiplier.at(0) = 1;
	players::MagicOf(PlayerNames::PLAYER_ONE).tribalPower.at(0) = 1.5f;
	const auto spell = MakeSpell(MagicType::LightningBolt, 5000.0f);
	const auto creature = Registry().Create();
	ASSERT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::Object, {100.0f, 9.0f, 120.0f}, creature)), 1);
	EXPECT_FLOAT_EQ(Registry().Get<Spell>(spell).chants, 4998.0f);
	ASSERT_EQ(World().applied.size(), 1u);
	EXPECT_EQ(World().applied[0].first, creature);
	// full strength, which carries the tribal power, times the tribal power once more
	EXPECT_NEAR(World().applied[0].second.numbers[static_cast<size_t>(EffectValues::Number::Burn)], 800.0f * 1.5f * 1.5f,
	            0.01f);
	EXPECT_TRUE(World().appliedAt.empty());
	// only an event at an object acts on it alone; any other acts round its point, on the land under it
	ASSERT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::Landed, {100.0f, 9.0f, 120.0f}, creature)), 1);
	ASSERT_EQ(World().appliedAt.size(), 1u);
	EXPECT_FLOAT_EQ(World().appliedAt[0].first.y, 0.0f);
	EXPECT_EQ(World().applied.size(), 1u);
	// from the player who cast it, not a creature
	EXPECT_EQ(World().lastSource.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(World().lastSource.casterCreature, entt::entity {entt::null});
	// the miracle moves to the event, at no altitude
	EXPECT_FLOAT_EQ(Registry().Get<Spell>(spell).position.z, 120.0f);
	EXPECT_FLOAT_EQ(Registry().Get<Spell>(spell).position.y, 0.0f);
	// without a target it acts round the point
	ASSERT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::Landed, {90.0f, 2.0f, 90.0f})), 1);
	EXPECT_EQ(World().appliedAt.size(), 2u);
	// a closed miracle acts no more
	Registry().Get<Spell>(spell).closedDown = true;
	EXPECT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::Landed, {90.0f, 2.0f, 90.0f})), 0);
	EXPECT_EQ(World().appliedAt.size(), 2u);
}

TEST_F(SpellBehaviours, TheStartOfTheEffectIsNotAnEventToActOn)
{
	const auto spell = MakeSpell(MagicType::LightningBolt, 100.0f);
	EXPECT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::Started, {})), 1);
	EXPECT_TRUE(World().applied.empty() && World().appliedAt.empty());
	EXPECT_FLOAT_EQ(Registry().Get<Spell>(spell).chants, 100.0f);
}

TEST_F(SpellBehaviours, AnObjectGoneTakesNothingAndAMiracleOnlyAsksWhetherItMayDestroyOne)
{
	const auto spell = MakeSpell(MagicType::LightningBolt, 100.0f);
	const auto gone = Registry().Create();
	Registry().Destroy(gone);
	ASSERT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::Object, {100.0f, 0.0f, 100.0f}, gone)), 1);
	EXPECT_TRUE(World().applied.empty() && World().appliedAt.empty());
	// the fake world lets no miracle destroy anything
	const auto rock = Registry().Create();
	EXPECT_EQ(spell_event::SpellEvent(spell, EventAt(EventType::CanDestroy, {100.0f, 0.0f, 100.0f}, rock)), 0);
	EXPECT_TRUE(World().applied.empty() && World().appliedAt.empty());
}

TEST_F(SpellBehaviours, TheSourceOfAMiraclesEffectsIsItsPlayerAndTheCreatureThatCastItAsItsValuesSay)
{
	const auto creature = Registry().Create();
	const auto thing = Registry().Create();
	struct Case
	{
		SpellCreator creator;
		bool hasPlayer;
	};
	for (const auto& [creator, hasPlayer] :
	     {Case {{.kind = SpellCreator::Kind::None, .player = PlayerNames::NEUTRAL, .entity = entt::null}, false},
	      Case {{.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null}, true},
	      Case {{.kind = SpellCreator::Kind::WorshipSpellIcon, .player = PlayerNames::PLAYER_TWO, .entity = thing}, true},
	      Case {{.kind = SpellCreator::Kind::Creature, .player = PlayerNames::PLAYER_ONE, .entity = creature}, true},
	      Case {{.kind = SpellCreator::Kind::Thing, .player = PlayerNames::NEUTRAL, .entity = thing}, false}})
	{
		const auto spell = MakeSpell(MagicType::LightningBolt, 100.0f);
		auto& component = Registry().Get<Spell>(spell);
		component.creator = creator;
		component.hasPlayer = hasPlayer;
		component.player = hasPlayer ? creator.player : PlayerNames::NEUTRAL;
		EffectValues values;
		static_cast<void>(spell_event::GetPaidEffectValues(spell, values));
		const auto source = spell_event::SourceOf(spell);
		EXPECT_EQ(source.player, values.player.value_or(PlayerNames::NEUTRAL));
		EXPECT_EQ(source.casterCreature, values.appliedByCreature ? values.appliedBy : entt::entity {entt::null});
		// no player at all when the spell has none, as its values name none
		EXPECT_EQ(source.playerless, !hasPlayer);
		EXPECT_EQ(source.playerless, !values.player.has_value());
		// nothing of a blow's
		EXPECT_FALSE(source.appliedBy.has_value());
		EXPECT_FALSE(source.blow);
		EXPECT_FALSE(source.point.has_value());
	}
}

TEST_F(SpellBehaviours, FoodLandsOnDryLandAsPilesAndPaysForEveryGrain)
{
	auto& food = Info().magicFood.at(0);
	food.resourceType = ResourceType::Food;
	food.resourceAmountFirstEvent = 200;
	food.resourceAmountPerEvent = 18;
	food.costPerUnit = 7;
	World().wetBelowX = 50.0f;
	const auto spell = MakeSpell(MagicType::Food, 5000.0f);
	const auto& resourceOps = OpsOf(ecs::components::SpellClass::Resource);
	ASSERT_EQ(resourceOps.spellEvent(spell, EventAt(EventType::Landed, {100.0f, 0.0f, 100.0f})), 1);
	ASSERT_EQ(World().resources.size(), 1u);
	EXPECT_EQ(World().resources[0].type, ResourceType::Food);
	EXPECT_EQ(World().resources[0].amount, 200u);
	EXPECT_FALSE(World().resources[0].sparkles);
	// put down where it landed, on the land
	EXPECT_EQ(World().resources[0].point, glm::vec3(100.0f, 0.0f, 100.0f));
	EXPECT_FLOAT_EQ(Registry().Get<Spell>(spell).chants, 5000.0f - 1400.0f);
	resourceOps.spellEvent(spell, EventAt(EventType::Landed, {101.0f, 0.0f, 100.0f}));
	EXPECT_EQ(World().resources.back().amount, 18u);
	// in the water it is paid for but lost
	resourceOps.spellEvent(spell, EventAt(EventType::Landed, {10.0f, 0.0f, 100.0f}));
	EXPECT_EQ(World().resources.size(), 2u);
	EXPECT_FLOAT_EQ(Registry().Get<Spell>(spell).chants, 5000.0f - 1400.0f - 2.0f * 126.0f);
	EXPECT_TRUE(HasEnoughChantsForResourceRecast(food, Registry().Get<Spell>(spell).chants));
	// the power-up's food makes its new piles sparkle
	food.speedUp = 1;
	resourceOps.spellEvent(spell, EventAt(EventType::Landed, {102.0f, 0.0f, 100.0f}));
	ASSERT_EQ(World().resources.size(), 3u);
	EXPECT_TRUE(World().resources.back().sparkles);
}

TEST_F(SpellBehaviours, TheHealGivesItsEffectThePeopleRoundIt)
{
	auto& heal = Info().magicHeal.at(0);
	heal.dummyVar = 10.0f;
	heal.maxToHeal = 2;
	// the first player's tribal power of 2 counts for the heal
	Effect(MagicType::Heal).useTribalPowerMultiplier.at(0) = 1;
	players::MagicOf(PlayerNames::PLAYER_ONE).tribalPower.at(0) = 2.0f;
	for (int i = 0; i < 6; ++i)
	{
		World().objects[Registry().Create()] = {100.0f, 0.0f, 100.0f};
	}
	const auto spell = MakeSpell(MagicType::Heal, 5000.0f);
	// twice as far and twice as many with twice the tribal power
	EXPECT_EQ(cast_rules::FindHealTargets({100.0f, 0.0f, 100.0f}, spell), 4);
	EXPECT_FLOAT_EQ(World().lastHealRadius, 20.0f);
	// the cast check reads the row as it is
	EXPECT_TRUE(cast_rules::CanCastAt(MagicType::Heal, {100.0f, 0.0f, 100.0f}));
	EXPECT_FLOAT_EQ(World().lastHealRadius, 10.0f);
}

TEST_F(SpellBehaviours, TheHealCanOnlyBeCastWhereThereIsSomeoneToHeal)
{
	auto& heal = Info().magicHeal.at(0);
	heal.dummyVar = 10.0f;
	heal.maxToHeal = 20;
	EXPECT_FALSE(cast_rules::CanCastAt(MagicType::Heal, {100.0f, 0.0f, 100.0f}));
	World().objects[Registry().Create()] = {100.0f, 0.0f, 100.0f};
	EXPECT_TRUE(cast_rules::CanCastAt(MagicType::Heal, {100.0f, 0.0f, 100.0f}));
	// with nobody it may heal at most, it may not be cast
	heal.maxToHeal = 0;
	EXPECT_FALSE(cast_rules::CanCastAt(MagicType::Heal, {100.0f, 0.0f, 100.0f}));
}

TEST_F(SpellBehaviours, TheWaterRainsADropEachTurnAndLeavesRingsEveryTenthOfASecond)
{
	Effect(MagicType::Water).effectBurn = -4000.0f;
	const auto spell = MakeSpell(MagicType::Water, 5000.0f);
	Registry().Get<Spell>(spell).age = 0.2f;
	const auto& waterOps = OpsOf(ecs::components::SpellClass::Water);
	waterOps.process(spell);
	ASSERT_EQ(World().drops.size(), 1u);
	// somewhere in the rain round where it was cast, its radius times 0.7 plus 0.3 at most, a little above the land
	const auto& castPos = Registry().Get<Spell>(spell).castPos;
	const float distance = glm::distance(glm::vec2(World().drops[0].x, World().drops[0].z), glm::vec2(castPos.x, castPos.z));
	EXPECT_GE(distance, water::DropDistance(0.0f) - 1e-4f);
	EXPECT_LE(distance, water::DropDistance(water::RainRadius(MagicType::Water)) + 1e-4f);
	EXPECT_FLOAT_EQ(World().drops[0].y, 0.2f);
	ASSERT_TRUE(World().rings[0].has_value());
	EXPECT_FLOAT_EQ(*World().rings[0], 2.0f);
	// the drop cools what it lands on
	ASSERT_EQ(World().appliedAt.size(), 1u);
	EXPECT_LT(World().appliedAt[0].second.numbers[static_cast<size_t>(EffectValues::Number::Burn)], 0.0f);
	EXPECT_FALSE(World().extremeDrops[0]);
	// no ring in the same tenth of a second
	waterOps.process(spell);
	ASSERT_EQ(World().rings.size(), 2u);
	EXPECT_FALSE(World().rings[1].has_value());
	// the power-up's water is the extreme one, with wider rings
	const auto extreme = MakeSpell(MagicType::WaterPowerUpOne, 5000.0f);
	Registry().Get<Spell>(extreme).age = 0.2f;
	waterOps.process(extreme);
	ASSERT_EQ(World().drops.size(), 3u);
	EXPECT_TRUE(World().extremeDrops[2]);
	ASSERT_TRUE(World().rings[2].has_value());
	EXPECT_FLOAT_EQ(*World().rings[2], 4.0f);
	// other miracles rain nothing
	const auto bolt = MakeSpell(MagicType::LightningBolt, 5000.0f);
	OpsOf(ecs::components::SpellClass::General).process(bolt);
	EXPECT_EQ(World().drops.size(), 3u);
}

// The seeds and the hand: what they ask of the world

TEST_F(SpellBehaviours, ASeedThatFollowsItsSpellClosesItOutOfItsPlayersInfluence)
{
	auto& row = Info().spellSeed.at(static_cast<size_t>(SpellSeedType::CreatureSpellBig));
	row.castType = SpellCastType::SpellCastHandGesture;
	row.seedFollowsSpell = 1;
	const auto spell = MakeSpell(MagicType::Heal, 100.0f);
	const auto icon = Registry().Create();
	const auto follower = Registry().Create();
	auto& component = Registry().Assign<ecs::components::SpellSeed>(follower);
	component.seedType = SpellSeedType::CreatureSpellBig;
	component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
	component.icon = icon;
	component.spell = spell;
	Registry().Assign<ecs::components::Transform>(follower, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(seed::ProcessFromSpell(follower), 1);
	EXPECT_FALSE(Registry().Get<Spell>(spell).closedDown);
	World().influence = false;
	EXPECT_EQ(seed::ProcessFromSpell(follower), 1);
	EXPECT_TRUE(Registry().Get<Spell>(spell).closedDown);
}
