/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "3D/BlockBake.h"
#include "3D/BlockTexture.h"
#include "3D/BlockTextureQueue.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LowResTile.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
struct LandData;

class LandIsland final: public LandIslandInterface
{
public:
	explicit LandIsland(const std::filesystem::path& path);
	/// A land made in memory, as the testbed's flat land is
	explicit LandIsland(const LandData& data);
	~LandIsland() noexcept;

	void LoadFromFile(const std::filesystem::path& path);

	[[nodiscard]] float GetHeightAt(glm::vec2 vec) const override { return HeightAt(vec, true); }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2 vec) const override { return HeightAt(vec, false); }
	[[nodiscard]] float GetDrawnHeightAt(glm::vec2 vec) const override { return HeightAt(vec, false, true); }
	/// meshFlattening: every corner of 3 or less at 0, as the landscape mesh is drawn (GetDrawnHeightAt)
	[[nodiscard]] float HeightAt(glm::vec2 vec, bool seaFlattening, bool meshFlattening = false) const;
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const override;
	[[nodiscard]] const LandBlock* GetBlock(const glm::u8vec2& coordinates) const;
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const override;
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& coordinates) const override
	{
		return BlockIndexAt(coordinates >> static_cast<uint16_t>(0x4)) != 0;
	}
	[[nodiscard]] uint8_t GetAltitudeBits() const override { return _altitudeBits; }
	void SetCellAltitude(glm::u16vec2 cell, uint16_t altitude) override;
	void CommitAltitudeChanges() override;
	[[nodiscard]] uint16_t GetCellsPerSide() const override { return static_cast<uint16_t>(_blocksPerSide * k_CellCount); }
	/// From the cell's own block: its border row and column too, also where the next block is missing (the ray cell test)
	[[nodiscard]] std::array<uint16_t, 4> GetCellCorners(glm::u16vec2 cell) const override;
	void RequestBlockTextures(std::span<const glm::ivec2> drawnNearestFirst) override;
	void UpdateBlockTextures(glm::vec3 eye, int32_t detailIndex) override;
	void RepaintAllBlockTextures() override;
	void MarkBlocksChanged(glm::ivec2 minPlace, glm::ivec2 maxPlace) override;
	[[nodiscard]] std::optional<BlockTileUv> GetBlockTile(size_t blockIndex) const override;
	[[nodiscard]] const graphics::Texture2D* GetLowResTexture() const override { return _lowResTexture.get(); }
	void SetTilesOnly(bool tilesOnly) override { _tilesOnly = tilesOnly; }
	[[nodiscard]] bool TilesOnly() const override { return _tilesOnly; }
	void SetShowTextureLevels(bool show) override { _showTextureLevels = show; }
	[[nodiscard]] bool ShowTextureLevels() const override { return _showTextureLevels; }
	void SetBakeLayers(uint8_t layers) override { _bakeLayers = layers; }
	[[nodiscard]] uint8_t BakeLayers() const override { return _bakeLayers; }
	[[nodiscard]] size_t BlockTextureRequests() const override { return _textureQueue.Requests().size(); }

	// Debug
	void DumpTextures() const override;
	void DumpMaps() const override;

private:
	/// The land from its data; name only for the log
	void Build(const LandData& data, const std::string& name);
	[[nodiscard]] std::vector<float> CreateHeightMap() const;
	/// Index + 1 of the block at these block coordinates, 0 where there is none
	[[nodiscard]] uint16_t BlockIndexAt(glm::u16vec2 blockCoordinates) const;
	[[nodiscard]] std::vector<uint8_t> CreateCellMap() const;
	/// Builds the block texture of the whole island (BlockTexture.h); OPENBLACK_DUMP_COAST_ALPHA writes it to a PNG
	void CreateBlockTexture();
	/// What the block texture is painted from
	[[nodiscard]] block_texture::Sources BlockTextureSources() const;
	/// Paints the whole texture of the block at this index and uploads it
	void PaintBlockTextureNow(size_t index);
	std::vector<LandBlock> _landBlocks;
	std::vector<lnd::LNDCountry> _countries;
	std::vector<LandMaterialInfo> _materialInfo;

	/// Index + 1 of the block at (x * _blocksPerSide + z), 0 where there is none; built from the blocks' own
	/// coordinates because the header table only covers 32 x 32 blocks and 255 indices (BWLandEditor maps go beyond)
	std::vector<uint16_t> _blockIndexLookup;
	uint16_t _blocksPerSide {32};
	uint8_t _altitudeBits {8};
	/// The blocks whose altitudes changed since the last CommitAltitudeChanges, by index
	std::vector<size_t> _changedBlocks;
	/// Per block: its texture is out of date, and it has been drawn since the land opened
	block_texture::BlockMarks _blockTextureMarks;
	/// The block textures waiting to be painted again, and the steps of the last frame's work
	block_texture::TextureQueue _textureQueue;
	std::vector<block_texture::Step> _textureSteps;
	/// A request's own texture while its strips are painted (RGBA8, rows along z): the block's texture keeps showing
	/// until the request finishes
	struct PaintingBlock
	{
		uint16_t block;
		std::vector<uint8_t> rgba;
	};
	std::vector<PaintingBlock> _paintingBlocks;
	/// The prints step's scratch (the shadow mask and its rasteriser), and (debug) the layers it bakes
	std::unique_ptr<block_texture::BakeScratch> _bakeScratch;
	uint8_t _bakeLayers {block_texture::k_BakeAll};
	/// The block's prints baked into its texels as the prints step does it: RGBA8 of ARGB4444, `pitch` texels a row,
	/// the block's first texel at `origin`
	void BakeBlock(size_t index, std::span<uint8_t> rgba, size_t pitch, glm::ivec2 origin);
	/// (debug) every block's tile only; each block tinted by its texture level
	bool _tilesOnly {false};
	bool _showTextureLevels {false};
	/// The corners SetCellAltitude changed since the last CommitAltitudeChanges
	std::optional<CornerBox> _changedCorners;
	/// The land file's low-resolution pages (ARGB 4444), and which of them changed since they were last sent to the card
	std::vector<low_res::Page> _lowResPages;
	std::vector<bool> _lowResPagesChanged;
	/// The pages on the card, one layer each (RGBA8, each nibble x 17); none without pages
	std::unique_ptr<graphics::Texture2D> _lowResTexture;
	/// Each block's tile: the page it is on (none: -1), where on it, and whether it has been rewritten since the land
	/// opened
	struct BlockTile
	{
		int32_t page {-1};
		low_res::TilePlace place;
		bool rewritten {false};
	};
	std::vector<BlockTile> _blockTiles;
	/// The pages and each block's tile, from the land file: a block's page is the one of the number it names, the first
	/// page when there is none of that number
	void PlaceTiles(const LandData& data);
	/// A block's tile rewritten from its finished texture (RGBA8 of ARGB 4444, rows along z, `pitch` texels a row, the
	/// block's first texel at `origin`), as every paint of a block ends: its page is then out of date
	void RewriteTile(size_t index, std::span<const uint8_t> rgba, size_t pitch, glm::ivec2 origin);
	/// The changed pages to the card
	void SendLowResPages();

	// Renderer, Dynamics
public:
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() override { return _landBlocks; }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const override { return _landBlocks; }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const override { return _countries; }
	[[nodiscard]] const std::vector<LandMaterialInfo>& GetMaterialInfo() const override { return _materialInfo; }

	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const override { return *_materialArray; }
	[[nodiscard]] const graphics::Texture2D& GetBump() const override { return *_textureBumpMap; }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const override { return *_smallBump; }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const override { return *_heightMap; }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const override { return *_cellMap; }
	[[nodiscard]] const graphics::Texture2D* GetBlockTexture() const override { return _blockTexture.get(); }

	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return U16Extent2 {_extentIndexMin, _extentIndexMax}; }
	[[nodiscard]] Extent2 GetExtent() const override { return Extent2 {_extentMin, _extentMax}; }

	uint8_t GetNoise(glm::u8vec2 pos) override;

private:
	std::unique_ptr<graphics::Texture2D> _materialArray;
	std::unique_ptr<graphics::Texture2D> _countryLookup;

	std::unique_ptr<graphics::Texture2D> _heightMap;
	std::unique_ptr<graphics::Texture2D> _cellMap;
	std::unique_ptr<graphics::Texture2D> _blockTexture;
	uint16_t _texelsPerBlock {256};
	std::unique_ptr<graphics::Texture2D> _textureNoiseMap;
	std::unique_ptr<graphics::Texture2D> _textureBumpMap;
	std::shared_ptr<graphics::Texture2D> _smallBump; ///< from the texture cache

	glm::u16vec2 _extentIndexMin;
	glm::u16vec2 _extentIndexMax;
	glm::vec2 _extentMin;
	glm::vec2 _extentMax;

	std::array<uint8_t, 256 * 256> _noiseMap;
	std::array<uint8_t, 256 * 256> _bumpMap {};
	std::vector<uint16_t> _materialTexels; ///< the LND material textures one after another, raw B5G5R5 [x * 256 + z]
};
} // namespace openblack

/*
LH3DIsland methods:
AdjustAlti((void *))
BuildSoundTGA((char *))
Create((void))
CreateCommonPart((void))
CreateSmallBump((void))
Draw((void))
DrawUnderWater((void))
GetAltitude((LH3DMapCoords const &))
GetAltitudeAndSetColorSpecular((LH3DMapCoords const &,ulong *,ulong *))
GetCell((long,long))
GetColorAndSpecular((LH3DMapCoords const &,ulong *,ulong *))
GetColorAndSpecular((LHPoint const *,ulong *,ulong *))
GetCountry((long,long,uchar &))
GetFogValue((LHPoint const *,ulong,ulong *))
GetIndex((long,long))
GetNormal((LH3DMapCoords const &,LHPoint *))
GetTerrainMaterial((ulong,ulong))
IsCompressed((void))
LoadFromDisk((char *))
PreDraw((void))
RayCast((LHPoint const &,LHPoint const &,float *,float *))
RayCastFrom2DPoint((LHCoord const &,float *,float *,bool,float))
RayCastInternal((float,float,float,float,float,float))
Release((void))
ReleaseSmallBump((void))
RequestUpdateAllTextures((void))
SaveOnDisk((char *))
SetColor((long,long,uchar))
SetCountry((long,long,long))
SetFileToLoad((char *))
SetFogColor((float,float,float,ulong))
SetFogMinMax((float,float))
SetHeightAsByte((long,long,long))
UseLowResTexture((int))
*/
