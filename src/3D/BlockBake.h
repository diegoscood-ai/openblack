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
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "BlockPrints.h"
#include "BlockShadows.h"

/// The prints step of a block's texture: what the game bakes into the block's own texels once they are painted, in its
/// order (docs/wiki/world/terrain.md#the-prints-the-16th-step). No render dependencies: the objects, their meshes and
/// the land's own footprints come in as plain data.
namespace openblack::block_texture
{

/// A part of a mesh as the static shadow draws it: its vertices on the mesh and its triangles
struct ShadowPart
{
	std::span<const glm::vec3> vertices;
	std::span<const std::array<uint16_t, 3>> triangles;
	bool doubleSided {false};
	/// A skinned part: its runs of vertices and the bones' matrices that move them (none: the object's matrix)
	std::span<const BoneRun> runs;
	std::span<const std::array<float, 12>> bones;
};

/// An object over the block, in the order the bake reaches it (each object once)
struct BakeObject
{
	/// Its 3D object's matrix: right, up, forward and position
	std::array<float, 12> placement {};
	/// It casts a shadow on the land's textures; its mesh's parts that do (none for a part that is physics only or
	/// not drawn at the first level of detail)
	bool castsShadow {false};
	std::span<const ShadowPart> shadow;
	/// A tree's or a forest's: its shadow is its texture's alpha instead (block_texture::DrawTextureShadow), from these
	/// parts
	bool shadowFromAlpha {false};
	std::span<const TextureShadowPart> alphaShadow;
	/// It prints its footprint on the land's textures
	bool printsFootprint {false};
	/// Its mesh's footprint, when the mesh carries one: printed whenever the object casts a shadow or prints
	std::optional<PrintPicture> footprint;
	/// A temple's part: every triangle draws its shadow, whichever way it faces
	bool temple {false};
};

/// A footprint the land keeps of its own (a river bed and its channel, a waterfall): where it is, how it prints and
/// its box on the ground (min x, min z, max x, max z)
struct LandPrint
{
	PrintPicture picture;
	std::array<float, 12> placement {};
	PrintMode mode {PrintMode::Colour};
	uint32_t strength {k_DefaultPrintStrength};
	glm::vec4 box {0.0f};
};

/// (debug) The layers of the prints the bake puts in: all of them unless the Land Island window shows them one by one
constexpr uint8_t k_BakeShadows = 1;
constexpr uint8_t k_BakeFootprints = 2;
constexpr uint8_t k_BakeLandPrints = 4;
constexpr uint8_t k_BakeSnow = 8;
constexpr uint8_t k_BakeAll = 15;

/// What a block's prints come from
struct BakeSources
{
	std::span<const BakeObject> objects;
	/// In the order the land keeps them, the newest first
	std::span<const LandPrint> landPrints;
	/// None without the snow cover
	const SnowLayer* snow {nullptr};
	/// The low-resolution textures: the shadows half as dark
	bool lowResolution {false};
	/// (debug) Which layers go in (the k_Bake bits)
	uint8_t layers {k_BakeAll};
};

/// The scratch the bake keeps from one block to the next: the shadow mask (one byte a texel) and the rasteriser,
/// whose edge tables carry over as the game's do
struct BakeScratch
{
	explicit BakeScratch(int32_t size);
	int32_t size;
	std::vector<uint8_t> mask;
	ShadowRaster raster;
	std::array<uint8_t, 256> table {};
	bool lowResolution {false};
};

/// Bakes the prints of the block at (blockX, blockZ) (`mapPosition` its first corner, x and z) into its texels (ARGB
/// 4444, texel (x, z) at size x + z): each object's static shadow into the mask (cleared at the first) and then its
/// footprint, the land's footprints whose box meets the block, the snow, and last the shadows darken the texels
void BakePrints(std::span<uint16_t> texels, glm::uvec2 block, glm::vec2 mapPosition, const BakeSources& sources,
                BakeScratch& scratch);

} // namespace openblack::block_texture
