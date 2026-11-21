/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockTextureQueue.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <utility>

#include <glm/common.hpp>
#include <glm/mat3x3.hpp>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"

namespace openblack::block_texture
{

float RequestDistance(glm::vec3 centre, glm::vec3 eye)
{
	const float dx = centre.x - eye.x;
	const float dy = centre.y - eye.y;
	const float dz = centre.z - eye.z;
	const float squares = (dz * dz + dy * dy) + dx * dx;
	return std::sqrt(squares);
}

void TextureQueue::Request(uint16_t block, glm::vec2 blockMapPos)
{
	if (const auto found = std::ranges::find(_requests, block, &TextureRequest::block); found != _requests.end())
	{
		found->strip = 0;
		return;
	}
	const glm::vec3 centre {blockMapPos.x + k_RequestCentreOffset, 0.0f, blockMapPos.y + k_RequestCentreOffset};
	_requests.insert(_requests.begin(), TextureRequest {.block = block, .centre = centre, .distance = 0.0f, .strip = 0});
}

bool TextureQueue::Has(uint16_t block) const
{
	return std::ranges::find(_requests, block, &TextureRequest::block) != _requests.end();
}

void TextureQueue::Work(TextureRequest& request, int32_t detailIndex, std::vector<Step>& out)
{
	if (request.strip < k_StripsPerBlock)
	{
		// all the strips from this one on at the highest detail, or else this one
		const int32_t count = detailIndex == k_AllStripsDetailIndex ? k_StripsPerBlock : 1;
		for (int32_t i = 0; i < count; ++i)
		{
			out.push_back({.kind = StepKind::Strip, .block = request.block, .strip = request.strip});
			if (count > 1)
			{
				++request.strip;
			}
		}
	}
	// the prints come right after the last strip, only when it was the 16th
	if (request.strip == k_StripsPerBlock)
	{
		out.push_back({.kind = StepKind::Prints, .block = request.block, .strip = request.strip});
	}
	++request.strip;
}

void TextureQueue::Update(glm::vec3 eye, int32_t detailIndex, std::vector<Step>& out)
{
	for (auto& request : _requests)
	{
		request.distance = RequestDistance(request.centre, eye);
	}
	// passes of adjacent swaps until one makes none; only a strictly farther request moves back, so a NaN distance
	// stays where it is
	for (bool swapped = true; swapped;)
	{
		swapped = false;
		for (size_t i = 0; i + 1 < _requests.size(); ++i)
		{
			if (_requests[i].distance > _requests[i + 1].distance)
			{
				std::swap(_requests[i], _requests[i + 1]);
				swapped = true;
			}
		}
	}
	if (_requests.empty())
	{
		return;
	}
	// the nearest is worked once more before the frame's turn
	Work(_requests.front(), detailIndex, out);
	for (size_t i = 0; i < _requests.size() && i < static_cast<size_t>(k_RequestsPerFrame); ++i)
	{
		Work(_requests[i], detailIndex, out);
		if (_requests[i].strip > k_StripsPerBlock)
		{
			out.push_back({.kind = StepKind::Finish, .block = _requests[i].block, .strip = _requests[i].strip});
			_requests.erase(_requests.begin() + static_cast<std::ptrdiff_t>(i));
			return;
		}
	}
}

void BlockMarks::Reset(size_t blocks)
{
	_outOfDate.assign(blocks, 0);
	_drawn.assign(blocks, 0);
}

void BlockMarks::Changed(size_t block)
{
	if (block < _outOfDate.size())
	{
		_outOfDate[block] = 1;
	}
}

bool BlockMarks::TakePaintNow(size_t block)
{
	if (block >= _outOfDate.size() || _outOfDate[block] == 0 || _drawn[block] != 0)
	{
		return false;
	}
	_outOfDate[block] = 0;
	return true;
}

bool BlockMarks::TakeDrawn(size_t block)
{
	if (block >= _outOfDate.size())
	{
		return false;
	}
	_drawn[block] = 1;
	return std::exchange(_outOfDate[block], uint8_t {0}) != 0;
}

void BlockMarks::AllOutOfDate()
{
	std::ranges::fill(_outOfDate, uint8_t {1});
}

std::optional<BlockRange> BlocksMarkedBy(const MarkingObject& object)
{
	if (!object.shadow && !object.footprint)
	{
		return std::nullopt;
	}
	// the scale on the diagonal, turned about the vertical when there is an angle; glm's m[r][c] is the original's
	// cell 3 r + c
	glm::mat3 m(object.scale);
	if (object.yAngle != 0.0f)
	{
		affine::RotateY(m, object.yAngle);
	}
	const float tx = object.position.x;
	const float tz = object.position.y;
	float minX = tx;
	float maxX = tx;
	float minZ = tz;
	float maxZ = tz;
	// a side is only raised when the point is not below the other one
	const auto take = [&](float x, float z) {
		if (x < minX)
		{
			minX = x;
		}
		else if (x > maxX)
		{
			maxX = x;
		}
		if (z < minZ)
		{
			minZ = z;
		}
		else if (z > maxZ)
		{
			maxZ = z;
		}
	};
	if (object.shadow)
	{
		const auto& c = object.boxCentre;
		const auto& e = object.boxHalfSize;
		// the eight corners, + for centre + half size, in the original's order
		constexpr std::array<std::array<bool, 3>, 8> k_Corners {{{true, true, true},
		                                                         {false, true, true},
		                                                         {true, true, false},
		                                                         {true, false, true},
		                                                         {false, true, false},
		                                                         {true, false, false},
		                                                         {false, false, false},
		                                                         {false, false, true}}};
		for (const auto& corner : k_Corners)
		{
			const float px = corner[0] ? c.x + e.x : c.x - e.x;
			const float py = corner[1] ? c.y + e.y : c.y - e.y;
			const float pz = corner[2] ? c.z + e.z : c.z - e.z;
			const float wx = ((m[2][0] * pz + m[1][0] * py) + m[0][0] * px) + tx;
			// the height over the object's base: its own height cancels out
			const float wy = (m[0][1] * px + m[2][1] * pz) + m[1][1] * py;
			const float wz = ((m[1][2] * py + m[0][2] * px) + m[2][2] * pz) + tz;
			take(wx + k_ShadowSlopeX * wy, k_ShadowSlopeZ * wy + wz);
		}
	}
	if (object.footprint)
	{
		for (const auto& corner : object.footprintCorners)
		{
			const float x = (corner.y * m[2][0] + corner.x * m[0][0]) + tx;
			const float z = (corner.x * m[0][2] + corner.y * m[2][2]) + tz;
			take(x, z);
		}
	}
	BlockRange range {
	    .min = {map_coords::FtoL(minX * k_BlocksPerMetre), map_coords::FtoL(minZ * k_BlocksPerMetre)},
	    .max = {map_coords::FtoL(maxX * k_BlocksPerMetre), map_coords::FtoL(maxZ * k_BlocksPerMetre)},
	};
	range.min = glm::max(range.min, glm::ivec2(0));
	for (int axis = 0; axis < 2; ++axis)
	{
		if (static_cast<uint32_t>(range.max[axis]) > static_cast<uint32_t>(k_LastBlockPlace))
		{
			range.max[axis] = k_LastBlockPlace;
		}
	}
	return range;
}

} // namespace openblack::block_texture
