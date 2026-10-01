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

#include <algorithm>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The byte arithmetic of LH3DColor, CPU side; the GPU twin is assets/shaders/lh3d_colour.sh and the two must stay the
/// same. Wiki: rendering-objects.md, "Aritmética de LH3DColor".
///
/// An LH3DColor is a D3DCOLOR, 0xAARRGGBB. The engine combines two of them in two families, and every routine of both
/// truncates, none rounds:
/// - (c t) >> 8 per channel: an imul over the masked byte and the bits of the byte kept (fn_0080BF10, fn_00809D80,
///   Tree::Draw 0x74B077, fn_0074B3A0, fn_0084BA90). With t = 0xFF a channel loses 1, so 0xFF stays at 0xFE.
/// - c l / 255 per channel: the 0x80808081 multiply and the shift by 7 plus the sign bit, which is trunc(x / 255) for
///   every x in 0..65025 (fn_007ACF70, LH3DMist 0x7FA6DF, LH3DCreature::DrawNow 0x48EF10).
/// Each routine has its own alpha rule, so the name says it: _4 works the alpha like the other channels, _3KeepA keeps
/// the FIRST argument's alpha (that is what every original does: fn_00809D80 a.A, fn_00809DE0 a.A, LH3DMist the
/// colour's and not the light's, Tree::Draw the object's), _3OpaqueA writes 0xFF.
///
/// ToAbgr / ToVec4 / ToVec3 have no original behind them: bgfx wants the bytes as ABGR (R first in memory) and the
/// shaders want 0..1 floats.
namespace openblack::lh3d_colour
{

[[nodiscard]] constexpr uint32_t Alpha(uint32_t argb) noexcept
{
	return argb >> 24;
}
[[nodiscard]] constexpr uint32_t Red(uint32_t argb) noexcept
{
	return (argb >> 16) & 0xFFu;
}
[[nodiscard]] constexpr uint32_t Green(uint32_t argb) noexcept
{
	return (argb >> 8) & 0xFFu;
}
[[nodiscard]] constexpr uint32_t Blue(uint32_t argb) noexcept
{
	return argb & 0xFFu;
}
/// 0xAARRGGBB from four 0..255 bytes (the bits above the byte are dropped)
[[nodiscard]] constexpr uint32_t Argb(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha = 0) noexcept
{
	return (alpha & 0xFFu) << 24 | (red & 0xFFu) << 16 | (green & 0xFFu) << 8 | (blue & 0xFFu);
}

/// fn_0080BF10 0x80BFA3..0x80C00B, the diffuse of DrawWithLandscapeColor (fn_0080BEC0): (c t) >> 8 in the four
/// channels, the alpha too (0x80BFC5..0x80BFD3: ((c >> 8) & 0xFF0000) t.A, masked with 0xFF00FFFF), so a 0xFF alpha
/// tinted with 0xFF comes out 0xFE. The same inline in Field::Draw 0x528809..0x528862 and SpellWolf::Draw
/// 0x51C751..0x51C7B7 (the colour times the charring grey of fn_00730570).
[[nodiscard]] constexpr uint32_t MulShr8_4(uint32_t c, uint32_t t) noexcept
{
	return Argb((Red(c) * Red(t)) >> 8, (Green(c) * Green(t)) >> 8, (Blue(c) * Blue(t)) >> 8,
	            (Alpha(c) * Alpha(t)) >> 8);
}

/// fn_0080BF10 0x80BF1B..0x80BFB9, the specular: min(a + b, 255) in the four channels, the alpha too (`cmp 0xFF` /
/// `jb` each, 0x80BF5B..0x80BF98)
[[nodiscard]] constexpr uint32_t AddSat_4(uint32_t a, uint32_t b) noexcept
{
	return Argb(std::min(Red(a) + Red(b), 0xFFu), std::min(Green(a) + Green(b), 0xFFu),
	            std::min(Blue(a) + Blue(b), 0xFFu), std::min(Alpha(a) + Alpha(b), 0xFFu));
}

/// fn_00809D80 (cdecl, called only at 0x80A290 with the object's colour and the mesh part's): (a b) >> 8 in R, G and B,
/// the alpha of `a` (`and esi, 0xFF000000` at 0x809DCF)
[[nodiscard]] constexpr uint32_t MulShr8_3KeepA(uint32_t a, uint32_t b) noexcept
{
	return Argb((Red(a) * Red(b)) >> 8, (Green(a) * Green(b)) >> 8, (Blue(a) * Blue(b)) >> 8, Alpha(a));
}

/// The scalar form of the same product: (c k) >> 8 in R, G and B through the masks 0xFF0000FF / 0xFF0000 / 0xFF00 and
/// `shr 8`, the alpha of `c` kept. fn_0084BA90's model light (0x84BBEA..0x84BC1D, k = the factor f), Tree::Draw
/// (0x74B077..0x74B0C4, k = [0xC22FA0], `and eax, 0xFF000000` at 0x74B0BD) and fn_0074B3A0 (k = the burning grey,
/// alpha kept at 0x74B4C9). `k` is 0..256, so that the masked channels do not run into each other.
[[nodiscard]] constexpr uint32_t ScaleShr8_3KeepA(uint32_t c, uint32_t k) noexcept
{
	return Argb((Red(c) * k) >> 8, (Green(c) * k) >> 8, (Blue(c) * k) >> 8, Alpha(c));
}

/// fn_00809DE0 (0x80A2A6 with the object's specular and the mesh part's): min(a + b, 255) in R, G and B (`jle` at
/// 0x809E01, 0x809E19, 0x809E35), the alpha of `a` (0x809E46)
[[nodiscard]] constexpr uint32_t AddSat_3KeepA(uint32_t a, uint32_t b) noexcept
{
	return Argb(std::min(Red(a) + Red(b), 0xFFu), std::min(Green(a) + Green(b), 0xFFu),
	            std::min(Blue(a) + Blue(b), 0xFFu), Alpha(a));
}

/// One byte times another over 255, truncated: the 0x80808081 multiply of fn_005E25C0 (0x5E2729..0x5E273B, the cloud's
/// edge alpha times the alignment colour's alpha) and of every Mul255 below
[[nodiscard]] constexpr uint32_t Mul255(uint32_t a, uint32_t b) noexcept
{
	return a * b / 255u;
}

/// fn_007ACF70 (0x7ACF79..0x7AD03C, ecx = the primitive, `ret 4`): trunc(a b / 255) in the four channels, the alpha too
[[nodiscard]] constexpr uint32_t Mul255_4(uint32_t a, uint32_t b) noexcept
{
	return Argb(Mul255(Red(a), Red(b)), Mul255(Green(a), Green(b)), Mul255(Blue(a), Blue(b)),
	            Mul255(Alpha(a), Alpha(b)));
}

/// LH3DMist fn_007FA300 0x7FA6C8..0x7FA75F: the mist's colour (+0x4C) times the land light of fn_00801C90 after the
/// haze fn_007FEB30, trunc(c l / 255) in R, G and B (0x7FA6DF, 0x7FA711, 0x7FA740); the alpha is the colour's
/// (`mov [esp + 0x13], ch` at 0x7FA753), not the light's
[[nodiscard]] constexpr uint32_t Mul255_3KeepA(uint32_t colour, uint32_t light) noexcept
{
	return Argb(Mul255(Red(colour), Red(light)), Mul255(Green(colour), Green(light)),
	            Mul255(Blue(colour), Blue(light)), Alpha(colour));
}

/// LH3DCreature::DrawNow 0x48EF00..0x48EF98: the body's colour (+0x4C) times the object's, trunc(a b / 255) in R, G
/// and B, the alpha forced to 0xFF (`mov byte ptr [esp + 0x57], 0xff` at 0x48EF8F)
[[nodiscard]] constexpr uint32_t Mul255_3OpaqueA(uint32_t a, uint32_t b) noexcept
{
	return Argb(Mul255(Red(a), Red(b)), Mul255(Green(a), Green(b)), Mul255(Blue(a), Blue(b)), 0xFFu);
}

/// bgfx's packed colour (no original): 0xAARRGGBB to 0xAABBGGRR, R and B swapped
[[nodiscard]] constexpr uint32_t ToAbgr(uint32_t argb) noexcept
{
	return (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
}
/// The same with the alpha replaced by a 0..255 byte
[[nodiscard]] constexpr uint32_t ToAbgr(uint32_t argb, uint32_t alpha) noexcept
{
	return (ToAbgr(argb) & 0x00FFFFFFu) | (alpha & 0xFFu) << 24;
}
/// The same from 0..1 floats, each clamped and rounded to the nearest byte (halves up)
[[nodiscard]] inline uint32_t ToAbgr(const glm::vec4& rgba) noexcept
{
	const auto byte = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return byte(rgba.a) << 24 | byte(rgba.b) << 16 | byte(rgba.g) << 8 | byte(rgba.r);
}

/// The colour as 0..1 floats for a uniform (no original): r, g, b, a = byte / 255
[[nodiscard]] inline glm::vec4 ToVec4(uint32_t argb) noexcept
{
	return glm::vec4(static_cast<float>(Red(argb)), static_cast<float>(Green(argb)), static_cast<float>(Blue(argb)),
	                 static_cast<float>(Alpha(argb))) /
	       255.0f;
}
/// The RGB of ToVec4
[[nodiscard]] inline glm::vec3 ToVec3(uint32_t argb) noexcept
{
	return glm::vec3(static_cast<float>(Red(argb)), static_cast<float>(Green(argb)), static_cast<float>(Blue(argb))) /
	       255.0f;
}

} // namespace openblack::lh3d_colour
