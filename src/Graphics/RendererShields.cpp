/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The physical shields' domes as the shield service hands them to the renderer: each one queued in the frame's single
// queue, keyed where it stands. The service hands out none, the domes being map objects queued with the other models,
// so nothing here reaches bgfx.

#include <cstdio>
#include <cstdlib>

#include "Camera/Camera.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "Graphics/ShieldDomes.h"
#include "Graphics/ZObject.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The shield service the renderer reads the domes from (Locator::magicShieldSystem). Without one the game stops with
/// a message
const ecs::systems::MagicShieldSystemInterface& ShieldSource()
{
	if (!Locator::magicShieldSystem::has_value())
	{
		std::fputs("renderer: no shield service in the locator (Locator::magicShieldSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicShieldSystem::value();
}
} // namespace

void Renderer::DrawShieldDomes(const DrawSceneDesc& desc, zsort::Queue<ZObject>& queue) const
{
	_frameShieldDomes.clear();
	// the world view only: the reflection draws no dome
	if (desc.viewId != RenderPass::Main || desc.camera == nullptr)
	{
		return;
	}
	_frameShieldDomes = shield_domes::Submit(ShieldSource(), desc.clock.turnFraction, queue, desc.camera->GetOrigin(),
	                                         [](int i) { return ZObject {.shieldDome = i}; });
}
