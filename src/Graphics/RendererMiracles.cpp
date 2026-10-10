/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// How the miracles look outside their effects, as the renderer draws them: the one-shot globes' draws, each queued in
// the frame's single queue keyed where it is sorted, and the hand's glow, inside the hand's own Z object. The list of
// draws stays empty, the bubbles, the seeds inside them and the bands being entities queued with the other models, and
// nothing hands the renderer a glow, so nothing here reaches bgfx.

#include <optional>
#include <span>

#include "Camera/Camera.h"
#include "Graphics/Globes.h"
#include "Graphics/HandGlow.h"
#include "Graphics/ZObject.h"
#include "Graphics/ZSort.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawGlobes(const DrawSceneDesc& desc, zsort::Queue<ZObject>& queue) const
{
	// the world view only: the reflection draws no globe of this list
	if (desc.viewId != RenderPass::Main || desc.camera == nullptr)
	{
		return;
	}
	globes::Submit(std::span<const globes::GlobeDraw>(_frameGlobes), queue, desc.camera->GetOrigin(),
	               [](int i) { return ZObject {.globe = i}; });
}

void Renderer::DrawHandGlow(RenderPass viewId, const std::optional<HandGlowDraw>& glow) const
{
	// no miracle in the hand, or no glow handed in (none is, today)
	if (!glow.has_value() || glow->alpha <= 0.0f)
	{
		return;
	}
	// The pass itself, the hand's skinned mesh again over itself, added in glow->colour at glow->alpha with the flowing
	// texture's cell glow->uvOffset and its alpha sheet, into viewId, needs a program with a texture of its own and a UV
	// transform, which the object program does not have. Until it exists a glow handed in draws nothing
	static_cast<void>(viewId);
}
