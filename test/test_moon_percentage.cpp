/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// GET_MOON_PERCENTAGE, as a script calls it: how full the moon was the last time it showed, read from a fake sky that
// has only the moon's entity. docs/wiki/world/sky.md#sky-sun-moon-and-clouds-original

#include <cstdlib>

#include <algorithm>
#include <string_view>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "CHLApi.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "Graphics/Moon.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// A sky with no meshes or textures: only its moon's entity is read
class MoonOnlySky final: public ecs::systems::SkySystemInterface
{
public:
	explicit MoonOnlySky(entt::entity moon)
	    : _moon(moon)
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
	[[nodiscard]] entt::entity GetSun() const override { return entt::null; }
	[[nodiscard]] entt::entity GetMoon() const override { return _moon; }

private:
	float _time {12.0f};
	DayNightClock _clock;
	entt::entity _moon;
};

class GetMoonPercentage: public ::testing::Test
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
		const auto moon = registry.Create();
		_moon = &registry.AssignState<ecs::components::Moon>(moon);
		Locator::skySystem::emplace<MoonOnlySky>(moon);
	}

	/// The native as a script calls it: it takes nothing and gives back one float
	float Call()
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "GET_MOON_PERCENTAGE"; });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return -1.0f;
		}
		EXPECT_EQ(native->stackIn, 0);
		EXPECT_EQ(native->stackOut, 1u);
		native->impl();
		return Locator::vm::value().Popf();
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::skySystem> _restoreSky;
	chlapi::CHLApi _api;
	ecs::components::Moon* _moon {nullptr};
};
} // namespace

TEST_F(GetMoonPercentage, IsANewMoonBeforeTheMoonFirstShows)
{
	// The phase the moon last showed with starts at 0, which scripts are told as 1
	EXPECT_EQ(ecs::components::Moon {}.phase, 0.0f);
	EXPECT_EQ(Call(), 1.0f);
}

TEST_F(GetMoonPercentage, FollowsThePhaseTheMoonLastShowedWith)
{
	// The original's constants: pi and 1 / pi as floats
	constexpr float k_HalfTurn = 3.14159274f;
	constexpr float k_OverHalfTurn = 0.318309873f;
	// Waxing to the full moon, half way through the month, 1 down to 0; waning from it, 0 back up to 1
	for (const float phase : {0.5f, 1.5707964f, 3.0f, k_HalfTurn, 3.5f, 4.712389f, 6.0f, 6.2831855f})
	{
		_moon->phase = phase;
		const float expected = phase < k_HalfTurn ? 1.0f - phase * k_OverHalfTurn : (phase - k_HalfTurn) * k_OverHalfTurn;
		EXPECT_EQ(Call(), expected) << phase;
		EXPECT_EQ(Call(), graphics::moon::ScriptPercentage(phase)) << phase;
	}
	// A full moon is 0, and a quarter either side a half
	_moon->phase = k_HalfTurn;
	EXPECT_NEAR(Call(), 0.0f, 1e-6f);
	_moon->phase = 1.5707964f;
	EXPECT_NEAR(Call(), 0.5f, 1e-6f);
	_moon->phase = 4.712389f;
	EXPECT_NEAR(Call(), 0.5f, 1e-6f);
}
