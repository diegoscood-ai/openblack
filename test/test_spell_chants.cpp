/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Alignment.h"
#include "ECS/Effects/Alignment.h"
#include "InfoConstants.h"
#include "Magic/Core/Chants.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellChants.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;

namespace
{
/// The lightning bolt row of info.dat (MAGIC_TYPE 4): initial 5000, cost 50 a turn, 2 an event, 6 s
GMagicEffectInfo Lightning()
{
	GMagicEffectInfo effect {};
	effect.initialChants = 5000.0f;
	effect.costPerGameTurn = 50.0f;
	effect.costPerEvent = 2.0f;
	effect.timerWhenPlayerCasting = 6.0f;
	effect.divideCostsByTribalPower = 0;
	return effect;
}

/// The shield row (MAGIC_TYPE 19): initial 5000, 20 a turn at radiusForNormalCost 30, no time limit for a player,
/// divideCostsByTribalPower
GMagicEffectInfo Shield()
{
	GMagicEffectInfo effect {};
	effect.initialChants = 5000.0f;
	effect.costPerGameTurn = 20.0f;
	effect.costPerEvent = 0.0f;
	effect.timerWhenPlayerCasting = -1.0f;
	effect.divideCostsByTribalPower = 1;
	return effect;
}

/// A creator: the neutral (script) player refills everything asked, a normal player nothing
chants::Context ContextFor(const GMagicEffectInfo& effect, bool neutral, float* given)
{
	chants::Context context;
	context.effect = &effect;
	context.recharged = true;
	context.hasCreator = true;
	context.costToMaintain = effect.costPerGameTurn;
	context.turnMs = 100;
	context.maintain = [neutral, given](float amount) {
		const float result = neutral ? amount : 0.0f;
		if (given != nullptr)
		{
			*given += result;
		}
		return result;
	};
	return context;
}

struct TurnResult
{
	float strength;
	float chants;
	bool closed;
};

/// The maintain request then CoreProcess for one turn, on the chant level
TurnResult Turn(Spell& spell, const chants::Context& context)
{
	spell.age += 0.1f;
	if (spell.duration >= 0.0f && spell.age > spell.duration)
	{
		spell.closedDown = true;
	}
	float strength = 0.0f;
	if (!spell.closedDown)
	{
		strength = chants::GetSpellStrength(spell, context);
		chants::PayForOneTurn(spell, context);
		spell.processInfo.power = strength;
		chants::Recharge(spell, context);
		if (spell.processInfo.power <= 0.0f)
		{
			spell.closedDown = true;
		}
	}
	return {strength, spell.chants, spell.closedDown};
}
} // namespace

TEST(SpellChants, lightningSafetyLevel)
{
	const auto effect = Lightning();
	Spell spell;
	chants::SetChants(spell, effect.initialChants);
	const auto context = ContextFor(effect, false, nullptr);
	// min(50 x (1000 / 100) x 5, 5000) = 2500, at least costPerEvent 2
	EXPECT_FLOAT_EQ(chants::GetChantSafetyLevel(spell, context), 2500.0f);
	EXPECT_FLOAT_EQ(chants::GetSpellStrength(spell, context), 1.0f);
}

TEST(SpellChants, lightningPlayerLivesOnItsInitialChants)
{
	// a player's hand cast: nothing refills, 50 a turn; strength 1 until the chants fall under 2500, closed at 6 s
	const auto effect = Lightning();
	Spell spell;
	chants::SetChants(spell, effect.initialChants);
	spell.duration = effect.timerWhenPlayerCasting;
	float given = 0.0f;
	const auto context = ContextFor(effect, false, &given);
	int turns = 0;
	for (; turns < 100; ++turns)
	{
		const auto result = Turn(spell, context);
		if (result.closed)
		{
			break;
		}
		const float before = result.chants + 50.0f; // the chants the strength was read from
		if (before >= 2500.0f)
		{
			EXPECT_FLOAT_EQ(result.strength, 1.0f) << "turn " << turns;
		}
		else
		{
			EXPECT_NEAR(result.strength, before / 2500.0f, 1e-5f) << "turn " << turns;
		}
	}
	// the turn whose age (a float sum of 0.1 s) passes the 6 s closes it: about 60 turns of 50 chants
	int expected = 0;
	for (float age = 0.1f; !(age > 6.0f); age += 0.1f)
	{
		++expected;
	}
	EXPECT_EQ(turns, expected);
	EXPECT_GE(turns, 59);
	EXPECT_LE(turns, 60);
	EXPECT_FLOAT_EQ(given, 0.0f);
	EXPECT_NEAR(spell.chants, 5000.0f - static_cast<float>(turns) * 50.0f, 1e-2f);
}

TEST(SpellChants, lightningNeutralPlayerRefills)
{
	// the script's neutral player: once under the safety level every payment is refilled (at most the cost)
	const auto effect = Lightning();
	Spell spell;
	chants::SetChants(spell, effect.initialChants);
	float given = 0.0f;
	const auto context = ContextFor(effect, true, &given);
	for (int turn = 0; turn < 200; ++turn)
	{
		const auto result = Turn(spell, context);
		ASSERT_FALSE(result.closed);
		EXPECT_FLOAT_EQ(result.strength, 1.0f);
	}
	EXPECT_NEAR(spell.chants, 2500.0f, 1e-2f);
	EXPECT_GT(given, 0.0f);
}

TEST(SpellChants, lightningEventCost)
{
	// PayForOneEvent: costPerEvent 2
	const auto effect = Lightning();
	Spell spell;
	chants::SetChants(spell, 3000.0f);
	const auto context = ContextFor(effect, false, nullptr);
	EXPECT_FLOAT_EQ(chants::PayForOneEvent(spell, context), 1.0f);
	EXPECT_FLOAT_EQ(spell.chants, 2998.0f);
	// a free spell pays nothing and has strength 1
	spell.free = true;
	EXPECT_FLOAT_EQ(chants::PayForOneEvent(spell, context), 1.0f);
	EXPECT_FLOAT_EQ(spell.chants, 2998.0f);
}

TEST(SpellChants, shieldIsMaintained)
{
	// a maintained spell's safety is its initial chants; the SpellShield upkeep is costPerGameTurn x (r / 30)^2
	const auto effect = Shield();
	Spell spell;
	chants::SetChants(spell, effect.initialChants);
	spell.magnitude = 60.0f;
	auto context = ContextFor(effect, false, nullptr);
	context.maintained = true;
	context.costToMaintain = effect.costPerGameTurn * (spell.magnitude / 30.0f) * (spell.magnitude / 30.0f);
	EXPECT_FLOAT_EQ(chants::GetChantSafetyLevel(spell, context), 5000.0f);
	EXPECT_FLOAT_EQ(chants::PayForOneTurn(spell, context), (5000.0f - 80.0f) / 5000.0f);
	EXPECT_FLOAT_EQ(spell.chants, 4920.0f);
	// no time limit for the player (timerWhenPlayerCasting -1): only the chants end it
	spell.duration = effect.timerWhenPlayerCasting;
	for (int turn = 0; turn < 1000 && !spell.closedDown; ++turn)
	{
		Turn(spell, context);
	}
	EXPECT_TRUE(spell.closedDown);
	EXPECT_LE(spell.chants, 0.0f);
}

TEST(SpellChants, shieldDividesByTribalPower)
{
	const auto effect = Shield();
	Spell spell;
	chants::SetChants(spell, effect.initialChants);
	auto context = ContextFor(effect, false, nullptr);
	context.maintained = true;
	context.costToMaintain = 20.0f;
	context.tribalPower = 2.0f;
	chants::PayForOneTurn(spell, context);
	EXPECT_FLOAT_EQ(spell.chants, 4990.0f);
	// a tribal power under 1 does not raise the cost (max(power, 1))
	context.tribalPower = 0.5f;
	chants::PayForOneTurn(spell, context);
	EXPECT_FLOAT_EQ(spell.chants, 4970.0f);
}

TEST(SpellChants, strengthEdges)
{
	GMagicEffectInfo effect {};
	Spell spell;
	auto context = ContextFor(effect, false, nullptr);
	context.costToMaintain = 0.0f;
	// safety 0 (no upkeep, no event cost): 1 with chants, 0 without
	chants::SetChants(spell, 10.0f);
	EXPECT_FLOAT_EQ(chants::GetChantSafetyLevel(spell, context), 0.0f);
	EXPECT_FLOAT_EQ(chants::GetSpellStrength(spell, context), 1.0f);
	spell.chants = 0.0f;
	EXPECT_FLOAT_EQ(chants::GetSpellStrength(spell, context), 0.0f);
	// no upkeep: PayForOneTurn is 1 without paying
	EXPECT_FLOAT_EQ(chants::PayForOneTurn(spell, context), 1.0f);
	// no creator: strength 0, PayFor 0
	context.hasCreator = false;
	spell.chants = 10.0f;
	EXPECT_FLOAT_EQ(chants::GetSpellStrength(spell, context), 0.0f);
	EXPECT_FLOAT_EQ(chants::PayFor(spell, context, 1.0f, false), 0.0f);
	EXPECT_FLOAT_EQ(spell.chants, 10.0f);
	// PayForOneEvent without a creator: 0, and no mana path point (CreateSpellPoint is not called)
	bool pointMade = false;
	context.createSpellPoint = [&pointMade](float, bool) { pointMade = true; };
	EXPECT_FLOAT_EQ(chants::PayForOneEvent(spell, context), 0.0f);
	EXPECT_FALSE(pointMade);
	context.createSpellPoint = nullptr;
	// the multipliers
	context.hasCreator = true;
	context.tribalPower = 1.5f;
	context.seedPower = 0.5f;
	spell.strengthMultiplier = 2.0f;
	EXPECT_FLOAT_EQ(chants::GetSpellStrength(spell, context), 1.5f);
}

TEST(SpellChants, rechargedWithoutMaintainCallback)
{
	// a recharged spell whose context cannot maintain it is never refilled, and the empty callback is never called
	const auto effect = Lightning();
	Spell spell;
	chants::SetChants(spell, 3000.0f);
	spell.chants = 10.0f;
	auto context = ContextFor(effect, false, nullptr);
	context.maintain = nullptr;
	float strength = 0.0f;
	EXPECT_NO_THROW(strength = chants::PayFor(spell, context, 2.0f, true));
	EXPECT_FLOAT_EQ(spell.chants, 8.0f);
	EXPECT_FLOAT_EQ(strength, 8.0f / 2500.0f);
	float given = 1.0f;
	EXPECT_NO_THROW(given = chants::Recharge(spell, context));
	EXPECT_FLOAT_EQ(given, 0.0f);
	EXPECT_FLOAT_EQ(spell.chants, 8.0f);
	EXPECT_NO_THROW(chants::PayForOneTurn(spell, context));
	EXPECT_FLOAT_EQ(spell.chants, -42.0f);
}

TEST(SpellChants, spellPoints)
{
	// each paid turn and event reports a spell point with what it cost and whether it was the turn's upkeep
	const auto effect = Lightning();
	Spell spell;
	chants::SetChants(spell, 3000.0f);
	auto context = ContextFor(effect, false, nullptr);
	std::vector<std::pair<float, bool>> points;
	context.createSpellPoint = [&points](float chants, bool perTurn) { points.emplace_back(chants, perTurn); };
	chants::PayForOneTurn(spell, context);
	chants::PayForOneEvent(spell, context);
	ASSERT_EQ(points.size(), 2u);
	EXPECT_FLOAT_EQ(points[0].first, 50.0f);
	EXPECT_TRUE(points[0].second);
	EXPECT_FLOAT_EQ(points[1].first, 2.0f);
	EXPECT_FALSE(points[1].second);
	EXPECT_FLOAT_EQ(spell.chants, 2948.0f);
	// without a creator a paid turn still reports its point, pays nothing and has strength 0
	points.clear();
	context.hasCreator = false;
	EXPECT_FLOAT_EQ(chants::PayForOneTurn(spell, context), 0.0f);
	ASSERT_EQ(points.size(), 1u);
	EXPECT_FLOAT_EQ(points[0].first, 50.0f);
	EXPECT_TRUE(points[0].second);
	EXPECT_FLOAT_EQ(spell.chants, 2948.0f);
	// a turn with no upkeep reports nothing
	points.clear();
	context.costToMaintain = 0.0f;
	EXPECT_FLOAT_EQ(chants::PayForOneTurn(spell, context), 1.0f);
	EXPECT_TRUE(points.empty());
}

TEST(SpellChants, alignmentScale)
{
	// v of A's sign (0 counts as positive) -> v (1 - |A| / 2), the opposite sign -> v (1 + |A| / 2)
	ecs::components::Alignment alignment {0.5f, 0.0f};
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(alignment, 1.0f), 0.75f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(alignment, -1.0f), -1.25f);
	alignment.value = -0.5f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(alignment, 1.0f), 1.25f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(alignment, -1.0f), -0.75f);
	alignment.value = 0.0f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(alignment, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::ScaleChange(alignment, -1.0f), -1.0f);
}

/// With OPENBLACK_GAME_PATH: the real lightning and shield rows
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(SpellChants, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto& lightning = GetMagicEffectInfo(*info, MagicType::LightningBolt);
	EXPECT_FLOAT_EQ(lightning.initialChants, 5000.0f);
	EXPECT_FLOAT_EQ(lightning.costPerGameTurn, 50.0f);
	EXPECT_FLOAT_EQ(lightning.costPerEvent, 2.0f);
	EXPECT_FLOAT_EQ(lightning.timerWhenPlayerCasting, 6.0f);
	const auto& shield = GetMagicEffectInfo(*info, MagicType::Shield);
	EXPECT_FLOAT_EQ(shield.timerWhenPlayerCasting, -1.0f);
	EXPECT_FLOAT_EQ(shield.costPerGameTurn, 20.0f);
	EXPECT_EQ(shield.divideCostsByTribalPower, 1u);
	const auto* shieldInfo = GetMagicInfoAs<GMagicShieldInfo>(*info, MagicType::Shield);
	ASSERT_NE(shieldInfo, nullptr);
	EXPECT_FLOAT_EQ(shieldInfo->radiusForNormalCost, 30.0f);
	EXPECT_TRUE(IsMaintainedSpell(MagicType::Shield));
	EXPECT_EQ(GetMagicInfo(*info, MagicType::LightningBolt).isSpellRecharged, 1u);
	EXPECT_FLOAT_EQ(info->spellSystem.delayBeforeSeedActive, 1.5f);
	// the alignment update's constant: GPlayerInfo.applyEffectAlignmentChangeAddition
	EXPECT_GE(info->player.applyEffectAlignmentChangeAddition, 0.0f);
}

// The chant rules themselves (Magic/SpellChants.h), which the functions above apply to the Spell component

namespace
{
/// A miracle that lasts until its prayer power runs out
constexpr float k_RulesNoTimeLimit = -1.0f;

/// A caster that gives everything it is asked for (like the scripts' neutral player) or nothing (like a player casting
/// from the hand), and remembers what it gave and what was spent
class FakeRulesCaster final: public SpellCasterInterface
{
public:
	explicit FakeRulesCaster(bool refills)
	    : _refills(refills)
	{
	}

	float MaintainSpell(float amount) override
	{
		const float result = _refills ? amount : 0.0f;
		given += result;
		return result;
	}

	void OnChantsSpent(float chants, ChantCharge charge) override { spent.push_back({chants, charge}); }

	struct Spent
	{
		float chants;
		ChantCharge charge;
	};

	float given {0.0f};
	std::vector<Spent> spent;

private:
	bool _refills;
};

/// A lightning bolt: 5000 to start, 50 a turn, 2 an event, recharged
SpellChantRules LightningChantRules()
{
	return {.costPerEvent = 2.0f, .costToMaintain = 50.0f, .recharged = true};
}

/// A shield: maintained, 20 a turn at its normal size, cheaper with a strong tribal power
SpellChantRules ShieldChantRules()
{
	return {.costToMaintain = 20.0f, .maintained = true, .recharged = true, .divideCostsByTribalPower = true};
}

struct RulesTurn
{
	float strength;
	bool closed;
};

/// One turn of a running miracle: it ages, ends when its time is up, otherwise reads its strength, pays its upkeep
/// and is recharged, and ends once it has no strength left
RulesTurn RunTurn(SpellChants& spell, float& age, float duration, const SpellChantRules& rules, FakeRulesCaster& caster)
{
	age += 0.1f;
	if (duration >= 0.0f && age > duration)
	{
		return {0.0f, true};
	}
	const float strength = GetSpellStrength(spell, rules, &caster);
	PayForOneTurn(spell, rules, &caster);
	Recharge(spell, rules, caster);
	return {strength, strength <= 0.0f};
}
} // namespace

TEST(SpellChantRules, rulesFromTables)
{
	GMagicInfo magic {};
	magic.isSpellRecharged = 1;
	GMagicEffectInfo effect {};
	effect.costPerEvent = 2.0f;
	effect.costPerGameTurn = 20.0f;
	effect.divideCostsByTribalPower = 1;
	const auto rules = ChantRulesFor(MagicType::Shield, magic, effect);
	EXPECT_FLOAT_EQ(rules.costPerEvent, 2.0f);
	EXPECT_FLOAT_EQ(rules.costToMaintain, 20.0f);
	EXPECT_TRUE(rules.maintained);
	EXPECT_TRUE(rules.recharged);
	EXPECT_TRUE(rules.divideCostsByTribalPower);
	EXPECT_FALSE(ChantRulesFor(MagicType::LightningBolt, magic, effect).maintained);
}

TEST(SpellChantRules, safetyLevelIsFiveSecondsOfUpkeep)
{
	SpellChants spell;
	SetChants(spell, 5000.0f);
	FakeRulesCaster caster(false);
	// 50 a turn x 10 turns a second x 5 seconds
	EXPECT_FLOAT_EQ(GetChantSafetyLevel(spell, LightningChantRules()), 2500.0f);
	EXPECT_FLOAT_EQ(GetSpellStrength(spell, LightningChantRules(), &caster), 1.0f);
	// at most what it was filled with
	SetChants(spell, 1000.0f);
	EXPECT_FLOAT_EQ(GetChantSafetyLevel(spell, LightningChantRules()), 1000.0f);
	// at least one event's cost
	auto rules = LightningChantRules();
	rules.costToMaintain = 0.01f;
	rules.costPerEvent = 3.0f;
	EXPECT_FLOAT_EQ(GetChantSafetyLevel(spell, rules), 3.0f);
	// slower turns mean fewer turns a second
	rules = LightningChantRules();
	rules.turnDuration = std::chrono::milliseconds(300);
	SetChants(spell, 5000.0f);
	EXPECT_FLOAT_EQ(GetChantSafetyLevel(spell, rules), 50.0f * 3.0f * k_ChantSafetySeconds);
}

TEST(SpellChantRules, playerCastLivesOnItsStore)
{
	// Nothing refills a hand cast: full strength above 2500, weaker below, ended by its 6 second timer
	SpellChants spell;
	SetChants(spell, 5000.0f);
	FakeRulesCaster caster(false);
	const auto rules = LightningChantRules();
	float age = 0.0f;
	int turns = 0;
	for (; turns < 100; ++turns)
	{
		const float before = spell.chants;
		const auto turn = RunTurn(spell, age, 6.0f, rules, caster);
		if (turn.closed)
		{
			break;
		}
		EXPECT_NEAR(turn.strength, before >= 2500.0f ? 1.0f : before / 2500.0f, 1e-5f) << "turn " << turns;
	}
	EXPECT_GE(turns, 59);
	EXPECT_LE(turns, 60);
	EXPECT_FLOAT_EQ(caster.given, 0.0f);
	EXPECT_NEAR(spell.chants, 5000.0f - static_cast<float>(turns) * 50.0f, 1e-2f);
	ASSERT_EQ(caster.spent.size(), static_cast<size_t>(turns));
	EXPECT_EQ(caster.spent.front().charge, ChantCharge::PerTurn);
}

TEST(SpellChantRules, refillingCasterHoldsTheSafetyLevel)
{
	SpellChants spell;
	SetChants(spell, 5000.0f);
	FakeRulesCaster caster(true);
	const auto rules = LightningChantRules();
	float age = 0.0f;
	for (int turn = 0; turn < 200; ++turn)
	{
		const auto result = RunTurn(spell, age, k_RulesNoTimeLimit, rules, caster);
		ASSERT_FALSE(result.closed);
		EXPECT_FLOAT_EQ(result.strength, 1.0f);
	}
	EXPECT_NEAR(spell.chants, 2500.0f, 1e-2f);
	EXPECT_GT(caster.given, 0.0f);
}

TEST(SpellChantRules, eventCost)
{
	SpellChants spell;
	SetChants(spell, 3000.0f);
	FakeRulesCaster caster(false);
	EXPECT_FLOAT_EQ(PayForOneEvent(spell, LightningChantRules(), &caster), 1.0f);
	EXPECT_FLOAT_EQ(spell.chants, 2998.0f);
	ASSERT_EQ(caster.spent.size(), 1u);
	EXPECT_EQ(caster.spent[0].charge, ChantCharge::PerEvent);
	EXPECT_FLOAT_EQ(caster.spent[0].chants, 2.0f);
	// a free miracle pays nothing and has full strength
	spell.free = true;
	EXPECT_FLOAT_EQ(PayForOneEvent(spell, LightningChantRules(), &caster), 1.0f);
	EXPECT_FLOAT_EQ(spell.chants, 2998.0f);
}

TEST(SpellChantRules, refillUpToCostOrWhole)
{
	SpellChants spell;
	SetChants(spell, 5000.0f);
	spell.chants = 1000.0f; // 1500 under the safety level
	FakeRulesCaster caster(true);
	PayFor(spell, LightningChantRules(), &caster, 100.0f);
	EXPECT_FLOAT_EQ(spell.chants, 1000.0f); // paid 100, refilled 100
	PayFor(spell, LightningChantRules(), &caster, 100.0f, Refill::Whole);
	EXPECT_FLOAT_EQ(spell.chants, 2500.0f); // refilled to the safety level
}

TEST(SpellChantRules, maintainedShield)
{
	// A maintained miracle's safety level is its whole store; a shield of twice its normal radius costs four times as
	// much a turn
	SpellChants spell;
	SetChants(spell, 5000.0f);
	FakeRulesCaster caster(false);
	auto rules = ShieldChantRules();
	rules.costToMaintain = 20.0f * 2.0f * 2.0f;
	EXPECT_FLOAT_EQ(GetChantSafetyLevel(spell, rules), 5000.0f);
	EXPECT_FLOAT_EQ(PayForOneTurn(spell, rules, &caster), (5000.0f - 80.0f) / 5000.0f);
	EXPECT_FLOAT_EQ(spell.chants, 4920.0f);
	// no time limit: only running out of prayer power ends it
	float age = 0.0f;
	bool closed = false;
	for (int turn = 0; turn < 1000 && !closed; ++turn)
	{
		closed = RunTurn(spell, age, k_RulesNoTimeLimit, rules, caster).closed;
	}
	EXPECT_TRUE(closed);
	EXPECT_LE(spell.chants, 0.0f);
}

TEST(SpellChantRules, tribalPowerDividesCost)
{
	SpellChants spell;
	SetChants(spell, 5000.0f);
	FakeRulesCaster caster(false);
	auto rules = ShieldChantRules();
	rules.tribalPower = 2.0f;
	PayForOneTurn(spell, rules, &caster);
	EXPECT_FLOAT_EQ(spell.chants, 4990.0f);
	// a tribal power under 1 does not make it dearer
	rules.tribalPower = 0.5f;
	PayForOneTurn(spell, rules, &caster);
	EXPECT_FLOAT_EQ(spell.chants, 4970.0f);
}

TEST(SpellChantRules, strengthEdges)
{
	SpellChants spell;
	FakeRulesCaster caster(false);
	SpellChantRules rules {};
	// no safety level: full strength with any prayer power, none without
	SetChants(spell, 10.0f);
	EXPECT_FLOAT_EQ(GetChantSafetyLevel(spell, rules), 0.0f);
	EXPECT_FLOAT_EQ(GetSpellStrength(spell, rules, &caster), 1.0f);
	spell.chants = 0.0f;
	EXPECT_FLOAT_EQ(GetSpellStrength(spell, rules, &caster), 0.0f);
	// no upkeep: a turn is free and full strength
	EXPECT_FLOAT_EQ(PayForOneTurn(spell, rules, &caster), 1.0f);
	EXPECT_TRUE(caster.spent.empty());
	// no caster: no strength and no payments
	spell.chants = 10.0f;
	EXPECT_FLOAT_EQ(GetSpellStrength(spell, rules, nullptr), 0.0f);
	EXPECT_FLOAT_EQ(PayFor(spell, rules, nullptr, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(PayForOneEvent(spell, rules, nullptr), 0.0f);
	EXPECT_FLOAT_EQ(spell.chants, 10.0f);
	// the multipliers
	rules.tribalPower = 1.5f;
	rules.seedPower = 0.5f;
	spell.strengthMultiplier = 2.0f;
	EXPECT_FLOAT_EQ(GetSpellStrength(spell, rules, &caster), 1.5f);
}
