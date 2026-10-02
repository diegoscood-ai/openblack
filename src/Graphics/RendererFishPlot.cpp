/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The fish puzzle's nets (FishPlot, 0x829A30..0x829D54): one static LH3DObject of Data\MISC\Fishplot.l3d moved to each
// of the 7 floats and drawn cut by the plane, SetPosition(float, 0, 1.0) + DrawCutByPlane (vt+0x11C, fn_0080C050).
// Under the water (fn_00829BC0, from the shoals' fn_00824B90 before the sea) between SetClipPlane(0, -1, 0, 0) and
// SetClipPlane(0, 1, 0, 0); over the water (fn_00829B50, from fn_00824D60 in fn_005E5CD0) with the default plane.

#include <limits>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/FishPuzzle.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/InstanceDesc.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The object's +0x4C colour that DrawCutByPlane (fn_0080C050 0x80C0FD -> [0xC37D8C]) lights per vertex: the
/// LH3DMeshedObject default 0xFFFFFFFF (ctor 0x8164F7). The FishPlot ctor 0x829A30 never calls SetColour (vt+0x2C,
/// fn_007F9770); UseDynamicLighting (vt+0x58, fn_008168C0) only sets bit 0x20 of +4; and the only other writers of
/// +0x4C are the draws at vt+0x100 / +0x110 / +0x130 / +0x154, which the net never goes through (fn_00829B50 /
/// fn_00829BC0 call only SetPosition vt+0x20 and DrawCutByPlane vt+0x11C).
constexpr uint32_t k_NetColour = 0xFFFFFFFFu;
} // namespace

void Renderer::DrawFishPlots(RenderPass viewId, int8_t keep) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::vector<glm::mat4> instances;
	entt::id_type meshId = 0;
	Locator::entitiesRegistry::value().Each<const ecs::components::FishBait>(
	    [&instances, &meshId, &meshes](const ecs::components::FishBait& bait) {
		    if (bait.net.mesh == 0 || !meshes.Contains(bait.net.mesh))
		    {
			    return;
		    }
		    meshId = bait.net.mesh;
		    // SetPosition(float, angle 0, scale 1.0)
		    for (const auto& point : ecs::FishPlotFloats(bait.net))
		    {
			    instances.push_back(glm::translate(glm::mat4(1.0f), point));
		    }
	    });
	if (instances.empty())
	{
		return;
	}
	const auto count = static_cast<uint32_t>(instances.size());
	if (!bgfx::isValid(_fishPlotInstances) || _fishPlotCapacity < count)
	{
		if (bgfx::isValid(_fishPlotInstances))
		{
			bgfx::destroy(_fishPlotInstances);
		}
		// the layout of RenderingSystem's instances: the 4 columns of the model matrix
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .end();
		_fishPlotCapacity = count + 14;
		_fishPlotInstances = bgfx::createDynamicVertexBuffer(_fishPlotCapacity, layout);
	}
	// both passes of the frame write the same floats (the mirroring is the shader's)
	bgfx::update(_fishPlotInstances, 0, bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(glm::mat4))));

	const auto mesh = meshes.Handle(meshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	submitDesc.options = render_modes::k_ModelPass;
	submitDesc.cutByPlane = keep;
	submitDesc.cutColour = k_NetColour;
	// the part under the water goes into the reflection target, which shows through the sea (see DrawPass)
	submitDesc.mirrorInSea = keep < 0;
	submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(fromBgfx(_fishPlotInstances), 0, count);
	static const auto k_Identity = glm::mat4(1.0f);
	submitDesc.modelMatrices = &k_Identity;
	submitDesc.matrixCount = 1;
	submitDesc.program = _shaderManager->GetShader("ObjectInstanced");
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}
