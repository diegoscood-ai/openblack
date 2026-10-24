/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleTotem.h"

#include <cmath>

#include <limits>

#include "ECS/Components/PuzzleTotem.h"

using namespace openblack::ecs;
using openblack::ecs::components::PuzzleTotem;

int32_t puzzle_totem::Truncate(float value)
{
	// the conversion's "integer indefinite" for what has no whole number
	constexpr float k_Limit = 2147483648.0f;
	if (std::isnan(value) || value >= k_Limit || value < -k_Limit)
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(value);
}

float puzzle_totem::Travel(float meshHalfHeight, float scale)
{
	// each step rounded to a float, in this order
	const float kept = 1.0f - k_KeptAboveGround;
	return ((kept * meshHalfHeight) * scale) * 2.0f;
}

float puzzle_totem::Share(PuzzleTotem& totem)
{
	const float share = ShareAsRead(totem);
	if (totem.maxHeight != k_TapOnly && totem.blendCountdown != 0)
	{
		--totem.blendCountdown;
	}
	return share;
}

float puzzle_totem::ShareAsRead(const PuzzleTotem& totem)
{
	if (totem.maxHeight == k_TapOnly)
	{
		return 1.0f;
	}
	if (totem.blendCountdown != 0)
	{
		const int32_t left = totem.blendCountdown - 1;
		const int32_t newPart = (k_BlendReads - left) * totem.height;
		const float oldPart = static_cast<float>(totem.previousHeight) * static_cast<float>(left);
		return ((static_cast<float>(newPart) + oldPart) * k_BlendWeight) / static_cast<float>(totem.maxHeight);
	}
	return static_cast<float>(totem.height) / static_cast<float>(totem.maxHeight);
}

void puzzle_totem::SetShare(PuzzleTotem& totem, float share)
{
	if (totem.maxHeight == k_TapOnly)
	{
		totem.height = k_TapOnly;
		return;
	}
	const int32_t height = Truncate(static_cast<float>(totem.maxHeight) * share);
	totem.height = height > totem.maxHeight ? totem.maxHeight : height;
}

float puzzle_totem::HeightProperty(const PuzzleTotem& totem)
{
	return static_cast<float>(totem.height);
}

void puzzle_totem::SetHeightProperty(PuzzleTotem& totem, float value)
{
	// below its highest it takes the value; otherwise, a NaN included, its highest
	if (static_cast<float>(totem.maxHeight) > value)
	{
		totem.height = Truncate(value);
		return;
	}
	totem.height = totem.maxHeight;
}

float puzzle_totem::MaxHeightProperty(const PuzzleTotem& totem)
{
	return static_cast<float>(totem.maxHeight);
}

void puzzle_totem::SetMaxHeightProperty(PuzzleTotem& totem, float value)
{
	totem.maxHeight = Truncate(value);
}

float puzzle_totem::AltitudeAt(float restAltitude, float share, float travel)
{
	// the product rounded on its own, as the original's
	const float raised = share * travel;
	return restAltitude + (raised - travel);
}

void puzzle_totem::StartDrag(PuzzleTotem& totem)
{
	totem.handAltitude = totem.altitude;
	totem.dragOffset = totem.handAltitude - totem.restAltitude;
	totem.sentAltitude = totem.dragOffset;
}

float puzzle_totem::DragOffset(float offset, float slide, float travel)
{
	const float moved = slide / k_DragDivisor + offset;
	if (-travel > moved)
	{
		return -travel;
	}
	return moved > 0.0f ? 0.0f : moved;
}

float puzzle_totem::DragAltitude(float offset, float restAltitude, float travel, int32_t maxHeight)
{
	const float highest = static_cast<float>(maxHeight);
	int32_t step = Truncate(((travel - std::fabs(offset)) / travel) * highest);
	step = step > maxHeight ? maxHeight : step;
	const float sunk = travel * (1.0f - static_cast<float>(step) / highest);
	return restAltitude + -sunk;
}

std::optional<float> puzzle_totem::Drag(PuzzleTotem& totem, float slide, float travel)
{
	if (totem.maxHeight == k_TapOnly)
	{
		totem.height = k_TapOnly;
		return std::nullopt;
	}
	totem.dragOffset = DragOffset(totem.dragOffset, slide, travel);
	totem.handAltitude = DragAltitude(totem.dragOffset, totem.restAltitude, travel, totem.maxHeight);
	if (totem.handAltitude == totem.sentAltitude)
	{
		return std::nullopt;
	}
	totem.sentAltitude = totem.handAltitude;
	return totem.handAltitude;
}

float puzzle_totem::ShareFromAltitude(float altitude, float restAltitude, float travel)
{
	const float share = (travel - std::fabs(altitude - restAltitude)) / travel;
	if (share < 0.0f)
	{
		return 0.0f;
	}
	return share <= 1.0f || std::isnan(share) ? share : 1.0f;
}
