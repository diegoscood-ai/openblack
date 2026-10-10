/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The points of an object's model that glints sparkle on: those of a script highlight's mesh as it is drawn, and those
// of a spell seed graphic's mesh through the matrix it keeps for its glints. Every vertex of every part of the mesh, in
// the file's order. Wiki: docs/bw1-notes/particles.md, "The glints on a target".

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "GlintTargets.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::particles::maths::GlintModelPart;

namespace
{
/// Every part of a mesh in the resource cache, in the file's order; none when it is not loaded
std::vector<GlintModelPart> CachedModelParts(entt::id_type meshId)
{
	std::vector<GlintModelPart> parts;
	if (!Locator::resources::has_value())
	{
		return parts;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId))
	{
		return parts;
	}
	const auto mesh = meshes.Handle(meshId);
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		parts.push_back({
		    .positions = subMesh->GetSkinLocalPositions(),
		    .ranges = subMesh->GetCollisionVertexRanges(),
		});
	}
	return parts;
}

const ecs::Registry* GameRegistry()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}
} // namespace

std::optional<GlintTargets::Source> GlintTargets::SourceOf(const Registry& registry, entt::entity object)
{
	if (object == entt::null || !registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto* mesh = registry.TryGet<const components::Mesh>(object);
	if (mesh == nullptr)
	{
		return std::nullopt;
	}
	// A highlight's glints step after its mesh is drawn, so they read this frame's matrix
	if (registry.AllOf<components::ScriptHighlight, components::Transform>(object))
	{
		return Source {.mesh = mesh->id, .model = DrawnModel(registry, object, false)};
	}
	// A seed graphic's holder steps before the graphic is drawn, so its glints read the draw before
	if (const auto* graphic = registry.TryGet<const components::SpellSeedGraphic>(object))
	{
		return Source {.mesh = mesh->id, .model = graphic->glintModel};
	}
	return std::nullopt;
}

GlintTargets::GlintTargets(ModelParts parts)
    : _parts(parts ? std::move(parts) : ModelParts(CachedModelParts))
{
}

std::optional<glm::vec3> GlintTargets::ObjectPosition(entt::entity object) const
{
	const auto* registry = GameRegistry();
	if (registry == nullptr || !ecs::IsAvailable(object))
	{
		return std::nullopt;
	}
	const auto* transform = registry->TryGet<const components::Transform>(object);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}

uint32_t GlintTargets::TargetPointCount(entt::entity object) const
{
	const auto* registry = GameRegistry();
	const auto source = registry != nullptr ? SourceOf(*registry, object) : std::nullopt;
	if (!source.has_value())
	{
		return 0;
	}
	const auto parts = _parts(source->mesh);
	return particles::maths::GlintPointCount(parts);
}

std::optional<glm::vec3> GlintTargets::TargetPoint(entt::entity object, uint32_t index) const
{
	const auto* registry = GameRegistry();
	const auto source = registry != nullptr ? SourceOf(*registry, object) : std::nullopt;
	if (!source.has_value())
	{
		return std::nullopt;
	}
	const auto parts = _parts(source->mesh);
	const auto local = particles::maths::GlintLocalPoint(parts, index);
	if (!local.has_value())
	{
		return std::nullopt;
	}
	return particles::maths::GlintThroughModel(source->model, *local);
}

float GlintTargets::TargetScale(entt::entity object) const
{
	const auto* registry = GameRegistry();
	if (registry == nullptr || object == entt::null || !registry->Valid(object))
	{
		return 1.0f;
	}
	if (const auto* highlight = registry->TryGet<const components::ScriptHighlight>(object))
	{
		return highlight->scale;
	}
	if (const auto* graphic = registry->TryGet<const components::SpellSeedGraphic>(object))
	{
		return graphic->scale;
	}
	return 1.0f;
}
