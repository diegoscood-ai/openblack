/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The super villagers' temporary shadows are made at the head of the projected shadows' list as each villager becomes
// one, so the shadows list them newest first, as the super villagers' own list does: of several made in one frame the
// newest is at the head. Only an available villager whose shadow lasts has one.
// docs/wiki/rendering/shadows-and-reflections.md, docs/wiki/project/progress/rendering/shadows.md

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/DynamicShadow.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "Graphics/ShadowList.h"

using namespace openblack;
using ecs::components::DynamicShadow;

namespace
{
entt::entity SuperVillager(ecs::Registry& registry)
{
	const auto thing = registry.Create();
	registry.Assign<DynamicShadow>(thing, DynamicShadow {.onObjects = false, .useSun = true});
	return thing;
}
} // namespace

// Made oldest first from the list's newest first, so that each, made at the head, leaves the newest there
TEST(ShadowListOrder, TheTemporaryShadowsAreMadeOldestFirstSoTheNewestIsAtTheHead)
{
	ecs::Registry registry;
	const auto first = SuperVillager(registry);
	const auto second = SuperVillager(registry);
	const auto third = SuperVillager(registry);
	const std::array newestFirst {third, second, first};
	EXPECT_EQ(graphics::shadow_list::TemporaryShadowsInMakingOrder(registry, newestFirst),
	          (std::vector {first, second, third}));
}

// A villager no longer a super villager (its shadow gone), an unavailable one or a deleted one has none
TEST(ShadowListOrder, OnlyAnAvailableVillagerWhoseShadowLastsHasOne)
{
	ecs::Registry registry;
	const auto kept = SuperVillager(registry);
	const auto ended = SuperVillager(registry);
	registry.Remove<DynamicShadow>(ended);
	const auto unavailable = SuperVillager(registry);
	registry.Assign<ecs::components::Unavailable>(unavailable);
	const auto deleted = SuperVillager(registry);
	registry.Destroy(deleted);
	const std::array newestFirst {deleted, unavailable, ended, kept};
	EXPECT_EQ(graphics::shadow_list::TemporaryShadowsInMakingOrder(registry, newestFirst), (std::vector {kept}));
	EXPECT_TRUE(graphics::shadow_list::TemporaryShadowsInMakingOrder(registry, {}).empty());
}
