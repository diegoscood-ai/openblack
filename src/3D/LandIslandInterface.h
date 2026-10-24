/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "BlockTextureQueue.h"
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
	uint16_t type {0};       ///< the LND material type: a TerrainMaterialType (18 grass, 7 sand, 15 solid rock...)
	glm::vec3 colour {0.0f}; ///< average texel colour 0..1 (the type often doesn't match the look: green "Earth")
	/// k_SmallSize x k_SmallSize box averages of the texels, rgb 0..255 by rows (row = texture v): the ground colour a
	/// few mip levels down, like the foliage shader samples it
	std::vector<uint8_t> small;
	static constexpr int k_SmallSize = 32;
};

class LandIslandInterface
{
public:
	virtual ~LandIslandInterface() = default;

	static const uint8_t k_CellCount;
	static const float k_HeightUnit;
	static const float k_CellSize;

	[[nodiscard]] virtual float GetHeightAt(glm::vec2) const = 0;
	/// The altitude with the sea flattening off (as a fish farm's creation asks for it)
	[[nodiscard]] virtual float GetUnflattenedHeightAt(glm::vec2) const = 0;
	/// The height of the landscape mesh as it is drawn: every vertex of altitude 3 or less at 0, not only next to a
	/// base corner of 4 or less like GetHeightAt
	[[nodiscard]] virtual float GetDrawnHeightAt(glm::vec2 vec) const { return GetUnflattenedHeightAt(vec); }
	[[nodiscard]] virtual glm::vec3 GetNormalAt(glm::vec2) const = 0;
	[[nodiscard]] virtual const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const = 0;
	/// A block holds this cell (the block index at [x >> 4][z >> 4] is set); GetCell returns an empty cell where none
	/// does
	[[nodiscard]] virtual bool HasBlockAt(const glm::u16vec2& /*coordinates*/) const { return true; }
	/// Altitude bits of the loaded LND: 8 in the original, up to 16 in BWLandEditor maps (EXT0 chunk)
	[[nodiscard]] virtual uint8_t GetAltitudeBits() const { return 8; }
	/// Cells per side of the block grid: 512 (32 blocks of 16) in the original, up to 2048 in BWLandEditor maps
	[[nodiscard]] virtual uint16_t GetCellsPerSide() const { return 512; }
	/// The cell's altitude in height units (k_HeightUnit), with the extra altitude bits of BWLandEditor maps
	[[nodiscard]] uint16_t GetCellAltitude(const lnd::LNDCell& cell) const;
	/// The altitudes (height units) of a cell's corners (x, z), (x, z + 1), (x + 1, z), (x + 1, z + 1) as the original
	/// reads them from the cell's own block, its shared border row included. The default goes through GetCell;
	/// LandIsland reads the block
	[[nodiscard]] virtual std::array<uint16_t, 4> GetCellCorners(glm::u16vec2 cell) const;
	/// The land ray cast, the land's line test (3D/LandLine.h) over this island: the RAY from `from` through `to` (cell
	/// units: x, z x 0.1, y / 0.67), extended to the map's edge, walked cell by cell against the two triangles of each
	/// cell (RayCastCells). On a hit `hit` is its x, z in metres and the answer is true. Without one, a ray going down
	/// (to.y <= from.y, |dy| >= 0.0001) gets its crossing of y = 0 in `hit`, and true when that point is within 7500 m
	/// (in x z) of `camera` (the engine's camera).
	/// Users: the lightning's fork (Particles/Rules/Lightning.cpp) and, not ported yet, the camera update, the camera modes,
	/// the landscape draw and a few more
	[[nodiscard]] bool RayCast(const glm::vec3& from, const glm::vec3& to, glm::vec2& hit, const glm::vec3& camera) const;
	/// The ray cast in cell units (x, z, y / 0.67): the hit's x, z in cells
	[[nodiscard]] bool RayCastCells(float x0, float z0, float y0, float x1, float z1, float y1, glm::vec2& hit) const;

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
	/// Island-wide block texture (BlockTexture.h), RGBA8, same layout as the land alpha but rows along +z from the
	/// extent minimum: the original's ARGB4444 block textures (each nibble x 17), colour and coast alpha, 0 in the open
	/// sea cells; nullptr while none is built
	[[nodiscard]] virtual const graphics::Texture2D* GetBlockTexture() const { return nullptr; }

	[[nodiscard]] virtual U16Extent2 GetIndexExtent() const = 0;
	[[nodiscard]] virtual Extent2 GetExtent() const = 0;
	[[nodiscard]] virtual uint8_t GetNoise(glm::u8vec2 pos) = 0;

	/// Changes a cell's altitude (height units) in every block that stores it, the shared border row and column too;
	/// the land follows once CommitAltitudeChanges runs
	virtual void SetCellAltitude(glm::u16vec2 /*cell*/, uint16_t /*altitude*/) {}
	/// Rebuilds the meshes and physics shapes of the blocks whose meshes read a changed corner (the smooth normals read
	/// across the border, so a neighbour can be one) and the height map. The rebuilt blocks get new rigid bodies; in the
	/// physics world they take the old ones' places. The textures of the blocks that keep a changed corner are painted
	/// again: here for a block not drawn since the land opened, else through their queue (UpdateBlockTextures)
	virtual void CommitAltitudeChanges() {}
	/// The blocks drawn this frame, by their place on the block grid, nearest first: each one whose texture is out of
	/// date is asked for (block_texture::TextureQueue::Request), and is no longer out of date
	virtual void RequestBlockTextures(std::span<const glm::ivec2> /*drawnNearestFirst*/) {}
	/// One frame of the block textures' queue (block_texture::TextureQueue::Update) from the drawn eye, at the detail
	/// index 0..4: the strips it asks for are painted, and a finished block's texture replaces the old one
	virtual void UpdateBlockTextures(glm::vec3 /*eye*/, int32_t /*detailIndex*/) {}
	/// Every block's texture out of date, as the original's request to update all the land's textures does
	virtual void RepaintAllBlockTextures() {}
	/// The textures of the blocks in this rectangle of block places (map x and z / 160, both ends in) that exist are out
	/// of date (block_texture::BlocksMarkedBy): an object over them was put on the map's cells or taken off them
	virtual void MarkBlocksChanged(glm::ivec2 /*minPlace*/, glm::ivec2 /*maxPlace*/) {}
	/// (debug) Which layers of the prints the prints step bakes into the blocks' textures (block_texture::k_Bake bits),
	/// from the next paint of each block on
	virtual void SetBakeLayers(uint8_t /*layers*/) {}
	[[nodiscard]] virtual uint8_t BakeLayers() const { return 0; }
	/// The block textures waiting in the queue
	[[nodiscard]] virtual size_t BlockTextureRequests() const { return 0; }
	/// Where a block's low-resolution tile is: the layer of its page in GetLowResTexture and the tile's first texel as
	/// a page UV (the land file's fuLrs and fvLrs), none for a block without a tile
	struct BlockTileUv
	{
		uint16_t layer {0};
		glm::vec2 origin {0.0f};
	};
	[[nodiscard]] virtual std::optional<BlockTileUv> GetBlockTile(size_t /*blockIndex*/) const { return std::nullopt; }
	/// The land's low-resolution pages, one layer each; none without pages
	[[nodiscard]] virtual const graphics::Texture2D* GetLowResTexture() const { return nullptr; }
	/// (debug) Every block shows its low-resolution tile only, as if beyond the texture detail line
	virtual void SetTilesOnly(bool /*tilesOnly*/) {}
	[[nodiscard]] virtual bool TilesOnly() const { return false; }
	/// (debug) Each block tinted by its texture level: its tile only red, its texture and tile blended blue
	virtual void SetShowTextureLevels(bool /*show*/) {}
	[[nodiscard]] virtual bool ShowTextureLevels() const { return false; }
	/// One entry per material of the LND (empty while no island is loaded)
	[[nodiscard]] virtual const std::vector<LandMaterialInfo>& GetMaterialInfo() const
	{
		static const std::vector<LandMaterialInfo> k_None;
		return k_None;
	}
};
} // namespace openblack
