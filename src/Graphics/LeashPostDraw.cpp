/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashPostDraw.h"

#include "ECS/Components/LeashPost.h"
#include "Worship/LeashPosts.h"

using namespace openblack;
using namespace openblack::graphics;

std::optional<leash_post_draw::Post> leash_post_draw::Build(const ecs::components::LeashPost& post, const glm::mat4& postMatrix,
                                                            bool pickedHere, const std::optional<Hand>& hand,
                                                            uint32_t landLight)
{
	namespace rules = worship::leash_posts;
	// the local player's picked post is not shown while the hand is hidden
	if (pickedHere && hand.has_value() && hand->hidden)
	{
		return std::nullopt;
	}
	Post drawn;
	drawn.collar = pickedHere && hand.has_value() && hand->root.has_value() ? rules::CollarOnHand(*hand->root, hand->scale)
	                                                                        : rules::CollarMatrix(postMatrix, post.spin);
	drawn.uv = {post.spin.scroll, rules::CollarBand(post.index)};
	// the smoke stays where the post was made, whichever post is picked
	drawn.smoke = billboard::Sprite {
	    .position = glm::vec3(postMatrix[3]),
	    .size = rules::k_SmokeHalfWidth,
	    .argb = rules::SmokeColour(rules::SmokeBrightness(landLight), pickedHere),
	    .cell = rules::SpriteCell(post.spin),
	};
	drawn.additive = pickedHere;
	return drawn;
}
