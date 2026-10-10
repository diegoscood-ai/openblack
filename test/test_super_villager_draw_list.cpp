/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The SuperVillager sites that ask for a draw-list rebuild (ECS/SuperVillager.cpp): SET_HIGH_GRAPHICS_DETAIL off
// before its release, and the end of the script's wide screen before every SuperVillager goes, each with a count of 1
// (a registry only: no files, no meshes, no help system). docs/bw1-notes/original-frame.md §6

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "Locator.h"
#include "support/DrawListRequests.h"
#include "support/WorldSystems.h"

namespace
{
namespace super_villager = openblack::ecs::super_villager;
using openblack::Locator;
using openblack::ecs::Registry;
using openblack::ecs::components::SuperVillager;
using openblack::ecs::components::Transform;

class SuperVillagerRebuilds: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		super_villager::ReleaseAll();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static entt::entity Host()
	{
		const auto host = Reg().Create();
		Reg().Assign<Transform>(host, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return host;
	}

	openblack::test::DrawListRequests _requests;
};
} // namespace

TEST_F(SuperVillagerRebuilds, HighGraphicsDetailOffAsksThenReleases)
{
	const auto host = Host();
	super_villager::testing::Adopt(host, 0);
	super_villager::SetHighGraphicsDetail(host, false);
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
	EXPECT_TRUE(super_villager::List().empty());
	EXPECT_FALSE(Reg().AllOf<SuperVillager>(host));
	// any thing the script found asks, a SuperVillager or not
	super_villager::SetHighGraphicsDetail(Host(), false);
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1, 1}));
}

TEST_F(SuperVillagerRebuilds, TheEndOfTheWideScreenAsksOnceTheListIsNotEmpty)
{
	// no SuperVillager: nothing to release, nothing asked
	super_villager::Update();
	EXPECT_TRUE(_requests.Counts().empty());
	// one, and no script wide screen (no help system here): asked, then every one released
	super_villager::testing::Adopt(Host(), 1);
	super_villager::Update();
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
	EXPECT_TRUE(super_villager::List().empty());
	// the list is empty again: the next frame asks nothing more
	super_villager::Update();
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
}
