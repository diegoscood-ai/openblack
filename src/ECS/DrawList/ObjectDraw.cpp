/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectDraw.h"

#include "3D/AffineMatrix.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::draw_list
{

OnScreenInputs OnScreenInputsFrom(const object_placement::Placed& placed, const AxisAlignedBoundingBox& box, bool dontDraw)
{
	const glm::vec3 centre = affine::BoxCentreThroughObject(affine::FromModel(placed.cells), box.Center());
	return {
	    .boxCentreWorld = centre,
	    .origin = glm::vec3(placed.cells[3]),
	    // the placement's one scale times the mesh's half diagonal, as the field of view's test takes it
	    .radius = placed.scale * object::HalfDiagonal(object::HalfExtents(box)),
	    .dontDraw = dontDraw,
	};
}

std::optional<OnScreenInputs> OnScreenInputsOf(const Registry& registry, entt::entity object)
{
	const auto* transform = registry.TryGet<const components::Transform>(object);
	const auto* mesh = registry.TryGet<const components::Mesh>(object);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	return OnScreenInputsFrom(object_placement::Of(registry, object), meshes.Handle(mesh->id)->GetBoundingBox(),
	                          registry.AllOf<components::DontDraw>(object));
}

bool DrawOnScreen(const std::optional<OnScreenInputs>& inputs, const OnScreenView& view)
{
	return !inputs.has_value() || OnScreen(*inputs, view);
}

} // namespace openblack::ecs::draw_list
