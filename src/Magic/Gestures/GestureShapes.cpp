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

#include <array>
#include <optional>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack::magic::gestures;

namespace
{
std::array<std::optional<Shape>, k_GestureCount> g_Shapes;

template <class T>
T Read(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

bool LoadShapeFile(int number, Shape& shape)
{
	if (!openblack::Locator::filesystem::has_value())
	{
		return false;
	}
	auto& fileSystem = openblack::Locator::filesystem::value();
	// ".\data\symbols\PathSymbol" + "%d.cam"
	const auto path = fileSystem.GetPath<openblack::filesystem::Path::Data>() / "Symbols" / fmt::format("PathSymbol{}.cam", number);
	try
	{
		if (!fileSystem.Exists(path))
		{
			return false;
		}
		return ParseShape(fileSystem.ReadAll(path), shape);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Gestures: {}: {}", path.generic_string(), e.what());
	}
	return false;
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
	// fn_00687E60: the segment whose end is past the distance, lerped
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
	// fn_0086D4A0: u32 file size, u32 (?), u32 count, then 24-byte points (x, y, z, ...)
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
	// (inferido: the +-1e7 box start, the 1e-4 aspect guard and the 1 / count resampling step are not read; the
	// resampling belongs to fn_0068C650 / fn_0068C340, not decoded)
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
	auto& slot = g_Shapes[index];
	if (!slot)
	{
		slot.emplace();
		// the gesture's own file, else the circle's
		if (!LoadShapeFile(static_cast<int>(index), *slot) && index != k_Circle)
		{
			slot = ShapeOf(k_Circle);
		}
	}
	return *slot;
}
