/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawSteps.h"

#include <algorithm>

#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/ZSort.h"
#include "Locator.h"

namespace openblack::ecs::draw_steps
{

bool ListFrame::StepsOnScreen(entt::entity object) const
{
	return Steps(everyFrame, ran, std::ranges::find(drawn, object) != drawn.end(),
	             std::ranges::find(onScreen, object) != onScreen.end());
}

bool ListFrame::StepsDrawn(entt::entity object) const
{
	return Steps(everyFrame, ran, std::ranges::find(drawn, object) != drawn.end(), true);
}

ListFrame ThisFrame()
{
	if (!Locator::objectDrawListSystem::has_value())
	{
		return {.everyFrame = true};
	}
	const auto& list = Locator::objectDrawListSystem::value();
	return {
	    .everyFrame = Locator::config::has_value() && Locator::config::value().effectsSteppedEveryFrame,
	    .ran = list.RanThisFrame(),
	    .drawn = list.DrawnThisFrame(),
	    .onScreen = list.OnScreenThisFrame(),
	};
}

std::vector<size_t> DrainOrder(std::span<const glm::vec3> points, const glm::vec3& camera)
{
	graphics::zsort::Queue<size_t> queue;
	for (size_t i = 0; i < points.size(); ++i)
	{
		queue.Submit(i, graphics::zsort::Key(points[i], camera));
	}
	std::vector<size_t> order;
	order.reserve(points.size());
	for (const auto& entry : queue.Ordered())
	{
		order.push_back(*entry.item);
	}
	return order;
}

} // namespace openblack::ecs::draw_steps
