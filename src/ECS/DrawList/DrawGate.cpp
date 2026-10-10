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
#include <vector>

#include "ECS/Components/DontDraw.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/ObjectListFrame.h"

namespace openblack::ecs::draw_gate
{

bool Hidden(bool listRan, bool inMap, bool dontDraw, bool drawn)
{
	return listRan && inMap && !dontDraw && !drawn;
}

void FillFrame(bool listRan, std::span<const entt::entity> drawn, const Registry& registry,
               const std::function<bool(entt::entity)>& inMap, graphics::ObjectListFrame& out)
{
	out.ran = listRan;
	out.hidden.clear();
	out.reasons.clear();
	if (!listRan)
	{
		return;
	}
	std::vector<entt::entity> sortedDrawn(drawn.begin(), drawn.end());
	std::ranges::sort(sortedDrawn);
	registry.Each<const components::Mesh, const components::Transform>(
	    [&](entt::entity entity, const components::Mesh& /*mesh*/, const components::Transform& /*transform*/) {
		    const bool wasDrawn = std::ranges::binary_search(sortedDrawn, entity);
		    if (Hidden(listRan, inMap(entity), registry.AllOf<components::DontDraw>(entity), wasDrawn))
		    {
			    out.hidden.push_back(entity);
		    }
	    });
	std::ranges::sort(out.hidden);
	out.reasons.reserve(out.hidden.size());
	for (const auto entity : out.hidden)
	{
		out.reasons.push_back(registry.AllOf<components::DrawListed>(entity) ? graphics::ObjectListHidden::NotDrawn
		                                                                     : graphics::ObjectListHidden::NotListed);
	}
}

} // namespace openblack::ecs::draw_gate
