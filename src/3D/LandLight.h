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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <StackedBitmap.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/FrameAnim.h"

namespace openblack
{
class LandIslandInterface;
class LandLightTable;
} // namespace openblack

/// The land's light per cell and vertex (wiki: docs/wiki/world/haze-and-mist.md#haze-and-land-light-the-common-api). In
/// the original every LandBlock holds 17 x 17 cells of 8 bytes; the first dword is the cell's colour read as a D3DCOLOR
/// (bytes 0..2: blue, green, red) and byte 3 its luminosity. The land vertices and the models
/// take table[luminosity] (LandLightTable) as the diffuse and the colour | 0xFF000000 as the specular. Every frame the
/// light and shadow stamps are written into those bytes (before the land and the models are drawn) and taken out at
/// the end. The GPU side is assets/shaders/land_light.sh, which reads the cells of this frame (the renderer uploads
/// Cells()).
///
/// Cells are global, 0..511 along x and z (world / 10; the island's block index is 32 x 32 blocks of 16).
namespace openblack::land_light
{

/// The diffuse and specular an object is drawn with
struct Sample
{
	uint32_t diffuse {0xFFFFFFFFu};  ///< table[luminosity] (D3DCOLOR)
	uint32_t specular {0xFF000000u}; ///< the cells' colour | 0xFF000000
};

/// The land light table the renderer built last (RenderFrameSystemInterface::GetLandLightTable): what the game reads
/// when it colours something by the land's light outside the renderer. Every light 0xFFFFFFFF before the first build
[[nodiscard]] const LandLightTable& CurrentTable() noexcept;

/// table[255]: the light of the cells off the map and of the missing blocks, and of the dove, the citadel heart, the
/// clouds and the sea
[[nodiscard]] uint32_t FullLight(const LandLightTable& table) noexcept;
/// The sea's colour: table[255], the land's light at full luminosity, opaque, so the sea darkens at night and reddens
/// under an evil sky
[[nodiscard]] glm::vec4 SeaColour(const LandLightTable& table) noexcept;

/// a + ((b - a) w >> 8) per RGB byte, w 0..256: the integer lerp of the model light (the logical shift with the masks
/// gives a + floor((b - a) w / 256)) and of the stamps; alpha 0
[[nodiscard]] uint32_t LerpBytes(uint32_t a, uint32_t b, int w) noexcept;

/// The cells of the land, this frame's (after the stamps) and as loaded
struct Cells
{
	glm::ivec2 firstCell {0};               ///< global cell of index 0 (the island's cell map: extent minimum / 10)
	glm::ivec2 size {0};                    ///< the island's cell map resolution
	std::vector<uint32_t> base;             ///< the dword as loaded (restored when the stamps are cleared)
	std::vector<uint32_t> cells;            ///< this frame's
	std::vector<uint8_t> covered;           ///< the cell is one of a block's 17 x 17
	std::array<uint8_t, 32 * 32> blocks {}; ///< the block [x >> 4][z >> 4] exists
	const LandIslandInterface* island {nullptr};
	uint32_t generation {0};
};
/// The cells read by At / AtCellShift / AtCell and uploaded for the shaders
[[nodiscard]] const Cells& GetCells() noexcept;
/// Replaces them (tests, and a land without blocks)
void SetCells(Cells cells);
/// The cell's dword this frame; false off 0..0x1FF or where no block covers it
[[nodiscard]] bool CellAt(const Cells& cells, glm::ivec2 cell, uint32_t& dword) noexcept;
/// The block [x >> 4][z >> 4] exists, the cell in 0..0x1FF
[[nodiscard]] bool HasBlock(const Cells& cells, glm::ivec2 cell) noexcept;
/// The luminosity bytes of this frame, for the stamps outside this module (the night lights); write them back with
/// SetLuminosity
[[nodiscard]] std::vector<uint8_t> Luminosity();
void SetLuminosity(const std::vector<uint8_t>& luminosity);
/// RGBA8 texels of this frame's cells for the shaders: r, g, b the colour as a D3DCOLOR (byte 2, 1, 0), a = luminosity
[[nodiscard]] std::vector<uint8_t> Texels();

/// The model light at a position: the cell trunc(x 0.1), trunc(z 0.1); off 0..0x1FF or in a missing block the full
/// light and specular 0xFF000000; else the 4 cells (the cell, z + 1, x + 1 and both) lerped with the integer weights
/// trunc(frac 256), along z first and then along x, the colours | 0xFF000000 and the lights table[byte 3]
[[nodiscard]] Sample At(const Cells& cells, const LandLightTable& table, glm::vec2 xz) noexcept;
[[nodiscard]] Sample At(const LandLightTable& table, glm::vec2 xz) noexcept; ///< with GetCells()
/// Whether At reads the cells at that position; false where it gives the full light (off the map, in a missing block)
[[nodiscard]] bool OnTheMap(const Cells& cells, glm::vec2 xz) noexcept;
/// The blocks of `island` that exist, entry block x * 32 + block z (Cells::blocks)
[[nodiscard]] std::array<uint8_t, 32 * 32> BlocksOf(const LandIslandInterface& island);
/// The model light's first cell there, trunc(x 0.1), trunc(z 0.1), is off 0..0x1FF or in a block that does not exist
/// (`blocks`, BlocksOf): the bilinear and the cell-shifted light are the full light and specular 0xFF000000 there, not
/// the neighbouring blocks' cells
[[nodiscard]] bool FullLightAt(std::span<const uint8_t> blocks, glm::vec2 xz) noexcept;
/// The same from a map cell, with the weights CellX >> 8 and CellZ >> 8, not the fraction: 0 or 1/256, so trees,
/// scaffolds and town artifacts take their cell almost unmixed. Off the map: the full light
[[nodiscard]] Sample AtCellShift(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept;
/// The colour part of the altitude and colour query: the cell alone, specular = cell | 0xFF000000, diffuse =
/// table[byte 3]; off the map or in a missing block the full light
[[nodiscard]] Sample AtCell(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept;

/// How a model takes the land light in vs_object: each row's own (the instance's fifth column, RowLightCode), else
/// its draw's (DrawLightField)
enum class ObjectMode : uint8_t
{
	Bilinear = 0,  ///< At: the models
	CellShift = 1, ///< AtCellShift: trees, scaffolds, town artifacts
	Cell = 2,      ///< AtCell: worship sites, spell icons, totems
	Full = 3,      ///< FullLight alone: the dove writes the diffuse only ((inferred) the specular left at 0)
};
struct ObjectLight
{
	ObjectMode mode {ObjectMode::Bilinear};
	bool haze {true}; ///< whether the draw applies the haze after the light
};
/// What decides how an object's draw takes the land's light: its class and its state this frame
struct ObjectLightClass
{
	bool tree {false};                ///< a living tree's draw
	bool bilinearWithoutHaze {false}; ///< the partly built draw, the boat's hull, a SuperVillager's HD body
	bool worshipSite {false};
	bool totem {false}; ///< a totem building, a town's or a script's puzzle totem
	bool spellIcon {false};
	bool cellSeed {false}; ///< a player seed's mesh in an icon or a one-shot orb that takes its cell's light
	bool burning {false};
	bool flyingAnimal {false};
};
/// The mode and the haze of each class's draw: a tree its cell almost unmixed, with the haze; the partly built
/// draws the bilinear with no haze; a worship site or a totem that is not burning, a spell icon and a seed in one their
/// cell alone with no haze; a flying animal the full light; every other model the bilinear with the haze
[[nodiscard]] ObjectLight ObjectLightOf(const ObjectLightClass& object) noexcept;
/// The light a row takes where it stands: the full light (mode Full, the haze kept) for the bilinear and the
/// cell-shifted light whose first cell gives the full light (FullLightAt); vs_object's samplers would mix in the
/// cells around it instead
[[nodiscard]] ObjectLight AtItsPlace(ObjectLight light, bool fullLight) noexcept;
/// The light of its own a particle effect's mesh atom row takes: one drawn with the land's colour whose first cell
/// gives the full light (FullLightAt) takes it where its draw's light (`drawLight`, the mesh's) would mix in the cells
/// around it, the draw's haze kept, as the original's sampler gives the whole sample the full light; every other atom
/// keeps its draw's (none)
[[nodiscard]] std::optional<ObjectLight> AtomRowLight(bool landscapeColour, bool fullLight, ObjectLight drawLight) noexcept;
/// The light a draw gives vs_object in u_objectLight.w (with u_objectLight.x 1 or 3): 1 = no haze for the whole draw
/// (the hand) + 2 x the mode + 8 = no haze for the mode, the mode and its haze being the mesh's
/// (RenderContext::meshLandLight). Only the rows with no light of their own take the mode
[[nodiscard]] float DrawLightField(bool drawNoHaze, ObjectLight light) noexcept;
/// A row's own light, so that things sharing a mesh each take theirs: 1 + no haze + 2 x the mode; 0 is "none, the
/// draw's" (argb_colour::PackInstanceLandLight)
[[nodiscard]] uint32_t RowLightCode(ObjectLight light) noexcept;
/// What vs_object takes for a row, its CPU twin: the row's own mode and haze if it has them, else the draw's; with no
/// haze for the whole draw no row takes the haze
[[nodiscard]] ObjectLight ResolveObjectLight(float drawField, uint32_t rowCode) noexcept;

/// Loads the cells of `island` again when the land changed (`generation`, Clouds::GetLandscapeGeneration), and then
/// puts back the loaded cells (colour 0, luminosity = the saved byte) on the stamped blocks. The land files have the
/// saved byte = the luminosity and colour 0 everywhere (checked on Land1, Land2, Norse), so the loaded cells are the
/// same
void BeginFrame(const LandIslandInterface& island, uint32_t generation);

/// A light or shadow stamp (at most 200 a frame)
struct Stamp
{
	glm::vec3 position {0.0f};       ///< its first texel's world point (after the centring)
	const uint8_t* texels {nullptr}; ///< the frame (pitch x pitch x bpp, rows along x)
	int pitch {0};
	int keepBrighter {0}; ///< mode 1 only: max instead of the saturated sum
	int alpha {0};        ///< trunc(clamp(alpha x 255, 0, 255))
	int mode {0};
};
/// Drops it past 200 stamps; alpha x 255 clamped to 0..255 and truncated; with `centre` the point moves back by
/// (pitch - 1) x 5 in x and z. Mode 1 = light (bpp 3), 2 = shadow (bpp 1); 3..8 only set a bpp and stamp nothing.
/// `texels` must stay valid until this frame's ApplyStamps.
/// Callers: the particle light maps (pos + (10, 0, 10), centred, mode by bpp, keepBrighter 0), the storm clouds
/// (sstorm.raw 40, centred, mode 2), the lightning flash (64, centred, mode 1), the map clouds (sclouds.raw 40, not
/// centred, mode 2), the dance light (not ported)
bool AddStamp(const glm::vec3& position, const uint8_t* texels, int pitch, bool centre, float alpha, int mode,
              int keepBrighter = 0);
/// Every stamp of the list into the cells (in list order), only into the cells of the blocks drawn this frame:
/// `drawnBlocks` entry block x * 32 + block z (ObjectDrawListSystemInterface::DrawnBlocks); empty, every block
void ApplyStamps(std::span<const uint8_t> drawnBlocks = {});
/// The cells a stamp may write this frame: the 17 x 17 cells of each drawn block that exists (1), the rest 0. A cell on
/// the edge between two blocks is written when either of them is drawn: the original keeps a copy of it in each block,
/// and the drawn block's copy is the one its land is drawn with (approximate: a model of the other block, which is not
/// drawn, reads that one). Empty `drawnBlocks`: every cell of a block (`covered`)
[[nodiscard]] std::vector<uint8_t> StampedCells(const Cells& cells, std::span<const uint8_t> drawnBlocks);
/// The list emptied; the cells come back with the next BeginFrame
void ClearStamps() noexcept;
[[nodiscard]] const std::vector<Stamp>& GetStamps() noexcept;
/// (openblack) how many times ClearStamps ran: the producers that must add once a frame compare it
[[nodiscard]] uint32_t StampFrame() noexcept;

/// One stamp against cells of `size` from global cell `firstCell` (`covered` marks the cells of a block; `cells` the
/// dwords). The cell trunc(x 0.1) / trunc(z 0.1) and the weight round(255 - frac 255) (round((ix - f) 255) and ix - 1
/// below 0), & 0xFF; clipped to 0..0x1FF; then for each covered cell (ix + i, iz + j), i, j < pitch - 1: the texels
/// t(i, j), t(i, j + 1), t(i + 1, j), t(i + 1, j + 1) lerped along z by wz and then along x by wx (LerpBytes). Shadow:
/// v = 255 - (255 - r) alpha / 255, at least 0x30, luminosity = min(luminosity, v). Light: per texel byte c,
/// v = r alpha / 255 into colour byte 2 - c (texel R -> D3DCOLOR red), + v capped at 0xFF, or max with keepBrighter.
/// `covered` is StampedCells: only the blocks drawn this frame are stamped
void ApplyStamp(const Stamp& stamp, glm::ivec2 firstCell, glm::ivec2 size, const std::vector<uint8_t>& covered,
                std::vector<uint32_t>& cells);

/// Loads a bitmap file under Data (".\Data\..." or "Data/..." names): psys::LoadStackedBitmap on its bytes, once per
/// name and arguments; null when it is missing or of another size. The frames' texels: psys::StackedBitmap::Frame
[[nodiscard]] std::shared_ptr<const psys::StackedBitmap> LoadBitmapFile(const std::string& path, int pitch, int bpp,
                                                                        int framesInFile, int framesInUse);
/// The bitmap file `name` (relative to Data) decoded uncached: LightBitmapLoader's read; null when it is missing or
/// of another size
[[nodiscard]] std::shared_ptr<psys::StackedBitmap> ReadBitmapFile(const std::string& name, int pitch, int bpp, int framesInFile,
                                                                  int framesInUse);

} // namespace openblack::land_light
