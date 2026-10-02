/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCameraScript.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <exception>
#include <utility>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

namespace openblack::player_camera
{
namespace
{
/// The data of one segment, read in order as LHFile::GetSegmentData(ptr, size, -1) 0x7BE040 does
class SegmentReader
{
public:
	SegmentReader(const uint8_t* data, size_t size)
	    : _data(data)
	    , _size(size)
	{
	}

	template <typename T>
	bool Read(T& value)
	{
		if (_offset + sizeof(T) > _size)
		{
			return false;
		}
		std::memcpy(&value, _data + _offset, sizeof(T));
		_offset += sizeof(T);
		return true;
	}

	bool Skip(size_t bytes)
	{
		if (_offset + bytes > _size)
		{
			return false;
		}
		_offset += bytes;
		return true;
	}

private:
	const uint8_t* _data;
	size_t _size;
	size_t _offset = 0;
};

/// The segment `name` of a Lionhead segment file (as CameraTracks.cpp reads camera.edt)
std::optional<std::pair<const uint8_t*, size_t>> FindSegment(const std::vector<uint8_t>& bytes, const char* name)
{
	if (bytes.size() < 8 || std::memcmp(bytes.data(), "LiOnHeAd", 8) != 0)
	{
		return std::nullopt;
	}
	size_t offset = 8;
	while (offset + 36 <= bytes.size())
	{
		char segmentName[33] {};
		std::memcpy(segmentName, bytes.data() + offset, 32);
		uint32_t size = 0;
		std::memcpy(&size, bytes.data() + offset + 32, 4);
		if (offset + 36 + size > bytes.size())
		{
			return std::nullopt;
		}
		if (std::strcmp(segmentName, name) == 0)
		{
			return std::make_pair(bytes.data() + offset + 36, static_cast<size_t>(size));
		}
		offset += 36 + size;
	}
	return std::nullopt;
}
} // namespace

State& Get()
{
	static State state;
	return state;
}

void ResetExclusionFile(uint32_t id)
{
	auto& zone = Get().zone;
	// RemoveByID 0x454A40: every CameraExclusion whose +8 is the id is deleted
	zone.exclusions.erase(
	    std::remove_if(zone.exclusions.begin(), zone.exclusions.end(), [id](const Exclusion& e) { return e.id == id; }),
	    zone.exclusions.end());
	zone.flag9CE6B0 = 1;                   // 0x45532F
	zone.drawForceField = 0;               // 0x455339
	zone.flagC5E14C = 0;                   // 0x45533E
	zone.flagC5E148 = 0;                   // 0x455343
	zone.limit9CE6AC = k_DefaultZoneLimit; // 0x455348
	zone.limit9CE6A8 = k_DefaultZoneLimit; // 0x455352
	zone.forceFieldPoints.clear();         // ForceFieldPointCount = 0 (0x45535C)
}

bool LoadExclusionFile(const std::vector<uint8_t>& bytes, uint32_t id)
{
	ResetExclusionFile(id); // 0x45537A
	const auto segment = FindSegment(bytes, k_ZoneSegment);
	if (!segment.has_value())
	{
		return false;
	}
	auto& zone = Get().zone;
	SegmentReader reader(segment->first, segment->second);
	int32_t pointCount = 0;
	// 0x45539A..0x455415, in this order
	if (!reader.Read(zone.header) || !reader.Read(zone.flag9CE6B0) || !reader.Read(zone.drawForceField) ||
	    !reader.Read(zone.flagC5E14C) || !reader.Read(zone.flagC5E148) || !reader.Read(zone.limit9CE6AC) ||
	    !reader.Read(zone.limit9CE6A8) || !reader.Read(pointCount))
	{
		return false;
	}
	// 0x45541A..0x455441: one LHPoint each. (inferido) more than 1024 would overrun the original's array: clamped here
	const auto count = static_cast<size_t>(std::clamp(pointCount, 0, static_cast<int32_t>(k_MaxForceFieldPoints)));
	for (size_t i = 0; i < count; ++i)
	{
		glm::vec3 point;
		if (!reader.Read(point.x) || !reader.Read(point.y) || !reader.Read(point.z))
		{
			return false;
		}
		zone.forceFieldPoints.push_back(point);
	}
	// 0x455443..0x455466: the record count (its local zeroed first) and the record size
	int32_t recordCount = 0;
	int32_t recordSize = 0;
	if (!reader.Read(recordCount) || !reader.Read(recordSize))
	{
		return false;
	}
	if (recordSize != k_ExclusionRecordSize && recordCount != 0)
	{
		// 0x45547C..0x4554B9: read into a buffer of that size and thrown away
		return reader.Skip(static_cast<size_t>(std::max(recordCount, 0)) * static_cast<size_t>(std::max(recordSize, 0)));
	}
	for (int32_t i = 0; i < recordCount; ++i) // 0x4554CF..0x455510
	{
		Exclusion exclusion;
		for (auto& b : exclusion.bytes)
		{
			if (!reader.Read(b))
			{
				return false;
			}
		}
		exclusion.id = id; // +8 = id after the read; +0 (next) kept
		zone.exclusions.push_back(exclusion);
	}
	return true;
}

bool SetCameraZone(const std::string& name)
{
	ResetExclusionFile(1); // 0x6ED8E8
	std::vector<uint8_t> bytes;
	bool opened = false;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		bytes = fileSystem.ReadAll(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / k_ZoneFolder / name));
		opened = true;
	}
	catch (const std::exception&)
	{
		opened = false;
	}
	if (!opened) // 0x6ED956..0x6ED96F
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "Couldn't load zone file-{}", name);
		}
		return false;
	}
	LoadExclusionFile(bytes, 1); // 0x6ED92A
	auto& zone = Get().zone;
	zone.flag9CE6B0 = 1;     // 0x6ED936
	zone.drawForceField = 1; // 0x6ED940
	return true;
}

bool InsideInclusion(const Zone& zone, const glm::vec3& p, const glm::vec3& dir, glm::vec3* hit, glm::vec3* normal)
{
	if (zone.drawForceField == 0) // 0x455E29..0x455E3F
	{
		return true;
	}
	if (hit != nullptr) // 0x455E40..0x455E55
	{
		*hit = p;
	}
	const auto& points = zone.forceFieldPoints;
	if (points.size() < 3) // 0x455E5E
	{
		return true;
	}
	int32_t crossings = 0;               // [esp+0x14]
	float bestAhead = k_InclusionFar;    // [esp+0x20]
	float bestBehind = k_InclusionFar;   // [esp+0x24]
	bool foundAhead = false;             // [esp+0x12]
	bool foundBehind = false;            // [esp+0x13]
	glm::vec3 hitAhead = p;              // [esp+0x5C]
	glm::vec3 hitBehind = p;             // [esp+0x68]
	glm::vec2 normalAhead(0.0f, 1.0f);   // [esp+0x28] (0x455EC0..0x455EC8)
	glm::vec2 normalBehind(0.0f, 1.0f);  // [esp+0x30]
	const glm::vec3* a = &points.back(); // 0x455E94..0x455E97: the last point
	for (const auto& b : points)
	{
		const float ex = a->x - b.x; // 0x455EEF..0x455EFF
		const float ez = a->z - b.z;
		const float wx = p.x - b.x;
		const float wz = p.z - b.z;
		const float length2 = ex * ex + ez * ez;
		if (wz * wz + wx * wx < k_InclusionSquaredEpsilon) // 0x455F24..0x455F3D: on a point
		{
			return true;
		}
		const glm::vec3* next = &b;
		if (length2 > k_InclusionSquaredEpsilon) // 0x455F43..0x455F52
		{
			const float nex = -ex; // fchs; fstp a double (0x455F60)
			const float denominator = dir.z * nex + dir.x * ez;
			if (static_cast<double>(std::abs(denominator)) > k_InclusionEpsilon) // 0x455F74..0x455F83
			{
				const float numerator = ((b.x * ez + b.z * nex) - p.x * ez) - p.z * nex; // 0x455F89..0x455FA4
				const float t = numerator / denominator;                                 // fdivrp; fst [esp+0x1c]
				const glm::vec3 q(t * dir.x + p.x, dir.y * t + p.y, t * dir.z + p.z);    // 0x455FAC..0x455FDE
				const float s = ((q.x - b.x) * ex + (q.z - b.z) * ez) / length2;         // 0x455FE2..0x455FFD
				// 0x456001..0x45601D: 0 <= s < 1
				if (!(s < 0.0f) && s < 1.0f)
				{
					if (static_cast<double>(std::abs(t)) < k_InclusionEpsilon) // 0x456023..0x456032: on the edge
					{
						return true;
					}
					if (t > 0.0f) // 0x456038..0x456043
					{
						++crossings; // 0x45604F
						if (t < bestAhead)
						{
							bestAhead = t;
							hitAhead = q;
							normalAhead = {ez, nex};
							foundAhead = true;
						}
					}
					else if (t < 0.0f && -t < bestBehind) // 0x45608E..0x4560AA
					{
						bestBehind = -t;
						hitBehind = q;
						normalBehind = {ez, nex};
						foundBehind = true;
					}
				}
			}
		}
		a = next; // 0x4560E7: the edge's end becomes the next start
	}
	bool found = false; // bl
	if (foundAhead)     // 0x456102..0x456164
	{
		if (hit != nullptr)
		{
			*hit = hitAhead;
		}
		if (normal != nullptr)
		{
			*normal = {normalAhead.x, 0.0f, normalAhead.y};
		}
		found = true;
	}
	else if (foundBehind) // 0x456168..0x4561C6
	{
		if (hit != nullptr)
		{
			*hit = hitBehind;
		}
		if (normal != nullptr)
		{
			*normal = {normalBehind.x, 0.0f, normalBehind.y};
		}
		found = true;
	}
	if (hit != nullptr) // 0x4561CB..0x45624A: the nearest polygon point when nearer in x / z
	{
		const float hx = hit->x - p.x;
		const float hz = hit->z - p.z;
		float best = hx * hx + hz * hz;
		for (const auto& v : points)
		{
			const float vx = v.x - p.x;
			const float vz = v.z - p.z;
			const float d = vz * vz + vx * vx;
			if (d < best || !found)
			{
				best = d;
				*hit = v;
				found = true;
			}
		}
	}
	return (crossings & 1) != 0; // 0x45624C..0x456252
}

void ForceRotateAboutPoint(const std::optional<glm::vec3>& point)
{
	auto& rotation = Get().fixedRotation;
	rotation.on = point.has_value(); // +0x88 (0x457338 / 0x457355)
	if (point.has_value())
	{
		rotation.point = *point; // +0x7C (0x45733F..0x45734F)
	}
}

void Reset()
{
	Get() = State {};
}

} // namespace openblack::player_camera
