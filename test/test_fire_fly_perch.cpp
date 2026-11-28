/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where the fireflies perch and start: a thing that answers it is a rock (a rock, the bonfire, a piece knocked off a
// building) or any kind of tree. No game data: a hand-made info.dat with the rock's row of the rock kind.

#include <array>
#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Tree.h"
#include "ECS/FireFlies.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class FireFliesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).mobileType = MobileStaticInfo::Rock;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static entt::entity Static(MobileStaticInfo row)
	{
		const auto thing = Reg().Create();
		Reg().Assign<MobileStatic>(thing, row);
		return thing;
	}
	template <typename Component>
	static entt::entity With()
	{
		const auto thing = Reg().Create();
		Reg().Assign<Component>(thing);
		return thing;
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(FireFliesTest, AFireFlyPerchesOnARockTheBonfireAFragmentOrAnyTree)
{
	EXPECT_TRUE(ecs::IsFireFlyPerch(Static(MobileStaticInfo::Rock)));
	EXPECT_TRUE(ecs::IsFireFlyPerch(Static(MobileStaticInfo::Bonfire)));
	EXPECT_TRUE(ecs::IsFireFlyPerch(With<Fragment>()));
	EXPECT_TRUE(ecs::IsFireFlyPerch(With<Tree>()));
	EXPECT_TRUE(ecs::IsFireFlyPerch(With<DeadTree>()));
	// not on a fence, nor on the plain base made of the singing stone base's row
	EXPECT_FALSE(ecs::IsFireFlyPerch(Static(MobileStaticInfo::CeltFenceShort)));
	EXPECT_FALSE(ecs::IsFireFlyPerch(Static(MobileStaticInfo::SingingStoneBase)));
}

TEST_F(FireFliesTest, AFireFlyNotAtATreeStartsAtTheFirstRockOfTheFixedListFromItsDraw)
{
	const auto fence = Static(MobileStaticInfo::CeltFenceShort);
	const auto bonfire = Static(MobileStaticInfo::Bonfire);
	const auto rock = Static(MobileStaticInfo::Rock);
	const auto fragment = With<Fragment>();
	const auto deadTree = With<DeadTree>();
	const std::array fixed {fence, bonfire, deadTree, rock, fragment, deadTree};
	constexpr entt::entity k_None {entt::null};
	// the bonfire is taken from the draw on, as a rock is
	EXPECT_EQ(ecs::FireFlyRockFrom(fixed, 0), bonfire);
	EXPECT_EQ(ecs::FireFlyRockFrom(fixed, 1), bonfire);
	// a dead tree is not a rock
	EXPECT_EQ(ecs::FireFlyRockFrom(fixed, 2), rock);
	EXPECT_EQ(ecs::FireFlyRockFrom(fixed, 4), fragment);
	// none after the draw: no firefly
	EXPECT_EQ(ecs::FireFlyRockFrom(fixed, 5), k_None);
	EXPECT_EQ(ecs::FireFlyRockFrom(fixed, fixed.size()), k_None);
}
