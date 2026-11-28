/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIconEffect.h"

#include <cmath>

#include <algorithm>
#include <utility>

using namespace openblack;

graphics::GestureIconEffect
graphics::IconEffectOf(std::vector<particles::Effect::OrderedItem> items,
                       const std::function<std::optional<mists::MistDesc>(const particles::Effect::DrawAtom&)>& mistOf,
                       const std::function<std::optional<GestureIconModelOverlay>(const particles::Effect::DrawAtom&)>& meshOf)
{
	GestureIconEffect effect;
	for (auto& item : items)
	{
		const auto* creator = item.atom.creator;
		if (item.chain >= 0 || creator == nullptr)
		{
			continue;
		}
		if (creator->kind == particles::Creator::Kind::Sprite && !item.atom.objectGlow.has_value())
		{
			// a run of sprites goes on only after sprites: a mist or a mesh before them starts a new draw
			if (effect.draws.empty() || effect.draws.back().mist.has_value() || effect.draws.back().mesh.has_value())
			{
				effect.draws.emplace_back();
			}
			effect.draws.back().sprites.push_back(std::move(item.atom));
			continue;
		}
		if (creator->kind == particles::Creator::Kind::Mesh)
		{
			if (auto mesh = meshOf(item.atom); mesh.has_value())
			{
				effect.draws.push_back({.mesh = *mesh});
			}
			continue;
		}
		if (auto mist = mistOf(item.atom); mist.has_value())
		{
			effect.draws.push_back({.mist = *mist});
		}
	}
	return effect;
}

std::optional<graphics::GestureIconModelOverlay> graphics::IconMeshOf(const particles::mesh_atoms::Instance& instance)
{
	const auto colour = (static_cast<uint32_t>(instance.colour[0]) << 16u) | (static_cast<uint32_t>(instance.colour[1]) << 8u) |
	                    static_cast<uint32_t>(instance.colour[2]);
	return GestureIconModelOverlay {
	    .mesh = instance.meshId,
	    .matrix = instance.model,
	    .uv = instance.uv,
	    .alpha = static_cast<uint8_t>(std::lround(std::clamp(instance.alpha, 0.0f, 1.0f) * 255.0f)),
	    .colour = colour,
	    .specular = instance.specular & 0xFFFFFFu,
	    .lit = instance.lit,
	    .landTint = instance.landscapeColour,
	    .fading = instance.globalAlpha,
	};
}
