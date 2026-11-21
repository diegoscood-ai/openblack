/******************************************************************************
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

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"

namespace openblack::test
{
/// Just the cells: blocks of 16 x 16 cells on a grid of `blocksPerSide` (the same fake as test_sea_cells)
class WaterCellIsland final: public LandIslandInterface
{
public:
	explicit WaterCellIsland(uint16_t blocksPerSide)
	    : _blocksPerSide(blocksPerSide)
	    , _cells(static_cast<size_t>(blocksPerSide) * 16 * blocksPerSide * 16)
	    , _hasBlock(static_cast<size_t>(blocksPerSide) * blocksPerSide, true)
	{
	}

	lnd::LNDCell& Cell(glm::u16vec2 cell) { return _cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y]; }

	/// the interpolated height is not modelled: a marker value so that the tests see when it is asked for
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return k_HeightMarker; }
	static constexpr float k_HeightMarker = 123.0f;
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		static const lnd::LNDCell k_Empty {};
		return HasBlockAt(cell) ? _cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y] : k_Empty;
	}
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& cell) const final
	{
		const auto block = cell >> static_cast<uint16_t>(4);
		return block.x < _blocksPerSide && block.y < _blocksPerSide &&
		       _hasBlock[static_cast<size_t>(block.x) * _blocksPerSide + block.y];
	}
	[[nodiscard]] uint16_t GetCellsPerSide() const final { return static_cast<uint16_t>(_blocksPerSide * 16); }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	uint16_t _blocksPerSide;
	std::vector<lnd::LNDCell> _cells;
	std::vector<bool> _hasBlock;
	std::vector<lnd::LNDCountry> _countries;
};

inline lnd::LNDCell MakeWaterCell(uint8_t altitude, bool water, bool coast)
{
	lnd::LNDCell cell {};
	cell.altitude = altitude;
	cell.properties.hasWater = water ? 1 : 0;
	cell.properties.coastLine = coast ? 1 : 0;
	return cell;
}

/// Flat dry land: every cell of the map at one altitude, and the land's height everywhere that altitude's
class FlatDryIsland final: public LandIslandInterface
{
public:
	explicit FlatDryIsland(uint8_t altitude)
	    : _cell(MakeWaterCell(altitude, false, false))
	{
	}

	/// The height of land at that altitude
	[[nodiscard]] static float HeightOf(uint8_t altitude) { return static_cast<float>(altitude) * k_HeightUnit; }

	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return HeightOf(_cell.altitude); }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return HeightOf(_cell.altitude); }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { return _cell; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	lnd::LNDCell _cell;
	std::vector<lnd::LNDCountry> _countries;
};

/// Dry land sloping up along x: the cells of column x at altitude base + rise x (clamped at 255), every row alike, and
/// the land's height that of the cells' corners between them, so that the height, the cells' corners and the normal agree
/// (x in metres, 10 to a cell). Its normal is the real slope's
class SlopedIsland final: public LandIslandInterface
{
public:
	SlopedIsland(uint8_t baseAltitude, uint8_t risePerCell)
	    : _base(baseAltitude)
	    , _rise(risePerCell)
	{
		_cells.reserve(k_Cells);
		for (uint16_t x = 0; x < k_Cells; ++x)
		{
			_cells.push_back(MakeWaterCell(AltitudeOf(x), false, false));
		}
	}

	/// The altitude of the cells of column x
	[[nodiscard]] uint8_t AltitudeOf(uint16_t x) const
	{
		return static_cast<uint8_t>(std::min<uint32_t>(255u, _base + static_cast<uint32_t>(_rise) * x));
	}
	/// The height of the land at x (in the slope's straight part)
	[[nodiscard]] float HeightOfX(float x) const
	{
		return (static_cast<float>(_base) + static_cast<float>(_rise) * (x / k_CellSize)) * k_HeightUnit;
	}

	[[nodiscard]] float GetHeightAt(glm::vec2 xz) const final { return HeightOfX(xz.x); }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2 xz) const final { return HeightOfX(xz.x); }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final
	{
		const float slope = static_cast<float>(_rise) * k_HeightUnit / k_CellSize;
		return glm::normalize(glm::vec3(-slope, 1.0f, 0.0f));
	}
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		return _cells[std::min<size_t>(cell.x, k_Cells - 1)];
	}
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	static constexpr uint16_t k_Cells = 512;
	uint8_t _base;
	uint8_t _rise;
	std::vector<lnd::LNDCell> _cells;
	std::vector<lnd::LNDCountry> _countries;
};

/// Just the heights: GetHeightAt from a function of the point, with every point it was asked for
class HeightFieldIsland final: public LandIslandInterface
{
public:
	explicit HeightFieldIsland(std::function<float(glm::vec2)> height)
	    : _height(std::move(height))
	{
	}

	/// The points GetHeightAt was asked for, in order
	mutable std::vector<glm::vec2> asked;

	[[nodiscard]] float GetHeightAt(glm::vec2 xz) const final
	{
		asked.push_back(xz);
		return _height(xz);
	}
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2 xz) const final { return _height(xz); }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final
	{
		static const lnd::LNDCell k_Empty {};
		return k_Empty;
	}
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	std::function<float(glm::vec2)> _height;
	std::vector<lnd::LNDCountry> _countries;
};
} // namespace openblack::test
