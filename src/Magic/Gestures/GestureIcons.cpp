/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIcons.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Locator.h"

namespace openblack::magic::gestures
{
namespace
{
/// The game's FPU rounds every step to a float: each one is its own statement here, so that none is fused
constexpr float k_SlotStepA = 0.005f;
constexpr float k_SlotStepB = 0.035f;
constexpr float k_SlotRightMargin = 0.015f;
constexpr float k_FadePerMs = 2.0f;
constexpr float k_MsToSeconds = 0.001f;
constexpr float k_PhaseBack = 0.1f;

/// Truncated towards zero, as the game converts a float to an integer
int32_t Truncate(float value)
{
	return static_cast<int32_t>(value);
}

/// Each byte of a colour times a factor 0..255, over 256
uint32_t ScaleByte(uint32_t argb, int shift, uint32_t factor)
{
	return (((argb >> shift) & 0xFFu) * factor >> 8u) << shift;
}
} // namespace

IconBox IconBoxOf(int slot, glm::ivec2 screen)
{
	const auto width = static_cast<float>(screen.x);
	const auto height = static_cast<float>(screen.y);
	const auto index = static_cast<float>(slot);
	// the left edge: 1 - ((0.005 + 0.035) slot + 0.035 + 0.015), times the width
	float step = k_SlotStepA + k_SlotStepB;
	float left = step * index;
	left = left + k_SlotStepB;
	left = left + k_SlotRightMargin;
	left = 1.0f - left;
	left = left * width;
	// the right edge: 1 - ((0.005 + 0.035) slot + 0.015), times the width
	step = k_SlotStepA + k_SlotStepB;
	float right = step * index;
	right = right + k_SlotRightMargin;
	right = 1.0f - right;
	right = right * width;
	// the top: the height less 0.035 of the width
	float side = width * k_SlotStepB;
	const float top = height - side;
	return {.min = {left, top}, .max = {right, height}};
}

glm::ivec2 IconPixel(const IconBox& box, float u, float v)
{
	float x = box.max.x - box.min.x;
	x = x * u;
	x = x + box.min.x;
	float y = box.max.y - box.min.y;
	y = y * v;
	y = y + box.min.y;
	return {Truncate(x), Truncate(y)};
}

LookingForArray GestureIconList::NothingLookedFor()
{
	LookingForArray lf;
	lf.fill(LookingFor {0, -1, -1, -1});
	return lf;
}

bool GestureIconList::Update(const LookingForArray& lf, glm::ivec2 screen, uint32_t playerRgb,
                             const std::function<float(Gesture)>& shapeLength)
{
	const auto same = [](const LookingFor& a, const LookingFor& b) {
		return a.type == b.type && a.seed == b.seed && a.leash == b.leash && a.powerUp == b.powerUp;
	};
	if (std::ranges::equal(lf, _kept, same))
	{
		return false;
	}
	_icons.clear();
	_kept = lf;
	const float circle = shapeLength(k_Circle);
	for (size_t g = 0; g < lf.size(); ++g)
	{
		const auto& entry = lf.at(g);
		if (entry.type == 0)
		{
			continue;
		}
		// before the first icon whose type is not above this one's
		const auto at = std::ranges::find_if(_icons, [&entry](const GestureIcon& icon) { return icon.order <= entry.type; });
		GestureIcon icon {
		    .gesture = static_cast<Gesture>(g),
		    .order = entry.type,
		    .seed = entry.seed,
		    .powerUp = entry.powerUp,
		    .leash = entry.leash,
		    .symbolArgb = (playerRgb & 0x00FFFFFFu) | 0xFF000000u,
		};
		// the shape's length against the circle's, twice
		float period = shapeLength(icon.gesture);
		period = period / circle;
		icon.period = period * 2.0f;
		_icons.insert(at, icon);
	}
	// each one placed in its slot of the list
	for (size_t slot = 0; slot < _icons.size(); ++slot)
	{
		_icons[slot].box = IconBoxOf(static_cast<int>(slot), screen);
	}
	return true;
}

void GestureIconList::Clear()
{
	_icons.clear();
	_kept = NothingLookedFor();
}

bool StepIconsAlpha(float& alpha, bool helpSystem, bool helpTextShown, uint32_t frameMs)
{
	if (helpSystem)
	{
		float step = static_cast<float>(static_cast<int32_t>(frameMs)) * k_FadePerMs;
		step = step * k_MsToSeconds;
		if (alpha > 0.0f && helpTextShown)
		{
			alpha = alpha - step;
			if (!(alpha > 0.0f))
			{
				alpha = 0.0f;
				return false;
			}
		}
		else if (alpha < 1.0f && !helpTextShown)
		{
			alpha = step + alpha;
			if (!(alpha < 1.0f))
			{
				alpha = 1.0f;
				return true;
			}
		}
	}
	return alpha != 0.0f;
}

IconSparkles StepIconSparkles(GestureIcon& icon, uint32_t frameMs)
{
	const float seconds = static_cast<float>(static_cast<int32_t>(frameMs)) * k_MsToSeconds;
	icon.seconds = seconds + icon.seconds;
	float phase = seconds / icon.period;
	phase = phase + icon.phase;
	const float back = 1.0f - k_PhaseBack;
	if (phase > 1.0f)
	{
		phase = phase - back;
	}
	icon.phase = std::fmod(phase, 1.0f);
	IconSparkles sparkles {.at = {icon.phase, 0.0f}, .count = 1};
	if (back < icon.phase)
	{
		sparkles.at[1] = icon.phase - back;
		sparkles.count = 2;
	}
	return sparkles;
}

uint32_t IconSymbolArgb(uint32_t argb, float alpha)
{
	float scaled = alpha * 255.0f;
	const auto factor = static_cast<uint32_t>(Truncate(scaled)) & 0xFFu;
	return ScaleByte(argb, 24, factor) | ScaleByte(argb, 16, factor) | ScaleByte(argb, 8, factor) | ScaleByte(argb, 0, factor);
}

uint32_t IconSparkleArgb(float t, float alpha)
{
	// 1 - (2t - 1)^2, at most 1
	float d = t + t;
	d = d - 1.0f;
	d = d * d;
	float fade = 1.0f - d;
	fade = std::min(fade, 1.0f);
	fade = fade * alpha;
	fade = fade * 255.0f;
	const auto factor = static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(fade)));
	return (k_IconSparkleArgb & 0xFF000000u) | ScaleByte(k_IconSparkleArgb, 16, factor) |
	       ScaleByte(k_IconSparkleArgb, 8, factor) | ScaleByte(k_IconSparkleArgb, 0, factor);
}

glm::vec3 ShapePointAt(const std::vector<glm::vec3>& points, float t)
{
	if (points.empty())
	{
		return glm::vec3(0.0f);
	}
	if (!(t > 0.0f))
	{
		t = 0.0f;
	}
	else if (!(t < 1.0f) || points.size() < 2)
	{
		return points.back();
	}
	float along = static_cast<float>(static_cast<int32_t>(points.size())) - 1.0f;
	along = along * t;
	const auto index = static_cast<size_t>(std::clamp(Truncate(along), 0, static_cast<int32_t>(points.size()) - 2));
	const float fraction = along - static_cast<float>(index);
	const auto& from = points[index];
	const auto& to = points[index + 1];
	glm::vec3 point;
	for (int axis = 0; axis < 3; ++axis)
	{
		float d = to[axis] - from[axis];
		d = d * fraction;
		point[axis] = d + from[axis];
	}
	return point;
}

std::vector<GestureIconDraw> StepIcons(std::span<GestureIcon> icons, float& alpha, bool helpSystem, bool helpTextShown,
                                       uint32_t frameMs, const std::function<const std::vector<glm::vec3>&(Gesture)>& shape)
{
	std::vector<GestureIconDraw> draws;
	if (!StepIconsAlpha(alpha, helpSystem, helpTextShown, frameMs))
	{
		return draws;
	}
	for (size_t index = 0; index < icons.size(); ++index)
	{
		auto& icon = icons[index];
		const auto& points = shape(icon.gesture);
		if (points.empty())
		{
			continue;
		}
		GestureIconDraw draw {
		    .icon = index,
		    .page = static_cast<uint8_t>(icon.gesture / 16),
		    .cell = static_cast<uint8_t>(icon.gesture % 16),
		    .symbolPixel = IconPixel(icon.box, 0.5f, 0.5f),
		    .symbolArgb = IconSymbolArgb(icon.symbolArgb, alpha),
		};
		const auto sparkles = StepIconSparkles(icon, frameMs);
		draw.sparkles = sparkles.count;
		for (int i = 0; i < sparkles.count; ++i)
		{
			const auto t = sparkles.at.at(static_cast<size_t>(i));
			const auto point = ShapePointAt(points, t);
			draw.sparklePixels.at(static_cast<size_t>(i)) = IconPixel(icon.box, point.x, point.z);
			draw.sparkleArgb.at(static_cast<size_t>(i)) = IconSparkleArgb(t, alpha);
		}
		draws.push_back(draw);
	}
	return draws;
}

GestureIconState& IconState()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("magic::gestures: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<GestureIconState>();
}
} // namespace openblack::magic::gestures
