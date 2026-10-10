/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature as a spell's target: the fire's creature test, the creature's row of the game's tables for the burn and
// defence multipliers and its size dividing them, the explosions and tornadoes leaving a creature alone, its life being
// its body's, the cast rule of the creature spells, a heal mending its marks, a miracle hurting it, fainting it or
// giving it back its life, and burning hurting it

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "Magic/CastRules.h"
#include "Particles/Rules/Explosion.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using Number = openblack::ecs::effects::EffectValues::Number;

namespace
{
/// The heals put on the skins
class HealingSkin final: public ecs::systems::CreatureSkinSystemInterface
{
public:
	std::vector<std::pair<entt::entity, uint32_t>> heals;

	void Update() override {}
	void ProcessTurn() override {}
	void SetTattoo(entt::entity, size_t, const creature_tattoo::Slot&) override {}
	void AddWound(entt::entity, const creature_marks::Mark&) override {}
	void AddBlood(entt::entity, const creature_marks::Mark&) override {}
	void Heal(entt::entity creature, uint32_t counts) override { heals.emplace_back(creature, counts); }
};

/// The faints asked for
class FaintingFight final: public ecs::systems::CreatureFightSystemInterface
{
public:
	std::vector<entt::entity> fainted;

	void ProcessTurn() override {}
	void Update(float, float) override {}
	void AnimationAt(entt::entity, float, ecs::components::CreatureAnimationInputs&) const override {}
	StartResult StartFight(entt::entity, entt::entity) override { return StartResult::NoOpponent; }
	void AbortFight(entt::entity) override {}
	[[nodiscard]] bool IsFighting(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity) const override { return std::nullopt; }
	bool QueueMove(entt::entity, const creature_fight::Move&, bool) override { return false; }
	void ReleaseCharge(entt::entity, float) override {}
	void SetAutoFighting(entt::entity, bool) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity) const override { return false; }
	bool Press(const glm::vec3&, const glm::vec3&) override { return false; }
	void Release() override {}
	[[nodiscard]] bool IsPressed() const override { return false; }
	void KnockOut(entt::entity) override {}
	void ForceFaint(entt::entity creature) override { fainted.push_back(creature); }
	void KillPermanently(entt::entity) override {}
	void Resurrect(entt::entity) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetAngerStartsFights(bool) override {}
	[[nodiscard]] bool GetAngerStartsFights() const override { return false; }
	void SetCameraWatches(bool) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
	[[nodiscard]] bool IsCameraOnFight() const override { return false; }
	[[nodiscard]] std::vector<ArenaView> GetArenas() const override { return {}; }
	[[nodiscard]] std::optional<ArenaView> ArenaOf(entt::entity) const override { return std::nullopt; }
};

class CreatureSpellHooksTest: public ::testing::Test
{
protected:
	CreatureSpellHooksTest()
	    : _skin(&static_cast<HealingSkin&>(Locator::creatureSkinSystem::emplace<HealingSkin>()))
	    , _fight(&static_cast<FaintingFight&>(Locator::creatureFightSystem::emplace<FaintingFight>()))
	{
	}

	/// The creature in a fight at a stage, its fight over for it or not
	static void Fighting(entt::entity creature, CreatureFighting::Stage stage, bool ended = false)
	{
		auto& fighting = test::creature_world::World::Registry().Assign<CreatureFighting>(creature);
		fighting.stage = stage;
		fighting.ended = ended;
	}

	/// An effect from a player's own spell
	static ecs::effects::EffectValues FromPlayer(ecs::effects::EffectValues values, PlayerNames player)
	{
		values.player = player;
		values.appliedByPlayer = true;
		return values;
	}

	/// An effect that hits by `hit`
	static ecs::effects::EffectValues Hitting(float hit)
	{
		ecs::effects::EffectValues values;
		values.numbers.at(static_cast<size_t>(Number::Hit)) = hit;
		return values;
	}

	/// An effect that heals by `heal`
	static ecs::effects::EffectValues Healing(float heal)
	{
		ecs::effects::EffectValues values;
		values.numbers.at(static_cast<size_t>(Number::Heal)) = heal;
		return values;
	}

	static entt::entity Rock()
	{
		auto& registry = test::creature_world::World::Registry();
		const auto rock = registry.Create();
		registry.Assign<Transform>(rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileObject>(rock, MobileObjectInfo::Ball);
		return rock;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::creatureSkinSystem> _restoreSkin;
	const test::RestoreService<Locator::creatureFightSystem> _restoreFight;
	HealingSkin* _skin;
	FaintingFight* _fight;
};

TEST_F(CreatureSpellHooksTest, OnlyACreatureThatIsStillThereIsACreatureToTheFire)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto rock = Rock();
	EXPECT_TRUE(ecs::fire::traits::IsCreature(creature));
	EXPECT_FALSE(ecs::fire::traits::IsCreature(rock));
	EXPECT_FALSE(ecs::fire::traits::IsCreature(entt::null));
	test::creature_world::World::Registry().Destroy(creature);
	EXPECT_FALSE(ecs::fire::traits::IsCreature(creature));
}

TEST_F(CreatureSpellHooksTest, ACreaturesObjectInfoIsItsSpeciesRow)
{
	auto& row = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
	row.defenceMultiplierBurn = 0.25f;
	row.defenceMultiplierHit = 0.5f;
	const auto creature = test::creature_world::World::MakeCreature();

	EXPECT_EQ(ecs::physics::PhysicsObjects::ObjectInfo(creature), &row);
	EXPECT_EQ(ecs::fire::traits::InfoOf(creature), &row);
}

TEST_F(CreatureSpellHooksTest, ACreatureDefendsByItsSizeAgainstBurnCrushHitAndFlyingAway)
{
	auto& row = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
	row.defenceMultiplierBurn = 0.25f;
	row.defenceMultiplierCrush = 1.0f;
	row.defenceMultiplierHit = 0.5f;
	row.defenceMultiplierHeal = 0.75f;
	row.defenceMultiplierFlyAway = 2.0f;
	row.defenceMultiplierAlignmentModification = 3.0f;
	row.defenceMultiplierBeliefModification = 4.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 1.0f;

	// size 1: 1 x 1.5 + 1
	const auto multipliers = ecs::effects::GetDefenseMultiplier(creature);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Burn)), 0.25f / 2.5f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Crush)), 1.0f / 2.5f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Hit)), 0.5f / 2.5f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Heal)), 0.75f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::FlyAway)), 2.0f / 2.5f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Alignment)), 3.0f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Belief)), 4.0f);

	registry.Get<Creature>(creature).size = 3.0f;
	EXPECT_FLOAT_EQ(ecs::effects::GetDefenseMultiplier(creature).at(static_cast<size_t>(Number::Hit)), 0.5f / 4.0f);
}

TEST_F(CreatureSpellHooksTest, TheDefenceKeepsTheSizeBetweenAThousandthAndTwo)
{
	EXPECT_FLOAT_EQ(ecs::effects::CreatureDefenceDivisor(0.5f), 1.75f);
	EXPECT_FLOAT_EQ(ecs::effects::CreatureDefenceDivisor(2.0f), 4.0f);
	EXPECT_FLOAT_EQ(ecs::effects::CreatureDefenceDivisor(5.0f), 4.0f);
	EXPECT_FLOAT_EQ(ecs::effects::CreatureDefenceDivisor(0.0f), 0.001f * 1.5f + 1.0f);
	EXPECT_FLOAT_EQ(ecs::effects::CreatureDefenceDivisor(std::nanf("")), 0.001f * 1.5f + 1.0f);
}

TEST_F(CreatureSpellHooksTest, ExplosionsAndTornadoesCannotDestroyACreature)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(psys::explosion::CanBeDestroyedBySpell(creature, entt::null));
}

TEST_F(CreatureSpellHooksTest, BurningHurtsACreatureAsItHurtsAnythingElse)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Get<CreatureNeeds>(creature).needs.life = 0.8f;
	const auto rock = Rock();
	registry.Assign<Life>(rock, 0.8f);

	EXPECT_FLOAT_EQ(ecs::fire::traits::ReduceLifeDueToBurning(creature, 0.25f, std::nullopt), 0.55f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 0.55f);
	EXPECT_FLOAT_EQ(ecs::fire::traits::ReduceLifeDueToBurning(creature, 1.0f, std::nullopt), 0.0f);
	EXPECT_TRUE(_fight->fainted.empty());
	EXPECT_FLOAT_EQ(ecs::fire::traits::ReduceLifeDueToBurning(rock, 0.25f, std::nullopt), 0.55f);
	EXPECT_FLOAT_EQ(registry.Get<Life>(rock).value, 0.55f);
}

TEST_F(CreatureSpellHooksTest, ACreaturesLifeIsItsBodysLife)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Get<CreatureNeeds>(creature).needs.life = 0.6f;

	EXPECT_FLOAT_EQ(ecs::life::LifeOf(creature), 0.6f);
	ecs::life::SetLife(creature, 0.3f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 0.3f);
	EXPECT_FALSE(registry.AllOf<Life>(creature));
	EXPECT_FLOAT_EQ(ecs::life::ReduceLife(creature, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(ecs::life::IncreaseLife(creature, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ(ecs::life::IncreaseLife(creature, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 1.0f);
}

TEST_F(CreatureSpellHooksTest, AnythingElseKeepsItsOwnLife)
{
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock();
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(rock), 1.0f);
	ecs::life::SetLife(rock, 0.4f);
	EXPECT_FLOAT_EQ(registry.Get<Life>(rock).value, 0.4f);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(rock), 0.4f);
}

TEST_F(CreatureSpellHooksTest, ACreatureSpellIsCastOnACreatureWithNoMoreThanFiveSpellsWaiting)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto rock = Rock();
	constexpr auto k_Freeze = MagicType::CreatureSpellFreeze;

	EXPECT_TRUE(magic::cast_rules::CanCastOn(k_Freeze, creature));
	EXPECT_FALSE(magic::cast_rules::CanCastOn(k_Freeze, rock));
	auto& spells = registry.Assign<CreatureSpells>(creature).spells;
	for (int i = 0; i < 5; ++i)
	{
		spells.waiting.push_back({.spell = creature_spells::Spell::Freeze, .holdTurns = 10, .miracle = entt::null});
	}
	EXPECT_TRUE(magic::cast_rules::CanCastOn(k_Freeze, creature));
	spells.waiting.push_back({.spell = creature_spells::Spell::Freeze, .holdTurns = 10, .miracle = entt::null});
	EXPECT_FALSE(magic::cast_rules::CanCastOn(k_Freeze, creature));
}

TEST_F(CreatureSpellHooksTest, ACreatureSpellIsNeverCastAtAPoint)
{
	EXPECT_FALSE(magic::cast_rules::CanCastAt(MagicType::CreatureSpellFreeze, glm::vec3(100.0f, 0.0f, 100.0f)));
}

TEST_F(CreatureSpellHooksTest, AHealHealsACreaturesLifeAndMendsItsMarks)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHeal = 0.5f;
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Get<CreatureNeeds>(creature).needs.life = 0.4f;

	auto values = Healing(0.5f);
	ecs::effects::ApplyEffect(creature, values);

	// 0.5 x its 0.5 multiplier: the life up by 0.25, the marks by 0.25 x 9600 counts
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 0.65f);
	ASSERT_EQ(_skin->heals.size(), 1U);
	EXPECT_EQ(_skin->heals.at(0).first, creature);
	EXPECT_EQ(_skin->heals.at(0).second, 2400U);
}

TEST_F(CreatureSpellHooksTest, TheMarksMendByWholeCountsAndNotWithoutAHeal)
{
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHeal = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();

	auto none = Healing(0.0f);
	ecs::effects::ApplyEffect(creature, none);
	EXPECT_TRUE(_skin->heals.empty());

	// 0.0001 x 9600 = 0.96: no whole count
	auto little = Healing(0.0001f);
	ecs::effects::ApplyEffect(creature, little);
	ASSERT_EQ(_skin->heals.size(), 1U);
	EXPECT_EQ(_skin->heals.at(0).second, 0U);

	auto rockHeal = Healing(1.0f);
	ecs::effects::ApplyEffect(Rock(), rockHeal);
	EXPECT_EQ(_skin->heals.size(), 1U);
}

TEST_F(CreatureSpellHooksTest, AMiracleHurtsACreatureOutOfAFight)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHit = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(creature).size = 1.0f;

	// 0.5 over its defence of 2.5
	auto values = Hitting(0.5f);
	ecs::effects::ApplyEffect(creature, values);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 0.8f);
	EXPECT_TRUE(_fight->fainted.empty());
}

TEST_F(CreatureSpellHooksTest, ACreatureTakesNoHarmFromItsOwnMiracle)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHit = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();

	auto values = Hitting(0.5f);
	values.appliedBy = creature;
	values.appliedByCreature = true;
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, values), 0.0f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 1.0f);
}

TEST_F(CreatureSpellHooksTest, InAFightAMiracleLeavesItsLifeAsItWas)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHit = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	Fighting(creature, CreatureFighting::Stage::Duel);

	auto values = Hitting(0.5f);
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, values), 1.0f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 1.0f);
}

TEST_F(CreatureSpellHooksTest, InAFightOnlyItsOwnPlayersHarmlessMiraclesReachIt)
{
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHeal = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	Fighting(creature, CreatureFighting::Stage::Duel);

	// another player's heal does nothing to a fighter
	auto other = FromPlayer(Healing(0.5f), PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, other), 0.0f);
	EXPECT_TRUE(_skin->heals.empty());
	// nor does one from an object of its own player, such as a worship site's icon
	auto icon = FromPlayer(Healing(0.5f), PlayerNames::PLAYER_ONE);
	icon.appliedByPlayer = false;
	icon.appliedBy = Rock();
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, icon), 0.0f);
	// nor one whose caster has gone: it has no applier at all
	auto orphan = Healing(0.5f);
	orphan.player = PlayerNames::PLAYER_ONE;
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, orphan), 0.0f);
	EXPECT_TRUE(_skin->heals.empty());
	// its own player's mends its marks, and its life stays as the fight has it
	auto own = FromPlayer(Healing(0.5f), PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, own), 1.0f);
	EXPECT_EQ(_skin->heals.size(), 1U);
	// and so does one it applies itself
	auto itself = Healing(0.5f);
	itself.appliedBy = creature;
	itself.appliedByCreature = true;
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(creature, itself), 1.0f);
	EXPECT_EQ(_skin->heals.size(), 2U);
}

TEST_F(CreatureSpellHooksTest, BeforeTheDuelAndOnceItsFightIsOverItHasNoOpponent)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHit = 1.0f;
	const auto walking = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(walking).size = 1.0f;
	Fighting(walking, CreatureFighting::Stage::Approach);
	auto hit = Hitting(0.5f);
	ecs::effects::ApplyEffect(walking, hit);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(walking).needs.life, 0.8f);

	// a duel whose end has been paid for is over
	const auto ended = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(ended).size = 1.0f;
	Fighting(ended, CreatureFighting::Stage::Duel, true);
	auto again = Hitting(0.5f);
	ecs::effects::ApplyEffect(ended, again);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(ended).needs.life, 0.8f);
}

TEST_F(CreatureSpellHooksTest, AWinnerShowingOffTakesHarmAndAnotherPlayersHeal)
{
	auto& registry = test::creature_world::World::Registry();
	auto& row = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
	row.defenceMultiplierHit = 1.0f;
	row.defenceMultiplierHeal = 1.0f;
	const auto winner = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(winner).size = 1.0f;
	Fighting(winner, CreatureFighting::Stage::Celebrate, true);

	auto hit = Hitting(0.5f);
	ecs::effects::ApplyEffect(winner, hit);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(winner).needs.life, 0.8f);

	auto heal = FromPlayer(Healing(0.1f), PlayerNames::PLAYER_TWO);
	ecs::effects::ApplyEffect(winner, heal);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(winner).needs.life, 0.9f);
	EXPECT_EQ(_skin->heals.size(), 1U);
}

TEST_F(CreatureSpellHooksTest, AMiracleThatTakesTheLastOfItsLifeFaintsIt)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHit = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(creature).size = 1.0f;
	registry.Get<CreatureNeeds>(creature).needs.life = 0.1f;

	auto values = Hitting(0.5f);
	ecs::effects::ApplyEffect(creature, values);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 0.0f);
	ASSERT_EQ(_fight->fainted.size(), 1U);
	EXPECT_EQ(_fight->fainted.at(0), creature);
}

TEST_F(CreatureSpellHooksTest, ACreatureThatCannotDieGetsAllItsLifeBack)
{
	auto& registry = test::creature_world::World::Registry();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).defenceMultiplierHit = 1.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(creature).size = 1.0f;
	registry.Get<Creature>(creature).canDie = false;
	registry.Get<CreatureNeeds>(creature).needs.life = 0.1f;

	auto values = Hitting(0.5f);
	ecs::effects::ApplyEffect(creature, values);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(creature).needs.life, 1.0f);
	EXPECT_TRUE(_fight->fainted.empty());
}
} // namespace
