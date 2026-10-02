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
#include <array>
#include <span>
#include <string_view>
#include <vector>

/// The cut of the original's 8-bit .raw textures to ARGB4444, CPU only. Wiki: rendering.md, "Texturas ARGB4444".
///
/// LH3DTexture keeps every texture with the alpha flag 0x40 (the 0x41 Create calls) as a 16-bit ARGB4444 surface:
/// fn_00837400 drops the low nibble of each byte of x.raw and of its xa.raw (0x8374F4, 0x837502, 0x837512, 0x837681)
/// and D3D7 expands the nibble back to 8 bits when it samples (n * 17, inferido: done by the driver, not in the exe).
/// There is no shader twin on purpose: the original filters nibbles that are already cut, so the cut has to happen
/// when the texture is loaded (Texture2DLoader) and not after the linear filtering of a fragment shader.
namespace openblack::graphics::argb4444
{

/// The nibble of an 8-bit value: v >> 4. fn_00837400: B = byte[+2] >> 4 (0x8374F4), G = byte[+1] & 0xF0 (0x837502),
/// R = (byte[+0] & 0xF0) << 4 (0x837512 / 0x837517), A = byte & 0xF0 (0x837681); fn_0081FAA0 human_shadow
/// (`and cl, 0xF0` 0x81FCDD). All of them keep the top 4 bits, none rounds.
[[nodiscard]] constexpr uint8_t Quantize(uint8_t v) noexcept
{
	return static_cast<uint8_t>(v >> 4);
}

/// A nibble back to 8 bits as D3D7 samples an ARGB4444 surface: n * 17 = (n << 4) | n, 15 -> 255 (inferido: the
/// expansion is done by the driver)
[[nodiscard]] constexpr uint8_t Expand(uint8_t n) noexcept
{
	return static_cast<uint8_t>((n & 0xFu) * 17u);
}

/// What the original samples for an 8-bit .raw byte: Expand(Quantize(v)) = (v & 0xF0) | (v >> 4) (fn_00837400 + D3D)
[[nodiscard]] constexpr uint8_t Cut(uint8_t v) noexcept
{
	return Expand(Quantize(v));
}

/// An ARGB4444 texel to 8-bit R, G, B, A (this order, as RGBA8 in memory), each Expand-ed. Format 0 of LH3DTexture is
/// the D3D pixel format with the masks A 0xF000, R 0x0F00, G 0x00F0, B 0x000F (0x85DCA1..0x85DCC2).
[[nodiscard]] constexpr std::array<uint8_t, 4> Unpack(uint16_t texel) noexcept
{
	return {Expand(static_cast<uint8_t>((texel >> 8) & 0xFu)), Expand(static_cast<uint8_t>((texel >> 4) & 0xFu)),
	        Expand(static_cast<uint8_t>(texel & 0xFu)), Expand(static_cast<uint8_t>(texel >> 12))};
}

/// The ARGB4444 texel fn_00837400 writes: R, G, B nibbles from the colour (`mov word` 0x837533, alpha 0) and the alpha
/// nibble OR-ed in (`mov dh, al / or [edi], dx` 0x837685 / 0x837687)
[[nodiscard]] constexpr uint16_t Pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
	return static_cast<uint16_t>(Quantize(a) << 12 | Quantize(r) << 8 | Quantize(g) << 4 | Quantize(b));
}

/// The pair x.raw (R, G, B bytes) + xa.raw (one byte per pixel) as fn_00837400 packs it, returned as the RGBA8 the
/// original samples (Cut in the four channels). An empty `alpha` is a missing xa.raw: LHLoadData(name, [0xEDD3D8],
/// 0x10000) (0x837600) failed, fn_00837400 only reports it (Report3D 0x837616) and carries on at 0x83761E with the
/// buffer that still holds the colour file, so pixel i takes the alpha (colour stream byte i) & 0xF0 (R0, G0, B0, R1..).
/// A shorter xa.raw is not an error either: LHLoadData reads min(length, 0x10000) (`cmp / jbe` 0x7BCEC7..0x7BCECD)
/// over the same buffer, so the pixels past its end keep the colour stream; a longer one is truncated.
[[nodiscard]] inline std::vector<uint8_t> PackRaw(std::span<const uint8_t> rgb, std::span<const uint8_t> alpha)
{
	const size_t pixels = rgb.size() / 3;
	std::vector<uint8_t> rgba(pixels * 4);
	for (size_t i = 0; i < pixels; ++i)
	{
		rgba[i * 4 + 0] = Cut(rgb[i * 3 + 0]);
		rgba[i * 4 + 1] = Cut(rgb[i * 3 + 1]);
		rgba[i * 4 + 2] = Cut(rgb[i * 3 + 2]);
		rgba[i * 4 + 3] = Cut(i < alpha.size() ? alpha[i] : rgb[i]);
	}
	return rgba;
}

/// The lower-case base names (no ".raw") of the colour textures the original creates with the alpha flag 0x40, that
/// is with Create 0x8379E0 flags 0x41: fn_00837DF0 (0x838079) and fn_00838AF0 (0x838D37) pass flags & 0x40 to
/// fn_00837400 (0x838087 / 0x838D41), and with it the 4444 branch runs (`cmp [esp+0x834], 0 / je` 0x8374CB /
/// 0x8374D2; without it the 565 / 555 branch 0x8376E3 / 0x837765).
inline constexpr auto k_AlphaFlagStems = std::to_array<std::string_view>({
    "atmos",                       // 0x835C29
    "blobs",                       // Data\blobs, 0x845E08
    "burn",                        // fn_0080BBD0 0x80BD37
    "c_ape_hair",                  // Data\C_Ape_Hair, 0x6186C7
    "choosesymbol",                // 0x5DE425
    "cool_effect",                 // fn_0080BBD0 0x80BCD4
    "envmap",                      // fn_0080B380 0x80B393, table 0xC37EAC
    "envmap_glass_fx",             // table 0xC37EAC
    "envmap_glass_fx2_inv",        // table 0xC37EAC
    "envmap_glass_fx_inv",         // table 0xC37EAC
    "forcefield",                  // 0x4568B9
    "front_end_buttons",           // 0x4120CA
    "gatheringtext",               // 0x5F8A92, 0x72E00D, 0x83109F
    "icons",                       // 0x787358
    "leash",                       // 0x8485F2
    "misc0",                       // fn_0080BBD0 0x80BD0E
    "mousehelp",                   // 0x447584
    "originalchoosesymbol",        // 0x5DE43D
    "p4",                          // fn_0080BBD0 0x80BC44
    "p4t",                         // fn_0080BBD0 0x80BC1B
    "picturetexture",              // 0x7900FD
    "pin",                         // 0x53C658
    "playerssymbols",              // 0x5DE410
    "rainbow",                     // 0x5C386B
    "s_beam",                      // spell files (*)
    "s_fire",                      // fn_0080BBD0 0x80BC99; GlobalTextures (*)
    "s_gesture0",                  // GlobalTextures table 0xBEF4A0 (*)
    "s_gesture1",                  // table 0xBEF4A0 (*)
    "s_hand_flow",                 // GlobalTextures (*)
    "s_iceenvmap",                 // table 0xBEF478 (*)
    "s_iceenvmapcolor",            // table 0xBEF478 (*), not in Data
    "s_iceenvmapgrey",             // table 0xBEF478 (*), 194823 bytes: not a .raw for fn_00837300
    "s_lightning",                 // spell files (*)
    "s_lightsheetstars",           // table 0xBEF484 (*)
    "s_spangle_a",                 // spell files (*) (inferido)
    "s_spritesheet1",              // GlobalTextures (*)
    "s_spritesheet2",              // GlobalTextures (*)
    "s_spritesheet3",              // GlobalTextures (*)
    "s_static",                    // GlobalTextures (*)
    "s_teleport_stars",            // table 0xBEF484 (*)
    "s_teleport_vortex_texture",   // spell files (*)
    "s_teleport_vortex_texture01", // spell files (*)
    "s_tilelandscape",             // table 0xBEF484 (*)
    "s_volcano_base",              // LandscapeVortex table 0xBF3F5C (fn_005FD4C0) (*)
    "s_volcano_base_alpha",        // table 0xBF3F68 (fn_005FD4D0) (*)
    "s_volcano_fire",              // spell files (*)
    "s_volcano_rock",              // spell files (*)
    "s_vollight",                  // table 0xBEF484 (*)
    "s_vollight2",                 // table 0xBEF484 (*)
    "s_vollight3",                 // table 0xBEF484 (*)
    "s_vollight4",                 // table 0xBEF484 (*)
    "s_vortexbasealphacopy",       // table 0xBF3F68 (*)
    "s_vortexbasemultiring",       // table 0xBF3F5C (*)
    "sky",                         // GLandscape::Open 0x5E5432 / 0x5E5461
    "smallbump",                   // fn_00804830 0x804844
    "smoke",                       // fn_0080BBD0 0x80BC6D
    "snow",                        // 0x835C7C
    "weather",                     // 0x81E831
    // (*) through fn_0057DBE0 -> fn_0057DB10 (Create flags 0x41 at 0x57DB78 / 0x57DB7B): GlobalTextures, the
    // ParticleSpriteCreator / ParticleChainCreator of the spell files (TextureFileName, fn_006AA030 0x6AA047 /
    // fn_006AA800 0x6AA817), ZR_SurfRevol 0x6863E4 and LandscapeVortex fn_005FEA70 (0x5FEA92 / 0x5FEB1B). The
    // computed 0x41 of fn_00822560 (0x822855..0x822874: 1, plus 0x40 if the a.raw exists) is the .cmp landscape
    // converter, and the save-game pictures (0x7926DA, 0x79286F) are made by the game: neither has a stem here.
});

/// Whether `stem` (the file name without ".raw", any case) is one of the colour stems of k_AlphaFlagStems
[[nodiscard]] constexpr bool IsAlphaFlagColour(std::string_view stem) noexcept
{
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	return std::ranges::any_of(k_AlphaFlagStems, [&](std::string_view entry) {
		return entry.size() == stem.size() &&
		       std::ranges::equal(entry, stem, [&](char a, char b) { return a == lower(b); });
	});
}

/// The colour stem of an alpha stem: `stem` without its final "a" (any case) when that is an IsAlphaFlagColour stem,
/// else empty. fn_00837400 builds the alpha name as the colour name without its last 4 characters plus "a.raw"
/// (0xC384AC / 0x8375B4..0x8375C1). That alpha is cut only together with its colour: fn_00837400 runs only for a
/// colour file of 0x30000 bytes (0x837318) and only then reads xa.raw (0x837600), so the caller checks the colour
/// file too (S_IceEnvMapGreya.raw is not cut: its colour is 194823 bytes).
[[nodiscard]] constexpr std::string_view ColourOfAlpha(std::string_view stem) noexcept
{
	if (stem.empty() || (stem.back() != 'a' && stem.back() != 'A'))
	{
		return {};
	}
	const auto colour = stem.substr(0, stem.size() - 1);
	return IsAlphaFlagColour(colour) ? colour : std::string_view {};
}

/// Whether the texture `stem` (the file name without ".raw", any case) is cut to ARGB4444 by the original: a colour
/// stem of k_AlphaFlagStems or its alpha, the same stem plus "a" (ColourOfAlpha)
[[nodiscard]] constexpr bool HasAlphaFlag(std::string_view stem) noexcept
{
	return IsAlphaFlagColour(stem) || !ColourOfAlpha(stem).empty();
}

/// fn_0081FAA0 builds the blob shadow from ".\Data\Textures\human_shadow.raw" (LHLoadData 0x400 bytes, 0x81FC86 /
/// 0x81FC91) as its own 0x44 memory texture (Create 0x81FC58, locked through fn_00838AF0 0x81FCA4): texel = (v & 0xF0)
/// << 8 (`and cl, 0xF0 / mov bh, cl` 0x81FCDD..0x81FCEC), alpha Quantize(v) with R = G = B = 0. Not an alpha flag
/// texture, but the same cut.
inline constexpr std::string_view k_HumanShadowStem = "human_shadow";

/// The colour file fn_00837400 accepts: exactly 0x30000 bytes, 256 x 256 R, G, B (fn_00837300 `cmp ecx, 0x30000 /
/// sete` 0x837318), and the 0x10000 bytes of its alpha (0x8375F5)
inline constexpr size_t k_ColourBytes = 0x30000;
inline constexpr size_t k_AlphaBytes = 0x10000;

} // namespace openblack::graphics::argb4444
