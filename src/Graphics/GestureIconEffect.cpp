/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIconEffect.h"

#include <utility>

using namespace openblack;

graphics::GestureIconEffect
graphics::IconEffectOf(std::vector<particles::Effect::OrderedItem> items,
                       const std::function<std::optional<mists::MistDesc>(const particles::Effect::DrawAtom&)>& mistOf)
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
			if (effect.draws.empty() || effect.draws.back().mist.has_value())
			{
				effect.draws.emplace_back();
			}
			effect.draws.back().sprites.push_back(std::move(item.atom));
			continue;
		}
		if (auto mist = mistOf(item.atom); mist.has_value())
		{
			effect.draws.push_back({.mist = *mist});
		}
	}
	return effect;
}
