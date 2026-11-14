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
/// (components::MagicShield), queued with the other models. Wiki: docs/wiki/magic/shields.md#the-object.
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

/// The debug readout of the held domes (the View menu): how many the world view's last frame queued after the
/// particle effects
struct HeldDomesReadout
{
	size_t lastFrame {0};
};

/// The physical shields' domes the object loop holds back: the spells draw the shields after the frame's particle
/// effects, so a dome and an effect at an equal key come out of the queue effect first. Each dome keeps the models'
/// key ((x^2 + z^2) + y^2 from the camera) and its shield's serial; they are queued the newest shield first (the
/// highest serial), and in the order they were held among equal serials. Wiki: docs/wiki/magic/shields.md#openblack.
template <typename Item>
class HeldDomes
{
public:
	/// Holds a dome standing at origin, keyed from the camera as a model is
	void Hold(const Item& item, const glm::vec3& origin, const glm::vec3& camera, uint32_t serial)
	{
		_held.push_back({.item = item, .key = zsort::Key(origin, camera, zsort::SumOrder::XZY), .serial = serial});
	}

	/// Queues every held dome, the newest shield first
	void Submit(zsort::Queue<Item>& queue)
	{
		std::ranges::stable_sort(_held, std::ranges::greater {}, &Held::serial);
		for (const auto& held : _held)
		{
			queue.Submit(held.item, held.key);
		}
	}

	[[nodiscard]] size_t Size() const noexcept { return _held.size(); }

private:
	struct Held
	{
		Item item;
		float key;
		uint32_t serial;
	};
	std::vector<Held> _held;
};

} // namespace openblack::graphics::shield_domes
