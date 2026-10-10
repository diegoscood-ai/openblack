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

#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "Graphics/ZSort.h"
#include "Magic/TribalPowerSpin.h"

/// The tribes' names in the world view (a ring round the hand, columns rising where a miracle was cast): the renderer
/// reads them from the miracles' looks (ecs::systems::MiracleFxSystemInterface) and queues each one as a see-through
/// object of its own in the frame's single queue (zsort::Queue). Renderer::DrawTribalPower draws one when the queue is
/// drained. Wiki: docs/bw1-notes/magic.md, "The hand".
namespace openblack::graphics::tribal_power
{

/// The miracles' looks the renderer reads the names from (Locator::miracleFxSystem). Without one the game stops with a
/// message
[[nodiscard]] const ecs::systems::MiracleFxSystemInterface& Source();

/// Queues every ring and column, in the order the miracles' looks give them, keyed at its centre ((x^2 + y^2) + z^2 from
/// the camera). The item of each is makeItem(its index in the list returned), which the drain reads back
template <typename Item, typename MakeItem>
std::vector<const magic::tribal_spin::Runner*> Submit(const ecs::systems::MiracleFxSystemInterface& source,
                                                      zsort::Queue<Item>& queue, const glm::vec3& camera, MakeItem&& makeItem)
{
	auto runners = source.GetTribalPowerRunners();
	for (size_t i = 0; i < runners.size(); ++i)
	{
		queue.Submit(makeItem(static_cast<int>(i)), zsort::Key(runners[i]->Position(), camera));
	}
	return runners;
}

} // namespace openblack::graphics::tribal_power
