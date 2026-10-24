/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandPrints.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>

#include "3D/AffineMatrix.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "3D/PickMask.h"
#include "3D/SnowCover.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::land_prints
{
namespace
{
using namespace components;

constexpr int32_t k_CellsPerBlock = 16;
constexpr int32_t k_Margin = 4;
constexpr int32_t k_LastCell = 511;

std::array<float, 12> Rows(const glm::mat4& m)
{
	// right, up, forward and position: the matrix's columns
	return {m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]};
}

// The classes the original bakes a static shadow for (set as their 3D object is made), without the ones that turn it
// off, and not for a building not built yet
bool CastsShadow(const Registry& registry, entt::entity entity)
{
	// (the citadel's heart and its worship sites are Fixed classes too, citadel parts)
	const bool shadowClass =
	    registry.AnyOf<Tree, Abode, Feature, BigForest, Fixed, MobileStatic, MobileObject, CitadelHeart, WorshipSite>(entity);
	const bool noShadowClass =
	    registry.AnyOf<Pot, DeadTree, Field, Villager, Creature, Hand, Alpha, AnimatedStatic, Flowers, TempleInteriorPart>(
	        entity);
	return shadowClass && !noShadowClass && abodes::CastsShadowOnTexture(entity);
}

// The ones that ask for their footprint on the land's textures as they are made: a field, a building while it is not
// built, and the citadel's heart and worship sites, built or not
bool AsksForFootprint(const Registry& registry, entt::entity entity)
{
	if (registry.AnyOf<Field, CitadelHeart, WorshipSite>(entity))
	{
		return true;
	}
	const auto* abode = registry.TryGet<const Abode>(entity);
	return abode != nullptr && (abode->buildFlags & Abode::k_Built) == 0;
}

// The texture's 64 x 64 alpha a primitive draws with: the mesh's own skin's, else the pack's texture of that id; none
// untextured
const uint8_t* AlphaOf(const graphics::L3DMesh& mesh, uint32_t skin)
{
	if (const auto* map = mesh.GetShadowAlphaMap(skin); map != nullptr)
	{
		return map->data();
	}
	const auto& masks = Locator::resources::value().GetPickMasks();
	if (mesh.GetSkins().contains(skin) || !masks.Contains(skin))
	{
		return nullptr;
	}
	return masks.Handle(skin)->cells.data();
}

std::vector<std::array<uint16_t, 3>> TrianglesOf(std::span<const uint16_t> indices, std::pair<uint32_t, uint32_t> range)
{
	std::vector<std::array<uint16_t, 3>> out;
	const auto end = std::min<size_t>(indices.size(), size_t {range.first} + range.second);
	for (size_t i = range.first; i + 2 < end; i += 3)
	{
		out.push_back({indices[i], indices[i + 1], indices[i + 2]});
	}
	return out;
}

void AddShadowParts(BlockSources& out, block_texture::BakeObject& object, const graphics::L3DMesh& mesh, const glm::mat4& model)
{
	auto& parts = out.parts.emplace_back();
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		// every sub-mesh but the physics ones and those with a status, whatever its level of detail
		const auto flags = subMesh->GetFlags();
		if (subMesh->IsPhysics() || flags.status != 0)
		{
			continue;
		}
		const auto& positions = out.positions.emplace_back(subMesh->GetSkinLocalPositions());
		const auto& primitives = subMesh->GetPrimitives();
		const auto& ranges = subMesh->GetCollisionRanges();
		std::span<const block_texture::BoneRun> runs;
		std::span<const std::array<float, 12>> bones;
		if (flags.hasBones && mesh.IsBoned())
		{
			auto& boneRows = out.bones.emplace_back();
			for (const auto& bone : mesh.GetBoneMatrices())
			{
				boneRows.push_back(Rows(model * bone));
			}
			auto& boneRuns = out.runs.emplace_back();
			for (const auto bone : subMesh->GetSkinBones())
			{
				if (boneRuns.empty() || boneRuns.back().bone != bone)
				{
					boneRuns.push_back({.count = 0, .bone = bone});
				}
				++boneRuns.back().count;
			}
			runs = boneRuns;
			bones = boneRows;
		}
		for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
		{
			const auto& triangles = out.triangles.emplace_back(TrianglesOf(subMesh->GetCollisionIndices(), ranges[p]));
			parts.push_back({.vertices = positions,
			                 .triangles = triangles,
			                 .doubleSided = primitives[p].twoSided,
			                 .runs = runs,
			                 .bones = bones});
		}
	}
	object.shadow = parts;
}

void AddAlphaParts(BlockSources& out, block_texture::BakeObject& object, const graphics::L3DMesh& mesh)
{
	auto& parts = out.alphaParts.emplace_back();
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		// the first level of detail's drawn sub-meshes; a skinned one draws none
		const auto flags = subMesh->GetFlags();
		if ((flags.lodMask & 1) != 1 || subMesh->IsPhysics() || flags.hasBones)
		{
			continue;
		}
		const auto& positions = out.positions.emplace_back(subMesh->GetSkinLocalPositions());
		const auto& primitives = subMesh->GetPrimitives();
		const auto& ranges = subMesh->GetCollisionRanges();
		for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
		{
			const auto* alpha = AlphaOf(mesh, primitives[p].skinID);
			if (alpha == nullptr)
			{
				continue;
			}
			const auto& triangles = out.triangles.emplace_back(TrianglesOf(subMesh->GetCollisionIndices(), ranges[p]));
			parts.push_back({.positions = positions,
			                 .uvs = subMesh->GetCollisionUVs(),
			                 .triangles = triangles,
			                 .alpha = std::span(alpha, 64 * 64)});
		}
	}
	object.alphaShadow = parts;
}

// The river beds and their channels, as the land keeps its own footprints: the newest first (each segment's channel
// went in before its bed); a channel lowers the land's alpha
void AddLandPrints(BlockSources& out, const Registry& registry)
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto footprintOf = [&meshes](const char* name) -> const graphics::L3DMesh::Footprint* {
		const entt::id_type id = entt::hashed_string(name).value();
		if (!meshes.Contains(id) || meshes.Handle(id)->GetFootprints().empty())
		{
			return nullptr;
		}
		return &meshes.Handle(id)->GetFootprints()[0];
	};
	const auto* bed = footprintOf("river2");
	const auto* channel = footprintOf("river");
	std::vector<std::pair<uint32_t, block_texture::LandPrint>> prints;
	registry.Each<const StreamFootprint, const Transform>(
	    [&](const StreamFootprint& stream, const Transform& transform) {
		    const auto* footprint = stream.channel ? channel : bed;
		    if (footprint == nullptr)
		    {
			    return;
		    }
		    const auto model = affine::Model(transform);
		    glm::vec4 box {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
		                   std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
		    for (const auto& corner : footprint->corners)
		    {
			    const auto world = model * glm::vec4(corner.x, 0.0f, corner.y, 1.0f);
			    box = {std::min(box.x, world.x), std::min(box.y, world.z), std::max(box.z, world.x), std::max(box.w, world.z)};
		    }
		    prints.emplace_back(stream.sequence,
		                        block_texture::LandPrint {
		                            .picture = footprint->Picture(),
		                            .placement = Rows(model),
		                            .mode = stream.channel ? block_texture::PrintMode::Alpha : block_texture::PrintMode::Colour,
		                            .strength = 0x100,
		                            .box = box,
		                        });
	    },
	    entt::exclude<Unavailable>);
	std::ranges::sort(prints, std::greater {}, &std::pair<uint32_t, block_texture::LandPrint>::first);
	for (auto& [sequence, print] : prints)
	{
		out.landPrints.push_back(print);
	}
}
// The snow lying on the land, when there is some: its depths, the noise its edges are ragged by (snowmap.raw) and the
// whites made from the noise, 14 or 15 a nibble, as the snow cover makes them
void AddSnow(BlockSources& out)
{
	if (!Locator::snowSystem::has_value() || !Locator::snowSystem::value().HasSnow())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto& noise =
	    resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(),
	                                fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "snowmap.raw", "snow's noise");
	constexpr size_t k_NoiseTexels = 256 * 256;
	if (noise.size() < k_NoiseTexels)
	{
		return;
	}
	out.whites.resize(k_NoiseTexels);
	for (size_t i = 0; i < k_NoiseTexels; ++i)
	{
		out.whites[i] = snow_cover::LandWhite(noise[i]);
	}
	out.snow =
	    std::make_unique<block_texture::SnowLayer>(block_texture::SnowLayer {.depths = Locator::snowSystem::value().GetDepths(),
	                                                                         .noise = std::span(noise).first(k_NoiseTexels),
	                                                                         .whites = out.whites});
}
} // namespace

std::vector<entt::entity> ObjectsOverBlock(glm::uvec2 block)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const int32_t first = static_cast<int32_t>(block.x) * k_CellsPerBlock - k_Margin;
	const int32_t firstZ = static_cast<int32_t>(block.y) * k_CellsPerBlock - k_Margin;
	const int32_t lastX = std::min(first + k_CellsPerBlock + 2 * k_Margin, k_LastCell);
	const int32_t lastZ = std::min(firstZ + k_CellsPerBlock + 2 * k_Margin, k_LastCell);
	std::vector<entt::entity> objects;
	std::unordered_set<entt::entity> reached;
	const auto reach = [&](entt::entity object) {
		if (map_cells::IsReadable(object) && reached.insert(object).second)
		{
			objects.push_back(object);
		}
	};
	for (int32_t x = std::max(first, 0); x <= lastX; ++x)
	{
		for (int32_t z = std::max(firstZ, 0); z <= lastZ; ++z)
		{
			const glm::ivec2 cell {x, z};
			for (auto object = map_cells::FirstFixed(cell); object != entt::null; object = map_cells::GetMapChild(object, cell))
			{
				// an unavailable object ends the cell's fixed list
				if (registry.AllOf<Unavailable>(object))
				{
					break;
				}
				reach(object);
			}
			for (auto object = map_cells::FirstMobile(cell); object != entt::null;
			     object = map_cells::GetMapChild(object, cell))
			{
				reach(object);
			}
		}
	}
	return objects;
}

BlockSources SourcesFor(glm::uvec2 block, bool lowResolution)
{
	BlockSources out;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	for (const auto entity : ObjectsOverBlock(block))
	{
		const auto* mesh = registry.TryGet<const Mesh>(entity);
		if (mesh == nullptr || !registry.AllOf<Transform>(entity) || !meshes.Contains(mesh->id))
		{
			continue;
		}
		const bool casts = CastsShadow(registry, entity);
		const bool asks = AsksForFootprint(registry, entity);
		if (!casts && !asks)
		{
			continue;
		}
		const auto& model3d = *meshes.Handle(mesh->id);
		const auto model = DrawnBodyModel(registry, entity);
		auto& object = out.objects.emplace_back();
		object.placement = Rows(model);
		object.castsShadow = casts;
		object.printsFootprint = asks;
		object.temple = registry.AllOf<Temple>(entity);
		if (model3d.PrintsFootprint())
		{
			object.footprint = model3d.GetFootprints()[0].Picture();
		}
		if (casts)
		{
			object.shadowFromAlpha = registry.AnyOf<Tree, BigForest>(entity);
			if (object.shadowFromAlpha)
			{
				AddAlphaParts(out, object, model3d);
			}
			else
			{
				AddShadowParts(out, object, model3d, model);
			}
		}
	}
	AddLandPrints(out, registry);
	AddSnow(out);
	out.sources = {
	    .objects = out.objects, .landPrints = out.landPrints, .snow = out.snow.get(), .lowResolution = lowResolution};
	return out;
}

} // namespace openblack::ecs::land_prints
