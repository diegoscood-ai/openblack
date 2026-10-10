/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the world view draws of a temple's leash post in a frame (Graphics/LeashPostDraw.h), with made-up posts and hands

#include <cstdint>

#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/LeashPost.h"
#include "Graphics/LeashPostDraw.h"
#include "Worship/LeashPosts.h"

using namespace openblack;
using namespace openblack::graphics;
namespace rules = openblack::worship::leash_posts;

namespace
{
/// The learning leash's post at (100, 5, 200), its spin a little way on
ecs::components::LeashPost RopePost()
{
	ecs::components::LeashPost post;
	post.index = 1;
	post.owner = PlayerNames::PLAYER_ONE;
	post.spin = {.scroll = 0.25f, .xAngle = 1.0f, .zAngle = 2.0f, .frame = 7.5f};
	return post;
}

const glm::mat4 k_PostMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f));
/// The land's light at noon: a smoke of brightness 124
constexpr uint32_t k_Noon = 0xFFF3FFFBu;

leash_post_draw::Hand HandAt(bool hidden, bool withBones)
{
	leash_post_draw::Hand hand;
	hand.scale = 0.01f;
	hand.hidden = hidden;
	if (withBones)
	{
		hand.root = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));
	}
	return hand;
}
} // namespace

TEST(LeashPostDraw, APostShowsItsTurningCollarAndItsSmoke)
{
	const auto post = RopePost();
	const auto drawn = leash_post_draw::Build(post, k_PostMatrix, false, HandAt(false, true), k_Noon);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(drawn->collar, rules::CollarMatrix(k_PostMatrix, post.spin));
	// the scroll along u, the learning leash's band in v
	EXPECT_EQ(drawn->uv, glm::vec2(0.25f, 0.125f));
	// the smoke at the post, 2 m across each way, white at a sixth of the land's light, cell 7
	EXPECT_EQ(drawn->smoke.position, glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_EQ(drawn->smoke.size, 2.0f);
	EXPECT_EQ(drawn->smoke.height, 1.0f);
	EXPECT_EQ(drawn->smoke.angle, 0.0f);
	EXPECT_FALSE(drawn->smoke.horizontal);
	EXPECT_EQ(drawn->smoke.argb, 0x7CFFFFFFu);
	EXPECT_EQ(drawn->smoke.cell, 7u);
	EXPECT_FALSE(drawn->additive);
}

TEST(LeashPostDraw, ThePickedPostsCollarHangsOnTheHand)
{
	const auto post = RopePost();
	const auto hand = HandAt(false, true);
	const auto drawn = leash_post_draw::Build(post, k_PostMatrix, true, hand, k_Noon);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(drawn->collar, rules::CollarOnHand(*hand.root, hand.scale));
	// its smoke stays at the post, orange and added to what is behind it
	EXPECT_EQ(drawn->smoke.position, glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_EQ(drawn->smoke.argb, 0x7CC18119u);
	EXPECT_TRUE(drawn->additive);
	EXPECT_EQ(drawn->uv, glm::vec2(0.25f, 0.125f));
}

TEST(LeashPostDraw, ThePickedPostIsNotShownWhileTheHandIsHidden)
{
	EXPECT_FALSE(leash_post_draw::Build(RopePost(), k_PostMatrix, true, HandAt(true, true), k_Noon).has_value());
	// another post is, the hand hidden or not
	EXPECT_TRUE(leash_post_draw::Build(RopePost(), k_PostMatrix, false, HandAt(true, true), k_Noon).has_value());
}

TEST(LeashPostDraw, WithoutTheHandsBonesThePickedCollarStaysOnItsPost)
{
	const auto post = RopePost();
	for (const auto& hand :
	     {std::optional<leash_post_draw::Hand> {HandAt(false, false)}, std::optional<leash_post_draw::Hand> {}})
	{
		const auto drawn = leash_post_draw::Build(post, k_PostMatrix, true, hand, k_Noon);
		ASSERT_TRUE(drawn.has_value());
		EXPECT_EQ(drawn->collar, rules::CollarMatrix(k_PostMatrix, post.spin));
		EXPECT_TRUE(drawn->additive);
	}
}
