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
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/Registry.h"
#include "ECS/SnowLevels.h"
#include "ECS/Systems/FieldSystemInterface.h"
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
	// a bird tests the screen with the matrix its draw rolls by its bank, for the draw alone
	auto placed = object_placement::Of(registry, object);
	if (const auto banked = BankedForItsDraw(registry, object); banked.has_value())
	{
		placed.cells = *banked;
	}
	return OnScreenInputsFrom(placed, meshes.Handle(mesh->id)->GetBoundingBox(), registry.AllOf<components::DontDraw>(object));
}

bool DrawOnScreen(const std::optional<OnScreenInputs>& inputs, const OnScreenView& view)
{
	return !inputs.has_value() || OnScreen(*inputs, view);
}

std::optional<bool> DrawField(const Registry& registry, systems::FieldSystemInterface& fields, entt::entity object,
                              uint32_t frameGameMs, const OnScreenView& view)
{
	if (!registry.AllOf<components::Field>(object))
	{
		return std::nullopt;
	}
	if (!fields.Draw(object, frameGameMs))
	{
		return true;
	}
	// a crop that shows takes its step of the snow where it stands
	snow_levels::Drawn(object, frameGameMs);
	// the crop is queued with its matrix risen by the sink, so its screen test is made there
	auto inputs = OnScreenInputsOf(registry, object);
	if (inputs.has_value())
	{
		const float rise = registry.Get<const components::Field>(object).drawnRise;
		inputs->boxCentreWorld.y += rise;
		inputs->origin.y += rise;
	}
	return DrawOnScreen(inputs, view);
}

} // namespace openblack::ecs::draw_list
