/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandHoldPose.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/geometric.hpp>

#include "3D/AffineMatrix.h"
#include "3D/ObjectMatrix.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::hand_hold;

namespace
{
/// A vector made unit length, left as it is when it is zero
glm::vec3 UnitUnlessZero(glm::vec3 v)
{
	if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f)
	{
		v *= 1.0f / std::sqrt((v.x * v.x + v.y * v.y) + v.z * v.z);
	}
	return v;
}

/// An axis-angle turn as the engine's twelve floats: its rows are affine::AxisAngle's columns, no translation
affine::AffineMatrix TurnRows(const glm::mat3& m)
{
	affine::AffineMatrix a;
	for (glm::length_t c = 0; c < 3; ++c)
	{
		for (glm::length_t r = 0; r < 3; ++r)
		{
			a.m[static_cast<size_t>(3 * c + r)] = m[c][r];
		}
	}
	return a;
}

/// A cursor lag held to the sway's reach
float ClampLag(float v)
{
	return v > -k_SwayLag ? (v < k_SwayLag ? v : k_SwayLag) : -k_SwayLag;
}
} // namespace

HoldType hand_hold::HoldTypeOf(bool ready, HoldType recorded)
{
	return ready ? recorded : HoldType::Magic;
}

int32_t hand_hold::HoldTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize)
{
	// The clip's length as a signed whole number; each pose's time is worked in floats and cut to whole milliseconds
	const auto duration = static_cast<int32_t>(durationMs);
	switch (hold)
	{
	case HoldType::None:
		return 0;
	case HoldType::Above:
	{
		const float grip = std::min(1.0f, reach / (k_StandardHandHeight * handSize * k_AboveReachShare));
		return static_cast<int32_t>(static_cast<float>(duration) * 0.5f * (1.0f - grip));
	}
	case HoldType::Magic:
		return duration >> 1;
	case HoldType::Grain:
	case HoldType::Fingers:
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		break;
	}
	const float grip = std::min(1.0f, reach / (k_StandardHandHeight * handSize));
	return static_cast<int32_t>(static_cast<float>(duration >> 1) * grip);
}

int32_t hand_hold::TugTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize)
{
	if (hold != HoldType::Above)
	{
		return HoldTimeMs(hold, durationMs, reach, handSize);
	}
	// The pull scales the reach and the hand's height alike, so the share is the reach against the hand's height
	const float grip = std::min(1.0f, (reach * k_AboveReachShare) / (k_StandardHandHeight * handSize * k_AboveReachShare));
	return static_cast<int32_t>(static_cast<float>(static_cast<int32_t>(durationMs)) * 0.5f * (1.0f - grip));
}

float hand_hold::SeedHang([[maybe_unused]] HoldType hold, float lowering, float height)
{
	return lowering * height;
}

float hand_hold::HoldLift(HoldType hold, float hang, float handSize)
{
	return hold == HoldType::Above   ? k_AboveLift
	       : hold == HoldType::Magic ? k_StandardHandHeight * handSize
	                                 : std::max(hang, k_MinimumSideLift);
}

glm::vec2 hand_hold::CursorSway(glm::vec2 cursorLag)
{
	return {(k_MaxSway / k_SwayLag) * ClampLag(cursorLag.x), (k_MaxSway / k_SwayLag) * ClampLag(cursorLag.y)};
}

glm::vec3 hand_hold::HeldUp(glm::vec3 camera, glm::vec3 hand, float roll, float pitch)
{
	// Straight up turned about the line to the camera, then about the level line across it: the two turns' product in
	// the engine's order, then the point (0, 1, 0) through it, each sum in the engine's order (its zeros kept)
	const auto toCamera = UnitUnlessZero(camera - hand);
	const auto across = UnitUnlessZero(glm::vec3(0.0f - toCamera.z, 0.0f, toCamera.x - 0.0f));
	const auto turn = affine::Mul(TurnRows(affine::AxisAngle(toCamera, roll)), TurnRows(affine::AxisAngle(across, pitch)));
	const auto& m = turn.m;
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	return {((up.y * m[3] + up.z * m[6]) + up.x * m[0]) + m[9], ((up.x * m[1] + up.y * m[4]) + up.z * m[7]) + m[10],
	        ((up.x * m[2] + up.y * m[5]) + up.z * m[8]) + m[11]};
}

float hand_hold::HeldFrameSize(float handScale)
{
	// The standard height over the model's height first, then times the scale (and the hand's own size, which is 1)
	return (k_StandardHandHeight / k_HandModelHeight) * handScale;
}

glm::mat3 hand_hold::HeldFrame(float heading, glm::vec3 up, float handSize, bool mirrored)
{
	// Half a turn on from the heading, a float sum; its sine and its negated cosine each rounded once to a float
	const float turn = heading + std::numbers::pi_v<float>;
	const auto s = static_cast<float>(std::sin(static_cast<double>(turn)));
	const auto c = -static_cast<float>(std::cos(static_cast<double>(turn)));
	// The way back (s, 0, c) across the up, the zero part not multiplied; made unit unless all three parts count as
	// zero (zero or not a number)
	glm::vec3 side {-(c * up.y), c * up.x - s * up.z, s * up.y};
	const auto isZero = [](float v) { return !(v < 0.0f) && !(v > 0.0f); };
	if (!isZero(side.x) || !isZero(side.y) || !isZero(side.z))
	{
		const float share = 1.0f / std::sqrt((side.x * side.x + side.y * side.y) + side.z * side.z);
		side = {side.x * share, side.y * share, side.z * share};
	}
	// The up across the side, negated unless the hand is mirrored
	glm::vec3 forward {side.z * up.y - side.y * up.z, side.x * up.z - side.z * up.x, side.y * up.x - side.x * up.y};
	if (!mirrored)
	{
		constexpr float k_Negate = -1.0f;
		forward = {forward.x * k_Negate, forward.y * k_Negate, forward.z * k_Negate};
	}
	const glm::mat3 axes {side, up, forward};
	// The size's matrix times the axes, each part's three products added in the original's order: the third, second
	// and first for the x and y parts, the first, third and second for the z part. The size's zero parts are positive
	const std::array<glm::vec3, 3> size {glm::vec3(mirrored ? -handSize : handSize, 0.0f, 0.0f),
	                                     glm::vec3(0.0f, handSize, 0.0f), glm::vec3(0.0f, 0.0f, handSize)};
	glm::mat3 frame {};
	for (glm::length_t i = 0; i < 3; ++i)
	{
		const auto& row = size[static_cast<size_t>(i)];
		const auto term = [&row, &axes](glm::length_t k, glm::length_t j) { return row[k] * axes[k][j]; };
		frame[i] = {(term(2, 0) + term(1, 0)) + term(0, 0), (term(2, 1) + term(1, 1)) + term(0, 1),
		            (term(0, 2) + term(2, 2)) + term(1, 2)};
	}
	return frame;
}

glm::mat3 hand_hold::HeldTurn(const glm::mat3& frame, float yRotate)
{
	// The cosine kept as a float, the sine wider; each product rounded to a float before the sum
	const auto cosine = static_cast<float>(std::cos(static_cast<double>(yRotate)));
	const double sine = std::sin(static_cast<double>(yRotate));
	glm::mat3 turned = frame;
	for (glm::length_t part = 0; part < 3; ++part)
	{
		const float side = frame[0][part];
		const float forward = frame[2][part];
		turned[0][part] = cosine * side + static_cast<float>(sine * static_cast<double>(forward));
		turned[2][part] = cosine * forward - static_cast<float>(sine * static_cast<double>(side));
	}
	return turned;
}

glm::mat3 hand_hold::HeldAxes(const glm::mat3& frame, float yRotate)
{
	// The hold's own turn of the side and the forward about the up
	const auto turned = HeldTurn(frame, yRotate);
	const auto side = turned[0];
	const auto forward = turned[2];
	// Each axis made unit unless all three parts count as zero (zero or not a number), the squares added in the given
	// order
	const auto unit = [](glm::vec3 v, glm::length_t first, glm::length_t second, glm::length_t third) {
		const auto isZero = [](float p) { return !(p < 0.0f) && !(p > 0.0f); };
		if (isZero(v.x) && isZero(v.y) && isZero(v.z))
		{
			return v;
		}
		const float share = 1.0f / std::sqrt((v[first] * v[first] + v[second] * v[second]) + v[third] * v[third]);
		return glm::vec3(share * v.x, share * v.y, share * v.z);
	};
	return {unit(side, 2, 0, 1), unit(frame[1], 2, 1, 0), unit(forward, 2, 1, 0)};
}

glm::vec3 hand_hold::HangBelow(glm::vec3 hand, glm::vec3 up, float hang)
{
	return hand - up * hang;
}

void hand_hold::StepHoldingSpring(HoldingSpring& spring, glm::vec3 required, uint32_t clockMs)
{
	do
	{
		spring.stepMs += k_SpringStepMs;
		const auto d = required - spring.position;
		spring.velocity += (d * k_SpringStiffness - spring.velocity * k_SpringDamping) * k_SpringStepSeconds;
		const float speed = glm::length(spring.velocity);
		if (speed > k_MaxHandSpeed)
		{
			spring.velocity *= k_MaxHandSpeed / speed;
		}
		spring.position += spring.velocity * k_SpringStepSeconds;
	} while (spring.stepMs < clockMs);
}

HeldPlace hand_hold::ReleasePlace(const HeldPlace& turnPlace, const std::optional<HeldPlace>& drawn)
{
	return drawn.value_or(turnPlace);
}
