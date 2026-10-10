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

#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "Graphics/ZSort.h"

/// The physical shields' domes in the world view, as the shield service hands them to the renderer
/// (ecs::systems::MagicShieldSystemInterface::GetDomes): each one a see-through object of its own in the frame's single
/// queue (zsort::Queue), keyed as the models are. The service hands out none today: the domes are map objects
/// (components::MapShield), queued with the other models. Wiki: docs/bw1-notes/miracles.md, the physical shield.
namespace openblack::graphics::shield_domes
{

using DomeDraw = ecs::systems::MagicShieldSystemInterface::DomeDraw;

/// Queues every dome the shield service hands out for the turn's fraction, in its order, keyed where it stands
/// ((x^2 + z^2) + y^2 from the camera). The item of each is makeItem(its index in the list returned), which the drain
/// reads back
template <typename Item, typename MakeItem>
std::vector<DomeDraw> Submit(const ecs::systems::MagicShieldSystemInterface& source, float turnFraction,
                             zsort::Queue<Item>& queue, const glm::vec3& camera, MakeItem&& makeItem)
{
	auto domes = source.GetDomes(turnFraction);
	for (size_t i = 0; i < domes.size(); ++i)
	{
		queue.Submit(makeItem(static_cast<int>(i)), zsort::Key(domes[i].position, camera, zsort::SumOrder::XZY));
	}
	return domes;
}

} // namespace openblack::graphics::shield_domes
