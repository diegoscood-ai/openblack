/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's meshes, animations, textures and sounds go into resource caches that load each once and look it up by
// its name: a second load of the same name keeps the first and does not load again, and the name, hashed, finds it.
// docs/wiki/project/progress/engine/platforms_and_performance.md

#include <memory>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Resources/ResourceManager.h"

using namespace openblack;

namespace
{
/// A loader that counts its loads; each resource holds the number of the load that made it
struct CountingLoader
{
	using result_type = std::shared_ptr<int>;
	using ResourceType = int;
	static inline int s_loads = 0;
	result_type operator()(int /*value*/) const { return std::make_shared<int>(++s_loads); }
};
} // namespace

TEST(ResourceCacheOnce, ASecondLoadOfTheSameNameKeepsTheFirstAndLoadsNothing)
{
	CountingLoader::s_loads = 0;
	resources::ResourceManager<CountingLoader> cache;
	static_cast<void>(cache.Load(std::string("rock"), 7));
	static_cast<void>(cache.Load(std::string("rock"), 8));
	EXPECT_EQ(CountingLoader::s_loads, 1);
	EXPECT_EQ(cache.Size(), 1u);
	EXPECT_EQ(*cache.Handle(entt::hashed_string("rock").value()), 1);
	static_cast<void>(cache.Load(std::string("tree"), 9));
	EXPECT_EQ(CountingLoader::s_loads, 2);
}

TEST(ResourceCacheOnce, ANameFindsItsResourceHashedOnceWhateverFormItIsGivenIn)
{
	CountingLoader::s_loads = 0;
	resources::ResourceManager<CountingLoader> cache;
	static_cast<void>(cache.Load(std::string("hand"), 1));
	EXPECT_TRUE(cache.Contains(entt::hashed_string("hand").value()));
	EXPECT_TRUE(cache.Contains(std::string("hand")));
	EXPECT_TRUE(cache.Contains(entt::hashed_string("hand")));
	EXPECT_TRUE(cache.Contains("hand"));
	EXPECT_FALSE(cache.Contains(std::string("Hand")));
}
