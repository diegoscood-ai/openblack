/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandPlaceRules.h"

#include <cmath>

#include <algorithm>

#include <glm/common.hpp>

#include "3D/MapCoords.h"

using namespace openblack;

hand_place::SmoothMouse hand_place::StepSmoothMouse(SmoothMouse mouse, glm::vec2 cursor, float seconds)
{
	mouse.position = glm::clamp(mouse.position + seconds * mouse.velocity, cursor - 80.0f, cursor + 80.0f);
	mouse.velocity = (mouse.velocity + (cursor - mouse.position) * 20.0f * seconds) * std::pow(0.03f, seconds);
	return mouse;
}

hand_place::Lean hand_place::LeanMotion(glm::vec2 cursor, glm::vec2 smooth, bool gripping)
{
	const auto clampLag = [](float v) { return v > -80.0f ? (v < 80.0f ? v : 80.0f) : -80.0f; };
	const float lagX = clampLag(gripping ? cursor.x - smooth.x : smooth.x - cursor.x);
	const float lagY = clampLag(cursor.y - smooth.y);
	return {.sideways = std::abs(lagX) > 0.0001f ? std::optional(lagX) : std::nullopt,
	        .forwards = std::abs(smooth.y) > 0.0001f ? std::optional(lagY) : std::nullopt};
}

bool hand_place::TakesNewUpTarget(float cursorX, float lastCursorX)
{
	return cursorX != lastCursorX;
}

std::optional<glm::vec3> hand_place::CollidePointOf(std::optional<glm::vec3> collided, std::optional<glm::vec3> land)
{
	return collided ? collided : land;
}

bool hand_place::IsNormalState(bool holdingOrTugging, HoldType hold, bool workingTotem)
{
	return (!holdingOrTugging || hold == HoldType::None) && !workingTotem;
}

float hand_place::HandDistanceTarget(bool screenObject, float screenObjectDepth, float pointDistance, float depthOffset)
{
	return screenObject ? std::max(1.0f, screenObjectDepth) : std::max(1.0f, pointDistance - depthOffset);
}

bool hand_place::SpringHoldingState(bool holding, bool tugging, HoldType hold, bool seed)
{
	return holding && !tugging && hold != HoldType::None && !seed;
}

hand_place::SpringFrame hand_place::SpringFrameOf(bool holdingState, bool active, bool placedBefore, bool readyToThrow)
{
	if (holdingState && active && placedBefore)
	{
		return SpringFrame::Step;
	}
	return holdingState && readyToThrow ? SpringFrame::Start : SpringFrame::Follow;
}

float hand_place::HoldHeight(float lift, bool rooted, float height)
{
	return lift + (rooted ? 0.1f * height : 0.0f);
}

hand_place::HoldClip hand_place::HoldClipOf(bool holding, HoldType hold, HoldClips has)
{
	if (holding && hold == HoldType::Magic && has.wiggle)
	{
		return HoldClip::Wiggle;
	}
	if (holding && hold == HoldType::Above && has.above)
	{
		return HoldClip::Above;
	}
	if (holding && hold != HoldType::Above && has.side)
	{
		return HoldClip::Side;
	}
	return HoldClip::None;
}

glm::vec3 hand_place::HeldTurnPlace(glm::vec3 hand, const land_morph::Ground* ground)
{
	const float x = map_coords::Quantise(hand.x);
	const float z = map_coords::Quantise(hand.z);
	float y = hand.y;
	if (ground != nullptr)
	{
		const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
		const float altitude = hand.y - (*ground)(glm::vec2(lookup(hand.x), lookup(hand.z)));
		y = (*ground)(glm::vec2(x, z)) + altitude;
	}
	return {x, y, z};
}
