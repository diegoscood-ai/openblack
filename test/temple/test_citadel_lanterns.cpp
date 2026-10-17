/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple's two lanterns: made once, on the first frame the temple is drawn more than 7 % built, each with a town
// light before the next lamp is made, at the original's points in the temple's frame; not drawn while the temple is
// drawn less; never found under the cursor; gone with their lights at once when the temple goes. A fake light service
// records the calls.

#include <cmath>

#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/NightLightsState.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "Worship/CitadelLanterns.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace lanterns = openblack::worship::citadel_lanterns;

namespace
{
/// No entity, as an entity, for the comparisons
constexpr entt::entity k_None = entt::null;

class FakeLights final: public ecs::systems::VillageLightSystemInterface
{
public:
	struct Added
	{
		entt::entity lantern;
		glm::vec3 position;
		uint8_t type;
	};

	void Update(std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	void AddLight(entt::entity lantern, const glm::vec3& position, uint8_t type) override
	{
		added.push_back({lantern, position, type});
	}
	void RemoveLight(entt::entity lantern) override { removed.push_back(lantern); }
	[[nodiscard]] bool IsDark() const override { return false; }
	[[nodiscard]] night_lights::State& GetState() noexcept override { return *_state; }

	std::vector<Added> added;
	std::vector<entt::entity> removed;

private:
	night_lights::State* _state {nullptr};
};

/// A temple at `position` turned by `angle`, drawn `percent` built, with its entrance
entt::entity MakeHeart(ecs::Registry& registry, const glm::vec3& position, float angle, float percent)
{
	const auto heart = registry.Create();
	registry.Assign<Transform>(heart, position, affine::AngleY(angle), glm::vec3(1.0f));
	const auto entrance = registry.Create();
	registry.Assign<CitadelEntrance>(entrance, heart);
	auto& component = registry.Assign<CitadelHeart>(heart);
	component.entrance = entrance;
	component.drawPercent = percent;
	return heart;
}

const CitadelEntrance& EntranceOf(const ecs::Registry& registry, entt::entity heart)
{
	return registry.Get<const CitadelEntrance>(registry.Get<const CitadelHeart>(heart).entrance);
}

/// The point the original works out, with each product and sum rounded to a float in its order
glm::vec3 Expected(const Transform& heart, glm::vec2 offset)
{
	glm::vec3 out;
	for (int c = 0; c < 3; ++c)
	{
		const volatile float along = offset.y * heart.rotation[2][c];
		const volatile float across = offset.x * heart.rotation[0][c];
		const volatile float sum = along + across;
		out[c] = sum + heart.position[c];
	}
	return out;
}
} // namespace

TEST(CitadelLanterns, TheLampsComeOnlyAboveSevenPercentBuilt)
{
	EXPECT_FALSE(lanterns::Lit(0.0f));
	EXPECT_FALSE(lanterns::Lit(0.07f));
	EXPECT_TRUE(lanterns::Lit(std::nextafter(0.07f, 1.0f)));
	EXPECT_TRUE(lanterns::Lit(1.0f));
}

TEST(CitadelLanterns, TheLampsStandAtTheirPointsInTheTemplesFrame)
{
	for (const float angle : {0.0f, 0.7f, glm::pi<float>() * 1.5f})
	{
		const Transform heart {glm::vec3(1234.5f, 37.25f, 2468.75f), affine::AngleY(angle), glm::vec3(1.0f)};
		for (size_t i = 0; i < lanterns::k_Count; ++i)
		{
			const auto position = lanterns::Position(heart, i);
			const auto expected = Expected(heart, lanterns::k_Offsets.at(i));
			EXPECT_EQ(position.x, expected.x);
			EXPECT_EQ(position.y, expected.y);
			EXPECT_EQ(position.z, expected.z);
		}
	}
	// unturned: across is x and along is z, on the temple's level
	const Transform flat {glm::vec3(100.0f, 5.0f, 200.0f), affine::AngleY(0.0f), glm::vec3(1.0f)};
	EXPECT_FLOAT_EQ(lanterns::Position(flat, 0).x, 100.0f - 8.85f);
	EXPECT_FLOAT_EQ(lanterns::Position(flat, 0).y, 5.0f);
	EXPECT_FLOAT_EQ(lanterns::Position(flat, 0).z, 200.0f - 10.34f);
	EXPECT_FLOAT_EQ(lanterns::Position(flat, 1).x, 100.0f - 2.3f);
	EXPECT_FLOAT_EQ(lanterns::Position(flat, 1).z, 200.0f - 13.4f);
}

TEST(CitadelLanterns, TheFirstLitFrameMakesBothLampsWithATownLightEach)
{
	ecs::Registry registry;
	FakeLights lights;
	const auto heart = MakeHeart(registry, glm::vec3(10.0f, 2.0f, 20.0f), 0.3f, 1.0f);
	lanterns::DrawAll(registry, &lights);
	const auto& lamps = EntranceOf(registry, heart).lamps;
	ASSERT_EQ(lights.added.size(), 2u);
	const auto& transform = registry.Get<const Transform>(heart);
	for (size_t i = 0; i < lanterns::k_Count; ++i)
	{
		ASSERT_NE(lamps.at(i), k_None);
		EXPECT_EQ(lights.added.at(i).lantern, lamps.at(i));
		EXPECT_EQ(lights.added.at(i).type, 0);
		EXPECT_EQ(lights.added.at(i).position, lanterns::Position(transform, i));
		const auto& lamp = registry.Get<const Transform>(lamps.at(i));
		EXPECT_EQ(lamp.position, lanterns::Position(transform, i));
		EXPECT_EQ(lamp.rotation, glm::mat3(1.0f));
		EXPECT_EQ(lamp.scale, glm::vec3(1.0f));
		EXPECT_TRUE(registry.AllOf<LanternLight>(lamps.at(i)));
		EXPECT_EQ(registry.Get<const CitadelLamp>(lamps.at(i)).heart, heart);
		EXPECT_FALSE(registry.AllOf<NotDrawn>(lamps.at(i)));
	}
	// later frames draw the same lamps, with no new light
	lanterns::DrawAll(registry, &lights);
	lanterns::DrawAll(registry, &lights);
	EXPECT_EQ(lights.added.size(), 2u);
}

TEST(CitadelLanterns, ATempleDrawnTooLittleBuiltHasNoLampsYet)
{
	ecs::Registry registry;
	FakeLights lights;
	const auto heart = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 0.07f);
	lanterns::DrawAll(registry, &lights);
	EXPECT_TRUE(lights.added.empty());
	EXPECT_EQ(EntranceOf(registry, heart).lamps.at(0), k_None);
	// once it is drawn more built, they come
	registry.Get<CitadelHeart>(heart).drawPercent = 0.5f;
	lanterns::DrawAll(registry, &lights);
	EXPECT_EQ(lights.added.size(), 2u);
}

TEST(CitadelLanterns, TheLampsAreNotDrawnWhileTheTempleFallsBackBelowSevenPercent)
{
	ecs::Registry registry;
	FakeLights lights;
	const auto heart = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 1.0f);
	lanterns::DrawAll(registry, &lights);
	const auto lamps = EntranceOf(registry, heart).lamps;
	registry.Get<CitadelHeart>(heart).drawPercent = 0.05f;
	lanterns::DrawAll(registry, &lights);
	for (const auto lamp : lamps)
	{
		EXPECT_TRUE(registry.AllOf<NotDrawn>(lamp));
	}
	registry.Get<CitadelHeart>(heart).drawPercent = 0.08f;
	lanterns::DrawAll(registry, &lights);
	for (const auto lamp : lamps)
	{
		EXPECT_FALSE(registry.AllOf<NotDrawn>(lamp));
	}
	// their lights stay as they were made
	EXPECT_EQ(lights.added.size(), 2u);
	EXPECT_TRUE(lights.removed.empty());
}

TEST(CitadelLanterns, ATempleWithoutAnEntranceHasNoLamps)
{
	ecs::Registry registry;
	FakeLights lights;
	const auto heart = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 1.0f);
	registry.Get<CitadelHeart>(heart).entrance = entt::null;
	lanterns::DrawAll(registry, &lights);
	EXPECT_TRUE(lights.added.empty());
}

TEST(CitadelLanterns, TheNewestTempleMakesItsLampsFirst)
{
	ecs::Registry registry;
	FakeLights lights;
	const auto older = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 1.0f);
	const auto newer = MakeHeart(registry, glm::vec3(500.0f, 0.0f, 0.0f), 0.0f, 1.0f);
	lanterns::DrawAll(registry, &lights);
	ASSERT_EQ(lights.added.size(), 4u);
	EXPECT_EQ(lights.added.at(0).lantern, EntranceOf(registry, newer).lamps.at(0));
	EXPECT_EQ(lights.added.at(1).lantern, EntranceOf(registry, newer).lamps.at(1));
	EXPECT_EQ(lights.added.at(2).lantern, EntranceOf(registry, older).lamps.at(0));
	EXPECT_EQ(lights.added.at(3).lantern, EntranceOf(registry, older).lamps.at(1));
}

TEST(CitadelLanterns, ATempleGoingTakesItsLampsAndTheirLightsAtOnce)
{
	ecs::Registry registry;
	FakeLights lights;
	const auto heart = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 1.0f);
	lanterns::DrawAll(registry, &lights);
	const auto lamps = EntranceOf(registry, heart).lamps;
	lanterns::Delete(registry, heart, &lights);
	ASSERT_EQ(lights.removed.size(), 2u);
	EXPECT_EQ(lights.removed.at(0), lamps.at(0));
	EXPECT_EQ(lights.removed.at(1), lamps.at(1));
	for (const auto lamp : lamps)
	{
		EXPECT_FALSE(registry.Valid(lamp));
	}
	EXPECT_EQ(EntranceOf(registry, heart).lamps.at(0), k_None);
	// a temple that never made them takes nothing
	const auto unlit = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 0.0f);
	lanterns::Delete(registry, unlit, &lights);
	EXPECT_EQ(lights.removed.size(), 2u);
}

TEST(CitadelLanterns, WithoutTheLightServiceTheLampsAreStillMade)
{
	ecs::Registry registry;
	const auto heart = MakeHeart(registry, glm::vec3(0.0f), 0.0f, 1.0f);
	lanterns::DrawAll(registry, nullptr);
	EXPECT_NE(EntranceOf(registry, heart).lamps.at(1), k_None);
	lanterns::Delete(registry, heart, nullptr);
	EXPECT_EQ(EntranceOf(registry, heart).lamps.at(1), k_None);
}
