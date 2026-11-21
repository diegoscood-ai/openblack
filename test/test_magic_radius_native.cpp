/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// SET_MAGIC_RADIUS, as a script calls it: a spell's radius, its magnitude, is set and its effect is asked for to take
// it too, through a fake particle system that records what it is asked (a registry only: no files).
// docs/wiki/scripts/script-commands.md#set_magic_radius-230

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// Records every effect it is asked for
class FindRecordingParticles final: public test::InertParticleSystem
{
public:
	[[nodiscard]] particles::Effect* Find(EffectId id) override
	{
		found.push_back(id);
		return nullptr;
	}
	std::vector<EffectId> found;
};

class MagicRadiusNative: public ::testing::Test
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
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id: the test's first entity must not be a scripted one
		registry.Create();
		_particles = &static_cast<FindRecordingParticles&>(Locator::particleSystem::emplace<FindRecordingParticles>());
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The native as a script calls it: the spell pushed first, then the radius; a value under them must stay
	void Call(uint32_t object, float radius)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_MAGIC_RADIUS"; });
		ASSERT_NE(native, table.end());
		ASSERT_EQ(native->stackIn, 2);
		ASSERT_EQ(native->stackOut, 0u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		vm.Pusho(object);
		vm.Pushf(radius);
		native->impl();
		EXPECT_EQ(vm.Pop().intVal, 77);
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	chlapi::CHLApi _api;
	FindRecordingParticles* _particles {nullptr};
};

uint32_t Id(entt::entity thing)
{
	return static_cast<uint32_t>(thing);
}
} // namespace

TEST_F(MagicRadiusNative, TheSpellsRadiusAndItsEffectsAreSet)
{
	const auto spell = Reg().Create();
	Reg().Assign<ecs::components::Spell>(spell, ecs::components::Spell {.effect = 9, .magnitude = 105.0f});
	Call(Id(spell), 70.0f);
	EXPECT_EQ(Reg().Get<ecs::components::Spell>(spell).magnitude, 70.0f);
	EXPECT_EQ(_particles->found, std::vector<uint32_t> {9});
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

TEST_F(MagicRadiusNative, ASpellWithNoEffectHasOnlyItsRadiusSet)
{
	const auto spell = Reg().Create();
	Reg().Assign<ecs::components::Spell>(spell, ecs::components::Spell {.magnitude = 35.0f});
	Call(Id(spell), 0.0f);
	EXPECT_EQ(Reg().Get<ecs::components::Spell>(spell).magnitude, 0.0f);
	EXPECT_TRUE(_particles->found.empty());
}

TEST_F(MagicRadiusNative, AnythingButASpellIsLeftAlone)
{
	// "Thing not valid": no thing, or one that is gone; a thing that is no spell is untouched
	const auto rock = Reg().Create();
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	Call(0, 10.0f);
	Call(Id(gone), 10.0f);
	Call(Id(rock), 10.0f);
	EXPECT_FALSE(Reg().AllOf<ecs::components::Spell>(rock));
	EXPECT_TRUE(_particles->found.empty());
}
