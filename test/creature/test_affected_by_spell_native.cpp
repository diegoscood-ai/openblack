/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// IS_AFFECTED_BY_SPELL, as a script calls it: the creature and a magic type, answered by whether the creature spell
// the magic type's table row casts is on the creature, one still waiting to start too.
// docs/wiki/scripts/script-commands.md#is_affected_by_spell-385

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <tuple>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Registry.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
using creature_spells::Phase;
using creature_spells::Spell;

/// The magic types of the invisible and freeze spells, and a miracle that casts no creature spell
constexpr int32_t k_InvisibleMagic = static_cast<int32_t>(MagicType::CreatureSpellInvisible);
constexpr int32_t k_FreezeMagic = static_cast<int32_t>(MagicType::CreatureSpellFreeze);
constexpr int32_t k_FireballMagic = static_cast<int32_t>(MagicType::Fireball);

class AffectedBySpellNative: public ::testing::Test
{
protected:
	AffectedBySpellNative()
	    : _vm(&Locator::vm::emplace<lhvm::LHVM>())
	{
		// the natives log their errors to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::chlapi::emplace<chlapi::CHLApi>();
		// the creature spells' rows name the spell each casts, as info.dat does: in their order
		auto& rows = _world.Info().magicCreatureSpell;
		for (size_t i = 0; i < rows.size(); ++i)
		{
			rows.at(i).creatureReceiveSpellType = static_cast<CreatureReceiveSpellType>(i);
		}
		// a script's object 0 is no thing, so the first entity is not one the tests name
		std::ignore = test::creature_world::World::Registry().Create();
	}

	/// The native's answer, the creature pushed first, then the magic type; a value under them must stay
	bool Call(uint32_t object, int32_t magicType)
	{
		const auto& table = Locator::chlapi::value().GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "IS_AFFECTED_BY_SPELL"; });
		EXPECT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 2);
		EXPECT_EQ(native->stackOut, 1u);
		_vm->Pushi(77);
		_vm->Pusho(object);
		_vm->Pushi(magicType);
		native->impl();
		const bool answer = _vm->Pop().intVal != 0;
		EXPECT_EQ(_vm->Pop().intVal, 77);
		return answer;
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }

	static void SetPhase(entt::entity creature, Spell spell, Phase phase)
	{
		auto& registry = test::creature_world::World::Registry();
		if (!registry.AllOf<ecs::components::CreatureSpells>(creature))
		{
			registry.AssignState<ecs::components::CreatureSpells>(creature);
		}
		registry.Get<ecs::components::CreatureSpells>(creature).spells[spell].phase = phase;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::chlapi> _restoreApi;
	lhvm::LHVM* _vm;
};
} // namespace

TEST_F(AffectedBySpellNative, AnyPartOfTheSpellOnTheCreatureCounts)
{
	const auto creature = test::creature_world::World::MakeCreature();
	SetPhase(creature, Spell::Invisible, Phase::Off);
	EXPECT_FALSE(Call(Id(creature), k_InvisibleMagic));
	for (const auto phase : {Phase::Waiting, Phase::Starting, Phase::Holding, Phase::Finishing})
	{
		SetPhase(creature, Spell::Invisible, phase);
		EXPECT_TRUE(Call(Id(creature), k_InvisibleMagic)) << static_cast<int>(phase);
	}
	// another spell on it is not the one asked
	EXPECT_FALSE(Call(Id(creature), k_FreezeMagic));
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}

TEST_F(AffectedBySpellNative, TheSpellIsTheOneTheMagicTypesRowCasts)
{
	const auto creature = test::creature_world::World::MakeCreature();
	// the freeze row made to cast the invisible spell: asking for freeze finds the invisible spell on
	_world.Info().magicCreatureSpell.at(0).creatureReceiveSpellType = CreatureReceiveSpellType::CreatureReceiveSpellInvisible;
	SetPhase(creature, Spell::Invisible, Phase::Holding);
	EXPECT_TRUE(Call(Id(creature), k_FreezeMagic));
}

TEST_F(AffectedBySpellNative, AMiracleThatCastsNoCreatureSpellIsFalse)
{
	const auto creature = test::creature_world::World::MakeCreature();
	SetPhase(creature, Spell::Freeze, Phase::Holding);
	EXPECT_FALSE(Call(Id(creature), k_FireballMagic));
	EXPECT_FALSE(Call(Id(creature), -1));
	EXPECT_FALSE(Call(Id(creature), 42));
}

TEST_F(AffectedBySpellNative, NoThingOrNoCreatureIsFalse)
{
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	const auto gone = registry.Create();
	registry.Destroy(gone);
	EXPECT_FALSE(Call(0, k_InvisibleMagic));
	EXPECT_FALSE(Call(Id(gone), k_InvisibleMagic));
	EXPECT_FALSE(Call(Id(rock), k_InvisibleMagic));
}
