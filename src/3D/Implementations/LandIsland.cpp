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

#include <stdexcept>

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/LandBlock.h"
#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::graphics;

const uint8_t LandIslandInterface::k_CellCount = 16;
const float LandIslandInterface::k_HeightUnit = 0.67f;
const float LandIslandInterface::k_CellSize = 10.0f;

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

	_blockIndexLookup = lnd.GetHeader().lookUpTable;

	const auto& lndBlocks = lnd.GetBlocks();
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} blocks", lndBlocks.size());
	_landBlocks.resize(lndBlocks.size());
	for (size_t i = 0; i < _landBlocks.size(); i++)
	{
		_landBlocks[i].SetLndBlock(lndBlocks[i]);
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
	const auto heightMapData = CreateHeightMap();
	_heightMap->Create(indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::R8,
	                   Wrapping::ClampEdge, Filter::Linear,
	                   bgfx::makeRef(heightMapData.data(), static_cast<uint32_t>(heightMapData.size())));

	const auto res = indexSize * glm::u16vec2(lnd::LNDMaterial::k_Width, lnd::LNDMaterial::k_Height);
	_footprintFrameBuffer = std::make_unique<FrameBuffer>("Footprints", res.x, res.y, graphics::TextureFormat::RGBA8);

	_proj = glm::ortho(_extentMin.x, _extentMax.x, _extentMin.y, _extentMax.y);
	_view = glm::rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} countries", lnd.GetCountries().size());
	_countries = lnd.GetCountries();

	auto materialCount = static_cast<uint16_t>(lnd.GetMaterials().size());
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} textures", materialCount);
	std::vector<uint16_t> rgba5TextureData;
	rgba5TextureData.resize(lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * lnd.GetMaterials().size());
	for (size_t i = 0; i < lnd.GetMaterials().size(); i++)
	{
		std::memcpy(&rgba5TextureData[lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * i],
		            lnd.GetMaterials()[i].texels.data(),
		            sizeof(lnd.GetMaterials()[i].texels[0]) * lnd.GetMaterials()[i].texels.size());
	}
	_materialArray = std::make_unique<Texture2D>("LandIslandMaterialArray");
	_materialArray->Create(
	    lnd::LNDMaterial::k_Width, lnd::LNDMaterial::k_Height, materialCount, TextureFormat::BGR5A1, Wrapping::ClampEdge,
	    Filter::Linear,
	    bgfx::makeRef(rgba5TextureData.data(), static_cast<uint32_t>(rgba5TextureData.size() * sizeof(rgba5TextureData[0]))));

	// read noise map into Texture2D
	_noiseMap = lnd.GetExtra().noise.texels;
	_textureNoiseMap = std::make_unique<Texture2D>("LandIslandNoiseMap");
	_textureNoiseMap->Create(lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::ClampEdge,
	                         Filter::Linear,
	                         bgfx::makeRef(_noiseMap.data(), static_cast<uint32_t>(_noiseMap.size() * sizeof(_noiseMap[0]))));

	// read bump map into Texture2D
	_textureBumpMap = std::make_unique<Texture2D>("LandIslandBumpMap");
	_textureBumpMap->Create(
	    lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::Repeat, Filter::Linear,
	    bgfx::makeRef(lnd.GetExtra().bump.texels.data(),
	                  static_cast<uint32_t>(sizeof(lnd.GetExtra().bump.texels[0]) * lnd.GetExtra().bump.texels.size())));

	// build the meshes (we could move this elsewhere)
	for (auto& block : _landBlocks)
	{
		block.BuildMesh(*this);
	}
	bgfx::frame();
}

float LandIsland::GetHeightAt(glm::vec2 vec) const
{
	// LH3DIsland::GetAltitude (0x803090): the height of the landscape triangle under the point, in the original's
	// integer arithmetic. MapCoords are 16.16 fixed point with 10 units per cell; each cell is split into two triangles
	// along the diagonal chosen by its split bit, and the fourth corner is extrapolated from the other three so that
	// the bilinear blend below is planar on that triangle.
	const auto fixedX = static_cast<int64_t>(vec.x * 6553.6f);
	const auto fixedZ = static_cast<int64_t>(vec.y * 6553.6f);
	if (fixedX < 0 || fixedZ < 0 || (fixedX >> 16) >= 512 || (fixedZ >> 16) >= 512)
	{
		return 0.0f;
	}
	const auto cellX = static_cast<uint16_t>(fixedX >> 16);
	const auto cellZ = static_cast<uint16_t>(fixedZ >> 16);
	const auto fracX = static_cast<uint32_t>(fixedX & 0xFFFF);
	const auto fracZ = static_cast<uint32_t>(fixedZ & 0xFFFF);

	// The block stores 17 x 17 cells (one shared border row), so the neighbours are +1 (z) and +17 (x).
	const auto mapCoordinates = glm::u16vec2(cellX, cellZ) >> static_cast<uint16_t>(0x4);
	const uint8_t blockIndex = _blockIndexLookup.at(mapCoordinates.x << 5u | mapCoordinates.y);
	if (blockIndex == 0)
	{
		return 0.0f;
	}
	const auto* cells = _landBlocks[blockIndex - 1].GetCells();
	const auto* base = &cells[(cellX & 0xF) * 0x11u + (cellZ & 0xF)];
	int v00 = base[0].altitude;
	int v01 = base[1].altitude;
	int v10 = base[0x11].altitude;
	int v11 = base[0x12].altitude;
	// Next to the sea (base corner at most 4) heights of 3 or less count as 0 (g 0xC37BF4, on by default).
	if (v00 <= 4)
	{
		const auto sea = [](int v) { return v > 3 ? v : 0; };
		v00 = sea(v00);
		v01 = sea(v01);
		v10 = sea(v10);
		v11 = sea(v11);
	}
	int c00 = v00;
	int c01 = v01;
	int c10 = v10;
	int c11 = v11;
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
	const int fx = static_cast<int>(fracX >> 8);
	const int fz = static_cast<int>(fracZ >> 8);
	const int atX1 = (c11 - c10) * fz + (c10 << 8);
	const int atX0 = (c01 - c00) * fz + (c00 << 8);
	const int height = (((atX1 - atX0) * fx) >> 8) + atX0;
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

const LandBlock* LandIsland::GetBlock(const glm::u8vec2& coordinates) const
{
	// our blocks can only be between [0-31, 0-31]
	if (coordinates.x > 32 || coordinates.y > 32)
	{
		return nullptr;
	}

	const uint8_t blockIndex = _blockIndexLookup.at(coordinates.x * 32 + coordinates.y);
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

const lnd::LNDCell& LandIsland::GetCell(const glm::u16vec2& coordinates) const
{
	if (coordinates.x > 511 || coordinates.y > 511)
	{
		return k_EmptyCell;
	}

	const auto mapCoordinates = coordinates >> static_cast<uint16_t>(0x4);
	const auto cellCoordinates = static_cast<glm::u8vec2>(coordinates) & static_cast<uint8_t>(0xF);
	const auto lookupIndex = mapCoordinates.x << 5u | mapCoordinates.y;
	const auto cellIndex = cellCoordinates.x * 0x11u + cellCoordinates.y;

	const uint8_t blockIndex = _blockIndexLookup.at(lookupIndex);

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

std::vector<uint8_t> LandIsland::CreateHeightMap() const
{
	// 16x16 cells but the last is shared
	// max of 32x32 block grid
	// max of 512 x 512 pixels
	// extra pixel at the end of the map
	std::vector<uint8_t> data;
	const auto extentSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto resolution = extentSize * static_cast<uint16_t>(k_CellCount) + static_cast<uint16_t>(1);
	data.resize(resolution.x * resolution.y, 0);

	for (const auto& block : _landBlocks)
	{
		const auto blockOffset = static_cast<glm::u16vec2>(block.GetBlockPosition() * 16);
		const auto mapPos = block.GetBlockPosition() - static_cast<glm::ivec2>(_extentIndexMin);
		for (int y = 0; y < k_CellCount; y++)
		{
			for (int x = 0; x < k_CellCount; x++)
			{
				const auto offset = glm::u16vec2(x, y);
				const auto cellPos = mapPos * static_cast<int>(k_CellCount) + static_cast<glm::ivec2>(offset);
				const auto& cell = GetCell(blockOffset + offset);
				if ((cellPos.y * resolution.x) + cellPos.x < static_cast<int>(data.size()))
				{
					data.at((cellPos.y * resolution.x) + cellPos.x) = cell.altitude;
				}
			}
		}
	}
	return data;
}

void LandIsland::DumpMaps() const
{
	auto data = CreateHeightMap();
	FILE* fptr = fopen("dump.raw", "wb");
	fwrite(data.data(), data.size() * sizeof(data[0]), 1, fptr);
	fclose(fptr);
}
