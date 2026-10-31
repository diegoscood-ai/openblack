/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextureDetail.h"

#include <cmath>

namespace openblack::ecs::draw_list
{

LodLine TextureLineAtLandCreation(const LodLine& current, int32_t detailIndex, int32_t poolCount)
{
	constexpr int32_t k_HighestDetailIndex = 4;
	if (detailIndex != k_HighestDetailIndex)
	{
		return current;
	}
	LodLine line {.along = {1.0f, 0.0f, 0.0f}, .point = {}, .distance = 400.0f, .width = 70.0f, .halfWidth = 35.0f};
	const auto wide = [&line](float distance) {
		line = {.along = {1.0f, 0.0f, 0.0f}, .point = {}, .distance = distance, .width = 170.0f, .halfWidth = 85.0f};
	};
	if (poolCount > 600)
	{
		wide(800.0f);
	}
	else if (poolCount > 500)
	{
		wide(700.0f);
	}
	else if (poolCount > 400)
	{
		wide(600.0f);
	}
	return line;
}

LodLine RebuildTextureLine(const LodLine& previous, glm::vec3 eye, glm::vec3 viewDirection)
{
	constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
	constexpr float k_SmallestSquare = 0.0001f;
	// the height unit times the land's highest altitude, worked each time: 110.55
	constexpr float k_HeightUnit = 0.67f;
	constexpr float k_HighestAltitude = 165.0f;
	const float highest = k_HeightUnit * k_HighestAltitude;
	const float reference = (highest < eye.y || std::isnan(eye.y)) ? highest : eye.y;
	const float m = -reference;
	const glm::vec3 f = viewDirection;
	LodLine line = previous;

	const float aheadX = eye.x + line.distance * f.x;
	const float aheadY = eye.y + line.distance * f.y;
	const float aheadZ = line.distance * f.z + eye.z;
	const float offset = -((aheadZ * f.z + aheadY * f.y) + aheadX * f.x);

	line.along = {
	    f.z * k_Up.y - k_Up.z * f.y,
	    k_Up.z * f.x - k_Up.x * f.z,
	    k_Up.x * f.y - f.x * k_Up.y,
	};
	const float squareX = line.along.x * line.along.x;
	const float squareY = line.along.y * line.along.y;
	const float squareZ = line.along.z * line.along.z;
	if (squareZ > squareY && squareZ > squareX && squareZ > k_SmallestSquare)
	{
		const float inverse = 1.0f / line.along.z;
		const float x = offset - m * f.y;
		const float y = m * f.x;
		line.point = {inverse * x, inverse * y, inverse * 0.0f};
	}
	else if (squareX > k_SmallestSquare)
	{
		const float inverse = 1.0f / line.along.x;
		const float y = -(m * f.z);
		const float z = m * f.y - offset;
		line.point = {inverse * 0.0f, inverse * y, inverse * z};
	}
	else
	{
		// looking straight down: the direction stays unscaled and the point stays last frame's
		return line;
	}
	const float scale = 1.0f / std::sqrt((squareZ + squareY) + squareX);
	line.along = {scale * line.along.x, scale * line.along.y, scale * line.along.z};
	return line;
}

TextureLevel BlockTextureLevel(glm::vec2 blockMapPos, const LodLine& line, bool pending)
{
	if (pending)
	{
		return TextureLevel::TileOnly;
	}
	// the side is added to the block's position before the line's point is taken away
	const float x = blockMapPos.x;
	const float z = blockMapPos.y;
	const float farX = blockMapPos.x + k_BlockSideLength;
	const float farZ = blockMapPos.y + k_BlockSideLength;
	bool anyNear = false;
	bool anyFar = false;
	for (const LineSide side :
	     {SideOfLine(line, x, z), SideOfLine(line, x, farZ), SideOfLine(line, farX, z), SideOfLine(line, farX, farZ)})
	{
		if (side == LineSide::Band)
		{
			return TextureLevel::Blended;
		}
		anyNear = anyNear || side == LineSide::Near;
		anyFar = anyFar || side == LineSide::Far;
	}
	if (anyNear && anyFar)
	{
		return TextureLevel::Blended;
	}
	return anyNear ? TextureLevel::Own : TextureLevel::TileOnly;
}

float TileAlphaScale(const LodLine& line)
{
	return 255.0f / line.width;
}

uint8_t TileAlpha(const LodLine& line, float alphaScale, float x, float z)
{
	const float v = (x - line.point.x) * line.along.z - (z - line.point.z) * line.along.x;
	if (v > line.halfWidth)
	{
		return 0;
	}
	if (-line.halfWidth < v)
	{
		// the product is stored as a float, then converted in the FPU's rounding mode: to the nearest, ties to even
		const float scaled = (line.halfWidth - v) * alphaScale;
		return static_cast<uint8_t>(static_cast<int32_t>(std::nearbyint(scaled)) & 0xFF);
	}
	return 255;
}

} // namespace openblack::ecs::draw_list
