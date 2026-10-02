/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The projected shadows (graphics::shadow_list, the ShadowInfo list [0xFAA7E0]; wiki: rendering.md, "Sombras
// proyectadas"): their update once a frame, their draw over the land blocks (fn_007FF610 -> fn_00878350) and over the
// objects (the tail loop of the objects' Draw, fn_0080B050).

#include <memory>

#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "Camera/Camera.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/ShadowList.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::UpdateShadows(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !drawDesc.drawEntities)
	{
		_shadows->Clear();
		return;
	}
	shadow_list::FrameInputs inputs;
	inputs.camera = drawDesc.camera->GetOrigin();                    // g_camera [0xEA1DB8]
	inputs.worldToClip = drawDesc.camera->GetViewProjectionMatrix(); // g_world_to_clipping [0xEA9E40]
	inputs.nearW = billboard::CameraFrame::From(*drawDesc.camera).nearZ; // [0xE839E0]
	inputs.landRef = GetDetailLevel(Locator::config::value().detailLevel).landReflection; // [0xE9CD8C]
	_shadows->Frame(inputs);
}
