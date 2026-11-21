/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ADD_SPOT_VISUAL_TARGET_OBJECT and ADD_SPOT_VISUAL_TARGET_POS, as a script calls them: the spot visual's effect is
// given the object or the point to act on, such as the thing or the place a beam ends on, through a fake particle
// system that records what it is handed (a registry only: no files). docs/wiki/physics/particles.md#the-original

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// Records every target given to a spot visual
class TargetRecordingParticles final: public test::InertParticleSystem
{
public:
	void AddSpotVisualTargetObject(entt::entity visual, entt::entity target) override { targets.emplace_back(visual, target); }
	void AddSpotVisualTarget(entt::entity visual, glm::vec3 position) override { points.emplace_back(visual, position); }
	std::vector<std::pair<entt::entity, entt::entity>> targets;
	std::vector<std::pair<entt::entity, glm::vec3>> points;
};

class SpotVisualTargetNative: public ::testing::Test
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
		_particles = &static_cast<TargetRecordingParticles&>(Locator::particleSystem::emplace<TargetRecordingParticles>());
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The native as a script calls it: the visual pushed first, then the target; it gives back nothing
	void Call(uint32_t visual, uint32_t target)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "ADD_SPOT_VISUAL_TARGET_OBJECT"; });
		ASSERT_NE(native, table.end());
		ASSERT_EQ(native->stackIn, 2);
		ASSERT_EQ(native->stackOut, 0u);
		auto& vm = Locator::vm::value();
		vm.Pusho(visual);
		vm.Pusho(target);
		native->impl();
	}

	/// ADD_SPOT_VISUAL_TARGET_POS as a script calls it: the visual pushed first, then the point's three values
	void CallPos(uint32_t visual, glm::vec3 point)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "ADD_SPOT_VISUAL_TARGET_POS"; });
		ASSERT_NE(native, table.end());
		ASSERT_EQ(native->stackIn, 4);
		ASSERT_EQ(native->stackOut, 0u);
		auto& vm = Locator::vm::value();
		vm.Pusho(visual);
		vm.Pushf(point.x);
		vm.Pushf(point.y);
		vm.Pushf(point.z);
		native->impl();
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	chlapi::CHLApi _api;
	TargetRecordingParticles* _particles {nullptr};
};

uint32_t Id(entt::entity thing)
{
	return static_cast<uint32_t>(thing);
}
} // namespace

TEST_F(SpotVisualTargetNative, TheVisualsEffectIsGivenTheTarget)
{
	const auto visual = Reg().Create();
	const auto target = Reg().Create();
	Call(Id(visual), Id(target));
	const std::vector<std::pair<entt::entity, entt::entity>> k_Given {{visual, target}};
	EXPECT_EQ(_particles->targets, k_Given);
}

TEST_F(SpotVisualTargetNative, NothingIsGivenWithoutAVisual)
{
	// "Thing not valid": no visual, or one that is gone
	const auto target = Reg().Create();
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	Call(0, Id(target));
	Call(Id(gone), Id(target));
	EXPECT_TRUE(_particles->targets.empty());
}

TEST_F(SpotVisualTargetNative, ATargetThatIsNotThereIsSkipped)
{
	const auto visual = Reg().Create();
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	Call(Id(visual), 0);
	Call(Id(visual), Id(gone));
	EXPECT_TRUE(_particles->targets.empty());
}

TEST_F(SpotVisualTargetNative, TheVisualsEffectIsGivenThePoint)
{
	// a value under the native's four, which it must leave there
	Locator::vm::value().Pushi(77);
	const auto visual = Reg().Create();
	CallPos(Id(visual), {1.5F, 105.0F, -2.0F});
	ASSERT_EQ(_particles->points.size(), 1u);
	EXPECT_EQ(_particles->points[0].first, visual);
	EXPECT_EQ(_particles->points[0].second, glm::vec3(1.5F, 105.0F, -2.0F));
	EXPECT_EQ(Locator::vm::value().Pop().intVal, 77);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

TEST_F(SpotVisualTargetNative, NoPointIsGivenWithoutAVisual)
{
	// "Thing not valid": no visual, or one that is gone; the point is popped all the same
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	Locator::vm::value().Pushi(77);
	CallPos(0, {1.0F, 2.0F, 3.0F});
	CallPos(Id(gone), {1.0F, 2.0F, 3.0F});
	EXPECT_TRUE(_particles->points.empty());
	EXPECT_EQ(Locator::vm::value().Pop().intVal, 77);
}
