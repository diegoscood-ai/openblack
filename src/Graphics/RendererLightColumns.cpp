/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

// The columns of light (Graphics/LightColumn.h) in the world view: once a frame every village centre and every temple
// whose owner's influence border shows gets one, turned to face the eye, and each on screen goes to the transparency
// queue on its own, where it is added over the scene in its owner's colour. Nothing inside the temple, where the world
// is not drawn. The column's matrix, mesh and strengths are checked against v1.42.

#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <unordered_map>

#include <bgfx/bgfx.h>
#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/LightColumn.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Particles/Rules/SurfRevol.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
using region_on_screen::BoxInView;

/// The column of the mesh pack
entt::id_type ColumnMesh()
{
	return resources::HashIdentifier(MeshId::SpellColumn);
}

/// The owner's colour, 0xRRGGBB, from the players' colour table the town symbols use
uint32_t ColourOf(PlayerNames player)
{
	return particles::surf_revol::PlayerColour(static_cast<int>(player)) & 0xFFFFFFu;
}
} // namespace

void Renderer::CollectLightColumns(const DrawSceneDesc& drawDesc) const
{
	_frameLightColumns.clear();
	if (drawDesc.camera == nullptr || !Locator::influenceSystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto columnMesh = ColumnMesh();
	if (!meshes.Contains(columnMesh))
	{
		return;
	}
	using ecs::components::Abode;
	using ecs::components::Temple;
	using ecs::components::Town;
	using ecs::components::Transform;
	const auto& columnBox = meshes.Handle(columnMesh)->GetBoundingBox();
	const auto& influence = Locator::influenceSystem::value();
	const auto eye = drawDesc.camera->GetOrigin();
	const auto viewProjection = drawDesc.camera->GetViewProjectionMatrix();

	const auto add = [&](const glm::vec3& position, PlayerNames owner, uint8_t alpha) {
		const auto model = light_column::Model(position, eye);
		// (inferred) a column off screen is left out, as the leash posts' collars are
		if (BoxInView(viewProjection, columnBox, model))
		{
			_frameLightColumns.push_back({.model = model, .colour = ColourOf(owner), .alpha = alpha});
		}
	};

	// Over each village centre, at the middle of its model, in its town's player's colour
	std::unordered_map<uint32_t, PlayerNames> owners;
	drawDesc.entities.Each<const Town>([&owners](const Town& town) { owners.emplace(town.id, town.owner); });
	drawDesc.entities.Each<const Abode, const Transform, const ecs::components::Mesh>(
	    [&](const Abode& abode, const Transform& transform, const ecs::components::Mesh& mesh) {
		    if (abode.type != AbodeNumber::TownCentre)
		    {
			    return;
		    }
		    const auto owner = owners.find(abode.townId);
		    if (owner == owners.end() || !light_column::Shown(owner->second, influence.IsBorderShown(owner->second)) ||
		        !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto centre = meshes.Handle(mesh.id)->GetBoundingBox().Center();
		    add(glm::vec3(affine::Model(transform) * glm::vec4(centre, 1.0f)), owner->second, light_column::k_TownAlpha);
	    });

	// Over each temple, where its heart stands, beating with its player's heart
	const float turnFraction = drawDesc.clock.turnFraction;
	drawDesc.entities.Each<const Temple, const Transform>([&](const Temple& temple, const Transform& transform) {
		if (!light_column::Shown(temple.owner, influence.IsBorderShown(temple.owner)) ||
		    !light_column::TempleShown(temple.destroying, temple.destructionClock))
		{
			return;
		}
		const float pulse =
		    Locator::heartbeatSystem::has_value() ? Locator::heartbeatSystem::value().Pulse(temple.owner, turnFraction) : 0.0f;
		add(transform.position, temple.owner, light_column::TempleAlpha(pulse));
	});
}

void Renderer::DrawLightColumn(RenderPass viewId, uint32_t index) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto columnMesh = ColumnMesh();
	if (index >= _frameLightColumns.size() || !meshes.Contains(columnMesh))
	{
		return;
	}
	const auto& column = _frameLightColumns[index];
	// the column's own instance row: its matrix, its strength as the fade (1 - alpha) in the w of the first column, and
	// its owner's colour set on it unlit in the fifth (argb_colour::PackInstanceColour / PackInstanceUnlit)
	std::array<glm::vec4, 5> row {column.model[0], column.model[1], column.model[2], column.model[3], glm::vec4(0.0f)};
	row[0].w = 1.0f - static_cast<float>(column.alpha) / 255.0f;
	row[1].w = 0.0f;
	argb_colour::PackInstanceColour(row[4], column.colour);
	argb_colour::PackInstanceUnlit(row[4]);
	constexpr auto k_Stride = static_cast<uint16_t>(sizeof(row));
	if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
	{
		return; // (openblack guard) no room left this frame
	}
	bgfx::InstanceDataBuffer instance {};
	bgfx::allocInstanceDataBuffer(&instance, 1, k_Stride);
	std::memcpy(instance.data, row.data(), sizeof(row));
	const glm::mat4 identity(1.0f);
	L3DMeshSubmitDesc submit {};
	submit.viewId = viewId;
	submit.program = _shaderManager->GetShader("ObjectInstanced");
	submit.options = render_modes::StateOptions {.msaa = true};
	// added over what is behind by its texture's alpha times its strength (SRCALPHA / ONE), tested against the depth
	// but writing none: mode 13 for every primitive
	submit.mode = render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite;
	submit.modelMatrices = &identity;
	submit.matrixCount = 1;
	submit.transientInstance = &instance;
	DrawMesh(*meshes.Handle(columnMesh), submit, std::numeric_limits<uint8_t>::max());
}
