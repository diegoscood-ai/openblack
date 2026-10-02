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
#include <optional>

/// The original's materials and render modes: one LH3DMaterial of 16 bytes (LH3DRender::CreateMaterial 0x82FD30) and
/// one of 19 mode functions (the table 0xC38728, or its twin 0xC387C8 for the objects drawn with their own alpha) set
/// every blending, alpha test, Z write and texture stage state of a draw (wiki: rendering-objects.md, "Modos de render
/// y materiales"). Every openblack draw that stands for an original one asks State() for its bgfx state.
///
/// What is the mode's and what is the draw's:
/// - the mode (the table): ALPHABLENDENABLE, SRCBLEND / DESTBLEND, ALPHATESTENABLE, ALPHAREF, ZWRITEENABLE and stage 0
///   (colour = TEXTURE x DIFFUSE; alpha = TEXTURE x DIFFUSE or TEXTURE). ALPHAFUNC is GREATEREQUAL for every mode, set
///   once at 0x82CBA6.
/// - the inline SetMaterial (about 105 copies, e.g. SetupThing::DrawLine 0x412662..0x4126BD): CULLMODE from the
///   material byte +5 bit 0, tiling from +5 bit 2 or g_b_need_tilling [0xECA614].
/// - written by hand around a draw, never by a mode: ZFUNC (LESSEQUAL 4 from 0x82CCC5, ALWAYS 8, EQUAL 3) and a few
///   ZWRITEENABLE 0 (the 2D rectangles 0x81E64C, the sea 0x879FD9, the mirrored land 0x5E48C5). StateOptions carries
///   them, with what openblack's own targets add (alpha written, MSAA, the primitive type).
namespace openblack::graphics::render_modes
{

/// LH3DRender::RenderMode, the index of the tables 0xC38728 / 0xC387C8. The L3D material type is the same number
/// (l3d::L3DMaterial::Type), so the names are the L3D ones; 14 and 17 have no L3D type.
enum class Mode : uint8_t
{
	Smooth = 0,
	SmoothAlpha = 1,
	Textured = 2,
	TexturedAlpha = 3,
	AlphaTextured = 4,
	AlphaTexturedAlpha = 5,
	AlphaTexturedAlphaNz = 6,
	SmoothAlphaNz = 7,
	TexturedAlphaNz = 8,
	TexturedChroma = 9,
	AlphaTexturedAlphaAdditiveChroma = 10,
	AlphaTexturedAlphaAdditiveChromaNz = 11,
	AlphaTexturedAlphaAdditive = 12,
	AlphaTexturedAlphaAdditiveNz = 13,
	Landscape = 14, ///< the land blocks (fn_007FEDB0 0x7FEDE2, LH3DIsland::Create 0x80442A): the function of mode 5
	TexturedChromaAlpha = 15,
	TexturedChromaAlphaNz = 16,
	Mode17 = 17, ///< the function of mode 2; no CreateMaterial caller and no L3D type
	ChromaJustZ = 18,
};
inline constexpr uint32_t k_ModeCount = 19;

/// ALPHABLENDENABLE and SRCBLEND / DESTBLEND of a mode (D3D7 values: 1 ZERO, 2 ONE, 5 SRCALPHA, 6 INVSRCALPHA)
enum class Blend : uint8_t
{
	Disabled, ///< ALPHABLENDENABLE 0
	Standard, ///< SRCALPHA / INVSRCALPHA
	Additive, ///< SRCALPHA / ONE
	JustZ,    ///< ZERO / ONE: no colour is written, only Z where the alpha test passes (mode 18)
};

/// The D3D states one mode function sets (stage 0; with stage != 0 a mode only binds its texture, and the six alpha
/// tested modes their ALPHAREF too)
struct ModeDesc
{
	uint32_t function;  ///< the mode function in the table 0xC38728
	Blend blend;        ///< ALPHABLENDENABLE (0x1B), SRCBLEND (0x13), DESTBLEND (0x14)
	bool alphaTest;     ///< ALPHATESTENABLE (0xF), ALPHAREF (0x18) = the material's +4 (AlphaRef)
	bool zWrite;        ///< ZWRITEENABLE (0xE), compared on every call, outside the mode cache [0xC38718]
	bool alphaModulate; ///< ALPHAOP MODULATE(TEXTURE, DIFFUSE); else SELECTARG1(TEXTURE), or no texture
	bool textured;      ///< binds the material's texture (fn_00837DF0); 0, 1 and 7 clear the stage (SetTexture 0)
};

/// The table 0xC38728 (19 {function, word} pairs and a 0; the word, 1 for the blended modes, has no reader). Read in
/// the mode functions 0x82D470..0x82ECD0 (tmp_dis\render\frame_A_modes.txt; 2, 6, 9, 10, 11, 16 and 18 checked again)
// clang-format off
inline constexpr std::array<ModeDesc, k_ModeCount> k_Modes = {{
	{0x82D470, Blend::Disabled, false, true,  false, false}, // 0
	{0x82D5C0, Blend::Standard, false, true,  false, false}, // 1: stage 0 left as it was
	{0x82D820, Blend::Disabled, false, true,  false, true},  // 2: ABLEND = ATEST = 0 (0x82D841..0x82D862)
	{0x82D920, Blend::Standard, false, true,  true,  true},  // 3
	{0x82DC20, Blend::Standard, false, true,  false, true},  // 4
	{0x82DD90, Blend::Standard, false, true,  true,  true},  // 5
	{0x82DF10, Blend::Standard, false, false, true,  true},  // 6: 0x82DF45..0x82DFA8, ZWRITE 0 0x82E063
	{0x82D6F0, Blend::Standard, false, false, false, false}, // 7: ZWRITE 0 0x82D807
	{0x82DAA0, Blend::Standard, false, false, true,  true},  // 8: ZWRITE 0 0x82DBF6
	{0x82E080, Blend::Standard, true,  true,  false, true},  // 9
	{0x82E830, Blend::Additive, true,  true,  true,  true},  // 10: SA/ONE 0x82E87C, ATEST 0x82E88E, Z 0x82E91F
	{0x82E9C0, Blend::Additive, true,  false, true,  true},  // 11: 0x82EA0C, ATEST 0x82EA1E, ZWRITE 0 0x82EAAF
	{0x82EB50, Blend::Additive, false, true,  true,  true},  // 12
	{0x82ECD0, Blend::Additive, false, false, true,  true},  // 13
	{0x82DD90, Blend::Standard, false, true,  true,  true},  // 14 = 5
	{0x82E470, Blend::Standard, true,  true,  true,  true},  // 15
	{0x82E6A0, Blend::Standard, true,  false, true,  true},  // 16: ATEST 0x82E6EC..0x82E700, ZWRITE 0 0x82E78F
	{0x82D820, Blend::Disabled, false, true,  false, true},  // 17 = 2
	{0x82E2A0, Blend::JustZ,    true,  true,  false, true},  // 18: see below
}};
// clang-format on

/// g_set_render_mode_data [0xECA618]: which table SetMaterial calls through
enum class Table : uint8_t
{
	Normal,      ///< 0xC38728 (LH3DRender::Open 0x82B4EA)
	GlobalAlpha, ///< 0xC387C8: an object with its own alpha (vt+0x4C = fn_007F9D80, obj+4 bit 7; LH3DObject Draw 0x80DF09)
};

/// The table 0xC387C8: the opaque modes become their blended twin (alpha = texture x diffuse), the others stay
// clang-format off
inline constexpr std::array<Mode, k_ModeCount> k_GlobalAlphaModes = {{
	Mode::SmoothAlpha, Mode::SmoothAlpha,                // 0, 1 -> 0x82D5C0
	Mode::TexturedAlpha, Mode::TexturedAlpha,            // 2, 3 -> 0x82D920
	Mode::AlphaTexturedAlpha, Mode::AlphaTexturedAlpha,  // 4, 5 -> 0x82DD90
	Mode::AlphaTexturedAlphaNz, Mode::SmoothAlphaNz, Mode::TexturedAlphaNz,
	Mode::TexturedChromaAlpha,                           // 9 -> 0x82E470
	Mode::AlphaTexturedAlphaAdditiveChroma, Mode::AlphaTexturedAlphaAdditiveChromaNz, Mode::AlphaTexturedAlphaAdditive,
	Mode::AlphaTexturedAlphaAdditiveNz, Mode::Landscape, Mode::TexturedChromaAlpha, Mode::TexturedChromaAlphaNz,
	Mode::TexturedAlpha,                                 // 17 -> 0x82D920
	Mode::ChromaJustZ,
}};
// clang-format on

[[nodiscard]] constexpr const ModeDesc& Desc(Mode mode)
{
	return k_Modes[static_cast<uint8_t>(mode)];
}

/// The mode whose function SetMaterial calls: g_set_render_mode_data[m->mode].fn (0x412662..0x4126BD)
[[nodiscard]] constexpr Mode Select(Mode mode, Table table)
{
	return table == Table::GlobalAlpha ? k_GlobalAlphaModes[static_cast<uint8_t>(mode)] : mode;
}

/// D3DRS_ZFUNC around a draw (written by hand, never by a mode)
enum class ZFunc : uint8_t
{
	LessEqual, ///< 4, the global one (0x82CCC5). openblack's inverted Z: BGFX_STATE_DEPTH_TEST_GREATER (aproximado:
	           ///< strict, as every openblack pass)
	Equal,     ///< 3: the shadows on the objects (0x80E488), the object fade (0x80EBD6)
	Always,    ///< 8: drawn over everything (FinishFrame 0x82F460, the 2D rectangles, the sea, text, the sun's glare)
};

/// D3DRS_CULLMODE: the inline SetMaterial puts ((~flags5) & 1) * 2 + 1, 1 = NONE, 3 = CCW
enum class Cull : uint8_t
{
	None, ///< D3DCULL_NONE
	Ccw,  ///< D3DCULL_CCW (0x84C34A) = bgfx's CCW in openblack's view
	Cw,   ///< the CCW of the original seen through a mirroring camera (the reflection pass)
};

/// The culling of a material: none if two-sided (byte +5 bit 0), else CCW, flipped when the camera mirrors
[[nodiscard]] constexpr Cull CullFor(bool twoSided, bool mirrored)
{
	return twoSided ? Cull::None : (mirrored ? Cull::Cw : Cull::Ccw);
}

/// What a draw adds to its mode's states
struct StateOptions
{
	ZFunc zFunc {ZFunc::LessEqual};
	Cull cull {Cull::None};
	/// false: ZWRITEENABLE 0 written by hand after the mode (0x81E64C, 0x879FD9, 0x5E48C5..0x5E4900)
	bool zWrite {true};
	/// openblack's targets: the colour's alpha is written too (the original's back buffer has none, (inferido))
	bool writeAlpha {false};
	bool msaa {false};
	/// SRCALPHA / INVSRCALPHA as ONE / INVSRCALPHA: the shader premultiplies the colour by its alpha after sampling,
	/// which gives the same colour (rendering-objects.md, mode 6); only the destination alpha differs
	bool premultiplied {false};
	/// openblack's sea: fs_water composes the reflection itself, so the mode's blending is left out (water.md)
	bool blendInShader {false};
	/// bgfx bits that are not the original's states (the primitive type: BGFX_STATE_PT_LINES)
	uint64_t extra {0};
};

/// The bgfx state of a draw in a mode: the mode function's blending and Z write, then the draw's options
[[nodiscard]] uint64_t State(Mode mode, const StateOptions& options = {});

/// MSAA mod (off by default): the SRCALPHA / INVSRCALPHA edge of an alpha tested mode drawn as coverage
[[nodiscard]] constexpr bool AlphaToCoverage(Mode mode, bool enabled)
{
	return enabled && Desc(mode).alphaTest && Desc(mode).blend == Blend::Standard;
}

/// One L3D primitive in a model draw (Renderer::DrawSubMesh): State() of its mode (already through the table), without
/// the target's alpha for openblack's back-to-front primitives (`sorted`), and the MSAA mod's alpha to coverage
[[nodiscard]] uint64_t PrimitiveState(Mode mode, StateOptions options, bool sorted, bool alphaToCoverage);

/// The passes of L3D models (objects, reflections, the hand): opaque modes write colour, alpha and Z, with MSAA
inline constexpr StateOptions k_ModelPass {.writeAlpha = true, .msaa = true};

/// ALPHAREF of a draw in the alpha tested modes (9, 10, 11, 15, 16, 18): [0xECA65C] if the override switch [0xECA658]
/// is on (`forced`), else the material's +4. With the table 0xC387C8, modes 9 and 15 scale it by the object's alpha:
/// max(0, ftol(ref * A * (1 / 255) - 5)) (0x82E15C..0x82E1CE, 0x82E557..0x82E5C9; [0x900058], [0x8AB6E4], [0x8AA398]),
/// A = the alpha byte of the object's diffuse [0xC37D8C]. 0 for the modes without alpha test.
[[nodiscard]] uint8_t AlphaRef(Mode selected, Table table, uint8_t materialRef, std::optional<uint8_t> forced = std::nullopt,
                               uint8_t globalAlpha = 255);

/// The object's alpha byte A from openblack's 0..1 opacity (components::Alpha) (inferido: rounded to the nearest)
[[nodiscard]] uint8_t AlphaByte(float opacity);

/// The alpha of stage 0 that fs_object tests and writes for one primitive (u_skyAlphaThreshold.y and .w)
enum class AlphaSource : uint8_t
{
	None,     ///< 0: the mode neither blends nor tests (0, 2, 17): the texture's alpha is not used, alpha 1
	Texture,  ///< 1: SELECTARG1(TEXTURE) (4, 9, 18; mode 9 0x82E120), or no texture (1, 7)
	Modulate, ///< 2: MODULATE(TEXTURE, DIFFUSE) (mode 15 0x82E510): the alpha test sees texture x object alpha
};
struct ShaderAlpha
{
	float ref;          ///< ALPHAREF / 255 of the alpha tested modes (GREATEREQUAL, 0x82CBA6), -1 without the test
	AlphaSource source; ///< the alpha the stage outputs
};

/// One primitive in the mode it is drawn in (already through the table, or forced): its alpha test and stage 0 alpha
[[nodiscard]] ShaderAlpha PrimitiveAlpha(Mode drawn, Table table, uint8_t materialRef, uint8_t globalAlpha = 255);

/// GJUtils::MaterialProperties, 5 bytes (GJUtils::GetSharedMesh 0x57DFB0, GJUtils::SetMaterialProperties 0x57E120)
struct MaterialProperties
{
	bool additive;    ///< +0: the additive mode 13 (SRCALPHA / ONE)
	bool zWrite;      ///< +1: the Z-writing variant (6 -> 5, 13 -> 12, 8 -> 3, 16 -> 9), else the one without Z
	bool doubleSided; ///< +2: material byte +5 bit 0 set (D3DCULL_NONE), else cleared (back faces culled)
	bool change;      ///< +3: PGetSharedMesh 0x57DF18 applies the properties to the mesh it loads (fn_0057E1D0)
	bool alpha;       ///< +4: 0 makes every type TexturedAlpha (3) before the rules above
};

/// GJUtils::SetMaterialProperties 0x57E120: the new mode of a material (the double-sided bit is the caller's)
[[nodiscard]] Mode ModeFromProperties(Mode mode, const MaterialProperties& properties);

/// The states of an LH3DMaterial (16 bytes, LH3DRender::CreateMaterial 0x82FD30: +0 the mode, +4 ALPHAREF = 0, +5
/// flags = 0, +8 the texture, +0xC 0xFF0000FF) that its draws use. The texture is the draw's: one texture is shared by
/// several materials (smoke.raw by modes 6 and 13, 0x80BC7D / 0x80BCB8)
struct Material
{
	Mode mode;         ///< +0
	uint8_t flags {0}; ///< +5: bit 0 two-sided, bit 2 tiling, bit 4 no texture offset (fn_0057D4C0 0x57D4F0)

	[[nodiscard]] constexpr bool TwoSided() const { return (flags & 1u) != 0; }
};

inline constexpr uint8_t k_TwoSided = 1u; ///< +5 bit 0, `or byte [m+5], 1`
inline constexpr uint8_t k_Tiling = 4u;   ///< +5 bit 2, `or byte [m+5], 4` (SetD3DTillingOn 0x82FF10)

/// State() of a draw through the inline SetMaterial of a material: its mode, and without a cull in the options the
/// material's (+5 bit 0, `((~m[5]) & 1) * 2 + 1`), flipped when the camera mirrors
[[nodiscard]] uint64_t State(const Material& material, StateOptions options = {}, bool mirrored = false);

/// The materials with a name that openblack's draws stand for (they are made once, at the addresses below)
namespace materials
{
/// [0xEA1ABC] (fn_0080BBD0 0x80BC7D): smoke.raw [0xEA1A98], mode 6, +5 |= bl = 1 (0x80BC8D, bl 0x80BBF3): two-sided.
/// The chimney smoke (g_smoke_mat), the mists and clouds (fn_007FA300), the boats' wake and SmokyStuff puffs
inline constexpr Material k_Smoke {Mode::AlphaTexturedAlphaNz, k_TwoSided};
/// [0xEA1AC4] (0x80BCB8): the same smoke.raw, mode 13, two-sided (0x80BCC8). The water
/// rings (GWater::InitialiseCircles 0x54BA72, (inferido): the reference is not decoded inside the routine)
inline constexpr Material k_SmokeAdditive {Mode::AlphaTexturedAlphaAdditiveNz, k_TwoSided};
/// [0xEA1AB0] (0x80BD1B): misc0.raw, mode 6, two-sided (0x80BD2B). The fish farm
/// shoals (fn_00824740 0x8247CC, (inferido) likewise)
inline constexpr Material k_Misc0 {Mode::AlphaTexturedAlphaNz, k_TwoSided};
/// LH3DAtmos::AtmosMaterial [0xEDC368] (fn_00835AD0 0x835C36): atmos.raw [0xEDC370], mode 6, +5 |= 1 | 4 (0x835C58,
/// 0x835C6F..0x835C79). The rain streaks
inline constexpr Material k_Atmos {Mode::AlphaTexturedAlphaNz, k_TwoSided | k_Tiling};
/// LH3DAtmos::AdditiveMaterial [0xEDC364] (0x835C49): the same atmos.raw, mode 13, two-sided (0x835C61).
/// The hand's glow on the sea (0x5E4281)
inline constexpr Material k_AtmosAdditive {Mode::AlphaTexturedAlphaAdditiveNz, k_TwoSided};
} // namespace materials

} // namespace openblack::graphics::render_modes
