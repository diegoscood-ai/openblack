/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MistFades.h"

#include <cstdint>

#include <bit>
#include <utility>
#include <vector>

#include "ECS/Components/Mist.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs::mist_fade
{
using components::Mist;
using components::MistFade;

namespace
{
constexpr float k_TurnsPerSecond = 10.0f;
constexpr float k_MostAlpha = 255.0f;
/// What the x87 gives for nought over nought: the default not-a-number, negative and quiet
constexpr uint32_t k_NoughtOverNought = 0xFFC00000U;

/// The mist's colour with its alpha byte replaced
void SetAlpha(Mist& mist, float alpha)
{
	mist.colour = (mist.colour & 0x00FFFFFFU) | (AlphaOf(alpha) << 24U);
}
} // namespace

MistFade Start(float startSize, float endSize, float startAlpha, float endAlpha, float seconds)
{
	const float steps = seconds * k_TurnsPerSecond;
	return {
	    .steps = steps,
	    .taken = 0.0f,
	    .sizeStep = (endSize - startSize) / steps,
	    .alpha = startAlpha,
	    .alphaStep = (endAlpha - startAlpha) / steps,
	};
}

uint32_t AlphaOf(float alpha)
{
	// truncated, and only its low byte kept as the colour's alpha
	return static_cast<uint32_t>(static_cast<int32_t>(alpha)) & 0xFFU;
}

bool Step(MistFade& fade, float& size)
{
	const float next = fade.taken + 1.0f;
	if (next > fade.steps)
	{
		return false;
	}
	fade.taken = next;
	size = fade.sizeStep + size;
	fade.alpha = fade.alphaStep + fade.alpha;
	// below nought (or not a number) is nought, above 255 is 255
	if (!(fade.alpha >= 0.0f))
	{
		fade.alpha = 0.0f;
	}
	if (fade.alpha > k_MostAlpha)
	{
		fade.alpha = k_MostAlpha;
	}
	return true;
}

void Set(entt::entity mist, float startSize, float endSize, float startAlpha, float endAlpha, float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* bank = registry.Valid(mist) ? registry.TryGet<Mist>(mist) : nullptr;
	if (bank == nullptr)
	{
		return;
	}
	const auto fade = Start(startSize, endSize, startAlpha, endAlpha, seconds);
	bank->size = startSize;
	SetAlpha(*bank, fade.alpha);
	registry.AssignOrReplace<MistFade>(mist, fade);
}

void ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	// read through the const registry, which makes no storage while no script has faded a mist
	std::vector<entt::entity> fading;
	std::as_const(registry).Each<const MistFade>(
	    [&fading](entt::entity mist, const MistFade& /*fade*/) { fading.push_back(mist); });
	for (const auto mist : fading)
	{
		auto& fade = registry.Get<MistFade>(mist);
		auto* bank = registry.TryGet<Mist>(mist);
		if (bank == nullptr)
		{
			registry.Remove<MistFade>(mist);
			continue;
		}
		// a fade that is over takes no more steps, and stays for the scripts to read
		if (Step(fade, bank->size))
		{
			SetAlpha(*bank, fade.alpha);
		}
	}
}
float Progress(const MistFade* fade)
{
	if (fade == nullptr)
	{
		return std::bit_cast<float>(k_NoughtOverNought);
	}
	return fade->taken / fade->steps;
}
} // namespace openblack::ecs::mist_fade
