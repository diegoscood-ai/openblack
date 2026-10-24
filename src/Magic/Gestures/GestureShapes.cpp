/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureShapes.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <optional>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::magic::gestures;

namespace
{
template <class T>
T Read(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}
} // namespace

void Path::Add(const glm::vec3& point)
{
	_points.push_back(point);
	Measure();
}

void Path::Measure()
{
	_distances.resize(_points.size());
	if (_points.empty())
	{
		return;
	}
	_distances[0] = 0.0f;
	for (size_t i = 1; i < _points.size(); ++i)
	{
		_distances[i] = _distances[i - 1] + glm::distance(_points[i - 1], _points[i]);
	}
}

glm::vec3 Path::At(float t) const
{
	if (_points.empty())
	{
		return glm::vec3(0.0f);
	}
	const float d = t * Length();
	if (d >= Length())
	{
		return _points.back();
	}
	// the segment whose end is past the distance, lerped
	size_t i = 0;
	while (i + 2 < _points.size() && d > _distances[i + 1])
	{
		++i;
	}
	if (i + 1 >= _points.size())
	{
		return _points[i];
	}
	const float span = _distances[i + 1] - _distances[i];
	if (span == 0.0f)
	{
		return _points[i];
	}
	return _points[i] + (_points[i + 1] - _points[i]) * ((d - _distances[i]) / span);
}

bool openblack::magic::gestures::ParseShape(const std::vector<uint8_t>& bytes, Shape& shape)
{
	// u32 file size, u32 (?), u32 count, then 24-byte points (x, y, z, ...)
	if (bytes.size() < 12)
	{
		return false;
	}
	const auto count = Read<uint32_t>(bytes, 8);
	if (count == 0 || bytes.size() < 12 + static_cast<size_t>(count) * 24)
	{
		return false;
	}
	shape = Shape {};
	// (inferred: the +-1e7 box start, the 1e-4 aspect guard and the 1 / count resampling step are not read; the
	// original's resampling is not decoded)
	shape.maxX = -1e7f;
	shape.minX = 1e7f;
	shape.maxZ = -1e7f;
	shape.minZ = 1e7f;
	Path path;
	for (uint32_t i = 0; i < count; ++i)
	{
		const size_t at = 12 + static_cast<size_t>(i) * 24;
		const float x = (Read<float>(bytes, at) + 100.0f) * 0.005f;
		const float z = (100.0f - Read<float>(bytes, at + 8)) * 0.005f;
		path.Add(glm::vec3(x, 0.0f, z));
		shape.maxX = std::max(shape.maxX, x);
		shape.minX = std::min(shape.minX, x);
		shape.maxZ = std::max(shape.maxZ, z);
		shape.minZ = std::min(shape.minZ, z);
	}
	shape.aspect = 1.0f;
	if (shape.maxZ - shape.minZ > 1e-4f)
	{
		shape.aspect = (shape.maxX - shape.minX) / (shape.maxZ - shape.minZ);
	}
	// resampled evenly along its length into as many points
	const float step = 1.0f / static_cast<float>(count);
	shape.points.resize(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		shape.points[i] = path.At(static_cast<float>(i) * step);
	}
	shape.length = path.Length();
	return true;
}

const Shape& openblack::magic::gestures::ShapeOf(Gesture gesture)
{
	const size_t index = gesture < k_GestureCount ? gesture : k_Circle;
	// the gesture's own file, read once through the resource cache, else the circle's; a circle with no file is empty
	static const Shape k_NoShape;
	if (!openblack::Locator::resources::has_value())
	{
		return k_NoShape;
	}
	auto& shapes = openblack::Locator::resources::value().GetGestureShapes();
	const auto id = entt::hashed_string(fmt::format("gestures/shape/{}", index).c_str()).value();
	if (!shapes.Contains(id))
	{
		shapes.Load(id, openblack::resources::GestureShapeLoader::FromDiskTag {}, static_cast<int>(index));
	}
	if (const auto shape = shapes.Handle(id); shape)
	{
		return *shape;
	}
	if (index != k_Circle)
	{
		return ShapeOf(k_Circle);
	}
	return k_NoShape;
}

RecognisedGesture openblack::magic::gestures::LandGestureTrail(const Shape& shape, glm::vec3 corner, float size,
                                                               const std::function<float(float x, float z)>& landHeight,
                                                               bool ofThisInterface)
{
	RecognisedGesture record;
	record.fromInterface = ofThisInterface;
	if (shape.points.empty())
	{
		return record;
	}
	// The shape's square laid on the land from the corner, each point dropped onto the land at the map point it falls
	// in. Every step rounds to a float, as the game's arithmetic does
	std::vector<glm::vec3> laid;
	laid.reserve(shape.points.size());
	const auto mapPoint = [](float metres) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(metres)); };
	for (const auto& point : shape.points)
	{
		const float x = (point.x * size) + corner.x;
		const float z = (point.z * size) + corner.z;
		laid.emplace_back(x, landHeight ? landHeight(mapPoint(x), mapPoint(z)) : corner.y, z);
	}
	// Taken at a fixed number of points evenly through its points (not along its length): the first at its start, the
	// last at its end, the rest between the two points either side; where a point falls and its share between the two
	// points either side are floats
	const float step = 1.0f / static_cast<float>(k_LandGesturePoints - 1);
	const auto last = laid.size() - 1;
	for (size_t i = 0; i < k_LandGesturePoints; ++i)
	{
		const float along = static_cast<float>(i) * step;
		glm::vec3 taken = laid.back();
		if (along != 1.0f)
		{
			const float at = along * static_cast<float>(last);
			const auto index = std::min(static_cast<size_t>(at), last);
			// The end of the path rounded up holds at its last point
			const auto& from = laid[index];
			const auto& to = laid[std::min(index + 1, last)];
			const float t = at - static_cast<float>(index);
			taken = ((to - from) * t) + from;
		}
		record.stroke.Add(taken);
		record.ideal.Add(taken);
	}
	return record;
}
