/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The miracles' shared rules pinned to the original game: what goes when a miracle is removed, how an applied effect
// and the player's turn move the player's alignment, and what the script commands of the miracles take and give back.
// The expected numbers are the original's formulas and constants (docs/wiki/magic/spell-core.md#the-original,
// docs/wiki/magic/influence-and-alignment.md#player-alignment)

// The fake of the miracles owns the real stores of the miracles' service
#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/MagicTables.h"
#include "Magic/Script/CHLSpells.h"
#include "Magic/Script/CHLWorship.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::PlayerMagic;
using openblack::ecs::effects::EffectValues;
using openblack::ecs::systems::PlayerSystemInterface;
using ReachedThing = openblack::ecs::effects::alignment::EffectTarget;

namespace
{
constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

/// Keeps an alignment and a magic for every player, as the player service does for the players without an entity
class FakePlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity) override {}
	[[nodiscard]] entt::entity GetPlayer(PlayerNames) const override { return entt::null; }
	void AddCreature(entt::entity) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames) const override { return std::nullopt; }
	void SetPrimaryCreature(PlayerNames, std::optional<entt::entity>) override {}
	void KeepForNextLand() override {}
	void TakeUpKept(entt::entity) override {}
	void ClearPlayers() override {}
	[[nodiscard]] ecs::components::Alignment& Alignment(PlayerNames name) override { return alignments.at(Index(name)); }
	[[nodiscard]] PlayerMagic& MagicWithoutEntity(PlayerNames name) override { return magic.at(Index(name)); }
	void ClearMagicWithoutEntity() override {}
	[[nodiscard]] ecs::components::InterfaceAlignment& InterfaceAlignmentWithoutEntity() override { return interfaceAlignment; }
	[[nodiscard]] ecs::components::InfluenceCrossing& InfluenceCrossingWithoutEntity() override { return crossing; }

	std::array<ecs::components::Alignment, k_Players> alignments {};
	std::array<PlayerMagic, k_Players> magic {};
	ecs::components::InterfaceAlignment interfaceAlignment {};
	ecs::components::InfluenceCrossing crossing {};

private:
	/// A name out of range is the neutral player's
	[[nodiscard]] static size_t Index(PlayerNames name)
	{
		const auto index = static_cast<size_t>(name);
		return index < k_Players ? index : static_cast<size_t>(PlayerNames::NEUTRAL);
	}
};

/// The player service, an empty registry and the tables the test sets, each put back when the test ends
class MiracleCoreRows: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_players = &static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::infoConstants::reset();
	}

	[[nodiscard]] FakePlayerSystem& Players() const { return *_players; }

	/// The tables the code under test reads: a copy of `info`
	static void UseInfo(const InfoConstants& info) { Locator::infoConstants::emplace(info); }

private:
	test::RestoreService<Locator::playerSystem> _restorePlayers;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	FakePlayerSystem* _players {nullptr};
};

/// Zeroed tables on the heap (they are large)
std::unique_ptr<InfoConstants> ZeroedInfo()
{
	return std::make_unique<InfoConstants>();
}

/// What the alignment change reads of the thing an effect reached
ReachedThing ThingReached(float life, AlignmentType type, float burnDamage)
{
	return {.life = life, .type = type, .burnDamage = burnDamage};
}

/// The script commands' virtual machine and their error log, each put back when the test ends
class MiracleCoreScript: public MiracleCoreRows
{
protected:
	void SetUp() override
	{
		MiracleCoreRows::SetUp();
		if (spdlog::get("scripting") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		_vm = &Locator::vm::emplace<lhvm::LHVM>();
	}

	[[nodiscard]] lhvm::LHVM& Vm() const { return *_vm; }

	/// SET_PLAYER_MAGIC(player, magic, on) as a script calls it: the arguments pushed in that order, the player as a
	/// float
	void SetPlayerMagic(float player, int32_t magicType, bool on) const
	{
		Vm().Pushi(on ? 1 : 0);
		Vm().Pushi(magicType);
		Vm().Pushf(player);
		magic::script::SetPlayerMagic();
	}

	/// HAS_PLAYER_MAGIC(player, magic): the boolean it gives back, none when it gives back something else
	[[nodiscard]] std::optional<bool> HasPlayerMagic(float player, int32_t magicType) const
	{
		Vm().Pushi(magicType);
		Vm().Pushf(player);
		magic::script::HasPlayerMagic();
		lhvm::DataType type {lhvm::DataType::None};
		const auto value = Vm().Pop(type);
		if (type != lhvm::DataType::Boolean)
		{
			return std::nullopt;
		}
		return value.intVal != 0;
	}

private:
	test::RestoreService<Locator::vm> _restoreVm;
	lhvm::LHVM* _vm {nullptr};
};

/// A value put under a command's arguments: what is on top of it afterwards is what the command gave back
constexpr int32_t k_Sentinel = 0x5EED;

/// The miracles over an empty registry: zeroed tables, the animals' shared state the spell classes register with, and
/// the inert miracles' service with its real stores, each put back as it was when the test ends
class MiracleCoreSpellLife: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		Locator::infoConstants::emplace(*ZeroedInfo());
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A seed, as a worship icon or the hand holds one
	[[nodiscard]] static entt::entity MakeSeed()
	{
		const auto seed = Registry().Create();
		Registry().Assign<ecs::components::SpellSeed>(seed);
		return seed;
	}

	/// A fireball the first player cast from `seed`, which knows the miracle it cast
	[[nodiscard]] static entt::entity MakeSpellFrom(entt::entity seed)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<ecs::components::Spell>(spell);
		component.magicType = MagicType::Fireball;
		component.spellClass = magic::ClassOf(MagicType::Fireball);
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		component.seed = seed;
		Registry().Get<ecs::components::SpellSeed>(seed).spell = spell;
		return spell;
	}

private:
	// put back in reverse order: the tables, the miracles' service with its stores, the registry, then the animals'
	// shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

// Put in the hand, a thing's reactions whose kind ends while their initiator is held are no longer available; let go,
// every reaction it started is available again

TEST(MiracleCoreReactions, AReactionStartedByAThingPickedUpByTheHandStopsWhileItIsHeld)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::reactionSystem> restoreReactions;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& reactions = Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto info = ZeroedInfo();
	info->reaction.at(static_cast<size_t>(Reaction::LookAtObject)).whetherReactionFinishesIfInitiatorInHand = 1;
	Locator::infoConstants::emplace(*info);

	const auto held = registry.Create();
	const auto other = registry.Create();
	const auto looked = reactions.Add({.initiator = held, .type = Reaction::LookAtObject});
	const auto followed = reactions.Add({.initiator = held, .type = Reaction::FollowObject});
	const auto elsewhere = reactions.Add({.initiator = other, .type = Reaction::LookAtObject});

	ecs::effects::reactions::SetUnavailableInHand(held);
	EXPECT_FALSE(reactions.Find(looked)->available);
	// a kind whose info does not end it in the hand goes on, and another thing's reaction is not touched
	EXPECT_TRUE(reactions.Find(followed)->available);
	EXPECT_TRUE(reactions.Find(elsewhere)->available);

	// Let go of it: all of its reactions are available again
	ecs::effects::reactions::SetAvailable(held, true);
	EXPECT_TRUE(reactions.Find(looked)->available);
	EXPECT_TRUE(reactions.Find(followed)->available);
	EXPECT_TRUE(reactions.Find(elsewhere)->available);
}

// Removing a miracle also deletes the seed it was cast from, and then the link between the two is gone

TEST_F(MiracleCoreSpellLife, ASeedGoesWithItsMiracleWhenTheMiracleIsRemoved)
{
	const auto seed = MakeSeed();
	const auto otherSeed = MakeSeed();
	const auto spell = MakeSpellFrom(seed);

	magic::DeleteSpell(spell);
	EXPECT_FALSE(Registry().Valid(spell));
	EXPECT_FALSE(Registry().Valid(seed));
	// a seed that cast nothing stays
	EXPECT_TRUE(Registry().Valid(otherSeed));
	EXPECT_EQ(Registry().Get<ecs::components::SpellSeed>(otherSeed).spell, entt::entity(entt::null));

	// A miracle whose seed is already gone is removed all the same
	const auto goneSeed = MakeSeed();
	const auto orphan = MakeSpellFrom(goneSeed);
	Registry().Destroy(goneSeed);
	magic::DeleteSpell(orphan);
	EXPECT_FALSE(Registry().Valid(orphan));
	EXPECT_TRUE(Registry().Valid(otherSeed));
}

// The alignment change of an applied effect: K = |the life changed| + the player info's addition; each of crush, hit,
// heal and fly away adds its amount x the alignment table's row for it in the column of the thing's kind x K, and
// burning adds the harm its heat does x the burn row x K; each change counts v (1 - |A| / 2) when it leans where the
// alignment A already does (0 counting as good), v (1 + |A| / 2) against it

TEST_F(MiracleCoreRows, AnEffectMovesTheAlignmentByTheKindOfThingAndTheLifeItChanged)
{
	auto info = ZeroedInfo();
	info->player.applyEffectAlignmentChangeAddition = 0.1f;
	constexpr size_t k_BurnRow = 0;
	constexpr size_t k_CrushRow = 1;
	constexpr size_t k_HitRow = 2;
	constexpr size_t k_HealRow = 3;
	constexpr size_t k_FlyAwayRow = 4;
	info->alignment.at(k_BurnRow).villager = -2.0f;
	info->alignment.at(k_CrushRow).villager = -1.0f;
	info->alignment.at(k_HitRow).villager = -0.5f;
	info->alignment.at(k_HealRow).villager = 1.0f;
	info->alignment.at(k_FlyAwayRow).villager = -0.25f;
	info->alignment.at(k_CrushRow).building = -3.0f;
	info->alignment.at(k_HealRow).building = 1.5f;
	UseInfo(*info);

	EffectValues values;
	values.numbers.at(static_cast<size_t>(EffectValues::Number::Crush)) = 2.0f;

	// A good player crushes a villager from life 1 to 0.25: K = 0.75 + 0.1, and the evil change is boosted by half the
	// alignment
	ecs::components::Alignment caster {.value = 0.5f, .pending = 0.0f};
	ecs::effects::alignment::Update(caster, ThingReached(0.25f, AlignmentType::Villager, 0.0f), values, 1.0f);
	const float k = 0.75f + 0.1f;
	EXPECT_FLOAT_EQ(caster.pending, (1.0f + 0.25f) * (2.0f * -1.0f * k));
	EXPECT_FLOAT_EQ(caster.value, 0.5f);

	// The same crush on a building reads the building's column
	caster.pending = 0.0f;
	ecs::effects::alignment::Update(caster, ThingReached(0.25f, AlignmentType::Building, 0.0f), values, 1.0f);
	EXPECT_FLOAT_EQ(caster.pending, (1.0f + 0.25f) * (2.0f * -3.0f * k));

	// An effect that leaves the life as it was moves nothing
	caster.pending = 0.0f;
	ecs::effects::alignment::Update(caster, ThingReached(1.0f, AlignmentType::Villager, 0.0f), values, 1.0f);
	EXPECT_EQ(caster.pending, 0.0f);

	// Every kind of effect adds its own part to what is already pending
	values.numbers.at(static_cast<size_t>(EffectValues::Number::Hit)) = 4.0f;
	values.numbers.at(static_cast<size_t>(EffectValues::Number::FlyAway)) = 1.0f;
	caster.pending = 0.125f;
	ecs::effects::alignment::Update(caster, ThingReached(0.25f, AlignmentType::Villager, 0.0f), values, 1.0f);
	EXPECT_FLOAT_EQ(caster.pending, 0.125f + ((1.0f + 0.25f) * (2.0f * -1.0f * k)) + ((1.0f + 0.25f) * (4.0f * -0.5f * k)) +
	                                    ((1.0f + 0.25f) * (1.0f * -0.25f * k)));

	// A neutral player counts as good, and with an alignment of 0 a change is neither damped nor boosted
	ecs::components::Alignment neutral {};
	ecs::effects::alignment::Update(neutral, ThingReached(0.25f, AlignmentType::Villager, 0.0f), values, 1.0f);
	EXPECT_FLOAT_EQ(neutral.pending, (2.0f * -1.0f * k) + (4.0f * -0.5f * k) + (1.0f * -0.25f * k));

	// Healing a building from 0.5 to 0.8: K = 0.3 + 0.1. An evil player's good deed is boosted, a good player's damped
	EffectValues heal;
	heal.numbers.at(static_cast<size_t>(EffectValues::Number::Heal)) = 1.0f;
	const float healK = (0.8f - 0.5f) + 0.1f;
	ecs::components::Alignment evil {.value = -0.5f, .pending = 0.0f};
	ecs::effects::alignment::Update(evil, ThingReached(0.8f, AlignmentType::Building, 0.0f), heal, 0.5f);
	EXPECT_FLOAT_EQ(evil.pending, (1.0f + 0.25f) * (1.0f * 1.5f * healK));
	ecs::components::Alignment good {.value = 0.5f, .pending = 0.0f};
	ecs::effects::alignment::Update(good, ThingReached(0.8f, AlignmentType::Building, 0.0f), heal, 0.5f);
	EXPECT_FLOAT_EQ(good.pending, (1.0f - 0.25f) * (1.0f * 1.5f * healK));
}

TEST_F(MiracleCoreRows, BurningCountsByTheHarmItsHeatDoes)
{
	auto info = ZeroedInfo();
	info->player.applyEffectAlignmentChangeAddition = 0.1f;
	info->alignment.at(0).villager = -2.0f;
	UseInfo(*info);

	// The effect's burn is a temperature: what counts is the harm it does, (T - Tc) / Tc x defenceMultiplierBurn x
	// 0.1, here T 300 over a combustion temperature of 200 with a defence multiplier of 2
	EffectValues fire;
	fire.numbers.at(static_cast<size_t>(EffectValues::Number::Burn)) = 300.0f;
	const float harm = (300.0f - 200.0f) / 200.0f * 2.0f * 0.1f;
	const float k = 0.75f + 0.1f;
	ecs::components::Alignment caster {.value = 0.5f, .pending = 0.0f};
	ecs::effects::alignment::Update(caster, ThingReached(0.25f, AlignmentType::Villager, harm), fire, 1.0f);
	EXPECT_FLOAT_EQ(caster.pending, (1.0f + 0.25f) * (harm * -2.0f * k));

	// A heat below the combustion temperature does no harm, and then the burn counts for nothing, whatever the
	// temperature
	caster.pending = 0.0f;
	ecs::effects::alignment::Update(caster, ThingReached(0.25f, AlignmentType::Villager, 0.0f), fire, 1.0f);
	EXPECT_EQ(caster.pending, 0.0f);
}

// The player's turn: the pending change, held to -1..1, times maxAlignmentChangePerGameTurn (0.0019444, 0.7 per game
// hour) is added to the alignment, which stays in -1..1, and the pending change goes back to 0

TEST_F(MiracleCoreRows, PendingAlignmentMovesThePlayerByAtMostOneStepATurnAndTheRestIsDropped)
{
	auto info = ZeroedInfo();
	constexpr float k_MaxChangePerTurn = 0.0019444f;
	info->player.maxAlignmentChangePerGameTurn = k_MaxChangePerTurn;
	UseInfo(*info);
	constexpr auto k_Player = PlayerNames::PLAYER_THREE;
	auto& kept = Players().alignments.at(static_cast<size_t>(k_Player));

	// Half a step pending: half a step taken
	kept = {.value = 0.25f, .pending = 0.5f};
	ecs::effects::alignment::ProcessForPlayer(k_Player);
	EXPECT_FLOAT_EQ(kept.value, 0.25f + (k_MaxChangePerTurn * 0.5f));
	EXPECT_EQ(kept.pending, 0.0f);

	// Far more than a step pending: one step, and the rest is gone
	kept = {.value = 0.25f, .pending = 40.0f};
	ecs::effects::alignment::ProcessForPlayer(k_Player);
	EXPECT_FLOAT_EQ(kept.value, 0.25f + k_MaxChangePerTurn);
	EXPECT_EQ(kept.pending, 0.0f);
	ecs::effects::alignment::ProcessForPlayer(k_Player);
	EXPECT_FLOAT_EQ(kept.value, 0.25f + k_MaxChangePerTurn);

	kept = {.value = -0.25f, .pending = -7.0f};
	ecs::effects::alignment::ProcessForPlayer(k_Player);
	EXPECT_FLOAT_EQ(kept.value, -0.25f - k_MaxChangePerTurn);
	EXPECT_EQ(kept.pending, 0.0f);

	// The alignment stays within -1..1
	kept = {.value = 0.9995f, .pending = 1.0f};
	ecs::effects::alignment::ProcessForPlayer(k_Player);
	EXPECT_EQ(kept.value, 1.0f);
	kept = {.value = -0.9995f, .pending = -1.0f};
	ecs::effects::alignment::ProcessForPlayer(k_Player);
	EXPECT_EQ(kept.value, -1.0f);

	// Another player's alignment is not touched
	EXPECT_EQ(Players().alignments.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)).value, 0.0f);
}

// The script commands of the miracles. Script player 0 is the neutral player and n is the game's player n - 1; the
// magic types run from 1 to 41

TEST_F(MiracleCoreScript, AScriptMiracleOfAnUnknownKindGivesNothingBack)
{
	// SPELL_AT_POS(magic, position, from, radius, duration, curl): every argument is taken, an error is logged and
	// nothing is pushed back, for a magic of 0 or less or from 42 on
	for (const int32_t kind : {0, -1, 42})
	{
		Vm().Pushi(k_Sentinel);
		Vm().Pushi(kind);
		Vm().Pushf(1000.0f);
		Vm().Pushf(0.0f);
		Vm().Pushf(1000.0f);
		Vm().Pushf(1010.0f);
		Vm().Pushf(20.0f);
		Vm().Pushf(1010.0f);
		Vm().Pushf(10.0f);
		Vm().Pushf(5.0f);
		Vm().Pushf(1.0f);
		magic::script::SpellAtPos();
		lhvm::DataType type {lhvm::DataType::None};
		EXPECT_EQ(Vm().Pop(type).intVal, k_Sentinel) << "magic " << kind;
		EXPECT_EQ(type, lhvm::DataType::Int) << "magic " << kind;
	}

	// SPELL_AT_THING(magic, thing, from, radius, duration, curl) the same, on a thing that exists
	const auto thing = Locator::entitiesRegistry::value().Create();
	for (const int32_t kind : {0, 42})
	{
		Vm().Pushi(k_Sentinel);
		Vm().Pushi(kind);
		Vm().Pusho(static_cast<uint32_t>(thing));
		Vm().Pushf(1010.0f);
		Vm().Pushf(20.0f);
		Vm().Pushf(1010.0f);
		Vm().Pushf(10.0f);
		Vm().Pushf(5.0f);
		Vm().Pushf(1.0f);
		magic::script::SpellAtThing();
		lhvm::DataType type {lhvm::DataType::None};
		EXPECT_EQ(Vm().Pop(type).intVal, k_Sentinel) << "magic " << kind;
		EXPECT_EQ(type, lhvm::DataType::Int) << "magic " << kind;
	}
}

TEST_F(MiracleCoreScript, AScriptGivesOrTakesAPlayersMiracleAndAsksWhetherThePlayerHasIt)
{
	constexpr auto k_Heal = MagicType::Heal;
	constexpr auto k_HealNumber = static_cast<int32_t>(k_Heal);
	constexpr auto k_Player = PlayerNames::PLAYER_TWO;
	const auto& given = Players().magic.at(static_cast<size_t>(k_Player));

	// Never given: the player does not have it
	EXPECT_EQ(HasPlayerMagic(2.0f, k_HealNumber), std::optional<bool>(false));

	// Each SET_PLAYER_MAGIC on counts one more giver, and the player has had it from the first
	SetPlayerMagic(2.0f, k_HealNumber, true);
	SetPlayerMagic(2.0f, k_HealNumber, true);
	EXPECT_EQ(given.remainder.at(static_cast<size_t>(k_Heal)), 2);
	EXPECT_TRUE(magic::players::IsMagicTypeEnabled(k_Player, k_Heal));
	EXPECT_EQ(HasPlayerMagic(2.0f, k_HealNumber), std::optional<bool>(true));

	// Each one off takes one giver away, never below none; HAS_PLAYER_MAGIC asks whether it was ever given, so it
	// still answers yes
	SetPlayerMagic(2.0f, k_HealNumber, false);
	EXPECT_TRUE(magic::players::IsMagicTypeEnabled(k_Player, k_Heal));
	SetPlayerMagic(2.0f, k_HealNumber, false);
	SetPlayerMagic(2.0f, k_HealNumber, false);
	EXPECT_EQ(given.remainder.at(static_cast<size_t>(k_Heal)), 0);
	EXPECT_FALSE(magic::players::IsMagicTypeEnabled(k_Player, k_Heal));
	EXPECT_EQ(HasPlayerMagic(2.0f, k_HealNumber), std::optional<bool>(true));

	// The script's player is truncated: 2.9 is still the game's second player
	EXPECT_EQ(HasPlayerMagic(2.9f, k_HealNumber), std::optional<bool>(true));
	// Another player was given nothing, the neutral player (script player 0) included
	EXPECT_EQ(HasPlayerMagic(1.0f, k_HealNumber), std::optional<bool>(false));
	EXPECT_EQ(HasPlayerMagic(0.0f, k_HealNumber), std::optional<bool>(false));
	SetPlayerMagic(0.0f, k_HealNumber, true);
	EXPECT_EQ(Players().magic.at(static_cast<size_t>(PlayerNames::NEUTRAL)).remainder.at(static_cast<size_t>(k_Heal)), 1);
	EXPECT_EQ(HasPlayerMagic(0.0f, k_HealNumber), std::optional<bool>(true));

	// A player that does not exist has every miracle
	EXPECT_EQ(HasPlayerMagic(9.0f, k_HealNumber), std::optional<bool>(true));

	// A magic out of 1..41 is given to nobody, and nobody has it
	SetPlayerMagic(2.0f, 0, true);
	SetPlayerMagic(2.0f, 42, true);
	EXPECT_EQ(given.remainder.at(0), 0);
	EXPECT_EQ(HasPlayerMagic(2.0f, 0), std::optional<bool>(false));
	EXPECT_EQ(HasPlayerMagic(9.0f, 42), std::optional<bool>(false));

	// SET_PLAYER_MAGIC gives nothing back, and takes all three of its arguments
	Vm().Pushi(k_Sentinel);
	SetPlayerMagic(2.0f, k_HealNumber, true);
	EXPECT_EQ(Vm().Pop().intVal, k_Sentinel);
}

namespace
{
/// A miracle and the prayer power the game's file gives it (its effect row's costToCreate)
struct ManaForSpell
{
	MagicType magic;
	float mana;
};

/// The game's file's costs: no miracle, the fireball and the lightning bolt with their power-ups, the heal and its
/// power-up, the forest, the storm and the last creature miracle
constexpr std::array<ManaForSpell, 12> k_ManaForSpell {{
    {.magic = MagicType::None, .mana = 0.0f},
    {.magic = MagicType::Fireball, .mana = 3500.0f},
    {.magic = MagicType::FireballPowerUpOne, .mana = 7000.0f},
    {.magic = MagicType::FireballPowerUpTwo, .mana = 10000.0f},
    {.magic = MagicType::LightningBolt, .mana = 5000.0f},
    {.magic = MagicType::LightningBoltPowerUpOne, .mana = 7500.0f},
    {.magic = MagicType::LightningBoltPowerUpTwo, .mana = 10000.0f},
    {.magic = MagicType::Heal, .mana = 6000.0f},
    {.magic = MagicType::HealPowerUpOne, .mana = 9000.0f},
    {.magic = MagicType::Forest, .mana = 13000.0f},
    {.magic = MagicType::StormWindRain, .mana = 8000.0f},
    {.magic = MagicType::CreatureSpellItchy, .mana = 8000.0f},
}};

/// Each magic record's own magic type set to where the record stands, as the game's file has them
void NumberMagicRecords(InfoConstants& info)
{
	uint32_t next = 0;
	const auto number = [&next](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(next++);
		}
	};
	number(info.magicGeneral);
	number(info.magicHeal);
	number(info.magicTeleport);
	number(info.magicForest);
	number(info.magicFood);
	number(info.magicStormAndTornado);
	number(info.magicShield);
	number(info.magicWood);
	number(info.magicWater);
	number(info.magicFlockFlying);
	number(info.magicFlockGround);
	number(info.magicCreatureSpell);
}

/// GET_MANA_FOR_SPELL(magic) for each miracle of the list, as a script calls it over a value of its own: the cost, as
/// the one float it gives back. The game reads the effect row of the magic's own record; in `info` every record names
/// its own magic type, so that row is the magic's
void ExpectManaForSpell(lhvm::LHVM& vm, const InfoConstants& info)
{
	for (const auto& entry : k_ManaForSpell)
	{
		const auto number = static_cast<int32_t>(entry.magic);
		EXPECT_EQ(magic::GetMagicInfo(info, entry.magic).magicType, entry.magic) << number;
		vm.Pushi(k_Sentinel);
		vm.Pushi(number);
		magic::script::GetManaForSpell();
		lhvm::DataType given {lhvm::DataType::None};
		EXPECT_EQ(vm.Pop(given).floatVal, entry.mana) << number;
		EXPECT_EQ(given, lhvm::DataType::Float) << number;
		EXPECT_EQ(vm.Pop().intVal, k_Sentinel) << number;
	}
}
} // namespace

// GET_MANA_FOR_SPELL(magic) gives the prayer power the magic needs: the costToCreate of the effect row of the magic's
// own record, what an icon charges to (docs/wiki/magic/spell-core.md, the script commands; the costs in
// docs/wiki/magic/worship.md)

/// With OPENBLACK_GAME_PATH set to the install: the costs of the game's file
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(MiracleCoreScript, realInfoDatManaForSpell)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	// Scripts/info.dat: a 0x2C-byte pack header, then the InfoConstants block
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = ZeroedInfo();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	UseInfo(*info);
	ExpectManaForSpell(Vm(), *info);
}

/// The same reads on a zeroed info block: each record numbered as the game's file has it, and each listed miracle's
/// effect row costing what the game's file gives it
TEST_F(MiracleCoreScript, realInfoDatManaForSpellSynthetic)
{
	auto info = ZeroedInfo();
	NumberMagicRecords(*info);
	for (const auto& entry : k_ManaForSpell)
	{
		info->magicEffect.at(static_cast<size_t>(entry.magic)).costToCreate = entry.mana;
	}
	UseInfo(*info);
	ExpectManaForSpell(Vm(), *info);
}

// The prayer power a miracle needs is the cost of the effect row its own record names, as the original's record reads
// it: a record that names another miracle's row costs that row's (the game's records each name their own)
TEST_F(MiracleCoreScript, TheManaForAMiracleIsTheCostOfTheEffectRowItsRecordNames)
{
	auto info = ZeroedInfo();
	NumberMagicRecords(*info);
	info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).costToCreate = 3500.0f;
	info->magicEffect.at(static_cast<size_t>(MagicType::LightningBolt)).costToCreate = 5000.0f;
	EXPECT_EQ(magic::GetChantsRequiredToCreate(*info, MagicType::Fireball), 3500.0f);
	const_cast<GMagicInfo&>(magic::GetMagicInfo(*info, MagicType::Fireball)).magicType = MagicType::LightningBolt;
	EXPECT_EQ(magic::GetChantsRequiredToCreate(*info, MagicType::Fireball), 5000.0f);
	UseInfo(*info);
	Vm().Pushi(static_cast<int32_t>(MagicType::Fireball));
	magic::script::GetManaForSpell();
	lhvm::DataType type {lhvm::DataType::None};
	EXPECT_EQ(Vm().Pop(type).floatVal, 5000.0f);
	EXPECT_EQ(type, lhvm::DataType::Float);
}

// SET_MAGIC_IN_OBJECT(town, magic, on), the town pushed last. On, a town that does not hold the magic yet holds it, and
// its owner has the magic from one more holder; off, a town that holds it lets it go, and its owner has it from one
// holder less. The same call again changes nothing more. Only a town takes it: anything else is refused, and no town
// holds a magic from 42 on. Every argument is taken and nothing is given back (docs/wiki/magic/worship.md, the CHL
// natives)

TEST_F(MiracleCoreScript, AScriptPutsAMiracleIntoATownOrTakesItOut)
{
	UseInfo(*ZeroedInfo());
	auto& registry = Locator::entitiesRegistry::value();
	// the script's object 0 is no object: the towns are not the registry's first entity
	[[maybe_unused]] const auto first = registry.Create();
	const auto makeTown = [&registry](uint32_t id) {
		const auto made = registry.Create();
		auto& town = registry.Assign<ecs::components::Town>(made);
		town.id = id;
		town.owner = PlayerNames::PLAYER_TWO;
		registry.Assign<ecs::components::TownMagic>(made);
		return made;
	};
	const auto town = makeTown(1);
	const auto otherTown = makeTown(2);
	const auto setMagicInObject = [this](entt::entity thing, MagicType type, bool on) {
		Vm().Pushb(on);
		Vm().Pushi(static_cast<int32_t>(type));
		Vm().Pusho(static_cast<uint32_t>(thing));
		magic::script::SetMagicInObject();
	};
	const auto holds = [&registry](entt::entity thing, MagicType type) {
		return registry.Get<const ecs::components::TownMagic>(thing).held.at(static_cast<size_t>(type));
	};
	constexpr auto k_Owner = PlayerNames::PLAYER_TWO;
	constexpr auto k_Heal = MagicType::Heal;
	const auto& owner = Players().magic.at(static_cast<size_t>(k_Owner));
	const auto holders = [&owner]() { return owner.remainder.at(static_cast<size_t>(MagicType::Heal)); };

	// On: the town holds it, and its owner has it from one holder
	setMagicInObject(town, k_Heal, true);
	EXPECT_TRUE(holds(town, k_Heal));
	EXPECT_EQ(holders(), 1);
	EXPECT_TRUE(magic::players::IsMagicTypeEnabled(k_Owner, k_Heal));
	// Again: the town holds it already
	setMagicInObject(town, k_Heal, true);
	EXPECT_EQ(holders(), 1);
	// Another town of the same player is one more holder
	setMagicInObject(otherTown, k_Heal, true);
	EXPECT_TRUE(holds(otherTown, k_Heal));
	EXPECT_EQ(holders(), 2);

	// Off: the town lets it go, and its owner keeps it from the other town
	setMagicInObject(town, k_Heal, false);
	EXPECT_FALSE(holds(town, k_Heal));
	EXPECT_EQ(holders(), 1);
	EXPECT_TRUE(magic::players::IsMagicTypeEnabled(k_Owner, k_Heal));
	// Again: the town no longer holds it
	setMagicInObject(town, k_Heal, false);
	EXPECT_EQ(holders(), 1);
	setMagicInObject(otherTown, k_Heal, false);
	EXPECT_FALSE(holds(otherTown, k_Heal));
	EXPECT_EQ(holders(), 0);
	EXPECT_FALSE(magic::players::IsMagicTypeEnabled(k_Owner, k_Heal));

	// A thing that is not a town and no thing at all take nothing, and a magic from 42 on goes into no town; each call
	// takes its three arguments and gives nothing back
	const auto notATown = registry.Create();
	Vm().Pushi(k_Sentinel);
	setMagicInObject(notATown, k_Heal, true);
	Vm().Pushb(true);
	Vm().Pushi(static_cast<int32_t>(k_Heal));
	Vm().Pusho(0);
	magic::script::SetMagicInObject();
	setMagicInObject(town, static_cast<MagicType>(42), true);
	EXPECT_EQ(Vm().Pop().intVal, k_Sentinel);
	EXPECT_FALSE(registry.AllOf<ecs::components::TownMagic>(notATown));
	EXPECT_TRUE(std::ranges::none_of(registry.Get<const ecs::components::TownMagic>(town).held, [](bool on) { return on; }));
	EXPECT_TRUE(std::ranges::all_of(owner.remainder, [](int32_t count) { return count == 0; }));
}

// SET_MAGIC_PROPERTIES(dispenser, magic, seconds), the seconds pushed last. The dispenser's magic becomes the magic. Its
// period becomes the seconds at the game's 10 turns a second (1000 ms over the turn's 100, times the seconds,
// truncated) when they are above 0, else its abode info's timeEachMobileObjectTakesToProduce, 300 turns in the game's
// file. A period of 0 switches the dispenser off, and a period set again does not switch it back on. Anything but a
// dispenser is left alone. Every argument is taken and nothing is given back (docs/wiki/magic/worship.md, the
// dispenser's period)

TEST_F(MiracleCoreScript, AScriptSetsADispensersMiracleAndPeriod)
{
	constexpr float k_AbodePeriod = 300.0f;
	auto info = ZeroedInfo();
	auto& dispenserInfo = info->abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser));
	dispenserInfo.abodeNumber = AbodeNumber::SpellDispenser;
	dispenserInfo.timeEachMobileObjectTakesToProduce = k_AbodePeriod;
	UseInfo(*info);
	auto& registry = Locator::entitiesRegistry::value();
	// the script's object 0 is no object: the dispenser is not the registry's first entity
	[[maybe_unused]] const auto first = registry.Create();
	const auto dispenser = registry.Create();
	registry.Assign<ecs::components::Abode>(dispenser).type = AbodeNumber::SpellDispenser;
	{
		auto& made = registry.Assign<ecs::components::SpellDispenser>(dispenser);
		made.timer.period = 300;
		made.timer.active = true;
	}
	const auto setMagicProperties = [this](entt::entity thing, MagicType type, float seconds) {
		Vm().Pusho(static_cast<uint32_t>(thing));
		Vm().Pushi(static_cast<int32_t>(type));
		Vm().Pushf(seconds);
		magic::script::SetMagicProperties();
	};
	const auto state = [&registry, dispenser]() -> const ecs::components::SpellDispenser& {
		return registry.Get<const ecs::components::SpellDispenser>(dispenser);
	};

	// 25 seconds are 250 turns, and the dispenser stays on
	setMagicProperties(dispenser, MagicType::Heal, 25.0f);
	EXPECT_EQ(state().magicType, MagicType::Heal);
	EXPECT_EQ(state().timer.period, 250u);
	EXPECT_TRUE(state().timer.active);
	// 2.75 seconds are 27.5 turns, truncated
	setMagicProperties(dispenser, MagicType::Wood, 2.75f);
	EXPECT_EQ(state().magicType, MagicType::Wood);
	EXPECT_EQ(state().timer.period, 27u);
	// No seconds, or fewer than none: the abode info's period
	setMagicProperties(dispenser, MagicType::Fireball, 0.0f);
	EXPECT_EQ(state().magicType, MagicType::Fireball);
	EXPECT_EQ(state().timer.period, 300u);
	setMagicProperties(dispenser, MagicType::Fireball, 7.0f);
	EXPECT_EQ(state().timer.period, 70u);
	setMagicProperties(dispenser, MagicType::Fireball, -5.0f);
	EXPECT_EQ(state().timer.period, 300u);
	EXPECT_TRUE(state().timer.active);
	// Under a turn: a period of 0, which switches it off; a period set again leaves it off
	setMagicProperties(dispenser, MagicType::Fireball, 0.05f);
	EXPECT_EQ(state().timer.period, 0u);
	EXPECT_FALSE(state().timer.active);
	setMagicProperties(dispenser, MagicType::Food, 12.0f);
	EXPECT_EQ(state().magicType, MagicType::Food);
	EXPECT_EQ(state().timer.period, 120u);
	EXPECT_FALSE(state().timer.active);

	// A thing that is not a dispenser and no thing at all are left alone; each call takes its three arguments and gives
	// nothing back
	const auto notADispenser = registry.Create();
	Vm().Pushi(k_Sentinel);
	setMagicProperties(notADispenser, MagicType::Heal, 10.0f);
	Vm().Pusho(0);
	Vm().Pushi(static_cast<int32_t>(MagicType::Heal));
	Vm().Pushf(10.0f);
	magic::script::SetMagicProperties();
	EXPECT_EQ(Vm().Pop().intVal, k_Sentinel);
	EXPECT_FALSE(registry.AllOf<ecs::components::SpellDispenser>(notADispenser));
	EXPECT_EQ(state().magicType, MagicType::Food);
	EXPECT_EQ(state().timer.period, 120u);
}

// PLAYER_SPELL_CAST_TIME: the turns since the player's last cast, times the game's ms per turn (which
// SET_GAME_TICK_TIME changes), times 0.001, each step a float (docs/wiki/magic/spell-core.md)
TEST_F(MiracleCoreScript, AScriptAsksWhichMiracleAPlayerLastCastAndWhere)
{
	auto& last = Players().MagicWithoutEntity(PlayerNames::PLAYER_ONE).lastCast;
	last.magicType = MagicType::LightningBolt;
	last.position = glm::vec3(120.0f, 4.0f, 340.0f);
	// PLAYER_SPELL_LAST_CAST(player): its magic
	Vm().Pushf(1.0f); // the script's player 1 is the first game player
	magic::script::PlayerSpellLastCast();
	lhvm::DataType type {lhvm::DataType::None};
	EXPECT_EQ(Vm().Pop(type).intVal, static_cast<int32_t>(MagicType::LightningBolt));
	EXPECT_EQ(type, lhvm::DataType::Int);
	// GET_LAST_SPELL_CAST_POS(player): its point, the land's height under it plus its height above the land (the test's
	// land is at 0)
	Vm().Pushf(1.0f);
	magic::script::GetLastSpellCastPos();
	const float z = Vm().Pop(type).floatVal;
	const float y = Vm().Pop(type).floatVal;
	const float x = Vm().Pop(type).floatVal;
	EXPECT_FLOAT_EQ(x, 120.0f);
	EXPECT_FLOAT_EQ(y, 4.0f);
	EXPECT_FLOAT_EQ(z, 340.0f);
}

TEST_F(MiracleCoreScript, AScriptAsksHowManySecondsAgoAPlayerLastCastByTheGamesTurnLength)
{
	Players().MagicWithoutEntity(PlayerNames::PLAYER_ONE).lastCast.turn = 40;
	game_clock::SetTurn(100);
	const auto secondsAgo = [this]() {
		Vm().Pushf(1.0f); // the script's player 1 is the first game player
		magic::script::PlayerSpellCastTime();
		lhvm::DataType type {lhvm::DataType::None};
		const auto value = Vm().Pop(type);
		EXPECT_EQ(type, lhvm::DataType::Float);
		return value.floatVal;
	};
	const auto expected = [](float ms) { return (60.0f * ms) * 0.001f; };
	EXPECT_EQ(secondsAgo(), expected(100.0f));
	game_clock::SetMsPerTurn(50);
	EXPECT_EQ(secondsAgo(), expected(50.0f));
	game_clock::SetMsPerTurn(250);
	EXPECT_EQ(secondsAgo(), expected(250.0f));
	game_clock::SetMsPerTurn(game_clock::k_MsPerTurn);
	game_clock::SetTurn(0);
}

namespace
{
/// The scripts' table of objects, keeping only what was handed to it and whether the script made it
class GivenThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		given.emplace_back(object, createdByScript);
		return true;
	}
	void EnterNative(uint32_t /*native*/) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity /*object*/) override {}
	void RemoveReference(entt::entity /*object*/) override {}
	void ReleaseFromScript(entt::entity /*object*/) override {}
	void Replace(entt::entity /*from*/, entt::entity /*to*/) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity /*container*/) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}

	std::vector<std::pair<entt::entity, bool>> given;
};

/// The script's casts over the players, an empty registry, the inert miracles' service with its real stores, the
/// animals' shared state the spell classes register with, zeroed tables but for the fireball's starting prayer power,
/// and the script's virtual machine, each put back when the test ends. In these tables the fireball is a plain miracle
/// with no particle effect, so a cast keeps what it was given
class MiracleCoreScriptCast: public ::testing::Test
{
protected:
	static constexpr float k_FireballChants = 2500.0f;

	void SetUp() override
	{
		if (spdlog::get("scripting") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		Locator::playerSystem::emplace<FakePlayerSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		auto info = ZeroedInfo();
		info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).initialChants = k_FireballChants;
		Locator::infoConstants::emplace(*info);
		_vm = &Locator::vm::emplace<lhvm::LHVM>();
		_scriptObjects = &static_cast<GivenThings&>(Locator::scriptObjects::emplace<GivenThings>());
		// the script's object 0 is no object: nothing a test makes is the registry's first entity
		static_cast<void>(Registry().Create());
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] lhvm::LHVM& Vm() const { return *_vm; }
	/// What the casts handed to the scripts' table of objects, in order
	[[nodiscard]] const std::vector<std::pair<entt::entity, bool>>& GivenToScripts() const { return _scriptObjects->given; }

	/// A point as a script pushes it: x, y, then z
	void PushPoint(const glm::vec3& point) const
	{
		Vm().Pushf(point.x);
		Vm().Pushf(point.y);
		Vm().Pushf(point.z);
	}

	/// The object a command gave back; none when it gave back something else
	[[nodiscard]] std::optional<uint32_t> PoppedObject() const
	{
		lhvm::DataType type {lhvm::DataType::None};
		const auto value = Vm().Pop(type);
		if (type != lhvm::DataType::Object)
		{
			return std::nullopt;
		}
		return value.uintVal;
	}

	/// A thing standing at a world point
	[[nodiscard]] static entt::entity MakeThing(const glm::vec3& position)
	{
		const auto thing = Registry().Create();
		Registry().Assign<ecs::components::Transform>(
		    thing, ecs::components::Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		return thing;
	}

	/// A miracle of a kind at a map position, put at the end of the miracles' list
	[[nodiscard]] static entt::entity AddSpell(MagicType type, const glm::vec3& position)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<ecs::components::Spell>(spell);
		component.magicType = type;
		component.position = position;
		Locator::magicSystem::value().SpellStore().Spells().push_back(spell);
		return spell;
	}

private:
	// put back in reverse order: the scripts' table of objects, the virtual machine, the tables, the miracles' service
	// with its stores, the registry, the players, then the animals' shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::playerSystem> _restorePlayers;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	lhvm::LHVM* _vm {nullptr};
	GivenThings* _scriptObjects {nullptr};
};

/// The cast a script miracle was given: the neutral player's, the radius as its size, the miracle's starting prayer
/// power and the duration, and as its process the from point as its hand, the way from there to `target`, no
/// direction, a power of 1 and the curl
void ExpectNeutralScriptCast(const ecs::components::Spell& spell, const glm::vec3& target, const glm::vec3& from, float radius,
                             float duration, float curl)
{
	EXPECT_EQ(spell.magicType, MagicType::Fireball);
	EXPECT_TRUE(spell.hasPlayer);
	EXPECT_EQ(spell.player, PlayerNames::NEUTRAL);
	EXPECT_EQ(spell.creator.kind, ecs::components::SpellCreator::Kind::Player);
	EXPECT_EQ(spell.creator.player, PlayerNames::NEUTRAL);
	EXPECT_EQ(spell.magnitude, radius);
	EXPECT_EQ(spell.initialChants, 2500.0f);
	EXPECT_EQ(spell.duration, duration);
	const auto& info = spell.processInfo;
	EXPECT_EQ(info.handPos, from);
	EXPECT_EQ(info.cameraForward, target - from);
	EXPECT_EQ(info.direction, glm::vec3(0.0f));
	EXPECT_EQ(info.power, 1.0f);
	EXPECT_EQ(info.curl, curl);
	EXPECT_TRUE(info.enabled);
}
} // namespace

// SPELL_AT_POS(magic, position, from, radius, duration, curl): the neutral player casts the miracle at the place, with
// no cast check, the script's radius, duration and curl, and the spell is given back as an object, as the original
// (docs/wiki/magic/spell-core.md#chl-script, docs/wiki/magic/spell-core.md#cast-rules)
TEST_F(MiracleCoreScriptCast, AScriptMiracleAtAPlaceIsTheNeutralPlayersWithTheScriptsRadiusTimeAndCurl)
{
	const glm::vec3 at(1000.0f, 0.0f, 1010.0f);
	const glm::vec3 from(1010.0f, 20.0f, 1010.0f);
	Vm().Pushi(static_cast<int32_t>(MagicType::Fireball));
	PushPoint(at);
	PushPoint(from);
	Vm().Pushf(10.0f);
	Vm().Pushf(5.0f);
	Vm().Pushf(0.5f);
	magic::script::SpellAtPos();
	const auto given = PoppedObject();
	ASSERT_TRUE(given.has_value());
	const auto spell = static_cast<entt::entity>(*given);
	ASSERT_TRUE(Registry().Valid(spell));
	ASSERT_TRUE(Registry().AllOf<ecs::components::Spell>(spell));
	const auto& made = Registry().Get<const ecs::components::Spell>(spell);
	EXPECT_EQ(made.position, magic::ToMap(at));
	ExpectNeutralScriptCast(made, magic::ToWorld(magic::ToMap(at)), from, 10.0f, 5.0f, 0.5f);
	// the spell is the script's own, as one it made
	ASSERT_EQ(GivenToScripts().size(), 1u);
	EXPECT_EQ(GivenToScripts().front(), std::make_pair(spell, true));
}

// SPELL_AT_POS's from point goes through the map's coordinates as its target does: the hand the miracle is cast from
// has its x and z on the map's grid, and the way to the target is measured from there, as the original
// (docs/wiki/magic/spell-core.md#chl-script)
TEST_F(MiracleCoreScriptCast, AScriptMiracleAtAPlaceIsCastFromItsFromPointOnTheMapsGrid)
{
	const glm::vec3 at(1000.0f, 0.0f, 1010.0f);
	const glm::vec3 from(1010.31f, 20.0f, 1010.0f);
	const auto hand = magic::ToWorld(magic::ToMap(from));
	ASSERT_NE(hand.x, from.x);
	EXPECT_EQ(hand.x, map_coords::Quantise(from.x));
	Vm().Pushi(static_cast<int32_t>(MagicType::Fireball));
	PushPoint(at);
	PushPoint(from);
	Vm().Pushf(10.0f);
	Vm().Pushf(5.0f);
	Vm().Pushf(0.5f);
	magic::script::SpellAtPos();
	const auto given = PoppedObject();
	ASSERT_TRUE(given.has_value());
	const auto spell = static_cast<entt::entity>(*given);
	ASSERT_TRUE(Registry().AllOf<ecs::components::Spell>(spell));
	ExpectNeutralScriptCast(Registry().Get<const ecs::components::Spell>(spell), magic::ToWorld(magic::ToMap(at)), hand, 10.0f,
	                        5.0f, 0.5f);
}

// SPELL_AT_THING(magic, thing, from, radius, duration, curl): the neutral player casts the miracle on the thing with
// the same cast as SPELL_AT_POS, the way from the from point to the thing, and the spell is given back as an object;
// for a thing that is not there, no spell, and no object is given back, as the original
// (docs/wiki/magic/spell-core.md#chl-script)
TEST_F(MiracleCoreScriptCast, AScriptMiracleOnAThingIsTheNeutralPlayersWithTheScriptsRadiusTimeAndCurl)
{
	const glm::vec3 at(1000.0f, 0.0f, 1010.0f);
	const glm::vec3 from(1010.0f, 20.0f, 1010.0f);
	const auto spellAtThing = [this, &from](entt::entity thing) {
		Vm().Pushi(static_cast<int32_t>(MagicType::Fireball));
		Vm().Pusho(static_cast<uint32_t>(thing));
		PushPoint(from);
		Vm().Pushf(10.0f);
		Vm().Pushf(5.0f);
		Vm().Pushf(0.5f);
		magic::script::SpellAtThing();
		return PoppedObject();
	};
	const auto given = spellAtThing(MakeThing(at));
	ASSERT_TRUE(given.has_value());
	const auto spell = static_cast<entt::entity>(*given);
	ASSERT_TRUE(Registry().Valid(spell));
	ASSERT_TRUE(Registry().AllOf<ecs::components::Spell>(spell));
	ExpectNeutralScriptCast(Registry().Get<const ecs::components::Spell>(spell), at, from, 10.0f, 5.0f, 0.5f);
	// the spell is the script's own, as one it made
	ASSERT_EQ(GivenToScripts().size(), 1u);
	EXPECT_EQ(GivenToScripts().front(), std::make_pair(spell, true));

	const auto gone = MakeThing(at);
	Registry().Destroy(gone);
	EXPECT_EQ(spellAtThing(gone), std::optional<uint32_t>(0u));
	EXPECT_EQ(Locator::magicSystem::value().SpellStore().Spells().size(), 1u);
	// no spell, nothing handed to the scripts
	EXPECT_EQ(GivenToScripts().size(), 1u);
}

// SPELL_AT_THING's from point goes through the map's coordinates and back, as SPELL_AT_POS's does: the hand the miracle
// is cast from has its x and z on the map's grid, and the way to the thing is measured from there, as the original
// (docs/wiki/magic/spell-core.md#chl-script)
TEST_F(MiracleCoreScriptCast, AScriptMiracleOnAThingIsCastFromItsFromPointOnTheMapsGrid)
{
	const glm::vec3 at(1000.0f, 0.0f, 1010.0f);
	const glm::vec3 from(1010.31f, 20.0f, 1010.0f);
	const auto hand = magic::ToWorld(magic::ToMap(from));
	ASSERT_NE(hand.x, from.x);
	Vm().Pushi(static_cast<int32_t>(MagicType::Fireball));
	Vm().Pusho(static_cast<uint32_t>(MakeThing(at)));
	PushPoint(from);
	Vm().Pushf(10.0f);
	Vm().Pushf(5.0f);
	Vm().Pushf(0.5f);
	magic::script::SpellAtThing();
	const auto given = PoppedObject();
	ASSERT_TRUE(given.has_value());
	const auto spell = static_cast<entt::entity>(*given);
	ASSERT_TRUE(Registry().AllOf<ecs::components::Spell>(spell));
	ExpectNeutralScriptCast(Registry().Get<const ecs::components::Spell>(spell), at, hand, 10.0f, 5.0f, 0.5f);
}

// SPELL_AT_POINT(magic, position, radius) casts nothing: it gives back the first miracle of the kind in the list that
// is closer to the place than the radius, measured across the land, or no object, as the original
// (docs/wiki/magic/spell-core.md#chl-script)
TEST_F(MiracleCoreScriptCast, AScriptFindsTheFirstMiracleOfTheKindCloserThanTheRadius)
{
	static_cast<void>(AddSpell(MagicType::Heal, glm::vec3(5.0f, 0.0f, 5.0f)));
	const auto first = AddSpell(MagicType::Fireball, glm::vec3(6.0f, 0.0f, 5.0f));
	static_cast<void>(AddSpell(MagicType::Fireball, glm::vec3(5.0f, 0.0f, 5.0f)));
	const auto spellAtPoint = [this](MagicType type, const glm::vec3& position, float radius) {
		Vm().Pushi(static_cast<int32_t>(type));
		PushPoint(position);
		Vm().Pushf(radius);
		magic::script::SpellAtPoint();
		return PoppedObject();
	};

	// the height above the land does not count
	EXPECT_EQ(spellAtPoint(MagicType::Fireball, glm::vec3(5.0f, 30.0f, 5.0f), 2.0f),
	          std::optional<uint32_t>(static_cast<uint32_t>(first)));
	EXPECT_EQ(spellAtPoint(MagicType::Fireball, glm::vec3(20.0f, 0.0f, 5.0f), 2.0f), std::optional<uint32_t>(0u));
	EXPECT_EQ(spellAtPoint(MagicType::LightningBolt, glm::vec3(5.0f, 0.0f, 5.0f), 2.0f), std::optional<uint32_t>(0u));
}

// A miracle cast in a player's name is that player's, at a place or on an object, and one cast with no one behind it is
// the neutral player's: the spell takes its player from whoever cast it, as the original
// (docs/wiki/magic/spell-core.md#life-cycle)
TEST_F(MiracleCoreScriptCast, AMiracleCastInAPlayersNameBelongsToThatPlayer)
{
	const auto playerOf = [](entt::entity spell) {
		const auto& made = Registry().Get<const ecs::components::Spell>(spell);
		EXPECT_TRUE(made.hasPlayer);
		return made.player;
	};
	const glm::vec3 at(1000.0f, 0.0f, 1010.0f);
	const particles::ProcessInfo info {};
	magic::SpellCastData cast {.magnitude = 10.0f, .chants = k_FireballChants, .duration = -1.0f, .maxObjectsToCreate = -1};

	entt::entity spell = entt::null;
	ASSERT_EQ(magic::CastAtPos(MagicType::Fireball, magic::creator::OfPlayer(PlayerNames::PLAYER_TWO), magic::ToMap(at), &spell,
	                           &cast, info),
	          1);
	EXPECT_EQ(playerOf(spell), PlayerNames::PLAYER_TWO);

	ASSERT_EQ(magic::CastAtObject(MagicType::Fireball, magic::creator::OfPlayer(PlayerNames::PLAYER_THREE), MakeThing(at),
	                              &spell, &cast, info),
	          1);
	EXPECT_EQ(playerOf(spell), PlayerNames::PLAYER_THREE);

	ASSERT_EQ(magic::CastAtPos(MagicType::Fireball, {}, magic::ToMap(at), &spell, &cast, info), 1);
	EXPECT_EQ(playerOf(spell), PlayerNames::NEUTRAL);
}
