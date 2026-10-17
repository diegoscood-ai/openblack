/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraZoneSystem.h"

#include <cstring>

#include <exception>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Camera/PlayerCameraScript.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
template <typename T>
T Read(const std::array<uint8_t, 0x28>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

/// The camera zone file read once into the resources; none when it can't be read
const player_camera::ZoneFileRead* ReadOnce(std::string_view name)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& zones = Locator::resources::value().GetCameraZones();
	const auto id = entt::hashed_string((std::string("camera/zones/") + std::string(name)).c_str()).value();
	if (!zones.Contains(id))
	{
		std::vector<uint8_t> bytes;
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
			                            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() /
			                                                player_camera::k_ZoneFolder / std::string(name)));
		}
		catch (const std::exception&)
		{
			return nullptr;
		}
		zones.Load(id, resources::CameraZoneLoader::FromBufferTag {}, bytes);
	}
	return &*zones.Handle(id);
}
} // namespace

bool CameraZoneSystem::SetZones(std::string_view fileName)
{
	// The zones set before go first, so a file that can't be read leaves none
	_zones = {};
	_fileName.clear();
	const auto* file = ReadOnce(fileName);
	if (file == nullptr)
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "Couldn't load zone file-{}", fileName);
		}
		return false;
	}
	_zones = FromFile(file->zone);
	_fileName = std::string(fileName);
	return true;
}

camera_zones::Zones CameraZoneSystem::FromFile(const player_camera::Zone& file)
{
	camera_zones::Zones zones {
	    // A script's zones are always kept to, whatever the file says
	    .exclusionsOn = true,
	    .fenceOn = true,
	    .useMaxAltitude = file.flagC5E14C != 0,
	    .useHeightAboveLand = file.flagC5E148 != 0,
	    .maxAltitude = file.limit9CE6AC,
	    .heightAboveLand = file.limit9CE6A8,
	    .fence = file.forceFieldPoints,
	};
	// An exclusion record as the game keeps one: two links and an id, then its middle, radius, height and kind
	for (const auto& exclusion : file.exclusions)
	{
		zones.exclusions.push_back({
		    .position = {Read<float>(exclusion.bytes, 12), Read<float>(exclusion.bytes, 16), Read<float>(exclusion.bytes, 20)},
		    .radius = Read<float>(exclusion.bytes, 24),
		    .height = Read<float>(exclusion.bytes, 28),
		    .kind = Read<uint32_t>(exclusion.bytes, 32) == 1 ? camera_zones::Exclusion::Kind::Cylinder
		                                                     : camera_zones::Exclusion::Kind::Dome,
		});
	}
	return zones;
}
