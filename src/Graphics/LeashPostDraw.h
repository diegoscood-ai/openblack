/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

namespace openblack::ecs::components
{
struct LeashPost;
}

// What the world view draws of one of a temple's leash posts this frame (worship::leash_posts holds the rules): the
// collar, a leash's band of the rope's texture scrolling round it as it turns, and a puff of smoke behind it; the
// picked post's collar hangs on the hand instead and its smoke glows orange. Pure: the frame passes in the post, its
// matrix, the hand and the land's light. (wiki: creature.md, "The temple's leash posts")
namespace openblack::graphics::leash_post_draw
{

/// The player's hand as the picked post's collar hangs on it
struct Hand
{
	/// The hand's root bone in the world; none without the hand's bones, and the collar stays on its post
	std::optional<glm::mat4> root;
	/// The hand object's scale
	float scale {1.0f};
	/// The hand is hidden: the picked post is then not shown at all
	bool hidden {false};
};

/// One post drawn this frame
struct Post
{
	/// The collar's matrix (in the world)
	glm::mat4 collar {1.0f};
	/// The collar's texture offset: the scroll along u, the leash's band in v
	glm::vec2 uv {0.0f};
	/// The smoke: a sprite facing the screen at the post's point
	billboard::Sprite smoke;
	/// The picked post's smoke is added to what is behind it, the others' blended
	bool additive {false};
};

/// The post `post` with its spin stepped for this frame, its matrix `postMatrix` fixed when it was made: nothing when it
/// is the picked post of the local player's temple (`pickedHere`) and the hand is hidden, else its collar (on the hand
/// for that picked post), its band and its smoke in the brightness of `landLight` (the land light table's last entry,
/// 0xAARRGGBB)
[[nodiscard]] std::optional<Post> Build(const ecs::components::LeashPost& post, const glm::mat4& postMatrix, bool pickedHere,
                                        const std::optional<Hand>& hand, uint32_t landLight);

} // namespace openblack::graphics::leash_post_draw
