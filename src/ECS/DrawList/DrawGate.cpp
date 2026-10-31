/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawGate.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ECS/CarriedProps.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/ObjectListFrame.h"

namespace openblack::ecs::draw_gate
{

bool Hidden(bool listRan, bool inMap, bool dontDraw, bool drawn)
{
	return listRan && inMap && !dontDraw && !drawn;
}

bool PileBuried(float offset, float height)
{
	return -height >= offset;
}

bool GateLampHidden(bool listRan, bool gateOnScreen)
{
	return listRan && !gateOnScreen;
}

std::optional<glm::vec3> LampLightOrigin(const Registry& registry, entt::entity entity)
{
	const auto* lamp = registry.TryGet<const components::GateLamp>(entity);
	if (lamp == nullptr || !registry.Valid(lamp->gate))
	{
		return std::nullopt;
	}
	const auto* gate = registry.TryGet<const components::Transform>(lamp->gate);
	if (gate == nullptr)
	{
		return std::nullopt;
	}
	return gate->position;
}

void FillFrame(bool listRan, std::span<const entt::entity> drawn, std::span<const entt::entity> onScreen,
               const Registry& registry, const std::function<bool(entt::entity)>& inMap, graphics::ObjectListFrame& out)
{
	out.ran = listRan;
	out.hidden.clear();
	out.reasons.clear();
	out.visited.clear();
	if (!listRan)
	{
		return;
	}
	out.visited.assign(drawn.begin(), drawn.end());
	std::vector<entt::entity> sortedDrawn(drawn.begin(), drawn.end());
	std::ranges::sort(sortedDrawn);
	std::vector<entt::entity> sortedOnScreen(onScreen.begin(), onScreen.end());
	std::ranges::sort(sortedOnScreen);
	std::vector<std::pair<entt::entity, graphics::ObjectListHidden>> hidden;
	registry.Each<const components::Mesh, const components::Transform>(
	    [&](entt::entity entity, const components::Mesh& /*mesh*/, const components::Transform& /*transform*/) {
		    if (const auto* lamp = registry.TryGet<const components::GateLamp>(entity); lamp != nullptr)
		    {
			    if (GateLampHidden(listRan, std::ranges::binary_search(sortedOnScreen, lamp->gate)))
			    {
				    hidden.emplace_back(entity, graphics::ObjectListHidden::GateNotOnScreen);
			    }
			    return;
		    }
		    const bool wasDrawn = std::ranges::binary_search(sortedDrawn, entity);
		    if (Hidden(listRan, inMap(entity), registry.AllOf<components::DontDraw>(entity), wasDrawn))
		    {
			    hidden.emplace_back(entity, registry.AllOf<components::DrawListed>(entity)
			                                    ? graphics::ObjectListHidden::NotDrawn
			                                    : graphics::ObjectListHidden::NotListed);
			    return;
		    }
		    // a wood or food pile's Draw draws it only while some of it is above the land
		    if (const auto* sink = registry.TryGet<const components::PileSink>(entity);
		        sink != nullptr && registry.AllOf<components::Pot>(entity) && PileBuried(sink->offset.value, sink->height))
		    {
			    hidden.emplace_back(entity, graphics::ObjectListHidden::Buried);
		    }
	    });
	std::ranges::sort(hidden, {}, &std::pair<entt::entity, graphics::ObjectListHidden>::first);
	// The object a villager carries is drawn by the villager's own draw, right after its body, so it is left out with
	// its villager, for the same reason
	std::vector<std::pair<entt::entity, graphics::ObjectListHidden>> carried;
	registry.Each<const components::CarriedProp>([&](entt::entity prop, const components::CarriedProp& held) {
		const auto owner =
		    std::ranges::lower_bound(hidden, held.owner, {}, &std::pair<entt::entity, graphics::ObjectListHidden>::first);
		if (owner != hidden.end() && owner->first == held.owner &&
		    !std::ranges::binary_search(hidden, prop, {}, &std::pair<entt::entity, graphics::ObjectListHidden>::first))
		{
			carried.emplace_back(prop, owner->second);
		}
	});
	if (!carried.empty())
	{
		hidden.insert(hidden.end(), carried.begin(), carried.end());
		std::ranges::sort(hidden, {}, &std::pair<entt::entity, graphics::ObjectListHidden>::first);
	}
	out.hidden.reserve(hidden.size());
	out.reasons.reserve(hidden.size());
	for (const auto& [entity, reason] : hidden)
	{
		out.hidden.push_back(entity);
		out.reasons.push_back(reason);
	}
}

} // namespace openblack::ecs::draw_gate
