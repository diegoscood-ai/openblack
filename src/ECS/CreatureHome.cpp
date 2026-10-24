/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHome.h"

#include "3D/L3DMesh.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// Where a temple keeps its player's creature: its mesh's pen point, turned and moved with the temple, or the temple
/// itself when its mesh marks none
glm::vec3 TempleCreatureHome(const Transform& transform, const Mesh* mesh)
{
	if (mesh != nullptr && Locator::resources::has_value())
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id))
		{
			if (const auto pen = player_creature::TemplePenPoint(transform, meshes.Handle(mesh->id)->GetExtraMetrics()))
			{
				return *pen;
			}
		}
	}
	return transform.position;
}

} // namespace

std::optional<glm::vec3> creature_home::HomeOf(const Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	// The home it was given comes first, before its player's temple
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature); leash != nullptr && leash->home.has_value())
	{
		return leash->home;
	}
	std::optional<glm::vec3> temple;
	registry.Each<const Temple, const Transform>(
	    [&registry, &temple, owner = body->owner](entt::entity entity, const Temple& building, const Transform& transform) {
		    if (building.owner == owner && !temple.has_value())
		    {
			    temple = TempleCreatureHome(transform, registry.TryGet<const Mesh>(entity));
		    }
	    });
	return temple;
}
