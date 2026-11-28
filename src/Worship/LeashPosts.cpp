/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashPosts.h"

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

#include "3D/AffineMatrix.h"
#include "3D/FrameAnim.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"

using namespace openblack;
using namespace openblack::worship;

namespace
{
/// The reciprocals the wraps count whole turns with, as floats
constexpr float k_InvTwoPi = std::bit_cast<float>(0x3E22F983u);
constexpr float k_InvFrames = std::bit_cast<float>(0x3D888889u);
constexpr float k_MsToSeconds = 0.001f;
/// A sixth, as a float
constexpr float k_Sixth = std::bit_cast<float>(0x3E2AAAABu);

/// x less its whole turns of `period`, the turns counted as truncate(x times the reciprocal), each step a float
float WrapTurns(float x, float reciprocal, float period)
{
	const float turns = x * reciprocal;
	const auto whole = static_cast<float>(map_coords::FtoL(turns));
	const float taken = whole * period;
	return x - taken;
}

/// An object matrix back in openblack's layout: its rows as the columns, its translation as column 3
glm::mat4 ToModel(const affine::AffineMatrix& a)
{
	glm::mat4 model(1.0f);
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			model[r][c] = a.m[static_cast<size_t>((3 * r) + c)];
		}
	}
	return model;
}

/// The collar's size on the hand and its offset along the hand's own x and y
constexpr float k_HandCollarSize = 0.5f;
constexpr float k_HandCollarOffsetX = 0.05f;
constexpr float k_HandCollarOffsetY = 0.25f;
} // namespace

glm::vec3 leash_posts::Point(const glm::mat4& temple, std::span<const glm::mat4> specialPoints, size_t post)
{
	if (post >= specialPoints.size())
	{
		return glm::vec3(temple[3]);
	}
	const glm::vec3 p(specialPoints[post][3]);
	// x: the y and z terms are added first, then the x term, then the position
	const float x1 = p.y * temple[1][0];
	const float x2 = p.z * temple[2][0];
	const float x0 = p.x * temple[0][0];
	const float x = ((x1 + x2) + x0) + temple[3][0];
	// y and z: the x and y terms first, then the z term, then the position
	const float y0 = p.x * temple[0][1];
	const float y1 = p.y * temple[1][1];
	const float y2 = p.z * temple[2][1];
	const float y = ((y0 + y1) + y2) + temple[3][1];
	const float z0 = p.x * temple[0][2];
	const float z1 = p.y * temple[1][2];
	const float z2 = p.z * temple[2][2];
	const float z = ((z0 + z1) + z2) + temple[3][2];
	return {x, y, z};
}

glm::vec3 leash_posts::PointOnLand(const glm::mat4& temple, std::span<const glm::mat4> specialPoints, size_t post,
                                   const land_morph::Ground& ground)
{
	if (post >= specialPoints.size())
	{
		return glm::vec3(temple[3]);
	}
	auto point = Point(temple, specialPoints, post);
	const float originHeight = ground(glm::vec2(temple[3].x, temple[3].z));
	point.y = land_morph::Raised(ground, point, originHeight);
	return point;
}

float leash_posts::FrameSeconds(uint32_t gameTimeIncMs) noexcept
{
	// the milliseconds read as a signed integer
	return static_cast<float>(static_cast<int32_t>(gameTimeIncMs)) * k_MsToSeconds;
}

void leash_posts::Step(Spin& spin, float seconds) noexcept
{
	spin.xAngle = WrapTurns((k_XTurnRate * seconds) + spin.xAngle, k_InvTwoPi, k_TwoPi);
	spin.zAngle = WrapTurns((k_ZTurnRate * seconds) + spin.zAngle, k_InvTwoPi, k_TwoPi);
	// the scroll's period is 1: its whole part is taken off
	const float scroll = (k_ScrollRate * seconds) + spin.scroll;
	spin.scroll = scroll - static_cast<float>(map_coords::FtoL(scroll));
	spin.frame = WrapTurns((k_FrameRate * seconds) + spin.frame, k_InvFrames, k_Frames);
}

uint8_t leash_posts::SpriteCell(const Spin& spin) noexcept
{
	return graphics::frame_anim::SpriteCell(map_coords::FtoL(spin.frame));
}

uint32_t leash_posts::SmokeBrightness(uint32_t landLight) noexcept
{
	const uint32_t sum = (landLight & 0xFFu) + ((landLight >> 8u) & 0xFFu) + ((landLight >> 16u) & 0xFFu);
	return static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(sum) * k_Sixth));
}

glm::mat4 leash_posts::CollarMatrix(const glm::mat4& post, const Spin& spin)
{
	// the turn alone, no translation, before the post's matrix: the collar turns in its own frame
	const glm::mat4 turn(affine::RotationYXZ(0.0f, spin.xAngle, spin.zAngle));
	return ToModel(affine::MultiplyReversed(affine::FromModel(post), affine::FromModel(turn)));
}

glm::mat4 leash_posts::CollarOnHand(const glm::mat4& handRoot, float handScale)
{
	// at the bone's translation, unturned at scale 1, then the bone's YXZ angles written as the turn
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(glm::mat3(handRoot), y, x, z);
	glm::mat4 placed(affine::RotationYXZ(y, x, z));
	placed[3] = glm::vec4(glm::vec3(handRoot[3]), 1.0f);
	// diag(0.5 s) with the offset (0.05 s, 0.25 s, 0), before it
	const float s = handScale * k_HandCollarScale;
	const float size = s * k_HandCollarSize;
	affine::AffineMatrix local;
	local.m = {size, 0.0f, 0.0f, 0.0f, size, 0.0f, 0.0f, 0.0f, size, s * k_HandCollarOffsetX, s * k_HandCollarOffsetY, 0.0f};
	return ToModel(affine::MultiplyReversed(affine::FromModel(placed), local));
}
