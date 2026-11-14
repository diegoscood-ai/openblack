/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FotFile.h"

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "FileSystem/MemoryStream.h"
#include "GameThingSerializer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::filesystem;

FotFile::FotFile(Game& game)
    : _game(game)
{
}

void FotFile::Load(const std::filesystem::path& path)
{
	// the footpath file's bytes from the byte cache, read afresh with every map load (as its map script), through a
	// memory stream (a copy: the stream owns its data)
	auto& blobs = Locator::resources::value().GetBlobs();
	blobs.Erase(resources::BlobId(path));
	auto stream = std::make_unique<filesystem::MemoryStream>(std::vector<uint8_t>(resources::LoadBlob(blobs, path)));
	serializer::GameThingSerializer serializer(*stream);
	auto footpathLinkSaves = serializer.DeserializeList<serializer::GameThingSerializer::FootpathLinkSave>();
	auto footpaths = serializer.DeserializeList<serializer::GameThingSerializer::Footpath>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();

	// Keep track of footpath entities in order to associate them to the footpath link saves later
	std::vector<ecs::components::Footpath::Id> footpathEntities;

	for (const auto& footpath : footpaths)
	{
		// One new footpath per saved footpath (the original pushes each at the list's head)
		const auto entity = ecs::footpaths::Create();
		auto& footpathEntt = registry.Get<ecs::components::Footpath>(entity);
		footpathEntt.nodes.reserve(footpath.nodes.size());
		for (const auto& node : footpath.nodes)
		{
			// A saved node: its MapCoords, then a byte of flags
			const map_coords::MapCoords coords {static_cast<int32_t>(node.coords.x), static_cast<int32_t>(node.coords.z),
			                                    node.coords.altitude};
			// Drawn (the debug Footpaths lines) on the land at its exact map position, raised by the node's height
			// above it
			const glm::vec2 ground {map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z)};
			const glm::vec3 position {ground.x, coords.altitude + island.GetHeightAt(ground), ground.y};
			footpathEntt.nodes.push_back({position, coords, node.unknown, footpathEntt.nextNodeId++});
		}
		// A saved footpath: the nodes, then its active flag
		footpathEntt.active = footpath.unknown != 0;
		footpathEntities.push_back(static_cast<ecs::components::Footpath::Id>(entity));
	}

	uint32_t linked = 0;
	uint32_t dropped = 0;
	for (const auto& save : footpathLinkSaves)
	{
		std::vector<ecs::components::Footpath::Id> linkFootpathEntities;
		for (const auto& footpath : save.link.footpaths)
		{
			// Save links have duplicates to entries in footpaths
			const auto footpathListItr = std::find(footpaths.cbegin(), footpaths.cend(), footpath);
			assert(footpathListItr != footpaths.cend());
			const auto footpathIndex = std::distance(footpaths.cbegin(), footpathListItr);
			linkFootpathEntities.push_back(footpathEntities[footpathIndex]);
		}
		const map_coords::MapCoords coords {static_cast<int32_t>(save.coords.x), static_cast<int32_t>(save.coords.z),
		                                    save.coords.altitude};
		const glm::vec3 position {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
		const auto entity = registry.Create();
		registry.Assign<ecs::components::FootpathLink>(entity, position, std::move(linkFootpathEntities));
		// Each link save is resolved as it is read: to the fixed building or the planned abode at its point, else
		// deleted
		ecs::footpaths::AttachLoadedLink(entity, coords);
		++(registry.Valid(entity) ? linked : dropped);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Footpaths: {} paths, {} links on buildings or plans, {} links with nothing there",
	                   footpathEntities.size(), linked, dropped);
}
