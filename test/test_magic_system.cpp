/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The miracles' service in front of the spells: finding a miracle at a point measures across the land only, takes the
// first of the list and never answers for the shields; paying goes through the spell's chant rules, forced or not; and
// a miracle that has gone has no strength, takes no event and pays nothing. A one-shot bubble of a magic type holds the
// first seed with that type at its level; the dispensers are reported as their components have them, their period
// keeps a time shorter than a turn out, and only a dispenser without a bubble but with a miracle is charged. The hand is
// busy only with a seed, and only a seed is dropped; the pour's pose is the grain's; the debug window's cheat and the
// testbed's hand are kept as given; the running miracles are reported as their components have them, with no upkeep
// for one whose upkeep would make a record. The service owns its stores, the same objects on every call and empty when
// it is made. The test holds the miracles' service whose stores the spells' code reaches, the registry, the tables and
// the hand it puts in; the locator only injects them.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FallingSpellSystemInterface.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/MagicObjectsSystemInterface.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/HandMotion.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellForest.h"
#include "Particles/SpellLink.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellClass;
using openblack::ecs::components::SpellCreator;
using openblack::ecs::components::SpellDispenser;
using openblack::ecs::components::Transform;

namespace
{
class MagicSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spells' code reaches the spell list of the miracles' service in the locator, not of the ones a test makes
		_spells = &Locator::magicSystem::emplace<ecs::systems::MagicSystem>().SpellStore();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		// a fireball pays 10 a turn and is recharged by its caster; everything else in the tables is 0
		auto info = std::make_unique<InfoConstants>();
		info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).costPerGameTurn = 10.0f;
		const_cast<GMagicInfo&>(magic::GetMagicInfo(*info, MagicType::Fireball)).isSpellRecharged = 1;
		Locator::infoConstants::reset(info.release());
	}

	/// A spell of a magic type at a map position, put at the end of the list
	entt::entity AddSpell(MagicType type, glm::vec3 position)
	{
		const auto spell = _registry->Create();
		auto& component = _registry->Assign<Spell>(spell);
		component.magicType = type;
		component.position = position;
		_spells->Spells().push_back(spell);
		return spell;
	}

	/// A plain fireball whose caster pays all it is asked, below its safety level
	entt::entity AddPayingFireball()
	{
		const auto spell = AddSpell(MagicType::Fireball, glm::vec3(0.0f));
		auto& component = _registry->Get<Spell>(spell);
		component.spellClass = SpellClass::General;
		component.creator = SpellCreator {.kind = SpellCreator::Kind::Thing};
		component.chants = 40.0f;
		component.initialChants = 100.0f;
		return spell;
	}

	/// A dispenser as its components have it, standing at a point
	entt::entity AddDispenser(MagicType type, glm::vec3 position, magic::DispenserTimer timer, entt::entity orb)
	{
		const auto dispenser = _registry->Create();
		_registry->Assign<SpellDispenser>(dispenser, SpellDispenser {.magicType = type, .timer = timer, .orb = orb});
		_registry->Assign<Transform>(dispenser,
		                             Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		return dispenser;
	}

	[[nodiscard]] ecs::Registry& Registry() const { return *_registry; }

private:
	test::RestoreService<Locator::entitiesRegistry> _keepRegistry;
	test::RestoreService<Locator::infoConstants> _keepInfo;
	test::RestoreService<Locator::magicSystem> _keepMagic;
	test::RestoreService<Locator::animalSystem> _keepAnimals;
	ecs::Registry* _registry {nullptr};
	ecs::systems::SpellSystemInterface* _spells {nullptr};
};
} // namespace

TEST_F(MagicSystemTest, ASpellIsFoundAcrossTheLandStrictlyWithinTheRadius)
{
	const auto spell = AddSpell(MagicType::Fireball, glm::vec3(10.0f, 50.0f, 10.0f));

	// the height between them does not count: 3 across the land
	EXPECT_EQ(magic::FindSpellAt(MagicType::Fireball, glm::vec3(10.0f, 0.0f, 13.0f), 3.5f), spell);
	// exactly at the radius is outside
	EXPECT_EQ(magic::FindSpellAt(MagicType::Fireball, glm::vec3(10.0f, 0.0f, 13.0f), 3.0f), entt::entity {entt::null});
}

TEST_F(MagicSystemTest, TheFirstSpellOfTheKindInTheListIsFound)
{
	AddSpell(MagicType::Heal, glm::vec3(5.0f, 0.0f, 5.0f));
	const auto first = AddSpell(MagicType::Fireball, glm::vec3(6.0f, 0.0f, 5.0f));
	AddSpell(MagicType::Fireball, glm::vec3(5.0f, 0.0f, 5.0f));

	EXPECT_EQ(magic::FindSpellAt(MagicType::Fireball, glm::vec3(5.0f, 0.0f, 5.0f), 2.0f), first);
	EXPECT_EQ(magic::FindSpellAt(MagicType::LightningBolt, glm::vec3(5.0f, 0.0f, 5.0f), 2.0f), entt::entity {entt::null});
}

TEST_F(MagicSystemTest, NoShieldIsEverFound)
{
	AddSpell(MagicType::Shield, glm::vec3(5.0f, 0.0f, 5.0f));
	AddSpell(MagicType::PhysicalShield, glm::vec3(5.0f, 0.0f, 5.0f));

	EXPECT_EQ(magic::FindSpellAt(MagicType::Shield, glm::vec3(5.0f, 0.0f, 5.0f), 2.0f), entt::entity {entt::null});
	EXPECT_EQ(magic::FindSpellAt(MagicType::PhysicalShield, glm::vec3(5.0f, 0.0f, 5.0f), 2.0f), entt::entity {entt::null});
}

TEST_F(MagicSystemTest, TheServiceFindsASpellAtAWorldPoint)
{
	const auto spell = AddSpell(MagicType::Fireball, glm::vec3(10.0f, 0.0f, 10.0f));
	ecs::systems::MagicSystem miracles;

	EXPECT_EQ(miracles.SpellAt(MagicType::Fireball, glm::vec3(10.0f, 30.0f, 13.0f), 3.5f), std::optional<entt::entity> {spell});
	EXPECT_EQ(miracles.SpellAt(MagicType::Fireball, glm::vec3(10.0f, 30.0f, 13.0f), 2.5f), std::nullopt);
	EXPECT_EQ(miracles.SpellAt(MagicType::Shield, glm::vec3(10.0f, 30.0f, 10.0f), 3.5f), std::nullopt);
}

TEST_F(MagicSystemTest, PayingGoesThroughTheSpellsChantRules)
{
	ecs::systems::MagicSystem miracles;
	const auto paid = AddPayingFireball();
	const auto twin = AddPayingFireball();

	miracles.PayForSpell(paid, 10.0f);
	magic::chants::PayFor(Registry().Get<Spell>(twin), magic::ChantContextOf(twin), 10.0f, false);
	EXPECT_EQ(Registry().Get<Spell>(paid).chants, Registry().Get<Spell>(twin).chants);
}

TEST_F(MagicSystemTest, AForcedPaymentAsksTheCasterForTheWholeShortfall)
{
	ecs::systems::MagicSystem miracles;
	const auto forced = AddPayingFireball();
	const auto twin = AddPayingFireball();
	const auto unforced = AddPayingFireball();

	const float strength = miracles.ForcePayForSpell(forced, 10.0f);
	const float twinStrength = magic::chants::PayFor(Registry().Get<Spell>(twin), magic::ChantContextOf(twin), 10.0f, true);
	miracles.PayForSpell(unforced, 10.0f);

	EXPECT_EQ(strength, twinStrength);
	EXPECT_EQ(Registry().Get<Spell>(forced).chants, Registry().Get<Spell>(twin).chants);
	// the unforced payment asks only for the cost: the forced one is topped up further
	EXPECT_GT(Registry().Get<Spell>(forced).chants, Registry().Get<Spell>(unforced).chants);
}

TEST_F(MagicSystemTest, AGoneSpellHasNoStrengthAndTakesNothing)
{
	ecs::systems::MagicSystem miracles;
	const auto gone = Registry().Create();
	Registry().Destroy(gone);
	const auto notASpell = Registry().Create();
	const psys::SpellEventInfo event {};

	for (const auto spell : {gone, notASpell})
	{
		EXPECT_EQ(miracles.SpellStrength(spell), 0.0f);
		EXPECT_EQ(miracles.ForcePayForSpell(spell, 10.0f), 0.0f);
		EXPECT_FALSE(miracles.SpellEvent(spell, event));
		EXPECT_FALSE(miracles.SendSpellEvent(spell, event));
		miracles.PayForSpell(spell, 10.0f);
		miracles.CloseDown(spell);
	}
}

TEST_F(MagicSystemTest, AOneShotOfAMagicTypeHoldsTheFirstSeedWithItAtItsLevel)
{
	auto info = std::make_unique<InfoConstants>();
	// every slot of every seed is NONE in zeroed tables; seed 2 casts the fireball and its power-ups, seed 5 the second
	// power-up too
	info->spellSeed.at(2).magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo,
	                                    MagicType::None};
	info->spellSeed.at(5).magicTypes = {MagicType::Heal, MagicType::FireballPowerUpTwo, MagicType::None, MagicType::None};

	const auto base = magic::one_off::SeedFor(*info, MagicType::Fireball);
	ASSERT_TRUE(base.has_value());
	EXPECT_EQ(base->seed, static_cast<SpellSeedType>(2));
	EXPECT_EQ(base->powerUp, -1);
	const auto second = magic::one_off::SeedFor(*info, MagicType::FireballPowerUpTwo);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->seed, static_cast<SpellSeedType>(2));
	EXPECT_EQ(second->powerUp, 1);
	const auto heal = magic::one_off::SeedFor(*info, MagicType::Heal);
	ASSERT_TRUE(heal.has_value());
	EXPECT_EQ(heal->seed, static_cast<SpellSeedType>(5));
	EXPECT_FALSE(magic::one_off::SeedFor(*info, MagicType::LightningBolt).has_value());
}

TEST_F(MagicSystemTest, TheDispensersAreReportedAsTheirComponentsHaveThem)
{
	ecs::systems::MagicSystem miracles;
	EXPECT_TRUE(miracles.GetDispensers().empty());

	const auto orb = Registry().Create();
	const auto gone = Registry().Create();
	Registry().Destroy(gone);
	const auto full =
	    AddDispenser(MagicType::Wood, glm::vec3(1.0f, 2.0f, 3.0f), {.tick = 4, .period = 300, .active = true}, orb);
	const auto taken =
	    AddDispenser(MagicType::Fireball, glm::vec3(5.0f, 0.0f, 6.0f), {.tick = 0, .period = 0, .active = false}, gone);

	const auto dispensers = miracles.GetDispensers();
	ASSERT_EQ(dispensers.size(), 2u);
	for (const auto& dispenser : dispensers)
	{
		if (dispenser.entity == full)
		{
			EXPECT_EQ(dispenser.magicType, MagicType::Wood);
			EXPECT_EQ(dispenser.position, glm::vec3(1.0f, 2.0f, 3.0f));
			EXPECT_TRUE(dispenser.hasOrb);
			EXPECT_EQ(dispenser.tick, 4u);
			EXPECT_EQ(dispenser.period, 300u);
			EXPECT_TRUE(dispenser.active);
		}
		else
		{
			// a bubble that has gone is no bubble
			EXPECT_EQ(dispenser.entity, taken);
			EXPECT_EQ(dispenser.magicType, MagicType::Fireball);
			EXPECT_FALSE(dispenser.hasOrb);
			EXPECT_FALSE(dispenser.active);
		}
	}
}

TEST_F(MagicSystemTest, ADispensersPeriodKeepsATimeShorterThanATurnOutAndLeavesItsSwitch)
{
	ecs::systems::MagicSystem miracles;
	const auto dispenser =
	    AddDispenser(MagicType::Wood, glm::vec3(0.0f), {.tick = 0, .period = 300, .active = false}, entt::null);

	miracles.SetDispenserPeriod(dispenser, 0.0f);
	EXPECT_EQ(Registry().Get<SpellDispenser>(dispenser).timer.period, 300u);
	// 25 seconds as whole turns, on the clock the tests' listener puts in
	const auto turns = game_clock::TicksForSeconds(25.0f);
	ASSERT_GT(turns, 0);
	miracles.SetDispenserPeriod(dispenser, 25.0f);
	EXPECT_EQ(Registry().Get<SpellDispenser>(dispenser).timer.period, static_cast<uint32_t>(turns));
	EXPECT_FALSE(Registry().Get<SpellDispenser>(dispenser).timer.active);
}

TEST_F(MagicSystemTest, OnlyADispenserWithoutABubbleButWithAMiracleIsCharged)
{
	ecs::systems::MagicSystem miracles;
	const auto orb = Registry().Create();
	const auto holding = AddDispenser(MagicType::Wood, glm::vec3(0.0f), {.tick = 0, .period = 300, .active = true}, orb);
	const auto empty = AddDispenser(MagicType::None, glm::vec3(0.0f), {.tick = 0, .period = 300, .active = true}, entt::null);
	const auto notADispenser = Registry().Create();

	miracles.ChargeDispenser(holding);
	miracles.ChargeDispenser(empty);
	miracles.ChargeDispenser(notADispenser);
	EXPECT_EQ(Registry().Get<SpellDispenser>(holding).orb, orb);
	EXPECT_EQ(Registry().Get<SpellDispenser>(empty).orb, entt::entity {entt::null});
	EXPECT_FALSE(Registry().AllOf<SpellDispenser>(notADispenser));
}

TEST_F(MagicSystemTest, OnlyADispenserOrABubbleIsRemoved)
{
	ecs::systems::MagicSystem miracles;
	const auto thing = Registry().Create();
	const auto gone = Registry().Create();
	Registry().Destroy(gone);

	EXPECT_FALSE(miracles.Remove(thing));
	EXPECT_TRUE(Registry().Valid(thing));
	EXPECT_FALSE(miracles.Remove(gone));
	EXPECT_FALSE(miracles.Remove(entt::null));
}

TEST_F(MagicSystemTest, TheHandIsBusyOnlyWithASeedAndOnlyASeedIsDropped)
{
	test::RestoreService<Locator::handSystem> keepHand;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	// the hand belongs to a human player
	const auto player = Registry().Create();
	Registry().Assign<ecs::components::Player>(player, PlayerNames::PLAYER_ONE);
	Registry().Assign<ecs::components::PlayerMagic>(player).playerType = 1;
	ecs::systems::MagicSystem miracles;

	// an empty hand
	EXPECT_FALSE(miracles.IsHandBusy());
	EXPECT_EQ(miracles.GetHeldSeed(), std::nullopt);
	miracles.DiscardHeldSeed();
	EXPECT_EQ(hand.forcedDrops, 0);

	// a hand holding something that is not a seed
	hand.held = Registry().Create();
	EXPECT_FALSE(miracles.IsHandBusy());
	EXPECT_EQ(miracles.GetHeldSeed(), std::nullopt);
	miracles.DiscardHeldSeed();
	EXPECT_EQ(hand.forcedDrops, 0);

	// a hand holding a seed drops it as a shake does
	const auto seed = Registry().Create();
	Registry().Assign<ecs::components::SpellSeed>(seed);
	hand.held = seed;
	EXPECT_TRUE(miracles.IsHandBusy());
	EXPECT_EQ(miracles.GetHeldSeed(), std::optional<entt::entity> {seed});
	miracles.DiscardHeldSeed();
	EXPECT_EQ(hand.forcedDrops, 1);
}

TEST_F(MagicSystemTest, TheHandsPourIsTheGrainsPoseAtTheFraction)
{
	// the grain's state is in the fresh hand magic state of the fixture's miracles' service
	namespace hand_grain = ecs::systems::hand_grain;
	ecs::systems::MagicSystem miracles;

	// a pour two turns in, so that the last two turns' poses differ
	hand_grain::Start(true, 2.0f, 1.5f, 0.75f, false);
	hand_grain::GameTurnUpdate(0.1f);
	hand_grain::GameTurnUpdate(0.1f);
	ASSERT_TRUE(hand_grain::Active());

	const auto now = hand_grain::PoseAt(game_clock::TurnFraction());
	EXPECT_EQ(now.raise, hand_grain::Height());
	EXPECT_EQ(now.tilt, hand_grain::Tilt());
	EXPECT_EQ(now.pinned, hand_grain::ClampedPosition());
	for (const float fraction : {0.0f, 0.25f, 1.0f})
	{
		const auto pour = miracles.GetHandPour(fraction);
		const auto pose = hand_grain::PoseAt(fraction);
		EXPECT_EQ(pour.raise, pose.raise);
		EXPECT_EQ(pour.tilt, pose.tilt);
		EXPECT_EQ(pour.pinned, pose.pinned);
	}
	EXPECT_NE(miracles.GetHandPour(0.0f).raise, miracles.GetHandPour(1.0f).raise);
}

TEST_F(MagicSystemTest, TheCheatAndTheDrivenHandAreKeptAsGiven)
{
	ecs::systems::MagicSystem miracles;
	EXPECT_FALSE(miracles.IsIgnoringInfluence());
	EXPECT_EQ(miracles.GetDrivenHand(), std::nullopt);

	miracles.SetIgnoreInfluence(true);
	EXPECT_TRUE(miracles.IsIgnoringInfluence());
	miracles.SetIgnoreInfluence(false);
	EXPECT_FALSE(miracles.IsIgnoringInfluence());

	ecs::systems::MagicSystemInterface::HandFrame frame;
	frame.handPosition = glm::vec3(1.0f, 2.0f, 3.0f);
	frame.point = glm::vec3(4.0f, 0.0f, 6.0f);
	frame.overWorld = false;
	miracles.DriveHand(frame);
	const auto driven = miracles.GetDrivenHand();
	ASSERT_TRUE(driven.has_value());
	EXPECT_EQ(driven->handPosition, frame.handPosition);
	EXPECT_EQ(driven->point, frame.point);
	EXPECT_FALSE(driven->overWorld);
	miracles.DriveHand(std::nullopt);
	EXPECT_EQ(miracles.GetDrivenHand(), std::nullopt);
}

TEST_F(MagicSystemTest, TheRunningMiraclesAreReportedAsTheirComponentsHaveThem)
{
	ecs::systems::MagicSystem miracles;
	EXPECT_TRUE(miracles.GetSpells().empty());

	const auto fireball = AddPayingFireball();
	auto& component = Registry().Get<Spell>(fireball);
	component.position = glm::vec3(7.0f, 2.0f, 9.0f);
	component.player = PlayerNames::PLAYER_ONE;
	component.age = 3.0f;
	component.duration = 20.0f;
	component.closedDown = true;
	component.psys = 12;
	// a forest whose record of its trees has not been made yet
	const auto forest = AddSpell(MagicType::Forest, glm::vec3(1.0f, 0.0f, 1.0f));
	Registry().Get<Spell>(forest).spellClass = SpellClass::Forest;
	// a spell that has gone but is still in the list
	Registry().Destroy(AddSpell(MagicType::Heal, glm::vec3(0.0f)));

	const auto spells = miracles.GetSpells();
	ASSERT_EQ(spells.size(), 2u);
	const auto& reported = spells[0];
	EXPECT_EQ(reported.entity, fireball);
	EXPECT_EQ(reported.magicType, MagicType::Fireball);
	EXPECT_EQ(reported.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(reported.age, 3.0f);
	EXPECT_EQ(reported.duration, 20.0f);
	EXPECT_EQ(reported.chants, 40.0f);
	EXPECT_EQ(reported.initialChants, 100.0f);
	EXPECT_EQ(reported.strength, magic::GetSpellStrength(fireball));
	EXPECT_EQ(reported.upkeep, 10.0f);
	EXPECT_TRUE(reported.closing);
	EXPECT_FALSE(reported.fromHand);
	EXPECT_EQ(reported.effect, 12u);
	EXPECT_EQ(reported.position, magic::ToWorld(glm::vec3(7.0f, 2.0f, 9.0f)));

	// the forest reports no upkeep and no strength, and is left without the record
	EXPECT_EQ(spells[1].entity, forest);
	EXPECT_EQ(spells[1].upkeep, 0.0f);
	EXPECT_EQ(spells[1].strength, 0.0f);
	EXPECT_FALSE(Registry().AllOf<magic::SpellForestData>(forest));
}

TEST(MagicSystemStoresTest, TheServiceHandsBackTheSameEmptyStoresOnEveryCall)
{
	ecs::systems::MagicSystem miracles;

	EXPECT_EQ(&miracles.SpellStore(), &miracles.SpellStore());
	EXPECT_EQ(&miracles.MagicObjects(), &miracles.MagicObjects());
	EXPECT_EQ(&miracles.FallingSpellStore(), &miracles.FallingSpellStore());
	EXPECT_EQ(&miracles.HandMagic(), &miracles.HandMagic());

	EXPECT_TRUE(miracles.SpellStore().Spells().empty());
	EXPECT_TRUE(miracles.SpellStore().ShieldSpells().empty());
	EXPECT_TRUE(miracles.SpellStore().StormSpells().empty());
	EXPECT_TRUE(miracles.MagicObjects().Shields().empty());
	EXPECT_TRUE(miracles.MagicObjects().FireBalls().empty());
	EXPECT_FALSE(miracles.FallingSpellStore().Spell().has_value());
	EXPECT_FALSE(miracles.FallingSpellStore().SparksSeen());

	// what one call puts in a store, the next finds
	miracles.MagicObjects().FireBalls().push_back(entt::entity {7});
	ASSERT_EQ(miracles.MagicObjects().FireBalls().size(), 1u);
	EXPECT_EQ(miracles.MagicObjects().FireBalls().front(), entt::entity {7});
}
