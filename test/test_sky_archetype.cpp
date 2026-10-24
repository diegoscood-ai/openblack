/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The sky's entities (SkyArchetype): the dome, the sun and the moon with what they are drawn from, made as plain
// entities: first in an empty registry, and again first after a land's reset, so the land's entities come after them.

#include <gtest/gtest.h>

#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
using archetypes::SkyArchetype;

namespace
{
class SkyArchetypeTest: public ::testing::Test
{
protected:
	void SetUp() override { _registry = &Locator::entitiesRegistry::emplace<Registry>(); }

	test::RestoreService<Locator::entitiesRegistry> _restore;
	Registry* _registry {nullptr};
};
} // namespace

TEST_F(SkyArchetypeTest, MakesTheDomeTheSunAndTheMoonWithWhatTheyAreDrawnFrom)
{
	const auto sky = SkyArchetype::Create();

	EXPECT_EQ(_registry->Get<components::SkyDome>(sky.dome).meshId, entt::hashed_string("sky/sky.l3d").value());

	EXPECT_TRUE(_registry->AllOf<components::Sun>(sky.sun));
	const auto& sun = _registry->Get<components::CelestialBody>(sky.sun);
	EXPECT_EQ(sun.meshId, entt::hashed_string("sky/sun.l3d").value());
	EXPECT_EQ(sun.textureId, entt::hashed_string("raw/sun").value());
	EXPECT_EQ(sun.alphaTextureId, entt::hashed_string("raw/sun").value());
	EXPECT_FALSE(_registry->AllOf<components::CelestialGlow>(sky.sun));

	const auto& moon = _registry->Get<components::CelestialBody>(sky.moon);
	EXPECT_EQ(moon.meshId, entt::hashed_string("sky/moon.l3d").value());
	EXPECT_EQ(moon.textureId, entt::hashed_string("raw/weather").value());
	EXPECT_EQ(moon.alphaTextureId, entt::hashed_string("raw/weathera").value());
	const auto& glow = _registry->Get<components::CelestialGlow>(sky.moon);
	EXPECT_EQ(glow.textureId, entt::hashed_string("raw/ATMOS").value());
	EXPECT_EQ(glow.alphaTextureId, entt::hashed_string("raw/ATMOSA").value());
}

TEST_F(SkyArchetypeTest, TheMoonStartsAsItWasBeforeItFirstShows)
{
	const auto sky = SkyArchetype::Create();
	const auto& moon = _registry->Get<components::Moon>(sky.moon);
	EXPECT_EQ(moon.phase, 0.0f);
	EXPECT_EQ(moon.date, 0);
	EXPECT_EQ(moon.dateReadAt, 0u);
}

TEST_F(SkyArchetypeTest, TheSkyIsMadeFirstAndTheLandsEntitiesComeAfterIt)
{
	// In an empty registry the dome, the sun and the moon are the first three entities, in that order
	const auto sky = SkyArchetype::Create();
	EXPECT_EQ(entt::to_entity(sky.dome), 0u);
	EXPECT_EQ(entt::to_entity(sky.sun), 1u);
	EXPECT_EQ(entt::to_entity(sky.moon), 2u);
	EXPECT_EQ(entt::to_entity(_registry->Create()), 3u);

	// After a land's reset the sky made again first takes the same three, and the next land's entities come after
	_registry->Reset();
	const auto again = SkyArchetype::Create();
	EXPECT_EQ(entt::to_entity(again.dome), 0u);
	EXPECT_EQ(entt::to_entity(again.sun), 1u);
	EXPECT_EQ(entt::to_entity(again.moon), 2u);
	EXPECT_EQ(entt::to_entity(_registry->Create()), 3u);
	EXPECT_TRUE(_registry->Valid(again.moon));
}
