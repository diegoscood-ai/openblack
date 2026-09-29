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
class LandIslandInterface
{
public:
	static const uint8_t k_CellCount;
	static const float k_HeightUnit;
	static const float k_CellSize;

	[[nodiscard]] virtual float GetHeightAt(glm::vec2) const = 0;
	/// GetAltitude with the sea flattening off ([0xC37BF4] = 0, as FishFarm::CallVirtualFunctionsForCreation sets it)
	[[nodiscard]] virtual float GetUnflattenedHeightAt(glm::vec2) const = 0;
	[[nodiscard]] virtual glm::vec3 GetNormalAt(glm::vec2) const = 0;
	[[nodiscard]] virtual const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const = 0;

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

	[[nodiscard]] virtual U16Extent2 GetIndexExtent() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoView() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoProj() const = 0;
	[[nodiscard]] virtual Extent2 GetExtent() const = 0;
	virtual uint8_t GetNoise(glm::u8vec2 pos) = 0;
};
} // namespace openblack
