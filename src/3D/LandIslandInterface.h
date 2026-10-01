/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>

#include "Extent.h"

namespace openblack
{
class LandBlock;
namespace graphics
{
class FrameBuffer;
class Texture2D;
} // namespace graphics
namespace lnd
{
struct LNDCell;
struct LNDCountry;
} // namespace lnd

/// What the engine knows about one landscape material (LND texture)
struct LandMaterialInfo
{
	uint16_t type {0};    ///< the LND material type: a TerrainMaterialType (18 grass, 7 sand, 15 solid rock...)
	bool picture {false}; ///< one picture per block rather than a tiling texture (see the terrain-x2 mod)
	glm::vec3 colour {0.0f}; ///< average texel colour 0..1 (the type often doesn't match the look: green "Earth")
	/// k_SmallSize x k_SmallSize box averages of the texels, rgb 0..255 by rows (row = texture v): the ground colour a
	/// few mip levels down, like the foliage shader samples it
	std::vector<uint8_t> small;
	static constexpr int k_SmallSize = 32;
};

class LandIslandInterface
{
public:
	static const uint8_t k_CellCount;
	static const float k_HeightUnit;
	static const float k_CellSize;

	[[nodiscard]] virtual float GetHeightAt(glm::vec2) const = 0;
	/// GetAltitude with the sea flattening off ([0xC37BF4] = 0, as FishFarm::CallVirtualFunctionsForCreation sets it)
	[[nodiscard]] virtual float GetUnflattenedHeightAt(glm::vec2) const = 0;
	/// The height of the landscape mesh as it is drawn: every vertex of altitude 3 or less at 0 (fn_00874AA0 0x874B95,
	/// SSE 0x7A1EE7), not only next to a base corner of 4 or less like GetAltitude
	[[nodiscard]] virtual float GetDrawnHeightAt(glm::vec2 vec) const { return GetUnflattenedHeightAt(vec); }
	[[nodiscard]] virtual glm::vec3 GetNormalAt(glm::vec2) const = 0;
	[[nodiscard]] virtual const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const = 0;
	/// A block holds this cell (g_index_block[x >> 4][z >> 4] != 0); GetCell returns an empty cell where none does
	[[nodiscard]] virtual bool HasBlockAt(const glm::u16vec2& /*coordinates*/) const { return true; }
	/// Altitude bits of the loaded LND: 8 in the original, up to 16 in BWLandEditor maps (EXT0 chunk)
	[[nodiscard]] virtual uint8_t GetAltitudeBits() const { return 8; }
	/// Cells per side of the block grid: 512 (32 blocks of 16) in the original, up to 2048 in BWLandEditor maps
	[[nodiscard]] virtual uint16_t GetCellsPerSide() const { return 512; }
	/// The cell's altitude in height units (k_HeightUnit), with the extra altitude bits of BWLandEditor maps
	[[nodiscard]] uint16_t GetCellAltitude(const lnd::LNDCell& cell) const;

	// Debug
	virtual void DumpTextures() const = 0;
	virtual void DumpMaps() const = 0;

	[[nodiscard]] virtual std::vector<LandBlock>& GetBlocks() = 0;
	[[nodiscard]] virtual const std::vector<LandBlock>& GetBlocks() const = 0;
	[[nodiscard]] virtual const std::vector<lnd::LNDCountry>& GetCountries() const = 0;

	[[nodiscard]] virtual const graphics::Texture2D& GetAlbedoArray() const = 0;
	[[nodiscard]] virtual const graphics::Texture2D& GetBump() const = 0;
	/// Detail texture blended over the land near the camera: rgb = smallbump.raw, a = smallbumpa.raw.
	[[nodiscard]] virtual const graphics::Texture2D& GetSmallBump() const = 0;
	[[nodiscard]] virtual const graphics::Texture2D& GetHeightMap() const = 0;
	/// Per cell: rgb = the cell colour as the original reads it for model specular (a D3DCOLOR: R and B swapped), a =
	/// luminosity; same layout as the height map, nearest filtering
	[[nodiscard]] virtual const graphics::Texture2D& GetCellMap() const = 0;
	[[nodiscard]] virtual const graphics::FrameBuffer& GetFootprintFramebuffer() const = 0;
	/// Static object shadows over the whole island, same layout as the footprints (256 texels per block, like the
	/// original's block textures); red = coverage
	[[nodiscard]] virtual const graphics::FrameBuffer& GetStaticShadowFramebuffer() const = 0;
	/// Island-wide land alpha, one texel per footprint texel: 1, or lower in the river channels (min of the river.l3d
	/// footprints, like the alpha nibble of the original's block textures)
	[[nodiscard]] virtual const graphics::FrameBuffer& GetLandAlphaFramebuffer() const = 0;
	/// Island-wide block texture (BlockTexture.h), RGBA8, same layout as the land alpha but rows along +z from the
	/// extent minimum: the original's ARGB4444 block textures (each nibble x 17), colour and coast alpha, 0 in the open
	/// sea cells; nullptr while none is built
	[[nodiscard]] virtual const graphics::Texture2D* GetBlockTexture() const { return nullptr; }

	[[nodiscard]] virtual U16Extent2 GetIndexExtent() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoView() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoProj() const = 0;
	[[nodiscard]] virtual Extent2 GetExtent() const = 0;
	virtual uint8_t GetNoise(glm::u8vec2 pos) = 0;

	/// Changes a cell's altitude (height units) in every block that stores it, the shared border row and column too
	/// (fn_00800DA0); the land follows once RebuildAltitudes runs
	virtual void SetCellAltitude(glm::u16vec2 /*cell*/, uint16_t /*altitude*/) {}
	/// Rebuilds the meshes and physics shapes of the blocks whose altitudes changed (and their neighbours, whose
	/// smooth normals read across the border) and the height map. Before DynamicsSystem::RegisterIslandRigidBodies
	/// only: the blocks get new rigid bodies.
	virtual void RebuildAltitudes() {}
	/// One entry per material of the LND (empty while no island is loaded)
	[[nodiscard]] virtual const std::vector<LandMaterialInfo>& GetMaterialInfo() const
	{
		static const std::vector<LandMaterialInfo> k_None;
		return k_None;
	}
};
} // namespace openblack
