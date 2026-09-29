/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StaticGrounding.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

float StaticGrounding::FloatingGap(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::terrainSystem::has_value() || !registry.AllOf<Transform, Mesh>(entity))
	{
		return 0.0f;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& mesh = registry.Get<const Mesh>(entity);
	if (!meshes.Contains(mesh.id))
	{
		return 0.0f;
	}
	const auto& transform = registry.Get<const Transform>(entity);
	const auto& terrain = Locator::terrainSystem::value();
	float gap = std::numeric_limits<float>::max();
	for (const auto& subMesh : meshes.Handle(mesh.id)->GetSubMeshes())
	{
		if (subMesh->IsPhysics())
		{
			continue;
		}
		for (const auto& vertex : subMesh->GetCollisionPositions())
		{
			const auto world = transform.position + transform.rotation * (transform.scale * vertex);
			gap = std::min(gap, world.y - terrain.GetHeightAt(glm::vec2(world.x, world.z)));
		}
	}
	return gap == std::numeric_limits<float>::max() ? 0.0f : gap;
}

void StaticGrounding::Ground(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* statics = registry.TryGet<MobileStatic>(entity);
	if (statics == nullptr || statics->groundedDrop > 0.0f)
	{
		return;
	}
	const float gap = FloatingGap(entity);
	if (gap <= 0.0f)
	{
		return;
	}
	registry.Get<Transform>(entity).position.y -= gap;
	statics->groundedDrop = gap;
	registry.SetDirty();
}

void StaticGrounding::Unground(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* statics = registry.TryGet<MobileStatic>(entity);
	if (statics == nullptr || statics->groundedDrop <= 0.0f)
	{
		return;
	}
	registry.Get<Transform>(entity).position.y += statics->groundedDrop;
	statics->groundedDrop = 0.0f;
	registry.SetDirty();
}

void StaticGrounding::ApplyToAll(bool ground)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> entities;
	registry.Each<const MobileStatic>([&entities](entt::entity entity, const MobileStatic&) { entities.push_back(entity); });
	for (const auto entity : entities)
	{
		if (ground)
		{
			Ground(entity);
		}
		else
		{
			Unground(entity);
		}
	}
}
