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

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/LandLightTable.h"

namespace openblack
{
class LandBlock;
} // namespace openblack

/// The original's software distance haze, one formula with three implementations (wiki: rendering.md, "Neblina de
/// distancia"): the objects' fn_007FEB30 (once per object, at its origin), the land's per vertex in fn_00874AA0 (x87) /
/// fn_007A1800 (SSE) with the block class of fn_00877210, and the parameters fn_00869850 writes every frame through
/// fn_007FEAA0 / fn_007FEAD0. The GPU side is assets/shaders/haze.sh, with the same rounding.
///
/// Rounding of the original, kept here: the diffuse factor f = 256 - __ftol((256 - k) t) (truncated); the diffuse
/// (c f) >> 8 per byte with its alpha kept; the haze colour c t stored with fistp (rounded to nearest, halves to even, the
/// FPU's default mode); the land's class 2 the colour packed with __ftol ([0xE9B6D8], truncated) and f = k.
namespace openblack::graphics::haze
{

/// The seven globals of fn_007FEAA0 / fn_007FEAD0 and the "Fog" detail key [0xC37204]
struct Params
{
	bool on {false};             ///< [0xC37204] the "Fog" detail key (fn_008237B0 0x8239E2, fn_00823AD0 0x823C1F)
	float nearDistance {15.0f};  ///< [0xC37220] (static 15)
	float farDistance {400.0f};  ///< [0xC37224] (static 400)
	float range {385.0f};        ///< [0xE9B6DC] far - near (fn_007FEAA0 0x7FEABB)
	int k {256};                 ///< [0xC37228] the diffuse factor at t = 1, in 1/256 (fn_007FEAD0 0x7FEAE0)
	glm::vec3 colour {64.0f};    ///< [0xC37214] / [0xC37218] / [0xC3721C] R, G, B 0..255 (floats)
	uint32_t packed {0x404040u}; ///< [0xE9B6D8] ftol(b) | ftol(g) << 8 | ftol(r) << 16, alpha 0 (fn_007FEAD0 0x7FEB26)
};

/// fn_007FEAA0(near, far) + fn_007FEAD0(r, g, b, k), called by fn_00869850 0x869F59 / 0x869F78 with the haze of this
/// frame's light table; `on` is the "Fog" detail key
[[nodiscard]] Params FromTable(const LandLightTable::Haze& haze, bool on) noexcept;
/// The haze of this frame in one place: LandLightTable::Current() (the last Build, fn_00869850) and the "Fog" detail
/// key of the config. Off without a config.
[[nodiscard]] Params Frame() noexcept;

/// 0x7FEB4A..0x7FEB75 (and 0x874C14, 0x8773BC): the depth along the camera axis, the z column of the world-to-camera
/// matrix LH3DTech::g_world_to_clipping 0xEA9E40 (+0x08, +0x14, +0x20, translation +0x2C); in openblack the view
/// matrix's z row (the view looks down +z, Camera.h)
[[nodiscard]] float Depth(const glm::mat4& view, const glm::vec3& point) noexcept;

/// fistp with the FPU's default rounding (to nearest, halves to even)
[[nodiscard]] int RoundHalfEven(float value) noexcept;

/// t of fn_007FEB30 / fn_00874AA0 class 1: (min(max(z, near), far) - near) / range with fdiv (0x7FEBAD..0x7FEBBE,
/// 0x874C5B..0x874C9C). fn_007FEB30 has already returned when z < near, which gives the same t = 0
[[nodiscard]] float T(const Params& params, float depth) noexcept;
/// f = 256 - __ftol((256 - k) t) (0x7FEBC3..0x7FEBE3 / 0x874C90); the diffuse is scaled only when f < 256 (0x7FEBEB)
[[nodiscard]] int Factor(const Params& params, float t) noexcept;
/// (c f) >> 8 per RGB byte, alpha kept (0x7FEBED..0x7FEC30, 0x874DA1..0x874DF0); nothing when f >= 256 (unsigned jae)
[[nodiscard]] uint32_t ScaleDiffuse(uint32_t argb, int f) noexcept;
/// The haze colour c t of every channel stored with fistp (0x7FEC32..0x7FEC7E, 0x874CD2..0x874D06), as 0x00RRGGBB
[[nodiscard]] uint32_t Colour(const Params& params, float t) noexcept;
/// a + b per channel, each capped at 0xFF (0x7FECA2..0x7FED0D, 0x874D21..0x874D9E); alpha of neither
[[nodiscard]] uint32_t AddSaturated(uint32_t a, uint32_t b) noexcept;

/// fn_007FEB30(pos, specular, diffuse): with the haze off or the depth < near (strict, 0x7FEB7D) it returns the
/// specular untouched; else it scales *diffuse (when not null) by f and returns the haze colour with alpha 0xFF, added
/// with saturation to a non-zero specular (alpha 0xFF either way). Once per object, at the point the caller passes.
[[nodiscard]] uint32_t ApplyObject(const Params& params, float depth, uint32_t specular, uint32_t* diffuse) noexcept;

/// fn_00877210 0x87743D..0x87749B, from the camera depth of the 8 corners of the block's box: bit 2 for a corner past
/// far, bit 1 for one in (near, far]; class 0 with the haze off or no bit, 1 with any bit 1, else 2 (the whole block at
/// full haze; also, (inferido) never seen, a block with corners <= near and > far and none between)
[[nodiscard]] int BlockClass(const Params& params, const std::array<float, 8>& cornerDepths) noexcept;
/// fn_00877210 0x877232..0x877370: the 8 corners of a block's box, fed one by one through the jump table 0x877D04
/// (cases 0..6, the 8th the default 0x877356) to the depth: x and z the block's +0x90C / +0x910 + 80 ([0x8D060C]) +- 80;
/// y from h = fild(+0x924) x 0.67 ([0xC3720C]): centre h / 2 +- h / 2 (0x877272..0x877282), or with [0xE9CD8C] set
/// (0x877242) centre 0 +- h (the mirrored land). (inferido) +0x924 = LNDBlock::highestAltitude and [0xE9CD8C] = the
/// LandRef detail key (DetailLevel::landReflection). The order of the corners does not change BlockClass
[[nodiscard]] std::array<glm::vec3, 8> BlockCorners(glm::vec2 mapPosition, float highestAltitude, bool landRef) noexcept;
/// fn_00877210's class of one land block: BlockClass of the depths (Depth) of its BlockCorners
[[nodiscard]] int BlockClassOf(const Params& params, const glm::mat4& view, const LandBlock& block, bool landRef) noexcept;
/// fn_00874AA0 per vertex: class 0 nothing; class 2 (0x874C48) f = k and the colour [0xE9B6D8] (truncated, alpha 0);
/// class 1 (0x874C5B..0x874D1E) t from the clamped depth, f and the fistp colour. The specular gets the colour added
/// with saturation, keeping its own alpha (0x874D83), or becomes it (alpha 0) when it was 0 (0x874D9E); then the
/// diffuse (c f) >> 8 when f < 256 (0x874DA7). Returns them through the references.
void ApplyVertex(const Params& params, int blockClass, float depth, uint32_t& diffuse, uint32_t& specular) noexcept;

/// u_haze (x near, y far, z k, w on) and u_hazeColour (rgb the colour 0..255, w: unused) for haze.sh
[[nodiscard]] std::array<glm::vec4, 2> Uniforms(const Params& params) noexcept;

} // namespace openblack::graphics::haze
