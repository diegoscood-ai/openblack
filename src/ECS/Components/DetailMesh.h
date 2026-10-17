/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

namespace openblack::ecs::components
{

/// The finer model a villager, an animal or a bird is drawn with at the top graphics detail
/// (graphics::detail_level::k_Top): its high mesh. Below it the Mesh is drawn, the std one, which also stays its own
/// for the pose, the pick, the shadow, the reflection and the culling at every level, as the original keeps it
struct DetailMesh
{
	entt::id_type high;
};

} // namespace openblack::ecs::components
