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
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"

namespace openblack
{
class LandIslandInterface;
class LandLightTable;
} // namespace openblack

/// The land's light per cell and vertex (wiki: rendering.md, "Neblina y luz de la tierra: la API común"). In the original
/// every LandBlock holds 17 x 17 cells of 8 bytes; the first dword is the cell's colour read as a D3DCOLOR (bytes 0..2:
/// blue, green, red; the file's r, g, b bytes) and byte 3 its luminosity. The land vertex (fn_00874AA0) and the models
/// (fn_00801C90) take table[luminosity] ([0xEDD90C], LandLightTable) as the diffuse and the colour | 0xFF000000 as the
/// specular. Every frame the light and shadow stamps (fn_0086CFF0 -> the list 0xFA2920) are written into those bytes
/// (fn_0086D360 from fn_005E5830, before the land and the models are drawn) and taken out at the end (ClearLight
/// 0x5E57B0 -> fn_0086D460). The GPU side is assets/shaders/land_light.sh, which reads the cells of this frame (the
/// renderer uploads Cells()).
///
/// Cells are global, 0..511 along x and z (world / 10, LH3DIsland::g_index_block 0xE9C964 is 32 x 32 blocks of 16).
namespace openblack::land_light
{

/// What fn_00801C90 / fn_00802120 / 0x803340 leave in the object: obj +0x4C and +0x50
struct Sample
{
	uint32_t diffuse {0xFFFFFFFFu};  ///< table[luminosity] (D3DCOLOR)
	uint32_t specular {0xFF000000u}; ///< the cells' colour | 0xFF000000
};

/// [0xEDDD08] = [0xEDD90C + 0x3FC] = table[255]: the light of the cells off the map and of the missing blocks
/// (0x8020F8, 0x802525, 0x803365), and of Dove::Draw 0x41F75B, CitadelHeart 0x466958, the clouds fn_005E1DE0, the sea
/// fn_00879930 0x87993F
[[nodiscard]] uint32_t FullLight(const LandLightTable& table) noexcept;

/// a + ((b - a) w >> 8) per RGB byte, w 0..256: the integer lerp of fn_00801C90 (0x801E0B..0x8020F0; the logical shr
/// with the masks gives a + floor((b - a) w / 256)) and of the stamps (fn_00878C70 / fn_00878780); alpha 0
[[nodiscard]] uint32_t LerpBytes(uint32_t a, uint32_t b, int w) noexcept;

/// The cells of the land, this frame's (after the stamps) and as loaded
struct Cells
{
	glm::ivec2 firstCell {0}; ///< global cell of index 0 (the island's cell map: extent minimum / 10)
	glm::ivec2 size {0};      ///< the island's cell map resolution
	std::vector<uint32_t> base;  ///< the dword as loaded (ClearLight restores it: fn_00878700)
	std::vector<uint32_t> cells; ///< this frame's
	std::vector<uint8_t> covered; ///< the cell is one of a block's 17 x 17
	std::array<uint8_t, 32 * 32> blocks {}; ///< g_index_block[x >> 4][z >> 4] != 0
	const LandIslandInterface* island {nullptr};
	uint32_t generation {0};
};
/// The cells read by At / AtCellShift / AtCell and uploaded for the shaders
[[nodiscard]] const Cells& GetCells() noexcept;
/// Replaces them (tests, and a land without blocks)
void SetCells(Cells cells);
/// The cell's dword this frame; false off 0..0x1FF or where no block covers it
[[nodiscard]] bool CellAt(const Cells& cells, glm::ivec2 cell, uint32_t& dword) noexcept;
/// g_index_block[x >> 4][z >> 4] != 0, the cell in 0..0x1FF
[[nodiscard]] bool HasBlock(const Cells& cells, glm::ivec2 cell) noexcept;
/// The luminosity bytes of this frame, for the stamps outside this module (the night lights' fn_008229B0); write them
/// back with SetLuminosity
[[nodiscard]] std::vector<uint8_t> Luminosity();
void SetLuminosity(const std::vector<uint8_t>& luminosity);
/// RGBA8 texels of this frame's cells for the shaders: r, g, b the colour as a D3DCOLOR (byte 2, 1, 0), a = luminosity
[[nodiscard]] std::vector<uint8_t> Texels();

/// fn_00801C90(pos, diffuse, specular) (SSE twin fn_007A3EC0, the same result): the cell ftol(x 0.1), ftol(z 0.1)
/// ([0x8AC404]); off 0..0x1FF or in a missing block the full light and specular 0xFF000000 (0x8020F8); else the 4 cells
/// (+0, +8 = z + 1, +0x88 = x + 1, +0x90) lerped with the integer weights ftol(frac 256) ([0x8D45CC]), along z first and
/// then along x, the colours | 0xFF000000 and the lights table[byte 3]
[[nodiscard]] Sample At(const Cells& cells, const LandLightTable& table, glm::vec2 xz) noexcept;
[[nodiscard]] Sample At(const LandLightTable& table, glm::vec2 xz) noexcept; ///< with GetCells()
/// fn_00802120(LH3DMapCoords*) (SSE fn_007A4170): fn_00801C90 from the cell (CellX = word [mc + 2], CellZ = word
/// [mc + 6]) with the weights CellX >> 8 and CellZ >> 8 (0x802206, 0x802237; SSE 0x7A42AC / 0x7A42BC), not the fraction:
/// 0 or 1/256, so trees, scaffolds and town artifacts take their cell almost unmixed. Off the map 0x802525
[[nodiscard]] Sample AtCellShift(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept;
/// LH3DIsland::GetAltitudeAndSetColorSpecular 0x803340, its colour part: the cell alone, specular = cell | 0xFF000000
/// (0x8033FA..0x803405), diffuse = table[byte 3] (0x803407..0x803413); off the map or missing block 0x803365 / 0x8033DA
[[nodiscard]] Sample AtCell(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept;

/// How a model takes the land light in vs_object (the per-mesh mode of RenderContext; u_objectLight.w = haze off +
/// 2 x mode)
enum class ObjectMode : uint8_t
{
	Bilinear = 0,  ///< fn_00801C90: the models
	CellShift = 1, ///< fn_00802120: Tree::Draw 0x74AB1B, Scaffold::Draw 0x6EA6CA, TownArtifact::Draw 0x51CB14
	Cell = 2,      ///< 0x803340: WorshipSite::Draw 0x519460, SpellIcon::Draw 0x5196CC, Totem::Draw 0x51ACD1
	Full = 3,      ///< [0xEDDD08] alone: Dove::Draw 0x41F75B writes +0x4C only ((inferido) +0x50 left at 0)
};
struct ObjectLight
{
	ObjectMode mode {ObjectMode::Bilinear};
	bool haze {true}; ///< whether the Draw calls fn_007FEB30 after the light
};

/// Loads the cells of `island` again when the land changed (`generation`, Clouds::GetLandscapeGeneration), and then
/// puts back the loaded cells: fn_0086D460 -> fn_0086D390 -> fn_00878700 (colour 0, luminosity = byte +5) on the stamped
/// blocks. The land files have byte +5 = byte +3 and colour 0 everywhere (checked on Land1, Land2, Norse), so the
/// loaded cells are the same
void BeginFrame(const LandIslandInterface& island, uint32_t generation);

/// A stamp of the list 0xFA2920 (0x34 bytes, at most 200, count [0xFA51C0])
struct Stamp
{
	glm::vec3 position {0.0f};           ///< +0x00, its first texel's world point (after the centring)
	const uint8_t* texels {nullptr};     ///< +0x0C, the frame (pitch x pitch x bpp, rows along x)
	int pitch {0};                       ///< +0x10
	int keepBrighter {0};                ///< +0x28, mode 1 only: max instead of the saturated sum
	int alpha {0};                       ///< +0x2C, ftol(clamp(alpha x 255, 0, 255))
	int mode {0};                        ///< +0x30
};
/// fn_0086CFF0(pos, texels, pitch, centre, alpha, mode, keepBrighter) -> fn_0086CF50: drops it past 200 stamps
/// (0x86CFF5); alpha x 255 ([0x8AB270]) clamped to 0..255 and __ftol; with `centre` the point moves back by
/// (pitch - 1) x 5 ([0x8AB6E4]) in x and z (0x86CFC8..0x86CFE2). Mode 1 = light (fn_00878780, bpp 3), 2 = shadow
/// (fn_00878C70, bpp 1); 3..8 only set a bpp in the table 0x86D338 and stamp nothing. `texels` must stay valid until
/// this frame's ApplyStamps.
/// Callers: PSysLightMaps fn_006CA280 (pos + (10, 0, 10), centred, mode by bpp, keepBrighter 0), GWeather::DrawClouds
/// 0x8400C6 (sstorm.raw 40, centred, mode 2), the lightning flash fn_00837200 (0xED92F0 64, centred, mode 1), the map
/// clouds fn_005E25C0 0x5E2800 (sclouds.raw 40, not centred, mode 2), DanceLight 0x50F919 (not ported)
bool AddStamp(const glm::vec3& position, const uint8_t* texels, int pitch, bool centre, float alpha, int mode,
              int keepBrighter = 0);
/// fn_0086D360: every stamp of the list through fn_0086D060 into the cells (in list order)
void ApplyStamps();
/// fn_0086D460: the list emptied ([0xFA51C0] = 0, 0x86D487); the cells come back with the next BeginFrame
void ClearStamps() noexcept;
[[nodiscard]] const std::vector<Stamp>& GetStamps() noexcept;
/// (openblack) how many times ClearStamps ran: the producers that must add once a frame compare it
[[nodiscard]] uint32_t StampFrame() noexcept;

/// fn_0086D060 on one stamp against cells of `size` from global cell `firstCell` (`covered` marks the cells of a
/// block; `cells` the dwords). The cell ftol(x 0.1) / ftol(z 0.1) ([0x8AC404]) and the weight fistp(255 - frac 255)
/// (fistp((ix - f) 255) and ix - 1 below 0), & 0xFF; clipped to 0..0x1FF; then for each covered cell (ix + i, iz + j),
/// i, j < pitch - 1: the texels t(i, j), t(i, j + 1), t(i + 1, j), t(i + 1, j + 1) lerped along z by wz and then along
/// x by wx (LerpBytes). Shadow (fn_00878C70): v = 255 - (255 - r) alpha / 255 (0x80808081), at least 0x30 (0x878DAD),
/// luminosity = min(luminosity, v) (0x878DBD..0x878DC6). Light (fn_00878780): per texel byte c, v = r alpha / 255 into
/// colour byte 2 - c (texel R -> D3DCOLOR red), + v capped at 0xFF (0x878B09..0x878B19), or max with keepBrighter
/// (0x87890D). (aproximado) the original only stamps the blocks drawn this frame (+0x920 bit 4, set by fn_00877210
/// 0x8774E8): here every block
void ApplyStamp(const Stamp& stamp, glm::ivec2 firstCell, glm::ivec2 size, const std::vector<uint8_t>& covered,
                std::vector<uint32_t>& cells);

/// GJBitmap::LoadBitmapFromFile 0x57CA90 of a file under Data (".\Data\..." or "Data/..." names):
/// graphics::frame_anim::LoadBitmapFromFile on its bytes, once per name and arguments; null when it is missing or of
/// another size. The frames' texels: graphics::frame_anim::FrameTexels
[[nodiscard]] std::shared_ptr<const graphics::frame_anim::StackedFrames> LoadBitmapFile(const std::string& path, int pitch,
                                                                                         int bpp, int framesInFile,
                                                                                         int framesInUse);

} // namespace openblack::land_light
