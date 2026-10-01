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

#include <algorithm>
#include <cstdlib>
#include <array>
#include <stdexcept>

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/allocator.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/BlockTexture.h"
#include "3D/CoastAlpha.h"
#include "3D/LandBlock.h"
#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Argb4444.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"
#include "Graphics/TextureUpscale.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"

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
/// Clamped like the original, or repeated when the terrain-x2 mod tiles the materials more than once per block or
/// projects them from the side on cliffs (world coordinates)
Wrapping MaterialWrapping()
{
	const auto& config = Locator::config::value();
	return config.terrainTextureDensity > 1.0f || config.terrainTriplanar ? Wrapping::Repeat : Wrapping::ClampEdge;
}

/// Materials that are one picture per block rather than a tiling texture (the terrain-x2 mod draws them once): the
/// figure geoglyph (Land1 material 10, Land5 material 5) and the maze (Land5 material 1), found by an FNV-1a hash of
/// their texels because nothing in the LND marks them (their type is shared with ordinary grass)
bool IsPictureMaterial(const lnd::LNDMaterial& material)
{
	constexpr std::array<uint64_t, 2> k_Pictures = {0xfa87f520ce8c6ff0, 0x75cf505eab6f1ecb};
	uint64_t hash = 0xcbf29ce484222325;
	const auto* bytes = reinterpret_cast<const uint8_t*>(material.texels.data());
	for (size_t i = 0; i < sizeof(material.texels); ++i)
	{
		hash = (hash ^ bytes[i]) * 0x100000001b3;
	}
	return std::ranges::find(k_Pictures, hash) != k_Pictures.end();
}

/// The small bump texture as the original builds it (fn_00804830 loads ".\data\Textures\smallbump.raw" with the
/// alpha flag; fn_00837400 packs it to ARGB4444 and ORs in "smallbumpa.raw" as the alpha nibble).
std::unique_ptr<Texture2D> CreateSmallBumpTexture()
{
	constexpr uint16_t k_Size = 256;
	constexpr size_t k_Pixels = static_cast<size_t>(k_Size) * k_Size;
	std::vector<uint8_t> rgba(k_Pixels * 4, 0);
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto directory = fileSystem.GetPath<filesystem::Path::Textures>();
		const auto rgb = fileSystem.ReadAll(fileSystem.FindPath(directory / "smallbump.raw"));
		// a missing smallbumpa.raw only makes fn_00837400 report it (0x837616): PackRaw takes the alpha from the colour
		// bytes left in the buffer (0x83761E)
		std::vector<uint8_t> alpha;
		try
		{
			alpha = fileSystem.ReadAll(fileSystem.FindPath(directory / "smallbumpa.raw"));
		}
		catch (const std::exception&)
		{
			alpha.clear();
		}
		if (rgb.size() != argb4444::k_ColourBytes || (!alpha.empty() && alpha.size() != argb4444::k_AlphaBytes))
		{
			throw std::runtime_error("unexpected size");
		}
		rgba = argb4444::PackRaw(rgb, alpha);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "[LandIsland] no small bump detail (smallbump.raw / smallbumpa.raw): {}",
		                   e.what());
	}
	auto texture = std::make_unique<Texture2D>("LandIslandSmallBump");
	texture->Create(k_Size, k_Size, 1, TextureFormat::RGBA8, Wrapping::Repeat, SurfaceTextureFilter(),
	                bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size())));
	return texture;
}
} // namespace

LandIsland::LandIsland(const std::filesystem::path& path)
{
	LoadFromFile(path);
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

	const auto& lndBlocks = lnd.GetBlocks();
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} blocks", lndBlocks.size());
	// BWLandEditor maps can have up to 128 x 128 blocks, more than 255 of them and up to 16 altitude bits; the original
	// ones have 32 x 32 and a header lookup table that matches the blocks' own coordinates (checked on all of them)
	_altitudeBits = lnd.GetAltitudeBits();
	_blocksPerSide = lnd.GetBlocksPerSide();
	constexpr uint16_t k_MaxBlocksPerSide = 128;
	if (_blocksPerSide > k_MaxBlocksPerSide || lndBlocks.size() >= std::numeric_limits<uint16_t>::max())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "[LandIsland] {}: {} blocks per side (at most {}), {} blocks", path.string(),
		                    _blocksPerSide, k_MaxBlocksPerSide, lndBlocks.size());
		throw std::runtime_error("LND block grid too large");
	}
	if (_altitudeBits != 8 || _blocksPerSide != 32)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "[LandIsland] {}: BWLandEditor map, {} x {} blocks, {} altitude bits",
		                   path.filename().string(), _blocksPerSide, _blocksPerSide, static_cast<int>(_altitudeBits));
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
	_heightMap->Create(indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RG32F,
	                   Wrapping::ClampEdge, Filter::Nearest,
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
	const auto res = indexSize * texelsPerBlock;
	_texelsPerBlock = texelsPerBlock;
	_footprintFrameBuffer = std::make_unique<FrameBuffer>("Footprints", res.x, res.y, graphics::TextureFormat::RGBA8);
	_staticShadowFrameBuffer = std::make_unique<FrameBuffer>("StaticShadows", res.x, res.y, graphics::TextureFormat::R8);
	_landAlphaFrameBuffer = std::make_unique<FrameBuffer>("LandAlpha", res.x, res.y, graphics::TextureFormat::R8);

	_proj = glm::ortho(_extentMin.x, _extentMax.x, _extentMin.y, _extentMax.y);
	_view = glm::rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} countries", lnd.GetCountries().size());
	_countries = lnd.GetCountries();

	auto materialCount = static_cast<uint16_t>(lnd.GetMaterials().size());
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} textures", materialCount);
	// kept: the block texture (BlockTexture.h) is built from them again when the altitudes change
	auto& rgba5TextureData = _materialTexels;
	rgba5TextureData.assign(lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * lnd.GetMaterials().size(), 0);
	for (size_t i = 0; i < lnd.GetMaterials().size(); i++)
	{
		std::memcpy(&rgba5TextureData[lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * i],
		            lnd.GetMaterials()[i].texels.data(),
		            sizeof(lnd.GetMaterials()[i].texels[0]) * lnd.GetMaterials()[i].texels.size());
	}
	_materialArray = std::make_unique<Texture2D>("LandIslandMaterialArray");
	if (Locator::config::value().terrainTexturesX2)
	{
		// Mod graphics.terrain-x2: decode the 5-bit texels and upscale every material 2x
		constexpr auto k_Width = lnd::LNDMaterial::k_Width;
		constexpr auto k_Height = lnd::LNDMaterial::k_Height;
		std::vector<uint8_t> rgba8(static_cast<size_t>(k_Width) * k_Height * 4 * materialCount);
		bx::DefaultAllocator allocator;
		for (uint16_t layer = 0; layer < materialCount; ++layer)
		{
			bimg::imageDecodeToRgba8(&allocator, &rgba8[static_cast<size_t>(k_Width) * k_Height * 4 * layer],
			                         &rgba5TextureData[static_cast<size_t>(k_Width) * k_Height * layer], k_Width, k_Height,
			                         k_Width * 4, bimg::TextureFormat::BGR5A1);
		}
		const auto upscaled = UpscaleRgba8Lanczos2x(rgba8.data(), k_Width, k_Height, materialCount);
		_materialArray->Create(k_Width * 2, k_Height * 2, materialCount, TextureFormat::RGBA8, MaterialWrapping(),
		                       SurfaceTextureFilter(), bgfx::copy(upscaled.data(), static_cast<uint32_t>(upscaled.size())));
	}
	else
	{
		_materialArray->Create(
		    lnd::LNDMaterial::k_Width, lnd::LNDMaterial::k_Height, materialCount, TextureFormat::BGR5A1,
		    MaterialWrapping(), SurfaceTextureFilter(),
		    bgfx::copy(rgba5TextureData.data(), static_cast<uint32_t>(rgba5TextureData.size() * sizeof(rgba5TextureData[0]))));
	}

	// read noise map into Texture2D
	_noiseMap = lnd.GetExtra().noise.texels;
	_textureNoiseMap = std::make_unique<Texture2D>("LandIslandNoiseMap");
	_textureNoiseMap->Create(lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::ClampEdge,
	                         Filter::Linear,
	                         bgfx::makeRef(_noiseMap.data(), static_cast<uint32_t>(_noiseMap.size() * sizeof(_noiseMap[0]))));

	// read bump map into Texture2D (and keep it for the block texture)
	_bumpMap = lnd.GetExtra().bump.texels;
	_textureBumpMap = std::make_unique<Texture2D>("LandIslandBumpMap");
	_textureBumpMap->Create(
	    lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::Repeat, SurfaceTextureFilter(),
	    bgfx::copy(lnd.GetExtra().bump.texels.data(),
	                  static_cast<uint32_t>(sizeof(lnd.GetExtra().bump.texels[0]) * lnd.GetExtra().bump.texels.size())));

	_smallBump = CreateSmallBumpTexture();
	CreateBlockTexture();

	std::vector<uint8_t> pictureMaterials(lnd.GetMaterials().size());
	_materialInfo.clear();
	for (size_t i = 0; i < lnd.GetMaterials().size(); ++i)
	{
		const auto& material = lnd.GetMaterials()[i];
		pictureMaterials[i] = IsPictureMaterial(material) ? 1 : 0;
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
		_materialInfo.push_back({material.type, pictureMaterials[i] != 0, colour, std::move(small)});
	}

	// build the meshes (we could move this elsewhere)
	_pictureMaterials = pictureMaterials;
	for (auto& block : _landBlocks)
	{
		block.BuildMesh(*this, pictureMaterials);
	}
	bgfx::frame();
}

float LandIsland::HeightAt(glm::vec2 vec, bool seaFlattening, bool meshFlattening) const
{
	// LH3DIsland::GetAltitude (0x803090): the height of the landscape triangle under the point, in the original's
	// integer arithmetic. MapCoords are 16.16 fixed point with 10 units per cell; each cell is split into two triangles
	// along the diagonal chosen by its split bit, and the fourth corner is extrapolated from the other three so that
	// the bilinear blend below is planar on that triangle.
	const auto fixedX = static_cast<int64_t>(vec.x * 6553.6f);
	const auto fixedZ = static_cast<int64_t>(vec.y * 6553.6f);
	const int64_t cellsPerSide = GetCellsPerSide();
	if (fixedX < 0 || fixedZ < 0 || (fixedX >> 16) >= cellsPerSide || (fixedZ >> 16) >= cellsPerSide)
	{
		return 0.0f;
	}
	const auto cellX = static_cast<uint16_t>(fixedX >> 16);
	const auto cellZ = static_cast<uint16_t>(fixedZ >> 16);
	const auto fracX = static_cast<uint32_t>(fixedX & 0xFFFF);
	const auto fracZ = static_cast<uint32_t>(fixedZ & 0xFFFF);

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
	// Next to the sea (base corner at most 4) heights of 3 or less count as 0 (g 0xC37BF4, on by default).
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
		// the drawn mesh (fn_00874AA0 0x874B95): every vertex of 3 or less at 0
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
	const auto delta = 0.1f;
	const auto posLeft = vec - glm::vec2(delta, 0.0f);
	const auto posRight = vec + glm::vec2(delta, 0.0f);
	const auto posBack = vec - glm::vec2(0.0f, delta);
	const auto posForward = vec + glm::vec2(0.0f, delta);

	const auto heightLeft = GetHeightAt(posLeft);
	const auto heightRight = GetHeightAt(posRight);
	const auto heightBack = GetHeightAt(posBack);
	const auto heightForward = GetHeightAt(posForward);

	const auto slopeLeftRight = glm::vec3(2.0f * delta, heightRight - heightLeft, 0.0f);
	const auto slopeBackForward = glm::vec3(0.0f, heightForward - heightBack, 2.0f * delta);

	auto normal = glm::cross(slopeBackForward, slopeLeftRight);
	normal = glm::normalize(normal);

	if (normal.y < 0)
	{
		normal = -normal;
	}

	return normal;
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
	cell.properties.fullWater = true;
	return cell;
}

constexpr lnd::LNDCell k_EmptyCell = EmptyCell();

void LandIsland::SetCellAltitude(glm::u16vec2 cell, uint16_t altitude)
{
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
			auto& target = block.cells.at(((cell.x & 0xF) + 16 * dx) * 17 + (cell.y & 0xF) + 16 * dz);
			target.altitude = static_cast<uint8_t>(altitude & 0xFFu);
			if (_altitudeBits > 8)
			{
				const auto highMask = static_cast<uint8_t>(0xFFu >> (16u - _altitudeBits));
				target.saveColor = static_cast<uint8_t>((target.saveColor & ~highMask) | ((altitude >> 8u) & highMask));
			}
			if (std::ranges::find(_changedBlocks, blockIndex - 1u) == _changedBlocks.end())
			{
				_changedBlocks.push_back(blockIndex - 1u);
			}
		}
	}
}

void LandIsland::RebuildAltitudes()
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
		// BuildMesh makes a new rigid body: the old one leaves the physics world first (if it is already in it) and the
		// new one takes its place with the same identity (DynamicsSystem::RegisterIslandRigidBodies)
		auto& body = _landBlocks[i].GetRigidBody();
		const bool inWorld = body != nullptr && body->getBroadphaseHandle() != nullptr && Locator::dynamicsSystem::has_value();
		const int userIndex = inWorld ? body->getUserIndex() : -1;
		const int userIndex2 = inWorld ? body->getUserIndex2() : -1;
		void* userPointer = inWorld ? body->getUserPointer() : nullptr;
		if (inWorld)
		{
			Locator::dynamicsSystem::value().RemoveRigidBody(body.get());
		}
		_landBlocks[i].BuildMesh(*this, _pictureMaterials);
		if (inWorld)
		{
			auto& rebuilt = _landBlocks[i].GetRigidBody();
			rebuilt->setUserIndex(userIndex);
			rebuilt->setUserIndex2(userIndex2);
			rebuilt->setUserPointer(userPointer);
			Locator::dynamicsSystem::value().AddRigidBody(rebuilt.get());
		}
	}
	_changedBlocks.clear();
	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto heightMapData = CreateHeightMap();
	_heightMap = std::make_unique<Texture2D>("Height Map");
	_heightMap->Create(indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RG32F,
	                   Wrapping::ClampEdge, Filter::Nearest,
	                   bgfx::copy(heightMapData.data(), static_cast<uint32_t>(heightMapData.size() * sizeof(heightMapData[0]))));
	CreateBlockTexture();
}

void LandIsland::CreateBlockTexture()
{
	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const block_texture::Materials materials {_materialTexels, _materialTexels.size() / lnd::LNDMaterial::k_Width /
	                                                               lnd::LNDMaterial::k_Height};
	const auto texels = block_texture::BuildIslandBlockTexture(*this, _extentIndexMin, indexSize, _texelsPerBlock,
	                                                           _noiseMap, _bumpMap, materials);
	const auto size = indexSize * _texelsPerBlock;
	// bilinear like the D3D sampling of the block textures (one texture over the whole island here, so the filter
	// also runs across block borders)
	_blockTexture = std::make_unique<Texture2D>("BlockTexture");
	_blockTexture->Create(size.x, size.y, 1, TextureFormat::RGBA8, Wrapping::ClampEdge, Filter::Linear,
	                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));

	// Debug: OPENBLACK_DUMP_COAST_ALPHA=1 (or =<file>.png) writes the alpha nibble (x 17) and OPENBLACK_DUMP_BLOCK_TEXTURE
	// the RGBA texture, x to the right and z down from the block extent minimum (compare with agua/sea_coast_alpha.py,
	// whose PNG starts at block 0)
	if (const char* dump = std::getenv("OPENBLACK_DUMP_COAST_ALPHA"); dump != nullptr && dump[0] != '\0')
	{
		const std::string file = std::string(dump) == "1" ? std::string("coast_alpha.png") : std::string(dump);
		std::vector<uint8_t> alpha(texels.size() / 4);
		for (size_t i = 0; i < alpha.size(); ++i)
		{
			alpha[i] = texels[i * 4 + 3];
		}
		const int ok = stbi_write_png(file.c_str(), size.x, size.y, 1, alpha.data(), size.x);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "[LandIsland] coast alpha {} x {} ({} texels per block, first block {}, {}) -> {} ({})",
		                   size.x, size.y, _texelsPerBlock, _extentIndexMin.x, _extentIndexMin.y, file,
		                   ok != 0 ? "ok" : "FAILED");
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
	// Where there is no block (the open sea) fn_00801C90 gives the full light, table[255] ([0xEDDD08]), and no
	// specular (0x8020F8): colour 0, luminosity 255
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
				// fn_00801C90 reads the cell's first dword as a D3DCOLOR: bytes r, g, b -> blue, green, red
				texel[0] = cell.b;
				texel[1] = cell.g;
				texel[2] = cell.r;
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
	FILE* fptr = fopen("dump.raw", "wb");
	fwrite(data.data(), data.size() * sizeof(data[0]), 1, fptr);
	fclose(fptr);
}
