/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' mist natives as a script calls them: CREATE_MIST makes a bank of mist with the colour, scale and edge
// shrink it was given, as a thing the script created, and SET_MIST_FADE sets its scale and alpha at once and moves
// them a step each game turn (ECS/MistFades.h)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <bit>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/MistFade.h"
#include "ECS/Components/Transform.h"
#include "ECS/MistFades.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The scripts' table of objects, as far as the natives reach it: what each Register was given
class MistRegisteredThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		registered.emplace_back(object, createdByScript);
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

	std::vector<std::pair<entt::entity, bool>> registered;
};

class ScriptMistNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id
		registry.Create();
		Locator::vm::emplace<lhvm::LHVM>();
		_things = &static_cast<MistRegisteredThings&>(Locator::scriptObjects::emplace<MistRegisteredThings>());
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	const lhvm::NativeFunction* Native(std::string_view name)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		return native == table.end() ? nullptr : &*native;
	}

	/// CREATE_MIST as the twinkle tune calls it: the point, the scale, red, green, blue, the transparency and the
	/// height ratio on top. The mist it gives, the only value it leaves above the script's
	entt::entity CreateMist(float transparency, float heightRatio)
	{
		const auto* native = Native("CREATE_MIST");
		if (native == nullptr)
		{
			return entt::null;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pushf(2106.78f);
		vm.Pushf(9.597f);
		vm.Pushf(2997.93f);
		vm.Pushf(0.8f);
		vm.Pushf(255.0f);
		vm.Pushf(128.0f);
		vm.Pushf(64.0f);
		vm.Pushf(transparency);
		vm.Pushf(heightRatio);
		native->impl();
		const auto mist = static_cast<entt::entity>(vm.Pop().uintVal);
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "CREATE_MIST left more than one value";
		return mist;
	}

	/// SET_MIST_FADE: the mist, the start and end scale, the start and end transparency, the seconds on top
	void SetMistFade(uint32_t mist, float seconds)
	{
		const auto* native = Native("SET_MIST_FADE");
		if (native == nullptr)
		{
			return;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pusho(mist);
		vm.Pushf(1.0f);
		vm.Pushf(2.0f);
		vm.Pushf(50.0f);
		vm.Pushf(0.0f);
		vm.Pushf(seconds);
		native->impl();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "SET_MIST_FADE took or left a value";
	}

	/// GET_OBJECT_FADE: the mist in, its fade out
	float GetObjectFade(uint32_t mist)
	{
		const auto* native = Native("GET_OBJECT_FADE");
		if (native == nullptr)
		{
			return -1.0f;
		}
		EXPECT_EQ(native->stackIn, 1);
		EXPECT_EQ(native->stackOut, 1u);
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pusho(mist);
		native->impl();
		const float fade = vm.Popf();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "GET_OBJECT_FADE took or left more than one value";
		return fade;
	}

	static constexpr int32_t k_ScriptsOwn = 77;
	chlapi::CHLApi _api;
	MistRegisteredThings* _things {nullptr};
};
} // namespace

TEST_F(ScriptMistNatives, ScriptMistHasTheColourScaleAndEdgeShrinkItWasGiven)
{
	const auto mist = CreateMist(0.1f, 0.6f);
	ASSERT_TRUE(Reg().Valid(mist));
	const auto& bank = Reg().Get<const Mist>(mist);
	// the transparency x 255 truncated (25.5 -> 25) as the alpha, each channel a byte
	EXPECT_EQ(bank.colour, 0x19FF8040U);
	EXPECT_EQ(bank.size, 0.8f);
	EXPECT_TRUE(bank.edgeShrink);
	EXPECT_EQ(bank.k, 1.0f / 0.6f);
	// without a land the height above it is the point's
	const auto& transform = Reg().Get<const Transform>(mist);
	EXPECT_NEAR(transform.position.x, 2106.78f, 0.001f);
	EXPECT_NEAR(transform.position.y, 9.597f, 0.001f);
	EXPECT_NEAR(transform.position.z, 2997.93f, 0.001f);
	const std::vector<std::pair<entt::entity, bool>> expected {{mist, true}};
	EXPECT_EQ(_things->registered, expected);

	// a height ratio of 1 gives no edge shrink
	EXPECT_FALSE(Reg().Get<const Mist>(CreateMist(0.1f, 1.0f)).edgeShrink);
}

TEST_F(ScriptMistNatives, ScriptMistFadeStartsAtOnceAndMovesATurnAtATime)
{
	const auto mist = CreateMist(0.1f, 0.6f);
	SetMistFade(static_cast<uint32_t>(mist), 10.0f);
	// the start scale and alpha at once, the colour kept
	EXPECT_EQ(Reg().Get<const Mist>(mist).size, 1.0f);
	EXPECT_EQ(Reg().Get<const Mist>(mist).colour, 0x32FF8040U);
	ASSERT_TRUE(Reg().AllOf<MistFade>(mist));

	// one turn: one step of the hundred
	ecs::mist_fade::ProcessTurn();
	EXPECT_EQ(Reg().Get<const Mist>(mist).colour >> 24U, 49u);
	for (int turn = 1; turn < 100; ++turn)
	{
		ecs::mist_fade::ProcessTurn();
	}
	EXPECT_EQ(Reg().Get<const Mist>(mist).colour, 0x00FF8040U);
	EXPECT_NEAR(Reg().Get<const Mist>(mist).size, 2.0f, 1e-4f);
	// the next turn ends the fade; the mist stays as it ended, and so does its fade, every step taken
	ecs::mist_fade::ProcessTurn();
	EXPECT_TRUE(Reg().Valid(mist));
	EXPECT_EQ(Reg().Get<const Mist>(mist).colour, 0x00FF8040U);
	ASSERT_TRUE(Reg().AllOf<MistFade>(mist));
	EXPECT_EQ(Reg().Get<const MistFade>(mist).taken, Reg().Get<const MistFade>(mist).steps);
	ecs::mist_fade::ProcessTurn();
	EXPECT_EQ(Reg().Get<const Mist>(mist).colour, 0x00FF8040U);
}

TEST_F(ScriptMistNatives, ScriptObjectFadeIsTheStepsTakenOverTheSteps)
{
	const auto mist = CreateMist(0.1f, 0.6f);
	// never faded: nought steps of nought, the processor's not-a-number of nought over nought
	EXPECT_EQ(std::bit_cast<uint32_t>(GetObjectFade(static_cast<uint32_t>(mist))), 0xFFC00000U);
	SetMistFade(static_cast<uint32_t>(mist), 10.0f);
	EXPECT_EQ(GetObjectFade(static_cast<uint32_t>(mist)), 0.0f);
	for (int turn = 0; turn < 25; ++turn)
	{
		ecs::mist_fade::ProcessTurn();
	}
	EXPECT_EQ(GetObjectFade(static_cast<uint32_t>(mist)), 25.0f / 100.0f);
	// over, it stays at its end
	for (int turn = 0; turn < 90; ++turn)
	{
		ecs::mist_fade::ProcessTurn();
	}
	EXPECT_EQ(GetObjectFade(static_cast<uint32_t>(mist)), 1.0f);
	// a fade of 2.55 seconds ends at 25 steps of 25.5
	SetMistFade(static_cast<uint32_t>(mist), 2.55f);
	for (int turn = 0; turn < 30; ++turn)
	{
		ecs::mist_fade::ProcessTurn();
	}
	const float steps = 2.55f * 10.0f;
	EXPECT_EQ(GetObjectFade(static_cast<uint32_t>(mist)), 25.0f / steps);
}

TEST_F(ScriptMistNatives, ScriptObjectFadeOfWhatIsNotAMistIsOne)
{
	const auto rock = Reg().Create();
	EXPECT_EQ(GetObjectFade(static_cast<uint32_t>(rock)), 1.0f);
	EXPECT_EQ(GetObjectFade(0), 1.0f);
	EXPECT_EQ(GetObjectFade(4000), 1.0f);
}

TEST_F(ScriptMistNatives, ScriptMistFadeOfWhatIsNotAMistDoesNothing)
{
	const auto rock = Reg().Create();
	SetMistFade(static_cast<uint32_t>(rock), 10.0f);
	SetMistFade(0, 10.0f);
	EXPECT_FALSE(Reg().AllOf<MistFade>(rock));
	// no fade going on: a turn makes no fade storage
	ecs::mist_fade::ProcessTurn();
	EXPECT_EQ(std::as_const(Reg()).Size<MistFade>(), 0u);
}
