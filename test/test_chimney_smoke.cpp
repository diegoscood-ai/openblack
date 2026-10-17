/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/ChimneySmoke.h"

using namespace openblack;
using Smoke = ecs::components::ChimneySmoke;

namespace
{
chimney_smoke::Random Fixed(float value)
{
	return [value](float, float) { return value; };
}
} // namespace

TEST(ChimneySmoke, StartsUnseenWithAgesSpreadOut)
{
	const auto smoke = chimney_smoke::Create({10.0f, 20.0f, 30.0f}, chimney_smoke::k_HomeSmoke, Fixed(3.0f));
	EXPECT_EQ(smoke.rgb, 0xFFFFFFu);
	EXPECT_EQ(smoke.chimney, glm::vec3(10.0f, 20.0f, 30.0f));
	for (size_t i = 0; i < Smoke::k_Puffs; ++i)
	{
		EXPECT_TRUE(smoke.puffs[i].hidden);
		EXPECT_EQ(smoke.puffs[i].age, static_cast<int32_t>(i) * 90);
		EXPECT_FLOAT_EQ(smoke.puffs[i].angle, 3.0f);
		// 3 is odd: clockwise
		EXPECT_TRUE(smoke.puffs[i].clockwise);
		// Stacked over the origin, not over the chimney (unlike raffclar's): the first new life moves them there
		EXPECT_EQ(smoke.puffs[i].position, glm::vec3(0.0f, static_cast<float>(i) * 0.5f, 0.0f));
	}
}

TEST(ChimneySmoke, DrawsTheAngleThenTheSpinOfEachPuff)
{
	std::vector<std::pair<float, float>> asked;
	const auto smoke = chimney_smoke::Create({}, chimney_smoke::k_WorkshopSmoke, [&asked](float a, float b) {
		asked.emplace_back(a, b);
		return 2.0f;
	});
	EXPECT_EQ(smoke.rgb, 0x808080u);
	ASSERT_EQ(asked.size(), 2 * Smoke::k_Puffs);
	EXPECT_FLOAT_EQ(asked[0].first, 0.0f);
	EXPECT_FLOAT_EQ(asked[0].second, glm::pi<float>());
	EXPECT_EQ(asked[1], std::make_pair(1.0f, 100.0f));
	// 2 is even: the other way
	EXPECT_FALSE(smoke.puffs[0].clockwise);
}

TEST(ChimneySmoke, LitWhileSomeoneIsHome)
{
	Smoke smoke;
	EXPECT_TRUE(chimney_smoke::UpdateState(smoke, true));
	EXPECT_EQ(smoke.state, Smoke::State::Smoking);
	EXPECT_TRUE(chimney_smoke::UpdateState(smoke, false));
	EXPECT_EQ(smoke.state, Smoke::State::Dying);
	smoke.state = Smoke::State::Out;
	EXPECT_FALSE(chimney_smoke::UpdateState(smoke, false));
	// Lit again, it smokes again
	EXPECT_TRUE(chimney_smoke::UpdateState(smoke, true));
}

TEST(ChimneySmoke, PuffsRiseAndStartAgainAtTheChimney)
{
	auto smoke = chimney_smoke::Create({0.0f, 100.0f, 0.0f}, chimney_smoke::k_HomeSmoke, Fixed(0.0f));
	smoke.puffs[0].hidden = false;
	smoke.puffs[0].position = {0.0f, 110.0f, 0.0f};
	smoke.puffs[0].age = 100;
	smoke.puffs[9].age = 890;
	// A second with no drift: 255 steps of age, 2.55 up
	chimney_smoke::Advance(smoke, 1000.0f, glm::vec3(0.0f));
	EXPECT_EQ(smoke.puffs[0].age, 355);
	EXPECT_NEAR(smoke.puffs[0].position.y, 112.55f, 1e-4f);
	// The last passed the end of its life: back at the chimney with what was left, risen for that long, seen
	EXPECT_EQ(smoke.puffs[9].age, (890 + 255) % 900);
	EXPECT_NEAR(smoke.puffs[9].position.y, 100.0f + (2.55f * 245.0f / 255.0f), 1e-4f);
	EXPECT_FALSE(smoke.puffs[9].hidden);
}

TEST(ChimneySmoke, KeepsThePartOfAStepLeftOver)
{
	auto smoke = chimney_smoke::Create({}, chimney_smoke::k_HomeSmoke, Fixed(0.0f));
	// 10 ms is 2.55 steps: 2 now, the rest carried over
	chimney_smoke::Advance(smoke, 10.0f, glm::vec3(0.0f));
	EXPECT_EQ(smoke.puffs[0].age, 2);
	chimney_smoke::Advance(smoke, 10.0f, glm::vec3(0.0f));
	EXPECT_EQ(smoke.puffs[0].age, 5);
}

TEST(ChimneySmoke, DriftPushesThePuffs)
{
	auto smoke = chimney_smoke::Create({0.0f, 0.0f, 0.0f}, chimney_smoke::k_HomeSmoke, Fixed(0.0f));
	smoke.puffs[0].age = 0;
	chimney_smoke::Advance(smoke, 1000.0f, {2.0f, 0.0f, 0.0f});
	// v = 1.5 x 1 x 2 = 3, moved by the mean of 0 and 3
	EXPECT_NEAR(smoke.puffs[0].velocity.x, 3.0f, 1e-5f);
	EXPECT_NEAR(smoke.puffs[0].position.x, 1.5f, 1e-5f);
}

TEST(ChimneySmoke, DyingSmokeGoesOut)
{
	auto smoke = chimney_smoke::Create({0.0f, 0.0f, 0.0f}, chimney_smoke::k_HomeSmoke, Fixed(0.0f));
	smoke.state = Smoke::State::Dying;
	chimney_smoke::Advance(smoke, 10.0f, glm::vec3(0.0f));
	EXPECT_EQ(smoke.state, Smoke::State::Out);
}

TEST(ChimneySmoke, HandStirsTheSmokeNearby)
{
	const auto hand = chimney_smoke::WindOf({5.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 40.0f});
	// 50 x 0.1 = 5, at most 5
	EXPECT_FLOAT_EQ(hand.speed, 5.0f);
	EXPECT_NEAR(hand.wind.x, 3.0f, 1e-5f);
	EXPECT_NEAR(hand.wind.z, 4.0f, 1e-5f);
	EXPECT_EQ(chimney_smoke::Drift({0.0f, 0.0f, 0.0f}, hand, Fixed(1.0f)), hand.wind);
	// Far off, or slow, a random breath
	EXPECT_EQ(chimney_smoke::Drift({100.0f, 0.0f, 0.0f}, hand, Fixed(1.0f)), glm::vec3(1.0f, 0.0f, 1.0f));
	const auto slow = chimney_smoke::WindOf({5.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(slow.speed, 0.5f);
	EXPECT_EQ(chimney_smoke::Drift({0.0f, 0.0f, 0.0f}, slow, Fixed(2.0f)), glm::vec3(2.0f, 0.0f, 2.0f));
	// The hand's speed eases 60 % of the way to its motion a turn
	const auto velocity = chimney_smoke::EaseHandVelocity({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 100.0f);
	EXPECT_NEAR(velocity.x, 6.0f, 1e-5f);
}

TEST(ChimneySmoke, TheBreathDrawsZFirst)
{
	std::vector<float> values {1.0f, 2.0f};
	size_t next = 0;
	const auto drift =
	    chimney_smoke::Drift({}, chimney_smoke::HandWind {}, [&values, &next](float, float) { return values.at(next++); });
	EXPECT_EQ(drift, glm::vec3(2.0f, 0.0f, 1.0f));
}

TEST(ChimneySmoke, LooksGrowAndFade)
{
	const auto young = chimney_smoke::LookOf(0, 0xFFFFFF);
	EXPECT_EQ(young.frame, 0);
	EXPECT_FLOAT_EQ(young.halfWidth, 0.5f);
	EXPECT_EQ(young.argb, 0x4FFFFFFFu);
	const auto old = chimney_smoke::LookOf(675, 0x808080);
	EXPECT_EQ(old.frame, (675 * 45 / 900) & 15);
	EXPECT_FLOAT_EQ(old.halfWidth, 2.0f);
	// (225 - 675) x 79 / 675 + 79 = 27
	EXPECT_EQ(old.argb, (27u << 24u) | 0x808080u);
	EXPECT_EQ(chimney_smoke::LookOf(900, 0xFFFFFF).argb >> 24u, 0u);
}
