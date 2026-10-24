/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <deque>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/BlockBake.h"

/// What the prints step of a land block's texture bakes, taken from the game: the objects over the block in the order
/// the game's map walk reaches them, each with its static shadow and its footprint, and the land's own footprints (the
/// river beds) (docs/wiki/world/terrain.md#the-prints-the-16th-step)
namespace openblack::ecs::land_prints
{

/// The objects the prints step of the block at (x, z) reaches, each once, in its order: the cells from 4 before the
/// block to 4 past it on each side (25 x 25, within the map), x outer and z inner; in each cell the fixed list (cut at
/// an unavailable object), then the mobile list, each from its head
[[nodiscard]] std::vector<entt::entity> ObjectsOverBlock(glm::uvec2 block);

/// Everything a block's prints come from, with the storage the spans in `sources` point into
struct BlockSources
{
	block_texture::BakeSources sources;
	std::vector<block_texture::BakeObject> objects;
	std::vector<block_texture::LandPrint> landPrints;
	std::deque<std::vector<block_texture::ShadowPart>> parts;
	std::deque<std::vector<block_texture::TextureShadowPart>> alphaParts;
	std::deque<std::vector<glm::vec3>> positions;
	std::deque<std::vector<std::array<uint16_t, 3>>> triangles;
	std::deque<std::vector<block_texture::BoneRun>> runs;
	std::deque<std::vector<std::array<float, 12>>> bones;
	/// The snow lying on the land, with the whites made from its noise; none without snow
	std::unique_ptr<block_texture::SnowLayer> snow;
	std::vector<uint8_t> whites;
};

/// The prints of the block at (x, z) from the game as it is now, the snow lying on it among them
[[nodiscard]] BlockSources SourcesFor(glm::uvec2 block, bool lowResolution);

} // namespace openblack::ecs::land_prints
