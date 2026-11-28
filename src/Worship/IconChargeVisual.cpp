/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IconChargeVisual.h"

#include <cmath>

#include "Graphics/ArgbColour.h"

using namespace openblack::worship;

namespace
{
/// The fraction while charging: (f + k_ChargingLift) / (k_ChargingLift + 1)
constexpr float k_ChargingLift = 0.2f;
/// The full icon's pulse: sin(k_FullPulseRate x phase), between k_FullLow and k_FullHigh of 255
constexpr float k_FullPulseRate = 12.566371f;
constexpr float k_FullLow = 0.1f;
constexpr float k_FullHigh = 0.6f;
constexpr float k_ByteScale = 255.0f;
/// The charging flashes: cos(k_FlashRate s) over the first k_FlashSeconds, k_FlashSpan above k_ChargingBase
constexpr float k_FlashRate = 8.0f;
constexpr float k_FlashSeconds = 1.5f;
constexpr float k_FlashSpan = 205.0f;
constexpr int k_ChargingBase = 50;
constexpr float k_Half = 0.5f;
constexpr float k_SecondsPerMs = 0.001f;
/// The sheet's points: the angle i x k_Turn x k_NinthOfATurn, on a circle of radius k_SheetRadius
constexpr float k_Turn = 6.2831855f;
constexpr float k_NinthOfATurn = 0.11111111f;
constexpr float k_SheetRadius = 1.0f;
/// The ring: t = (the phase - k_RingStart, taken round) x k_RingRate / k_RingLength
constexpr float k_RingStart = 0.1f;
constexpr float k_RingRate = 3.33f;
constexpr float k_RingLength = 1.0f;
/// Its scale from k_RingScaleFrom at the start to k_RingScaleTo at the end, times k_RingScale; its raise from
/// k_RingRaiseFrom to k_RingRaiseTo of the mesh's height; its alpha from k_RingAlphaFrom to k_RingAlphaTo
constexpr float k_RingScaleFrom = 1.0f;
constexpr float k_RingScaleTo = 0.0f;
constexpr float k_RingScale = 3.0f;
constexpr float k_RingRaiseFrom = 0.0f;
constexpr float k_RingRaiseTo = 0.1f;
constexpr float k_RingAlphaFrom = 0.0f;
constexpr float k_RingAlphaTo = 255.0f;
} // namespace

charge_visual::ChargeLevel charge_visual::Level(bool seedHeld, float store, float fraction, float needed, bool charging)
{
	ChargeLevel level;
	if (seedHeld)
	{
		level = {.fraction = 1.0f, .full = true};
	}
	else if (store != 0.0f && !std::isnan(store))
	{
		// full unless more is needed (a need that is not a number counts as full too)
		level = {.fraction = fraction, .full = !(needed > 0.0f)};
	}
	if (charging)
	{
		level.fraction = (level.fraction + k_ChargingLift) / (k_ChargingLift + 1.0f);
	}
	return level;
}

float charge_visual::ChargeSeconds(int32_t turnsSinceStart, float turnFraction, uint32_t msPerTurn)
{
	const float turns = static_cast<float>(turnsSinceStart) + turnFraction;
	return (turns * static_cast<float>(msPerTurn)) * k_SecondsPerMs;
}

std::optional<int> charge_visual::Brightness(bool full, bool charging, float phase, float chargeSeconds)
{
	if (full)
	{
		const float angle = phase * k_FullPulseRate;
		const auto wave = static_cast<float>(std::sin(static_cast<double>(angle)) + 1.0);
		const float level = (((wave * k_Half) * (k_FullHigh - k_FullLow)) + k_FullLow) * k_ByteScale;
		return static_cast<int>(std::nearbyint(level)) & 0xFF;
	}
	if (!charging)
	{
		return std::nullopt;
	}
	if (!(chargeSeconds < k_FlashSeconds))
	{
		return k_ChargingBase;
	}
	const float angle = k_FlashRate * chargeSeconds;
	const auto flash = static_cast<float>(1.0 - std::cos(static_cast<double>(angle)));
	float level = (1.0f - (chargeSeconds / k_FlashSeconds)) * (flash * k_Half);
	if (!(level > 0.0f))
	{
		level = 0.0f;
	}
	else if (!(level < 1.0f))
	{
		level = 1.0f;
	}
	// the sum is a byte's
	return (static_cast<int>(std::nearbyint(level * k_FlashSpan)) + k_ChargingBase) & 0xFF;
}

uint32_t charge_visual::Tint(uint32_t playerColour, bool full, bool charging, float phase, float chargeSeconds)
{
	const auto brightness = Brightness(full, charging, phase, chargeSeconds);
	if (!brightness.has_value())
	{
		return 0;
	}
	return argb_colour::ScaleRgbShift8KeepAlpha(playerColour, static_cast<uint32_t>(*brightness));
}

std::array<glm::vec3, charge_visual::k_SheetPoints> charge_visual::SheetPoints(const glm::mat4& model)
{
	const glm::vec3 x(model[0]);
	const glm::vec3 y(model[1]);
	const glm::vec3 z(model[2]);
	const glm::vec3 position(model[3]);
	std::array<glm::vec3, k_SheetPoints> points {};
	for (size_t i = 0; i < k_SheetPoints; ++i)
	{
		const float angle = (static_cast<float>(i) * k_Turn) * k_NinthOfATurn;
		const auto along = static_cast<float>(std::cos(static_cast<double>(angle))) * k_SheetRadius;
		const auto across = static_cast<float>(std::sin(static_cast<double>(angle))) * k_SheetRadius;
		// the point's local (along, 0, across), summed in the original's order: z, then y, then x, then the position
		for (int k = 0; k < 3; ++k)
		{
			points.at(i)[k] = (((across * z[k]) + (0.0f * y[k])) + (along * x[k])) + position[k];
		}
	}
	return points;
}

std::optional<charge_visual::RingPose> charge_visual::Ring(float phase, float meshHalfHeight)
{
	// the phase taken round past the ring's start, the turn after it when not beyond it
	const float wrapped = phase > k_RingStart ? phase : phase + 1.0f;
	const float t = ((wrapped - k_RingStart) * k_RingRate) / k_RingLength;
	if (!(t >= 0.0f && t <= 1.0f))
	{
		return std::nullopt;
	}
	// each part goes from its "from" at t = 1 to its "to" at t = 0, by 1 - t
	const float left = 1.0f - t;
	const float height = meshHalfHeight + meshHalfHeight;
	const float alpha = ((k_RingAlphaFrom - k_RingAlphaTo) * left) + k_RingAlphaTo;
	return RingPose {
	    .t = t,
	    .scale = ((((k_RingScaleFrom - k_RingScaleTo) * left) + k_RingScaleTo) * k_RingScale),
	    .raise = ((left * (k_RingRaiseFrom)) + k_RingRaiseTo) * height,
	    .alpha = static_cast<uint8_t>(static_cast<int>(alpha) & 0xFF),
	};
}

glm::mat4 charge_visual::RingModel(const glm::mat4& iconModel, const RingPose& pose)
{
	glm::mat4 model = iconModel;
	for (int k = 0; k < 3; ++k)
	{
		model[3][k] = (pose.raise * iconModel[1][k]) + iconModel[3][k];
	}
	for (int axis = 0; axis < 3; ++axis)
	{
		for (int k = 0; k < 3; ++k)
		{
			model[axis][k] = pose.scale * iconModel[axis][k];
		}
	}
	return model;
}
