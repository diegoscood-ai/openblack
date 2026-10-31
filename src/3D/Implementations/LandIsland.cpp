/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "LandIsland.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <stdexcept>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vector_relational.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/BlockTexture.h"
#include "3D/BlockTextureQueue.h"
#include "3D/CoastAlpha.h"
#include "3D/LandBlock.h"
#include "3D/LandData.h"
#include "3D/LandLine.h"
#include "3D/LandNormal.h"
#include "3D/LowResTile.h"
#include "3D/MapCoords.h"
#include "ECS/LandPrints.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Argb4444.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

const uint8_t LandIslandInterface::k_CellCount = 16;
const float LandIslandInterface::k_HeightUnit = 0.67f;
const float LandIslandInterface::k_CellSize = 10.0f;

uint16_t LandIslandInterface::GetCellAltitude(const lnd::LNDCell& cell) const
{
	return cell.Altitude(GetAltitudeBits());
}

namespace
{
/// The small bump texture as the original builds it ("smallbump.raw" loaded with the alpha flag, packed to ARGB4444
/// with "smallbumpa.raw" ORed in as the alpha nibble).
std::shared_ptr<Texture2D> CreateSmallBumpTexture()
{
	constexpr uint16_t k_Size = 256;
	constexpr auto k_Name = "LandIslandSmallBump";
	if (Locator::resources::has_value())
	{
		auto& textures = Locator::resources::value().GetTextures();
		const auto id = entt::hashed_string("land/smallbump").value();
		try
		{
			if (!textures.Contains(id))
			{
				// a missing smallbumpa.raw is allowed: PackRaw takes the alpha from the colour bytes left in its buffer
				auto& fileSystem = Locator::filesystem::value();
				const auto directory = fileSystem.GetPath<filesystem::Path::Textures>();
				textures.Load(id, resources::Texture2DLoader::FromColourAlphaTag {}, k_Name,
				              resources::Texture2DLoader::ColourAlphaDesc {
				                  .colour = directory / "smallbump.raw",
				                  .alpha = directory / "smallbumpa.raw",
				                  .size = k_Size,
				                  .packing = resources::Texture2DLoader::ColourAlpha::Argb4444,
				                  .wrap = Wrapping::Repeat,
				                  .filter = Filter::Linear,
				              });
			}
			return textures.Handle(id).handle();
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "[LandIsland] no small bump detail (smallbump.raw / smallbumpa.raw): {}",
			                   e.what());
		}
	}
	// no small bump detail: a blank texture (not cached, so the next land tries the files again)
	const std::vector<uint8_t> blank(static_cast<size_t>(k_Size) * k_Size * 4, 0);
	auto texture = std::make_shared<Texture2D>(k_Name);
	texture->Create(k_Size, k_Size, 1, TextureFormat::RGBA8, Wrapping::Repeat, Filter::Linear,
	                bgfx::copy(blank.data(), static_cast<uint32_t>(blank.size())));
	return texture;
}
} // namespace

LandIsland::LandIsland(const std::filesystem::path& path)
{
	LoadFromFile(path);
}

LandIsland::LandIsland(const LandData& data)
{
	Build(data, "generated land");
}

LandIsland::~LandIsland() noexcept = default;

void LandIsland::LoadFromFile(const std::filesystem::path& path)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading Land from file: {}", path.string());
	lnd::LNDFile lnd;

	const auto result = lnd.ReadFile(*Locator::filesystem::value().GetData(path));
	if (result != lnd::LNDResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open lnd file from filesystem {}: {}", path.string(),
		                    lnd::ResultToStr(result));
		throw lnd::ResultToStr(result);
	}

	Build(LandData::FromLnd(lnd), path.string());
}

void LandIsland::Build(const LandData& data, const std::string& name)
{
	const auto& lndBlocks = data.blocks;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} blocks", lndBlocks.size());
	// BWLandEditor maps can have up to 128 x 128 blocks, more than 255 of them and up to 16 altitude bits; the original
	// ones have 32 x 32 and a header lookup table that matches the blocks' own coordinates (checked on all of them)
	_altitudeBits = data.altitudeBits;
	_blocksPerSide = data.blocksPerSide;
	constexpr uint16_t k_MaxBlocksPerSide = 128;
	if (_blocksPerSide > k_MaxBlocksPerSide || lndBlocks.size() >= std::numeric_limits<uint16_t>::max())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "[LandIsland] {}: {} blocks per side (at most {}), {} blocks", name,
		                    _blocksPerSide, k_MaxBlocksPerSide, lndBlocks.size());
		throw std::runtime_error("LND block grid too large");
	}
	if (_altitudeBits != 8 || _blocksPerSide != 32)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "[LandIsland] {}: BWLandEditor map, {} x {} blocks, {} altitude bits", name,
		                   _blocksPerSide, _blocksPerSide, static_cast<int>(_altitudeBits));
	}
	_blockIndexLookup.assign(static_cast<size_t>(_blocksPerSide) * _blocksPerSide, 0);
	_landBlocks.resize(lndBlocks.size());
	for (size_t i = 0; i < _landBlocks.size(); i++)
	{
		auto block = lndBlocks[i];
		// like BWLandEditor, the block coordinates win over a map position that doesn't match them
		const auto expected = glm::vec2(block.blockX, block.blockZ) * (k_CellSize * k_CellCount);
		if (block.mapX != expected.x || block.mapZ != expected.y)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "[LandIsland] block {} at ({}, {}) has map position ({}, {})", i,
			                   block.blockX, block.blockZ, block.mapX, block.mapZ);
			block.mapX = expected.x;
			block.mapZ = expected.y;
		}
		// the file's value is not kept: the land works each block's highest point out of its cells as it opens
		block.highestAltitude = LandBlock::HighestAltitude(block, _altitudeBits);
		_landBlocks[i].SetLndBlock(block);
		auto& entry = _blockIndexLookup.at(static_cast<size_t>(block.blockX) * _blocksPerSide + block.blockZ);
		if (entry == 0)
		{
			entry = static_cast<uint16_t>(i + 1);
		}
	}

	_extentIndexMin.x = std::numeric_limits<uint16_t>::max();
	_extentIndexMin.y = std::numeric_limits<uint16_t>::max();
	_extentIndexMax.x = 0;
	_extentIndexMax.y = 0;
	for (auto& b : _landBlocks)
	{
		if (_extentIndexMin.x > b.GetLndBlock()->blockX)
		{
			_extentIndexMin.x = static_cast<uint16_t>(b.GetLndBlock()->blockX);
			_extentMin.x = b.GetMapPosition().x;
		}
		if (_extentIndexMax.x < b.GetLndBlock()->blockX)
		{
			_extentIndexMax.x = static_cast<uint16_t>(b.GetLndBlock()->blockX);
			_extentMax.x = b.GetMapPosition().x;
		}
		if (_extentIndexMin.y > b.GetLndBlock()->blockZ)
		{
			_extentIndexMin.y = static_cast<uint16_t>(b.GetLndBlock()->blockZ);
			_extentMin.y = b.GetMapPosition().y;
		}
		if (_extentIndexMax.y < b.GetLndBlock()->blockZ)
		{
			_extentIndexMax.y = static_cast<uint16_t>(b.GetLndBlock()->blockZ);
			_extentMax.y = b.GetMapPosition().y;
		}
	}
	_extentMax += k_CellSize * k_CellCount;

	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);

	_heightMap = std::make_unique<Texture2D>("Height Map");
	// per cell: altitude (height units) and split bit, point sampled: vs_object computes GetAltitude from them exactly
	const auto heightMapData = CreateHeightMap();
	_heightMap->Create(
	    indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RG32F, Wrapping::ClampEdge,
	    Filter::Nearest,
	    bgfx::copy(heightMapData.data(), static_cast<uint32_t>(heightMapData.size() * sizeof(heightMapData[0]))));

	_cellMap = std::make_unique<Texture2D>("Cell Map");
	const auto cellMapData = CreateCellMap();
	_cellMap->Create(indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RGBA8,
	                 Wrapping::ClampEdge, Filter::Nearest,
	                 bgfx::copy(cellMapData.data(), static_cast<uint32_t>(cellMapData.size())));

	// 256 texels per block like the original's block textures, fewer on BWLandEditor maps wider than 32 blocks (the
	// textures would pass 8192 texels)
	constexpr uint16_t k_MaxIslandTexture = 8192;
	const auto texelsPerBlock = static_cast<uint16_t>(
	    std::min<int>(lnd::LNDMaterial::k_Width, k_MaxIslandTexture / std::max(indexSize.x, indexSize.y)));
	_texelsPerBlock = texelsPerBlock;

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} countries", data.countries.size());
	_countries = data.countries;

	auto materialCount = static_cast<uint16_t>(data.materials.size());
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} textures", materialCount);
	// kept: the block texture (BlockTexture.h) is built from them again when the altitudes change
	auto& rgba5TextureData = _materialTexels;
	rgba5TextureData.assign(lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * data.materials.size(), 0);
	for (size_t i = 0; i < data.materials.size(); i++)
	{
		std::memcpy(&rgba5TextureData[lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * i],
		            data.materials[i].texels.data(), sizeof(data.materials[i].texels[0]) * data.materials[i].texels.size());
	}
	_materialArray = std::make_unique<Texture2D>("LandIslandMaterialArray");
	_materialArray->Create(
	    lnd::LNDMaterial::k_Width, lnd::LNDMaterial::k_Height, materialCount, TextureFormat::BGR5A1, Wrapping::ClampEdge,
	    Filter::Linear,
	    bgfx::copy(rgba5TextureData.data(), static_cast<uint32_t>(rgba5TextureData.size() * sizeof(rgba5TextureData[0]))));

	// read noise map into Texture2D
	if (data.noise.size() != _noiseMap.size() || data.bump.size() != _bumpMap.size())
	{
		throw std::runtime_error("LND noise or bump map of the wrong size");
	}
	std::ranges::copy(data.noise, _noiseMap.begin());
	_textureNoiseMap = std::make_unique<Texture2D>("LandIslandNoiseMap");
	_textureNoiseMap->Create(lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::ClampEdge,
	                         Filter::Linear,
	                         bgfx::copy(_noiseMap.data(), static_cast<uint32_t>(_noiseMap.size() * sizeof(_noiseMap[0]))));

	// read bump map into Texture2D (and keep it for the block texture)
	std::ranges::copy(data.bump, _bumpMap.begin());
	_textureBumpMap = std::make_unique<Texture2D>("LandIslandBumpMap");
	_textureBumpMap->Create(lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::Repeat,
	                        Filter::Linear,
	                        bgfx::copy(_bumpMap.data(), static_cast<uint32_t>(_bumpMap.size() * sizeof(_bumpMap[0]))));

	_smallBump = CreateSmallBumpTexture();
	PlaceTiles(data);
	CreateBlockTexture();

	_materialInfo.clear();
	for (size_t i = 0; i < data.materials.size(); ++i)
	{
		const auto& material = data.materials[i];
		glm::vec3 sum(0.0f);
		for (const auto& texel : material.texels)
		{
			sum += glm::vec3(texel.r, texel.g, texel.b);
		}
		const auto colour = sum / (31.0f * static_cast<float>(material.texels.size()));
		constexpr int k_Small = LandMaterialInfo::k_SmallSize;
		constexpr int k_Box = lnd::LNDMaterial::k_Width / k_Small;
		std::vector<uint8_t> small(static_cast<size_t>(k_Small) * k_Small * 3);
		for (int y = 0; y < k_Small; ++y)
		{
			for (int x = 0; x < k_Small; ++x)
			{
				glm::vec3 box(0.0f);
				for (int dy = 0; dy < k_Box; ++dy)
				{
					for (int dx = 0; dx < k_Box; ++dx)
					{
						const auto& texel = material.texels[static_cast<size_t>(y * k_Box + dy) * lnd::LNDMaterial::k_Width +
						                                    static_cast<size_t>(x * k_Box + dx)];
						box += glm::vec3(texel.r, texel.g, texel.b);
					}
				}
				box *= 255.0f / (31.0f * k_Box * k_Box);
				for (int c = 0; c < 3; ++c)
				{
					small[(static_cast<size_t>(y) * k_Small + x) * 3 + c] = static_cast<uint8_t>(box[c] + 0.5f);
				}
			}
		}
		_materialInfo.push_back({material.type, colour, std::move(small)});
	}

	// build the meshes (we could move this elsewhere); blocks built from the same inputs share their vertex list, and
	// each still gets its own vertex buffer, in the same order
	LandBlockMeshes sharedMeshes(_landBlocks, *this);
	for (size_t i = 0; i < _landBlocks.size(); ++i)
	{
		_landBlocks[i].BuildMesh(*this, sharedMeshes.Find(i));
	}
	bgfx::frame();
}

float LandIsland::HeightAt(glm::vec2 vec, bool seaFlattening, bool meshFlattening) const
{
	// The height of the landscape triangle under the point, in the original's integer arithmetic. MapCoords are 16.16
	// fixed point with 10 units per cell; each cell is split into two triangles along the diagonal chosen by its split
	// bit, and the fourth corner is extrapolated from the other three so that the bilinear blend below is planar on that
	// triangle.
	// The MapCoords of the point: ToFixed, as GetNormalAt
	const int32_t fixedX = map_coords::ToFixed(vec.x);
	const int32_t fixedZ = map_coords::ToFixed(vec.y);
	// the high words read signed, 0 when negative or past the side (the original checks against 512; openblack's
	// islands may be larger)
	const int32_t signedX = map_coords::SignedCellOf(fixedX);
	const int32_t signedZ = map_coords::SignedCellOf(fixedZ);
	const int32_t cellsPerSide = GetCellsPerSide();
	if (signedX < 0 || signedZ < 0 || signedX >= cellsPerSide || signedZ >= cellsPerSide)
	{
		return 0.0f;
	}
	const auto cellX = static_cast<uint16_t>(signedX);
	const auto cellZ = static_cast<uint16_t>(signedZ);
	const auto fracX = static_cast<uint32_t>(fixedX) & 0xFFFFu;
	const auto fracZ = static_cast<uint32_t>(fixedZ) & 0xFFFFu;

	// The block stores 17 x 17 cells (one shared border row), so the neighbours are +1 (z) and +17 (x).
	const auto mapCoordinates = glm::u16vec2(cellX, cellZ) >> static_cast<uint16_t>(0x4);
	const auto blockIndex = BlockIndexAt(mapCoordinates);
	if (blockIndex == 0)
	{
		return 0.0f;
	}
	const auto* cells = _landBlocks[blockIndex - 1].GetCells();
	const auto* base = &cells[(cellX & 0xF) * 0x11u + (cellZ & 0xF)];
	// (64 bits: with 16 altitude bits of BWLandEditor maps the products below overflow 32)
	int64_t v00 = GetCellAltitude(base[0]);
	int64_t v01 = GetCellAltitude(base[1]);
	int64_t v10 = GetCellAltitude(base[0x11]);
	int64_t v11 = GetCellAltitude(base[0x12]);
	// Next to the sea (base corner at most 4) heights of 3 or less count as 0 (a global switch, on by default).
	if (seaFlattening && v00 <= 4)
	{
		const auto sea = [](int64_t v) { return v > 3 ? v : int64_t {0}; };
		v00 = sea(v00);
		v01 = sea(v01);
		v10 = sea(v10);
		v11 = sea(v11);
	}
	else if (meshFlattening)
	{
		// the drawn mesh: every vertex of 3 or less at 0
		const auto sea = [](int64_t v) { return v > 3 ? v : int64_t {0}; };
		v00 = sea(v00);
		v01 = sea(v01);
		v10 = sea(v10);
		v11 = sea(v11);
	}
	int64_t c00 = v00;
	int64_t c01 = v01;
	int64_t c10 = v10;
	int64_t c11 = v11;
	if (base[0].properties.split)
	{
		if (fracZ > 0xFFFFu - fracX)
		{
			c00 = v10 + v01 - v11;
		}
		else
		{
			c11 = v10 + v01 - v00;
		}
	}
	else if (fracX > fracZ)
	{
		c01 = v00 + v11 - v10;
	}
	else
	{
		c10 = v00 + v11 - v01;
	}
	const int64_t fx = fracX >> 8;
	const int64_t fz = fracZ >> 8;
	const int64_t atX1 = (c11 - c10) * fz + (c10 << 8);
	const int64_t atX0 = (c01 - c00) * fz + (c00 << 8);
	const int64_t height = (((atX1 - atX0) * fx) >> 8) + atX0;
	return static_cast<float>(height) * LandIsland::k_HeightUnit * (1.0f / 256.0f);
}

glm::vec3 LandIsland::GetNormalAt(glm::vec2 vec) const
{
	// The normal at the MapCoords of the point: every caller builds them as x * 65536 * 0.1, and 65536 * 0.1f is exactly
	// 6553.6f: ToFixed
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	const int32_t fixedX = map_coords::ToFixed(vec.x);
	const int32_t fixedZ = map_coords::ToFixed(vec.y);
	// the high words read signed (the original checks against 512; openblack's islands may be larger)
	const int32_t cellX = map_coords::SignedCellOf(fixedX);
	const int32_t cellZ = map_coords::SignedCellOf(fixedZ);
	const int32_t cellsPerSide = GetCellsPerSide();
	if (cellX < 0 || cellX >= cellsPerSide || cellZ < 0 || cellZ >= cellsPerSide)
	{
		return up;
	}
	// no block there -> up
	const auto blockIndex = BlockIndexAt(glm::u16vec2(cellX, cellZ) >> static_cast<uint16_t>(0x4));
	if (blockIndex == 0)
	{
		return up;
	}
	// the cell inside its block of 17 x 17 (the shared border row): +17 cells = x + 1, +1 = z + 1; raw altitudes, no sea
	// flattening
	const auto* cells = _landBlocks[blockIndex - 1].GetCells();
	const auto* base = &cells[static_cast<uint32_t>(cellX & 0xF) * 0x11u + static_cast<uint32_t>(cellZ & 0xF)];
	return land_normal::OfCell(static_cast<uint32_t>(fixedX) & 0xFFFFu, static_cast<uint32_t>(fixedZ) & 0xFFFFu,
	                           base[0].properties.split, GetCellAltitude(base[0]), GetCellAltitude(base[1]),
	                           GetCellAltitude(base[0x11]), GetCellAltitude(base[0x12]));
}

uint8_t LandIsland::GetNoise(glm::u8vec2 pos)
{
	return _noiseMap.at(pos.x * 256 + pos.y);
}

uint16_t LandIsland::BlockIndexAt(glm::u16vec2 blockCoordinates) const
{
	if (blockCoordinates.x >= _blocksPerSide || blockCoordinates.y >= _blocksPerSide)
	{
		return 0;
	}
	return _blockIndexLookup[static_cast<size_t>(blockCoordinates.x) * _blocksPerSide + blockCoordinates.y];
}

const LandBlock* LandIsland::GetBlock(const glm::u8vec2& coordinates) const
{
	const auto blockIndex = BlockIndexAt(glm::u16vec2(coordinates));
	if (blockIndex == 0)
	{
		return nullptr;
	}

	return &_landBlocks[blockIndex - 1];
}

constexpr lnd::LNDCell EmptyCell() noexcept
{
	lnd::LNDCell cell {};
	cell.properties.unused6 = true;
	return cell;
}

constexpr lnd::LNDCell k_EmptyCell = EmptyCell();

void LandIsland::SetCellAltitude(glm::u16vec2 cell, uint16_t altitude)
{
	// the box of the changed corners, for the block meshes that read them
	const glm::ivec2 changed(cell);
	_changedCorners = _changedCorners.has_value() ? CornerBox {.min = glm::min(_changedCorners->min, changed),
	                                                           .max = glm::max(_changedCorners->max, changed)}
	                                              : CornerBox {.min = changed, .max = changed};
	// the corner is in its own block at (x & 15, z & 15) and, on a block border, also in the blocks before it as
	// their shared row / column 16
	for (int dx = 0; dx <= 1; ++dx)
	{
		for (int dz = 0; dz <= 1; ++dz)
		{
			if ((dx == 1 && (cell.x & 0xF) != 0) || (dz == 1 && (cell.y & 0xF) != 0))
			{
				continue;
			}
			const int blockX = (cell.x >> 4) - dx;
			const int blockZ = (cell.y >> 4) - dz;
			if (blockX < 0 || blockZ < 0)
			{
				continue;
			}
			const auto blockIndex = BlockIndexAt(glm::u16vec2(blockX, blockZ));
			if (blockIndex == 0)
			{
				continue;
			}
			auto& block = *_landBlocks[blockIndex - 1].GetLndBlock();
			const glm::ivec2 corner((cell.x & 0xF) + 16 * dx, (cell.y & 0xF) + 16 * dz);
			auto& target = block.cells.at(static_cast<size_t>(corner.x) * 17 + static_cast<size_t>(corner.y));
			target.altitude = static_cast<uint8_t>(altitude & 0xFFu);
			if (_altitudeBits > 8)
			{
				const auto highMask = static_cast<uint8_t>(0xFFu >> (16u - _altitudeBits));
				target.saveColor = static_cast<uint8_t>((target.saveColor & ~highMask) | ((altitude >> 8u) & highMask));
			}
			const size_t index = blockIndex - 1u;
			if (std::ranges::find(_changedBlocks, index) == _changedBlocks.end())
			{
				_changedBlocks.push_back(index);
			}
			// every block that keeps the corner has its texture painted again
			_blockTextureMarks.Changed(index);
		}
	}
}

void LandIsland::CommitAltitudeChanges()
{
	if (_changedBlocks.empty())
	{
		return;
	}
	for (size_t i = 0; i < _landBlocks.size(); ++i)
	{
		const auto position = _landBlocks[i].GetBlockPosition();
		const bool near = std::ranges::any_of(_changedBlocks, [&](size_t changed) {
			const auto delta = glm::abs(_landBlocks[changed].GetBlockPosition() - position);
			return delta.x <= 1 && delta.y <= 1;
		});
		if (!near)
		{
			continue;
		}
		// Only a block whose mesh reads a changed corner is built again; the others would build the same mesh
		if (!_changedCorners.has_value() || LandBlock::ReadsCorners(position, *_changedCorners))
		{
			_landBlocks[i].BuildMesh(*this);
		}
	}
	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto heightMapData = CreateHeightMap();
	_heightMap = std::make_unique<Texture2D>("Height Map");
	_heightMap->Create(
	    indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RG32F, Wrapping::ClampEdge,
	    Filter::Nearest,
	    bgfx::copy(heightMapData.data(), static_cast<uint32_t>(heightMapData.size() * sizeof(heightMapData[0]))));
	// a changed block that has not been drawn since the land opened is painted again at once: its first paint in the
	// original comes with its first draw, after every change before it. The others go through the queue
	// (UpdateBlockTextures) and keep their old texture until it reaches them
	for (const auto index : _changedBlocks)
	{
		if (_blockTextureMarks.TakePaintNow(index))
		{
			PaintBlockTextureNow(index);
		}
	}
	_changedBlocks.clear();
	_changedCorners.reset();
}

block_texture::Sources LandIsland::BlockTextureSources() const
{
	return {
	    .countries = _countries,
	    .materials = {_materialTexels, _materialTexels.size() / lnd::LNDMaterial::k_Width / lnd::LNDMaterial::k_Height},
	    .noise = _noiseMap,
	    .bump = _bumpMap,
	    .altitudeBits = _altitudeBits,
	};
}

void LandIsland::RequestBlockTextures(std::span<const glm::ivec2> drawnNearestFirst)
{
	for (const auto& coords : drawnNearestFirst)
	{
		if (glm::any(glm::lessThan(coords, glm::ivec2(0))))
		{
			continue;
		}
		const auto blockIndex = BlockIndexAt(glm::u16vec2(coords));
		if (blockIndex == 0)
		{
			continue;
		}
		// a block's texture is asked for once each time it goes out of date, by the first draw after that
		const size_t index = blockIndex - 1u;
		// at its first draw its texture is painted again at once, its prints baked in
		if (!_blockTextureMarks.Drawn(index))
		{
			PaintBlockTextureNow(index);
		}
		if (_blockTextureMarks.TakeDrawn(index))
		{
			_textureQueue.Request(static_cast<uint16_t>(index), _landBlocks[index].GetMapPosition());
		}
	}
}

void LandIsland::UpdateBlockTextures(glm::vec3 eye, int32_t detailIndex)
{
	_textureSteps.clear();
	_textureQueue.Update(eye, detailIndex, _textureSteps);
	if (_textureSteps.empty())
	{
		return;
	}
	const auto sources = BlockTextureSources();
	const auto indexSize = glm::ivec2(_extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1));
	const auto side = static_cast<size_t>(_texelsPerBlock);
	for (const auto& step : _textureSteps)
	{
		const auto& block = _landBlocks.at(step.block);
		const auto* cells = block.GetCells();
		const auto position = block.GetBlockPosition() - glm::ivec2(_extentIndexMin);
		const bool inExtent = cells != nullptr && glm::all(glm::greaterThanEqual(position, glm::ivec2(0))) &&
		                      glm::all(glm::lessThan(position, indexSize));
		// each request paints into a texture of its own, which replaces the block's when the request finishes
		auto painting = std::ranges::find(_paintingBlocks, step.block, &PaintingBlock::block);
		if (painting == _paintingBlocks.end())
		{
			painting = _paintingBlocks.insert(
			    _paintingBlocks.end(), PaintingBlock {.block = step.block, .rgba = std::vector<uint8_t>(side * side * 4)});
		}
		switch (step.kind)
		{
		case block_texture::StepKind::Strip:
		{
			// a strip past the last one (a request that went on to paint all its strips part way) holds nothing
			if (!inExtent || step.strip >= block_texture::k_StripsPerBlock)
			{
				break;
			}
			// the strip's cells straight into the request's texels, the rest of them left as they are
			const block_texture::CellBox strip {.min = {step.strip, 0},
			                                    .max = {step.strip, block_texture::k_CellsPerBlock - 1}};
			const block_texture::Target target {
			    .rgba = painting->rgba, .width = side, .origin = {0, 0}, .texelsPerBlock = _texelsPerBlock};
			block_texture::BuildBlock(
			    std::span(cells, static_cast<size_t>(block_texture::k_CellsPerSide) * block_texture::k_CellsPerSide), sources,
			    target, strip);
			break;
		}
		case block_texture::StepKind::Prints:
			// the block's prints baked into its texels, from the objects over it now; they show with the finish
			if (inExtent)
			{
				BakeBlock(step.block, painting->rgba, side, {0, 0});
			}
			break;
		case block_texture::StepKind::Finish:
			if (inExtent && _blockTexture != nullptr)
			{
				const auto origin = position * static_cast<int>(_texelsPerBlock);
				_blockTexture->UpdateRegion(glm::u16vec2(origin), glm::u16vec2(_texelsPerBlock, _texelsPerBlock),
				                            painting->rgba.data(), static_cast<uint32_t>(painting->rgba.size()));
				RewriteTile(step.block, painting->rgba, side, {0, 0});
			}
			_paintingBlocks.erase(painting);
			break;
		}
	}
	SendLowResPages();
}

void LandIsland::PaintBlockTextureNow(size_t index)
{
	const auto& block = _landBlocks.at(index);
	const auto* cells = block.GetCells();
	const auto indexSize = glm::ivec2(_extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1));
	const auto position = block.GetBlockPosition() - glm::ivec2(_extentIndexMin);
	if (_blockTexture == nullptr || cells == nullptr || glm::any(glm::lessThan(position, glm::ivec2(0))) ||
	    glm::any(glm::greaterThanEqual(position, indexSize)))
	{
		return;
	}
	auto patch = block_texture::PaintBox(
	    std::span(cells, static_cast<size_t>(block_texture::k_CellsPerSide) * block_texture::k_CellsPerSide),
	    BlockTextureSources(), _texelsPerBlock, {});
	// its prints, as the prints step puts them in
	BakeBlock(index, patch.rgba, _texelsPerBlock, {0, 0});
	const auto origin = position * static_cast<int>(_texelsPerBlock);
	_blockTexture->UpdateRegion(glm::u16vec2(origin), glm::u16vec2(_texelsPerBlock, _texelsPerBlock), patch.rgba.data(),
	                            static_cast<uint32_t>(patch.rgba.size()));
	RewriteTile(index, patch.rgba, _texelsPerBlock, {0, 0});
	SendLowResPages();
}

void LandIsland::BakeBlock(size_t index, std::span<uint8_t> rgba, size_t pitch, glm::ivec2 origin)
{
	const auto size = static_cast<int32_t>(_texelsPerBlock);
	if (index >= _landBlocks.size() || size <= 0 || size > static_cast<int32_t>(low_res::k_PageSize) ||
	    !Locator::entitiesRegistry::has_value() || !Locator::mapCellsSystem::has_value())
	{
		return;
	}
	const auto side = static_cast<size_t>(size);
	const auto at = [&](size_t x, size_t z) {
		return ((static_cast<size_t>(origin.y) + z) * pitch + static_cast<size_t>(origin.x) + x) * 4;
	};
	if (at(side - 1, side - 1) + 3 >= rgba.size())
	{
		return;
	}
	// the block's texels as the game keeps them, ARGB 4444 with a row along x
	std::vector<uint16_t> texels(side * side);
	for (size_t z = 0; z < side; ++z)
	{
		for (size_t x = 0; x < side; ++x)
		{
			const auto i = at(x, z);
			texels[x * side + z] = graphics::argb4444::Pack(rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]);
		}
	}
	if (_bakeScratch == nullptr || _bakeScratch->size != size)
	{
		_bakeScratch = std::make_unique<block_texture::BakeScratch>(size);
	}
	const auto& lnd = *_landBlocks[index].GetLndBlock();
	const glm::uvec2 block {lnd.blockX, lnd.blockZ};
	auto prints = ecs::land_prints::SourcesFor(block, size < static_cast<int32_t>(low_res::k_PageSize));
	prints.sources.layers = _bakeLayers;
	block_texture::BakePrints(texels, block, _landBlocks[index].GetMapPosition(), prints.sources, *_bakeScratch);
	for (size_t z = 0; z < side; ++z)
	{
		for (size_t x = 0; x < side; ++x)
		{
			const auto texel = graphics::argb4444::Unpack(texels[x * side + z]);
			std::ranges::copy(texel, rgba.begin() + static_cast<std::ptrdiff_t>(at(x, z)));
		}
	}
}

std::optional<LandIslandInterface::BlockTileUv> LandIsland::GetBlockTile(size_t blockIndex) const
{
	if (blockIndex >= _blockTiles.size() || _blockTiles[blockIndex].page < 0 || _lowResTexture == nullptr)
	{
		return std::nullopt;
	}
	const auto& block = *_landBlocks[blockIndex].GetLndBlock();
	return BlockTileUv {.layer = static_cast<uint16_t>(_blockTiles[blockIndex].page), .origin = {block.fuLrs, block.fvLrs}};
}

void LandIsland::PlaceTiles(const LandData& data)
{
	_lowResPages.clear();
	std::vector<uint32_t> numbers;
	for (const auto& record : data.lowResolutionTextures)
	{
		// the shipped lands' pages are all 256 x 256, the one size the card's texture holds
		if (auto page = low_res::DecodePage(record);
		    page.has_value() && page->width == low_res::k_PageSize && page->height == low_res::k_PageSize)
		{
			_lowResPages.push_back(std::move(*page));
			numbers.push_back(record.header.index);
		}
	}
	_lowResPagesChanged.assign(_lowResPages.size(), true);
	_blockTiles.assign(_landBlocks.size(), {});
	if (!_lowResPages.empty())
	{
		for (size_t i = 0; i < _landBlocks.size(); ++i)
		{
			const auto& block = *_landBlocks[i].GetLndBlock();
			const auto found = std::ranges::find(numbers, block.lowResTexture);
			_blockTiles[i] = {.page = found != numbers.end() ? static_cast<int32_t>(found - numbers.begin()) : 0,
			                  .place = {.column = block.iuLrs, .row = block.ivLrs}};
		}
	}
	_lowResTexture.reset();
	if (!_lowResPages.empty())
	{
		_lowResTexture = std::make_unique<Texture2D>("LowResolutionPages");
		// an array of at least two layers, so that the shaders sample an array however many pages the land has
		_lowResTexture->Create(static_cast<uint16_t>(low_res::k_PageSize), static_cast<uint16_t>(low_res::k_PageSize),
		                       static_cast<uint16_t>(std::max<size_t>(_lowResPages.size(), 2)), TextureFormat::RGBA8,
		                       Wrapping::ClampEdge, Filter::Linear, nullptr);
	}
}

void LandIsland::RewriteTile(size_t index, std::span<const uint8_t> rgba, size_t pitch, glm::ivec2 origin)
{
	if (index >= _blockTiles.size() || _blockTiles[index].page < 0 || _texelsPerBlock != low_res::k_PageSize)
	{
		return;
	}
	// the block's texture as the game keeps it: ARGB 4444, row x holding the texels of the block's x
	constexpr size_t k_Side = low_res::k_PageSize;
	std::vector<uint16_t> texels(k_Side * k_Side);
	for (size_t z = 0; z < k_Side; ++z)
	{
		for (size_t x = 0; x < k_Side; ++x)
		{
			const size_t at = ((static_cast<size_t>(origin.y) + z) * pitch + static_cast<size_t>(origin.x) + x) * 4;
			if (at + 3 >= rgba.size())
			{
				return;
			}
			texels[x * k_Side + z] = graphics::argb4444::Pack(rgba[at], rgba[at + 1], rgba[at + 2], rgba[at + 3]);
		}
	}
	// the neighbours whose tile has been rewritten already: before and after along z, then along x
	const auto& lnd = *_landBlocks[index].GetLndBlock();
	const auto neighbour = [this](int64_t x, int64_t z) -> std::optional<low_res::NeighbourTile> {
		if (x < 0 || z < 0 || x >= _blocksPerSide || z >= _blocksPerSide)
		{
			return std::nullopt;
		}
		const auto blockIndex = BlockIndexAt(glm::u16vec2(static_cast<uint16_t>(x), static_cast<uint16_t>(z)));
		if (blockIndex == 0)
		{
			return std::nullopt;
		}
		const auto& tile = _blockTiles.at(blockIndex - 1u);
		if (!tile.rewritten || tile.page < 0)
		{
			return std::nullopt;
		}
		return low_res::NeighbourTile {.page = _lowResPages.at(static_cast<size_t>(tile.page)).texels, .place = tile.place};
	};
	const auto x = static_cast<int64_t>(lnd.blockX);
	const auto z = static_cast<int64_t>(lnd.blockZ);
	const low_res::TileNeighbours neighbours {
	    .lowerZ = neighbour(x, z - 1),
	    .higherZ = neighbour(x, z + 1),
	    .lowerX = neighbour(x - 1, z),
	    .higherX = neighbour(x + 1, z),
	};
	auto& tile = _blockTiles[index];
	auto& page = _lowResPages.at(static_cast<size_t>(tile.page));
	if (low_res::UpdateTile(page.texels, page.width, tile.place, texels, static_cast<uint32_t>(k_Side), false, neighbours))
	{
		tile.rewritten = true;
		_lowResPagesChanged[static_cast<size_t>(tile.page)] = true;
	}
}

void LandIsland::SendLowResPages()
{
	if (_lowResTexture == nullptr || !_lowResTexture->IsValid())
	{
		return;
	}
	for (size_t i = 0; i < _lowResPages.size(); ++i)
	{
		if (!_lowResPagesChanged[i])
		{
			continue;
		}
		const auto& page = _lowResPages[i];
		std::vector<uint8_t> rgba(page.texels.size() * 4);
		for (size_t t = 0; t < page.texels.size(); ++t)
		{
			const auto texel = graphics::argb4444::Unpack(page.texels[t]);
			std::ranges::copy(texel, rgba.begin() + static_cast<std::ptrdiff_t>(t * 4));
		}
		_lowResTexture->UpdateRegion(glm::u16vec2(0, 0),
		                             glm::u16vec2(static_cast<uint16_t>(page.width), static_cast<uint16_t>(page.height)),
		                             rgba.data(), static_cast<uint32_t>(rgba.size()), static_cast<uint16_t>(i));
		_lowResPagesChanged[i] = false;
	}
}

void LandIsland::RepaintAllBlockTextures()
{
	_blockTextureMarks.AllOutOfDate();
}

void LandIsland::MarkBlocksChanged(glm::ivec2 minPlace, glm::ivec2 maxPlace)
{
	// only the places of the block grid can hold a block (BlockIndexAt answers 0 past it)
	const glm::ivec2 last(static_cast<int>(_blocksPerSide) - 1);
	const auto from = glm::max(minPlace, glm::ivec2(0));
	const auto to = glm::min(maxPlace, last);
	for (int x = from.x; x <= to.x; ++x)
	{
		for (int z = from.y; z <= to.y; ++z)
		{
			if (const auto blockIndex = BlockIndexAt(glm::u16vec2(x, z)); blockIndex != 0)
			{
				_blockTextureMarks.Changed(blockIndex - 1u);
			}
		}
	}
}

void LandIsland::CreateBlockTexture()
{
	// every block is painted here, as the land opens: none is out of date, none waits; the prints of every block are
	// drawn again until it is first drawn
	_blockTextureMarks.Reset(_landBlocks.size());
	_textureQueue.Clear();
	_paintingBlocks.clear();
	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto size = indexSize * _texelsPerBlock;
	const auto bytes = static_cast<uint32_t>(static_cast<size_t>(size.x) * size.y * 4);
	// bilinear like the D3D sampling of the block textures (one texture over the whole island here, so the filter
	// also runs across block borders)
	// made without texels and then filled, so that it stays updatable: a block painted again replaces only its own
	// rectangle
	_blockTexture = std::make_unique<Texture2D>("BlockTexture");
	_blockTexture->Create(size.x, size.y, 1, TextureFormat::RGBA8, Wrapping::ClampEdge, Filter::Linear, nullptr);
	// painted straight into the memory bgfx uploads, so that the texels are neither cleared first nor copied; bgfx
	// frees it after a later bgfx::frame(), so the dumps below can still read it
	const bgfx::Memory* memory = nullptr;
	std::vector<uint8_t> unsent;
	std::span<uint8_t> texels;
	if (_blockTexture->IsValid())
	{
		memory = bgfx::alloc(bytes);
		texels = std::span(memory->data, memory->size);
	}
	else
	{
		unsent.resize(bytes);
		texels = unsent;
	}
	block_texture::BuildIslandBlockTexture(*this, _extentIndexMin, indexSize, _texelsPerBlock, _noiseMap, _bumpMap,
	                                       BlockTextureSources().materials, texels);
	// every block's prints baked into its texels and its tile from them, as each paint of a block ends, in the blocks'
	// order
	for (size_t i = 0; i < _landBlocks.size(); ++i)
	{
		const auto position = _landBlocks[i].GetBlockPosition() - glm::ivec2(_extentIndexMin);
		if (glm::any(glm::lessThan(position, glm::ivec2(0))) ||
		    glm::any(glm::greaterThanEqual(position, glm::ivec2(indexSize))))
		{
			continue;
		}
		BakeBlock(i, texels, size.x, position * static_cast<int>(_texelsPerBlock));
		RewriteTile(i, texels, size.x, position * static_cast<int>(_texelsPerBlock));
	}
	SendLowResPages();
	if (memory != nullptr)
	{
		_blockTexture->UpdateMemory(memory);
	}

	// Debug: OPENBLACK_DUMP_COAST_ALPHA=1 (or =<file>.png) writes the alpha nibble (x 17) and OPENBLACK_DUMP_BLOCK_TEXTURE
	// the RGBA texture, x to the right and z down from the block extent minimum
	if (const char* dump = std::getenv("OPENBLACK_DUMP_COAST_ALPHA"); dump != nullptr && dump[0] != '\0')
	{
		const std::string file = std::string(dump) == "1" ? std::string("coast_alpha.png") : std::string(dump);
		std::vector<uint8_t> alpha(texels.size() / 4);
		for (size_t i = 0; i < alpha.size(); ++i)
		{
			alpha[i] = texels[i * 4 + 3];
		}
		const int ok = stbi_write_png(file.c_str(), size.x, size.y, 1, alpha.data(), size.x);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "[LandIsland] coast alpha {} x {} ({} texels per block, first block {}, {}) -> {} ({})", size.x,
		                   size.y, _texelsPerBlock, _extentIndexMin.x, _extentIndexMin.y, file, ok != 0 ? "ok" : "FAILED");
	}
	if (const char* dump = std::getenv("OPENBLACK_DUMP_BLOCK_TEXTURE"); dump != nullptr && dump[0] != '\0')
	{
		const std::string file = std::string(dump) == "1" ? std::string("block_texture.png") : std::string(dump);
		const int ok = stbi_write_png(file.c_str(), size.x, size.y, 4, texels.data(), size.x * 4);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "[LandIsland] block texture {} x {} -> {} ({})", size.x, size.y, file,
		                   ok != 0 ? "ok" : "FAILED");
	}
}

const lnd::LNDCell& LandIsland::GetCell(const glm::u16vec2& coordinates) const
{
	const auto mapCoordinates = coordinates >> static_cast<uint16_t>(0x4);
	const auto cellCoordinates = static_cast<glm::u8vec2>(coordinates) & static_cast<uint8_t>(0xF);
	const auto cellIndex = cellCoordinates.x * 0x11u + cellCoordinates.y;

	const auto blockIndex = BlockIndexAt(mapCoordinates);

	if (blockIndex == 0)
	{
		return k_EmptyCell;
	}
	assert(_landBlocks.size() >= blockIndex);
	return _landBlocks[blockIndex - 1].GetCells()[cellIndex];
}

void LandIsland::DumpTextures() const
{
	_materialArray->DumpTexture();
}

std::vector<float> LandIsland::CreateHeightMap() const
{
	// 16x16 cells but the last is shared
	// 32x32 block grid in the original maps (512 x 512 pixels), up to 128x128 in BWLandEditor ones
	// extra pixel at the end of the map
	// Two floats per cell (x along the texture's u, z along v): the altitude in height units and the split bit. All
	// 17 x 17 cells of each block, so the last row and column of the map are there too (0 where there is no block,
	// as GetAltitude gives over the sea).
	std::vector<float> data;
	const auto extentSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto resolution = extentSize * static_cast<uint16_t>(k_CellCount) + static_cast<uint16_t>(1);
	data.resize(static_cast<size_t>(resolution.x) * resolution.y * 2, 0.0f);

	for (const auto& block : _landBlocks)
	{
		const auto* cells = block.GetCells();
		const auto mapPos = block.GetBlockPosition() - static_cast<glm::ivec2>(_extentIndexMin);
		for (int z = 0; z <= k_CellCount; z++)
		{
			for (int x = 0; x <= k_CellCount; x++)
			{
				const auto cellPos = mapPos * static_cast<int>(k_CellCount) + glm::ivec2(x, z);
				if (cellPos.x >= resolution.x || cellPos.y >= resolution.y)
				{
					continue;
				}
				const auto& cell = cells[x * (k_CellCount + 1) + z];
				auto* texel = &data[(static_cast<size_t>(cellPos.y) * resolution.x + cellPos.x) * 2];
				texel[0] = static_cast<float>(GetCellAltitude(cell));
				texel[1] = cell.properties.split ? 1.0f : 0.0f;
			}
		}
	}
	return data;
}

std::vector<uint8_t> LandIsland::CreateCellMap() const
{
	const auto extentSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto resolution = extentSize * static_cast<uint16_t>(k_CellCount) + static_cast<uint16_t>(1);
	// Where there is no block (the open sea) the light is full (the last entry of the light table) and there is no
	// specular: colour 0, luminosity 255
	std::vector<uint8_t> data(static_cast<size_t>(resolution.x) * resolution.y * 4, 0);
	for (size_t i = 3; i < data.size(); i += 4)
	{
		data[i] = 255;
	}
	for (const auto& block : _landBlocks)
	{
		const auto blockOffset = static_cast<glm::u16vec2>(block.GetBlockPosition() * 16);
		const auto mapPos = block.GetBlockPosition() - static_cast<glm::ivec2>(_extentIndexMin);
		// 17 x 17: the last row and column are the first ones of the next block
		for (int y = 0; y <= k_CellCount; y++)
		{
			for (int x = 0; x <= k_CellCount; x++)
			{
				const auto cellPos = mapPos * static_cast<int>(k_CellCount) + glm::ivec2(x, y);
				if (cellPos.x >= resolution.x || cellPos.y >= resolution.y)
				{
					continue;
				}
				const auto& cell = GetCell(blockOffset + glm::u16vec2(x, y));
				auto* texel = &data[(static_cast<size_t>(cellPos.y) * resolution.x + cellPos.x) * 4];
				// the cell's first dword is read as a Direct3D colour: blue, green, red
				texel[0] = cell.red;
				texel[1] = cell.green;
				texel[2] = cell.blue;
				texel[3] = cell.luminosity;
			}
		}
	}
	return data;
}

void LandIsland::DumpMaps() const
{
	const auto heights = CreateHeightMap();
	std::vector<uint8_t> data(heights.size());
	std::ranges::transform(heights, data.begin(), [](float h) { return static_cast<uint8_t>(std::min(h, 255.0f)); });
	// in the working folder, as the debug window's button has always written it
	auto file =
	    Locator::filesystem::value().Open(std::filesystem::current_path() / "dump.raw", filesystem::Stream::Mode::Write);
	file->Write(data.data(), data.size() * sizeof(data[0]));
}

// ---------------------------------------------------------------------------------------------------------------------
// The island ray cast: the land's line test (land_line) over this island's cells.

std::array<uint16_t, 4> LandIslandInterface::GetCellCorners(glm::u16vec2 cell) const
{
	const auto next = [](uint16_t v) { return static_cast<uint16_t>(v + 1u); };
	return {GetCellAltitude(GetCell(cell)), GetCellAltitude(GetCell({cell.x, next(cell.y)})),
	        GetCellAltitude(GetCell({next(cell.x), cell.y})), GetCellAltitude(GetCell({next(cell.x), next(cell.y)}))};
}

std::array<uint16_t, 4> LandIsland::GetCellCorners(glm::u16vec2 cell) const
{
	const auto blockIndex = BlockIndexAt(cell >> static_cast<uint16_t>(0x4));
	if (blockIndex == 0)
	{
		return {0, 0, 0, 0};
	}
	// the cell is at (x & 15) x 17 + (z & 15) in its block; the neighbours are +1 (z) and +17 (x)
	const auto* base = &_landBlocks[blockIndex - 1].GetCells()[(cell.x & 0xFu) * 0x11u + (cell.y & 0xFu)];
	return {GetCellAltitude(base[0]), GetCellAltitude(base[1]), GetCellAltitude(base[0x11]), GetCellAltitude(base[0x12])};
}

bool LandIslandInterface::RayCastCells(float x0, float z0, float y0, float x1, float z1, float y1, glm::vec2& hit) const
{
	if (const auto found =
	        land_line::FirstHit(glm::vec3(x0, y0, z0), glm::vec3(x1, y1, z1), land_line::CellsOf(*this), GetCellsPerSide()))
	{
		hit = *found;
		return true;
	}
	return false;
}

bool LandIslandInterface::RayCast(const glm::vec3& from, const glm::vec3& to, glm::vec2& hit, const glm::vec3& camera) const
{
	if (const auto land = land_line::LandAlong(from, to, land_line::CellsOf(*this), GetCellsPerSide()))
	{
		hit = *land;
		return true;
	}
	// without land, a line going down gets its crossing of the sea's level in `hit` either way, a hit when it is near
	// enough to the camera
	const auto sea = land_line::SeaLevelAlong(from, to, camera);
	if (!sea.has_value())
	{
		return false;
	}
	hit = sea->point;
	return sea->inReach;
}
