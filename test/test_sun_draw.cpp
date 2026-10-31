/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// SET_SUN_DRAW, the scripts' switch that hides the sun and its glare, kept on the sun's entity, and what shows the sun
// again. docs/wiki/world/sky.md#the-sun-and-its-glare

#include <cstdlib>

#include <algorithm>
#include <bit>
#include <string_view>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "CHLApi.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/SunDraw.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// A sky with no meshes or textures: only its sun's entity is read
class SunOnlySky final: public ecs::systems::SkySystemInterface
{
public:
	explicit SunOnlySky(entt::entity sun)
	    : _sun(sun)
	{
	}
	void Initialize() override {}
	void KeepForNextLand() override {}
	void ProcessTurn() override {}
	void UpdateFrame(bool /*skyDrawn*/) override {}
	void SetTime(float time) override { _time = time; }
	[[nodiscard]] float GetTime() const override { return _time; }
	[[nodiscard]] DayNightTimes GetDayNightTimes() const override { return _clock.GetDayNightTimes(); }
	[[nodiscard]] DayNightClock& GetClock() override { return _clock; }
	[[nodiscard]] const DayNightClock& GetClock() const override { return _clock; }
	[[nodiscard]] graphics::L3DMesh& GetMesh() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::Texture2D& GetTexture() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::L3DMesh& GetSunMesh() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::L3DMesh& GetMoonMesh() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::L3DMesh& GetCloudMesh() const noexcept override { std::abort(); }
	[[nodiscard]] entt::entity GetDome() const override { return entt::null; }
	[[nodiscard]] entt::entity GetSun() const override { return _sun; }
	[[nodiscard]] entt::entity GetMoon() const override { return entt::null; }

private:
	float _time {12.0f};
	DayNightClock _clock;
	entt::entity _sun;
};

class SetSunDraw: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_sun = registry.Create();
		registry.AssignState<ecs::components::Sun>(_sun);
		Locator::skySystem::emplace<SunOnlySky>(_sun);
	}

	/// The native as a script calls it: one value in, nothing back
	void Call(uint32_t value)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_SUN_DRAW"; });
		ASSERT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 1);
		EXPECT_EQ(native->stackOut, 0u);
		// a value left below shows that the native took exactly one
		Locator::vm::value().Pushf(7.0f);
		Locator::vm::value().Pushf(std::bit_cast<float>(value));
		native->impl();
		EXPECT_EQ(Locator::vm::value().Popf(), 7.0f);
	}

	[[nodiscard]] bool Hidden() const { return ecs::sun_draw::IsHidden(Locator::entitiesRegistry::value(), _sun); }

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::skySystem> _restoreSky;
	chlapi::CHLApi _api;
	entt::entity _sun {entt::null};
};
} // namespace

TEST_F(SetSunDraw, AnyValueButNoughtHidesTheSunAndNoughtShowsIt)
{
	EXPECT_FALSE(Hidden());
	// a script's true, as an integer or as the float 1.0, and any other bits but nought
	Call(1U);
	EXPECT_TRUE(Hidden());
	Call(0U);
	EXPECT_FALSE(Hidden());
	Call(std::bit_cast<uint32_t>(1.0f));
	EXPECT_TRUE(Hidden());
	Call(0x80000000U);
	EXPECT_TRUE(Hidden());
	Call(0U);
	EXPECT_FALSE(Hidden());
}

TEST(SunDraw, ShowsAgainWhenTheScriptsRestartOrTheTempleIsDrawn)
{
	ecs::Registry registry;
	const auto sun = registry.Create();
	registry.AssignState<ecs::components::Sun>(sun);
	ecs::sun_draw::SetFromScript(registry, sun, 1U);
	EXPECT_TRUE(ecs::sun_draw::IsHidden(registry, sun));
	ecs::sun_draw::ShowAgain(registry, sun);
	EXPECT_FALSE(ecs::sun_draw::IsHidden(registry, sun));
}

TEST(SunDraw, WithoutASunNothingIsHidden)
{
	ecs::Registry registry;
	const auto notTheSun = registry.Create();
	ecs::sun_draw::SetFromScript(registry, notTheSun, 1U);
	EXPECT_FALSE(ecs::sun_draw::IsHidden(registry, notTheSun));
	EXPECT_FALSE(ecs::sun_draw::IsHidden(registry, entt::null));
	ecs::sun_draw::ShowAgain(registry, entt::null);
}
