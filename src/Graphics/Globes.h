/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/ZSort.h"

/// The one-shot globes in the world view as a list of draws for the renderer (Renderer::DrawGlobes): each one a
/// see-through object of its own in the frame's single queue (zsort::Queue), keyed where it is sorted as the models are.
/// The list stays empty today: the bubble, the seed inside and the bands are entities (magic::one_off,
/// worship::seed_graphic), drawn and queued with the other models. Wiki: docs/bw1-notes/magic.md, "Seeds and one-off
/// miracles".
namespace openblack::graphics::globes
{

/// What a globe's draw is
enum class GlobeDrawKind : uint8_t
{
	/// The miracle's seed floating inside
	Seed,
	/// The bubble round it
	Bubble,
	/// A ring round an extreme miracle
	Ring,
};

/// One draw of a globe
struct GlobeDraw
{
	GlobeDrawKind kind {GlobeDrawKind::Bubble};
	entt::id_type mesh {0};
	glm::mat4 model {1.0f};
	/// The texture's offset (the bubble's glint cell)
	glm::vec2 uvOffset {0.0f, 0.0f};
	/// Of 255
	uint8_t alpha {255};
	/// Where it is keyed: the bubble's centre moved towards the camera by its radius
	glm::vec3 sortPoint {0.0f, 0.0f, 0.0f};
};

/// Queues every draw of the list, in its order, keyed at its sort point ((x^2 + z^2) + y^2 from the camera). The item of
/// each is makeItem(its index in the list), which the drain reads back
template <typename Item, typename MakeItem>
void Submit(std::span<const GlobeDraw> draws, zsort::Queue<Item>& queue, const glm::vec3& camera, MakeItem&& makeItem)
{
	for (size_t i = 0; i < draws.size(); ++i)
	{
		queue.Submit(makeItem(static_cast<int>(i)), zsort::Key(draws[i].sortPoint, camera, zsort::SumOrder::XZY));
	}
}

} // namespace openblack::graphics::globes
