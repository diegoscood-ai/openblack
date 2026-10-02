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
#include <bit>
#include <functional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The projected shadows of the original (the ShadowInfo list [0xFAA7E0], wiki: rendering.md, "Sombras proyectadas"),
/// CPU only and without bgfx or ECS, so the tests can run it: the fade (fn_00874600), the alpha and the light of the two
/// updates (fn_00874850 generic, fn_00814FD0 complex), the projection of the caster's vertices (fn_00850900), the
/// 4 x 2 subsample rasterizer (fn_00850CC0, fn_0087FF70, fn_00880050), the resolve into 32 x 32 texels of n / 15
/// (fn_00880FC0 with the table fn_00880F20), the chroma blur (0x807635), the baked fade (0x80769A), and the tests the
/// land draw uses (fn_007FF610, fn_00878350, fn_00877210).
///
/// The game runs the x87 FPU at 24 bits (fn_007DEE00: `and 0xFCFF` of the control word, called from
/// GGame::Process3dEngine 0x54E426 / 0x54E4D1 among others), so every operation here is float, one rounding per x87
/// instruction in the original's order, no double and no FMA; __ftol (0x7A1400) truncates towards zero.
namespace openblack::graphics::shadow_math
{

inline constexpr float k_FadeFull = 50.0f;             ///< [0xC398F4] `00004842`: full shadow up to 50 radii
inline constexpr float k_FadeGone = 80.0f;             ///< [0xC398F8] `0000a042`: none from 80 radii
inline constexpr float k_FadeMax = 255.0f;             ///< [0x8AB270] `00007f43`
inline constexpr float k_BlockFar = 100000.0f;         ///< [0xC37200] `0050c347`: block+0x9BC at or past it -> 0
inline constexpr float k_NeighbourStep = 60.0f;        ///< [0x8C36A8] `00007042`: the 3 x 3 test's offsets
inline constexpr float k_CellScale = 0.1f;             ///< [0x8AC404] `cdcccc3d`: world -> cells
inline constexpr float k_InvByte = 0.003921568859f;    ///< [0x900058] `8180803b`: 1 / 255 as a float
inline constexpr float k_VerticalLight = 15000.0f;     ///< [0x9A3C10] `00606a46`: the generic light above the caster
inline constexpr float k_HandLight = 200.0f;           ///< [0x8C7B34] `00004843`: the hand's light above it
inline constexpr float k_CreatureRadii = 3.0f;         ///< [0x8C2C50] `00004040`: the creature's light at most 3 R away
inline constexpr double k_CreatureNear = 0.1;          ///< [0x8C9D40] `9a9999999999b93f` (a double)
inline constexpr float k_BlockSize = 160.0f;           ///< [0x8D151C] `00002043`: a land block's side
inline constexpr float k_ChromaSide = 32.0f;         ///< [0x8CF134] `00000042`: the chroma render's side
inline constexpr float k_SixtyFourth = 0.015625f;    ///< [0x8D8BD0] `0000803c`: 1 / 64, fn_00838F00
inline constexpr int k_CellLimit = 0x1FF;              ///< fn_00874600 0x87463D / 0x874649: cells 0..0x1FF
/// The box before the first vertex: 0x60AD78EC / 0xE0AD78EC (fn_00874850 0x87499B / 0x8749AA, fn_00806F60 0x8070DA)
inline constexpr float k_BoxEmpty = std::bit_cast<float>(0x60AD78ECu);
/// si+0x45C: a 32 x 32 texture (fn_0087FD50); the grid [0xC39B08] = 128 / [0xC39B0C] = 64 (0x807077 / 0x807081) is
/// 4 x 2 subsamples per texel
inline constexpr int k_Texels = 32;

// ---- Fade, alpha and light (fn_00874600, fn_00874850, fn_00814FD0) --------------------------------------------------

/// The two values of a land block that fn_00874600 reads: +0x920 & 1 (in the camera's view, fn_00877210 0x8774E8 /
/// 0x877CEB) and +0x9BC (its distance to the camera, 0x877C8A..0x877CCD)
struct BlockState
{
	bool exists {false}; ///< g_index_block[bx][bz] != 0 and g_ptr_blocks[] != 0
	bool visible {false};
	float distance {0.0f};
};
/// The block of the cells (16 per block side): g_index_block[(cx >> 4) * 32 + (cz >> 4)] (0x874650..0x87466F)
using BlockAt = std::function<BlockState(int blockX, int blockZ)>;

/// fn_00874600: 0 when the block under the caster is at 100000 or more (0x874673..0x874684); 0 when none of the 9
/// blocks at (-60, 0, +60) in x and z exists, is visible and is nearer than 100000 (0x87468A..0x874737; cells outside
/// 0..cellLimit skip the first test and count as no block); else q = |(x, ground, z) - camera| / (scale * radius)
/// (0x87478E..0x8747EE) gives 255 below 50, 255 - (q - 50) 255 / (80 - 50) up to 80 and 0 past it (0x8747F2..0x874844).
/// `ground` is GetAltitude at the caster's x, z (0x874789), `scale` the LH3DObject's +0x44, `meshRadius` its mesh's
/// +0x30 (the half diagonal, ecs::object::MeshHalfDiagonal).
[[nodiscard]] float Fade(glm::vec3 position, float ground, glm::vec3 camera, float scale, float meshRadius,
                         const BlockAt& blocks, int cellLimit = k_CellLimit);
/// fn_00874850 0x874872..0x87488E: si+0x10 = ftol(si+0x14 fade (1/255)); 0 means not drawn (fn_00881030)
[[nodiscard]] int AlphaGeneric(float fade, int base);
/// fn_00814FD0 0x815007..0x815051: a fade below 255 as the generic one, else 255
[[nodiscard]] int AlphaComplex(float fade, int base);
/// fn_00874850 0x8748E0..0x874932: holder+4 ? the fixed sun [0xEA1C88] = (-500000, 500000, -500000) (fn_00818920
/// 0x818930..0x81894E) : the caster's position + (0, 15000, 0)
[[nodiscard]] glm::vec3 LightGeneric(glm::vec3 position, bool useSun);
/// fn_00814FD0 0x8151C4..0x8151F1 with obj+0xBC (CHand 0x46CB28): the position + (0, 200, 0)
[[nodiscard]] glm::vec3 LightHand(glm::vec3 position);
/// fn_00814FD0 0x815058..0x815181 (the creature, obj+0xBC = 0): d = light [0xEA9E90] - position; R = mesh+0x30 x
/// obj+0x44 x 3; when the horizontal |d| < R: below 0.1 (a double) dx and dz + 1, then d (3D) scaled to length R
/// unless it is 0; then dy raised to the horizontal |d| when dy / horizontal < 1 (45 degrees at least)
[[nodiscard]] glm::vec3 LightCreature(glm::vec3 position, glm::vec3 light, float meshRadius, float scale);

// ---- Silhouette (fn_00806F60) ---------------------------------------------------------------------------------------

/// What fn_00850900 projects with: the light si+0x444, si+0x450 = caster - light (fn_00806F60 0x80708B..0x8070D4) and
/// the caster's own y (its matrix ty, emitter+0x3C = [0xEA1AE8]+0x3C) that each vertex height is taken from
struct Projection
{
	glm::vec3 light {0.0f};
	glm::vec3 dir {0.0f};
	float baseY {0.0f};
};
/// Projection of the caster at `position` (its matrix translation) from `light`: dir = position - light, one float
/// subtraction per axis (0x807091..0x8070D4)
[[nodiscard]] Projection MakeProjection(glm::vec3 position, glm::vec3 light);

/// si+0x1C..0x28 (x0, x1, z0, z1) and si+0x440 (the least k), reset to +-0x60AD78EC before the first vertex
struct Box
{
	float x0 {k_BoxEmpty};
	float x1 {-k_BoxEmpty};
	float z0 {k_BoxEmpty};
	float z1 {-k_BoxEmpty};
	float kMin {k_BoxEmpty};
};

/// fn_00850900, one vertex: W = M v with M's ty lowered by baseY (0x85094F..0x85096E; the skinned branch does the same
/// to its copy of the bone matrix, 0x850B2D..0x850B35), h = max(0, W.y) (0x8509D4..0x8509E3), k = W.z d.z + W.x d.x
/// (0x850A05..0x850A2C, the box's kMin), t = -Ly / (h - Ly) with Ly the light's ABSOLUTE y (0x850914 fchs, 0x850A2E
/// fsub, 0x850A3A fdivr), P = L.xz + (W.xz - L.xz) t; the box grows by P with no margin (0x850A70..0x850AB8). `matrix`
/// is the vertex's world matrix (the caster's, or the bone's), glm column-major (matrix[c][r]).
glm::vec2 Project(const Projection& projection, const glm::mat4& matrix, glm::vec3 local, Box& box);

/// The rasterizer's grid of one shadow: 4 x 2 subsamples per texel ([0xC39B08] = 4 texels, [0xC39B0C] = 2 texels),
/// one byte per texel (si+0x40, cleared by fn_00806F60 0x80706B..0x807075): bits 0..3 the even subrow, 4..7 the odd
/// one, bit x & 3 for the subsample x (the masks 0x9A3C74 / 0x9A3CB4 / 0x9A3CF4 / 0x9A3D34, little endian dwords of 4
/// texels). The original always has 32 texels; another size is openblack's own (kept for the hand's look, see
/// shadow_list).
struct Coverage
{
	explicit Coverage(int side = k_Texels);
	void Clear();
	[[nodiscard]] int GridX() const { return texels * 4; }
	[[nodiscard]] int GridZ() const { return texels * 2; }
	int texels;
	std::vector<uint8_t> bytes; ///< texels rows (z) of texels bytes (x)
};

/// fn_00806F60 0x807265..0x807305: every point into the grid, px = (x - x0) (128 / (x1 - x0)) and pz = (z - z0)
/// (64 / (z1 - z0)) ([0x8C6CA8] = 128, [0x930678] = 64, the factors stored as floats first), each clamped to 0
/// below 0 and to 127 ([0x8C4A00]) / 63 ([0x9A2BF8]) above
void ToGrid(const Box& box, std::span<glm::vec2> points, int texels = k_Texels);

/// fn_00850CC0 on one primitive: triangles of 16-bit indices into `grid`. One-sided ones (no `mat+5 & 1` and not a
/// mist, vt+0x1F8 = IsMist, 1 only in Mist 0x55EB90) are kept when (r0 - r2)(x1 - x2) >= (r1 - r2)(x0 - x2), rows
/// r = ftol(z) (0x850D03..0x850DA1), and walked 0 -> 2, 2 -> 1, 1 -> 0; two-sided ones are walked 0 -> 2 -> 1 when
/// (r0 - r1)(x2 - x1) < (r2 - r1) (x0 - x1), else 0 -> 1 -> 2 (0x850E29..0x850F6D). Edges fn_0087FF70, spans
/// fn_00880050; `halfRows` = si+0x3C (the even subrows are not written, 0x880141..0x880146).
void RasterTriangles(std::span<const glm::vec2> grid, std::span<const uint16_t> indices, bool bothFaces,
                     bool halfRows, Coverage& coverage);

/// One shadow's texels, the alpha nibbles n of the ARGB4444 texture (black, alpha n / 15), texels x texels by rows
using Texels = std::vector<uint8_t>;
/// fn_00880FC0: rows and columns 1..texels - 2 take popcount(coverage) (the table [0xFA95C4] of fn_00880F20 =
/// popcount(i) << 12); the outer ring is never written and keeps the constructor's 0 (fn_0087FD50 0x87FDBE..0x87FDFC)
void Resolve(const Coverage& coverage, Texels& texels);
/// The chroma casters' blur (0x807635..0x807688): for rows and columns 1..30, texel(r, c) |= ((t(r, c) + t(r, c + 1) +
/// t(r + 1, c) + t(r + 1, c + 1)) / 4) & 0xF000 of the 16-bit texels `rendered` the caster's DrawTextureShadow32x32
/// (vt+0x168) drew; an OR of nibbles, not a max
void ChromaFilter(std::span<const uint16_t> rendered, Texels& texels);
/// 0x80769A..0x8076EC, when si+0x10 != 255: rows and columns 1..texels - 2 become ((n << 12) a / 255) & 0xF000, that
/// is n' = floor(n a / 255) (0x80808081 / sar 7: the signed division by 255)
void BakeAlpha(Texels& texels, int alpha);

// ---- The chroma casters (fn_0080EE80 -> fn_0084B7D0 -> fn_00881DE0) ------------------------------------------------

/// fn_00838F00: the texture's 64 x 64 shadow map, cached at texture+0x12C: byte (r, c) = the high byte of the 16-bit
/// texel (ftol(r h / 64), ftol(c w / 64)) & 0xF0 (0x838F88..0x838FD0, [0x8D8BD0] = 1/64), the ARGB4444 alpha nibble
using AlphaMap = std::array<uint8_t, 64 * 64>;
[[nodiscard]] AlphaMap MakeAlphaMap(std::span<const uint16_t> texels, int width, int height);

/// One vertex of fn_0084B7D0 (0x84B83F..0x84B96D): W = M v; t = (si+0x18 - Ly) / (W.y - Ly) (the base y, no clamp of h);
/// x = ((W.x - Lx) t + Lx - x0) (32 / (x1 - x0)) clamped to [1, 31] ([0x8AA390] = 1, [0x92B6F4] = 31), z the same;
/// u, v = the vertex uv x 63 ([0x9A2BF8]). Returns (x, z, u, v), Table1 +0, +8, +0x18, +0x1C
[[nodiscard]] glm::vec4 ChromaVertex(const Projection& projection, const Box& box, const glm::mat4& matrix, glm::vec3 local,
                                     glm::vec2 uv);
/// fn_00881DE0 with the target of fn_008816F0 (side x side 16-bit texels, the 0x800 bytes 0x807339 for 32): the three
/// vertices ftol'd (0x84B9AA..0x84BA4C), clamped to the target, sorted (0x881E86..0x881EEA), the edges fn_00881A60 (16.16
/// x, u, v per row, rows inclusive) and the spans fn_00882080 (inclusive, u, v stepped by an integer division): each
/// texel ORs (map[(v >> 16) 64 + (u >> 16)] & 0xE0) << 7, i.e. alpha nibble (a >> 1)
void ChromaTriangle(const std::array<glm::vec4, 3>& vertices, const AlphaMap& map, std::span<uint16_t> target, int side);

// ---- Land and objects (fn_007FF610, fn_00878350, fn_0080B050) -------------------------------------------------------

/// fn_00878350 0x8784A7..0x8784B8: t' = (si+0x18 - Ly) / (H - Ly), H = GetAltitude(caster) with a caster (si+0x464,
/// fn_00874850 0x874993) and si+0x18 without one (the hand and the creature: t' = 1). One H per shadow (0x878394..
/// 0x87842F, before the vertex loop)
[[nodiscard]] float LandT(float baseY, float lightY, float ground);
/// fn_007FF610 0x7FF6D8..0x7FF744: bx 160 <= x1, (bx + 1) 160 >= x0 and the same in z, with the box +0x2C {x0, z0, x1,
/// z1}
[[nodiscard]] bool TouchesBlock(const Box& box, int blockX, int blockZ);
/// [0x932D08] `8104b53f` = 1.41419995 (not the float nearest sqrt 2, 0x3FB504F3): the morphable receiver's box factor
inline constexpr float k_MorphableBoxFactor = std::bit_cast<float>(0x3FB50481u);
/// The morphable Draw's own receiver test (fn_0080E550 0x80E78E..0x80E857, in place of ContainsThisBoundingBox): the
/// reach R = (obj+0x44 x mesh+0x30) + max(x1 - x0, z1 - z0) x 1.4142 [0x932D08] (si+0x20 - si+0x1C against si+0x28 -
/// si+0x24, the z one stored, 0x80E7A3; the x one kept only when the z one is less, 0x80E7AD), the mesh centre
/// mesh+0x18..0x20 through the object's matrix obj+0x14 (x = ((cy m3 + cz m6) + cx m0) + m9, z = ((cy m5 + cx m2) +
/// cz m8) + m11; rows m0..m11 = glm columns), dx = x - bx and dz = z - bz with the box centre (x0 + x1) x 0.5
/// [0x8AA3B4] (stored, 0x80E814) and (z0 + z1) x 0.5, dx and dz stored (0x80E828, 0x80E832); the shadow is drawn
/// (fn_0080AE40, 0x80E86A) when dx dx + dz dz < R R, strictly (0x80E84E: C0 or C3 of R R against it skips)
[[nodiscard]] bool ReachesMorphable(const Box& box, glm::vec3 meshCentre, const glm::mat4& model, float scale,
                                    float halfDiagonal);
/// fn_00877210 0x8773E4..0x8774E8: the 8 corners of the block (graphics::haze::BlockCorners) through g_world_to_clipping
/// [0xEA9E40] (x, y, w; glm: worldToClip * (corner, 1)); outcodes w < near ([0xE839E0], read by CameraModePath::
/// SetUpNearClipping 0x460F10), x > w, else -w > x, y > w, else -w > y. Not visible when one outcode holds for all 8.
[[nodiscard]] bool BlockVisible(const std::array<glm::vec3, 8>& corners, const glm::mat4& worldToClip, float nearW);

} // namespace openblack::graphics::shadow_math
