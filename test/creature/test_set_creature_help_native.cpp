/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// SET_CREATURE_HELP, as a script calls it: the value it pops is the creature help option, the switch for the advisor's
// messages about the player's creature. docs/wiki/project/progress/creature/lessons_and_help.md

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Creature/CreatureHelp.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// Keeps the switch and every time it is set, and nothing else
class SwitchedHelp final: public ecs::systems::CreatureHelpSystemInterface
{
public:
	void Raise(entt::entity /*creature*/, const creature_help::Message& /*message*/) override {}
	void ProcessTurn() override {}
	void PromptKnown(entt::entity /*creature*/, creature_help::Prompt /*prompt*/) override {}
	void Fainting(entt::entity /*creature*/) override {}
	void SetHelpOn(bool on) override { set.push_back(on); }
	[[nodiscard]] bool IsHelpOn() const override { return set.empty() || set.back(); }
	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

	std::vector<bool> set;

private:
	std::vector<Told> _told;
};

class SetCreatureHelpNative: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
	}

	/// The native called on a value, with another under it that it must leave there
	void Call(int32_t value)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_CREATURE_HELP"; });
		ASSERT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 1);
		EXPECT_EQ(native->stackOut, 0u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		vm.Pushi(value);
		native->impl();
		EXPECT_EQ(vm.Pop().intVal, 77);
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::creatureHelpSystem> _restoreHelp;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(SetCreatureHelpNative, ItSetsTheCreatureHelpSwitch)
{
	auto& help = static_cast<SwitchedHelp&>(Locator::creatureHelpSystem::emplace<SwitchedHelp>());
	Call(0);
	EXPECT_EQ(help.set, (std::vector<bool> {false}));
	EXPECT_FALSE(help.IsHelpOn());
	Call(1);
	EXPECT_EQ(help.set, (std::vector<bool> {false, true}));
	EXPECT_TRUE(help.IsHelpOn());
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

TEST_F(SetCreatureHelpNative, WithNoCreatureHelpItOnlyPopsTheValue)
{
	Locator::creatureHelpSystem::reset();
	Call(1);
}
