/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCameraScript.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <exception>
#include <span>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::player_camera
{
namespace
{
/// The data of one segment, read in order
class SegmentReader
{
public:
	explicit SegmentReader(std::span<const uint8_t> data)
	    : _data(data)
	{
	}

	template <typename T>
	bool Read(T& value)
	{
		if (_offset + sizeof(T) > _data.size())
		{
			return false;
		}
		std::memcpy(&value, _data.data() + _offset, sizeof(T));
		_offset += sizeof(T);
		return true;
	}

	bool Skip(size_t bytes)
	{
		if (_offset + bytes > _data.size())
		{
			return false;
		}
		_offset += bytes;
		return true;
	}

private:
	std::span<const uint8_t> _data;
	size_t _offset = 0;
};

/// The segment `name` of a Lionhead segment file (as CameraTracks.cpp reads camera.edt)
std::optional<std::span<const uint8_t>> FindSegment(const std::vector<uint8_t>& bytes, const char* name)
{
	if (bytes.size() < 8 || std::memcmp(bytes.data(), "LiOnHeAd", 8) != 0)
	{
		return std::nullopt;
	}
	size_t offset = 8;
	while (offset + 36 <= bytes.size())
	{
		std::array<char, 33> segmentName {};
		std::memcpy(segmentName.data(), bytes.data() + offset, 32);
		uint32_t size = 0;
		std::memcpy(&size, bytes.data() + offset + 32, 4);
		if (offset + 36 + size > bytes.size())
		{
			return std::nullopt;
		}
		if (std::strcmp(segmentName.data(), name) == 0)
		{
			return std::span<const uint8_t> {bytes.data() + offset + 36, static_cast<size_t>(size)};
		}
		offset += 36 + size;
	}
	return std::nullopt;
}
} // namespace

State& Get()
{
	return Locator::scriptState::value().Get<State>();
}

ZoneFileRead ReadZoneFile(const std::vector<uint8_t>& bytes)
{
	ZoneFileRead file;
	const auto segment = FindSegment(bytes, k_ZoneSegment);
	if (!segment.has_value())
	{
		return file;
	}
	auto& zone = file.zone;
	SegmentReader reader(*segment);
	file.headerRead = reader.Read(zone.header);
	int32_t pointCount = 0;
	// In file order
	if (!file.headerRead || !reader.Read(zone.useExclusions) || !reader.Read(zone.drawForceField) ||
	    !reader.Read(zone.useMaxAltitude) || !reader.Read(zone.useHeightAboveLand) || !reader.Read(zone.maxAltitude) ||
	    !reader.Read(zone.heightAboveLand) || !reader.Read(pointCount))
	{
		return file;
	}
	// Three floats each. (inferred) more than 1024 would overrun the original's array: clamped here
	const auto count = static_cast<size_t>(std::clamp(pointCount, 0, static_cast<int32_t>(k_MaxForceFieldPoints)));
	for (size_t i = 0; i < count; ++i)
	{
		glm::vec3 point;
		if (!reader.Read(point.x) || !reader.Read(point.y) || !reader.Read(point.z))
		{
			return file;
		}
		zone.forceFieldPoints.push_back(point);
	}
	// The record count (zeroed first) and the record size
	int32_t recordCount = 0;
	int32_t recordSize = 0;
	if (!reader.Read(recordCount) || !reader.Read(recordSize))
	{
		return file;
	}
	if (recordSize != k_ExclusionRecordSize && recordCount != 0)
	{
		// Read into a buffer of that size and thrown away
		file.whole = reader.Skip(static_cast<size_t>(std::max(recordCount, 0)) * static_cast<size_t>(std::max(recordSize, 0)));
		return file;
	}
	for (int32_t i = 0; i < recordCount; ++i)
	{
		Exclusion exclusion;
		for (auto& b : exclusion.bytes)
		{
			if (!reader.Read(b))
			{
				return file;
			}
		}
		zone.exclusions.push_back(exclusion);
	}
	file.whole = true;
	return file;
}

void ForceRotateAboutPoint(const std::optional<glm::vec3>& point)
{
	auto& rotation = Get().fixedRotation;
	rotation.on = point.has_value();
	if (point.has_value())
	{
		rotation.point = *point;
	}
}

void Reset()
{
	Get() = State {};
}

} // namespace openblack::player_camera
