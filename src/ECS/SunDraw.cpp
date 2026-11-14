/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SunDraw.h"

#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"

namespace openblack::ecs::sun_draw
{
void SetFromScript(Registry& registry, entt::entity sun, uint32_t value)
{
	if (auto* body = registry.TryGet<components::Sun>(sun); body != nullptr)
	{
		body->hiddenByScript = value != 0;
	}
}

void ShowAgain(Registry& registry, entt::entity sun)
{
	SetFromScript(registry, sun, 0);
}

bool IsHidden(const Registry& registry, entt::entity sun)
{
	const auto* body = registry.TryGet<const components::Sun>(sun);
	return body != nullptr && body->hiddenByScript;
}
} // namespace openblack::ecs::sun_draw
