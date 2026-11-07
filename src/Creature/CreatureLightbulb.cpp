/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLightbulb.h"

#include <cmath>

#include <algorithm>
#include <format>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

using namespace openblack;
using namespace openblack::creature_lightbulb;

bool creature_lightbulb::MayMake(uint32_t lastTurn, uint32_t turn)
{
	return turn - lastTurn > k_TurnsBetween;
}

std::u16string creature_lightbulb::Text(float progress)
{
	const auto text = std::format("{:.0f}%", static_cast<double>(progress * 100.0f));
	return {text.begin(), text.end()};
}

Bulb creature_lightbulb::Make(const glm::vec3& head, float creatureHeight, float progress)
{
	return Bulb {.position = head + glm::vec3(0.0f, k_AboveHead, 0.0f),
	             .size = creatureHeight * k_HeightShare,
	             .opacity = 1.0f,
	             .ageMs = 0,
	             .text = Text(progress)};
}

std::optional<Look> creature_lightbulb::Step(Bulb& bulb, uint32_t milliseconds)
{
	bulb.ageMs += milliseconds;
	const auto age = static_cast<float>(bulb.ageMs);
	if (age >= k_LifeMs)
	{
		return std::nullopt;
	}
	const auto scale = (age / k_LifeMs + 1.0f) * bulb.size;
	int32_t alpha = 0xFF;
	if (age >= k_FadeStartMs)
	{
		alpha = static_cast<int32_t>(((1.0f - (age - k_FadeStartMs) / k_FadeMs) * 255.0f) * bulb.opacity);
	}
	bulb.position.y += (static_cast<float>(milliseconds) * k_RiseSpeed) * bulb.size;
	// It grows no smaller than this, as the sprite's size is kept
	const auto glowSize = std::max(scale, 0.0001f);
	const auto glow =
	    static_cast<int32_t>(std::fabs(std::cos(2.0f * std::numbers::pi_v<float> * k_Blinks * age / k_LifeMs)) * 255.0f);
	return Look {
	    .glow = bulb.position,
	    .glowSize = glowSize,
	    .bulb = bulb.position - glm::vec3(0.0f, scale * k_BulbHeight / k_BulbScale, 0.0f),
	    .bulbScale = scale * (1.0f / k_BulbScale),
	    .text = bulb.position + glm::vec3(0.0f, scale * k_TextHeight, 0.0f),
	    .alpha = static_cast<uint8_t>(std::clamp(alpha, 0, 0xFF)),
	    .blink = static_cast<uint8_t>(std::clamp(glow, 0, 0xFF)),
	    .glowAlpha = static_cast<uint8_t>((0xFF - std::clamp(glow, 0, 0xFF)) / 2),
	};
}

std::vector<Look> creature_lightbulb::StepAll(std::vector<Bulb>& bulbs, uint32_t milliseconds)
{
	std::vector<Look> looks;
	looks.reserve(bulbs.size());
	std::erase_if(bulbs, [&looks, milliseconds](Bulb& bulb) {
		const auto look = Step(bulb, milliseconds);
		if (look.has_value())
		{
			looks.push_back(*look);
		}
		return !look.has_value();
	});
	return looks;
}

glm::mat4 creature_lightbulb::BulbModel(const Look& look)
{
	return glm::scale(glm::translate(glm::mat4(1.0f), look.bulb), glm::vec3(look.bulbScale));
}

std::optional<glm::vec2> creature_lightbulb::ScreenPoint(const glm::vec4& clip, float nearPlane, glm::vec2 resolution)
{
	if (clip.w < nearPlane)
	{
		return std::nullopt;
	}
	const float rhw = 1.0f / clip.w;
	return glm::vec2((clip.x * rhw + 1.0f) * (resolution.x * 0.5f), (1.0f - clip.y * rhw) * (resolution.y * 0.5f));
}

glm::vec2 creature_lightbulb::TextPlace(glm::vec2 screenPoint, float textWidth)
{
	return {screenPoint.x - textWidth * 0.5f, screenPoint.y - k_TextOffset};
}

std::optional<float> creature_lightbulb::ForAction(bool learnt, bool knownBefore, bool tooYoung)
{
	if (tooYoung || knownBefore)
	{
		return std::nullopt;
	}
	return learnt ? 1.0f : k_NearlyLearntNormal;
}

std::optional<float> creature_lightbulb::ForMiracle(bool learnt, float seen, float needed)
{
	if (learnt)
	{
		return seen - needed < k_RecentlyLearntSightings ? std::optional(1.0f) : std::nullopt;
	}
	return needed > 0.0f ? std::min(1.0f, seen / needed) : 1.0f;
}
