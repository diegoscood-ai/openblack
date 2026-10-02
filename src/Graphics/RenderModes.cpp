/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RenderModes.h"

#include <cmath>

#include <algorithm>

#include <bgfx/bgfx.h>

namespace openblack::graphics::render_modes
{

uint64_t State(Mode mode, const StateOptions& options)
{
	const auto& desc = Desc(mode);
	uint64_t state = BGFX_STATE_WRITE_RGB;
	if (options.writeAlpha)
	{
		state |= BGFX_STATE_WRITE_A;
	}
	if (desc.zWrite && options.zWrite)
	{
		state |= BGFX_STATE_WRITE_Z;
	}
	switch (options.zFunc)
	{
	case ZFunc::LessEqual:
		state |= BGFX_STATE_DEPTH_TEST_GREATER;
		break;
	case ZFunc::Equal:
		state |= BGFX_STATE_DEPTH_TEST_EQUAL;
		break;
	case ZFunc::Always:
		break;
	case ZFunc::LessEqualInclusive:
		state |= BGFX_STATE_DEPTH_TEST_GEQUAL; // D3DCMP_LESSEQUAL (0x82CCC5) with openblack's inverted Z
		break;
	}
	if (!options.blendInShader)
	{
		switch (desc.blend)
		{
		case Blend::Disabled:
			break;
		case Blend::Standard:
			state |= options.premultiplied ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA)
			                               : BGFX_STATE_BLEND_ALPHA;
			break;
		case Blend::Additive:
			state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
			break;
		case Blend::JustZ:
			state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE);
			break;
		}
	}
	switch (options.cull)
	{
	case Cull::None:
		break;
	case Cull::Ccw:
		state |= BGFX_STATE_CULL_CCW;
		break;
	case Cull::Cw:
		state |= BGFX_STATE_CULL_CW;
		break;
	}
	if (options.msaa)
	{
		state |= BGFX_STATE_MSAA;
	}
	return state | options.extra;
}

uint64_t State(const Material& material, StateOptions options, bool mirrored)
{
	if (options.cull == Cull::None)
	{
		options.cull = CullFor(material.TwoSided(), mirrored);
	}
	return State(material.mode, options);
}

uint64_t PrimitiveState(Mode mode, StateOptions options, bool sorted, bool alphaToCoverage)
{
	if (sorted)
	{
		options.writeAlpha = false;
	}
	auto state = State(mode, options);
	if (AlphaToCoverage(mode, alphaToCoverage))
	{
		state = (state & ~BGFX_STATE_BLEND_MASK) | BGFX_STATE_BLEND_ALPHA_TO_COVERAGE;
	}
	return state;
}

uint8_t AlphaRef(Mode selected, Table table, uint8_t materialRef, std::optional<uint8_t> forced, uint8_t globalAlpha)
{
	if (!Desc(selected).alphaTest)
	{
		return 0;
	}
	// 0x82E166..0x82E17E: [0xECA65C] if [0xECA658] != 0, else the material's +4
	const uint8_t ref = forced.value_or(materialRef);
	// 0x82E15C: only with the table 0xC387C8 (and only modes 9 and 15 have this branch, 0x82E15C / 0x82E557)
	if (table != Table::GlobalAlpha || (selected != Mode::TexturedChroma && selected != Mode::TexturedChromaAlpha))
	{
		return ref;
	}
	// fild (ref) * fimul (A) * [0x900058] - [0x8AB6E4], below [0x8AA398] = 0 -> 0, then __ftol 0x7A1400 (truncation),
	// in the original's 24-bit FPU precision (fn_007DEE00): float
	constexpr float k_InvByte = 1.0f / 255.0f; // [0x900058] = 0.00392157
	constexpr float k_Bias = 5.0f;             // [0x8AB6E4]
	float value = static_cast<float>(static_cast<uint32_t>(ref) * globalAlpha) * k_InvByte - k_Bias;
	if (value < 0.0f)
	{
		value = 0.0f;
	}
	return static_cast<uint8_t>(static_cast<int32_t>(value));
}

uint8_t AlphaByte(float opacity)
{
	return static_cast<uint8_t>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
}

ShaderAlpha PrimitiveAlpha(Mode drawn, Table table, uint8_t materialRef, uint8_t globalAlpha)
{
	const auto& desc = Desc(drawn);
	const float ref =
	    desc.alphaTest ? static_cast<float>(AlphaRef(drawn, table, materialRef, std::nullopt, globalAlpha)) / 255.0f : -1.0f;
	if (desc.alphaModulate)
	{
		return {ref, AlphaSource::Modulate};
	}
	return {ref, desc.blend != Blend::Disabled || desc.alphaTest ? AlphaSource::Texture : AlphaSource::None};
}

Mode ModeFromProperties(Mode mode, const MaterialProperties& properties)
{
	// GJUtils::SetMaterialProperties 0x57E120, in this order
	if (mode == Mode::AlphaTextured)
	{
		mode = Mode::AlphaTexturedAlphaNz; // 0x57E126
	}
	if (!properties.alpha)
	{
		mode = Mode::TexturedAlpha; // 0x57E138
	}
	if (properties.additive)
	{
		mode = Mode::AlphaTexturedAlphaAdditiveNz; // 0x57E142: cmp byte [ecx], 1
	}
	if (properties.zWrite) // 0x57E14C
	{
		switch (mode)
		{
		case Mode::AlphaTexturedAlphaNz:
			return Mode::AlphaTexturedAlpha;
		case Mode::AlphaTexturedAlphaAdditiveNz:
			return Mode::AlphaTexturedAlphaAdditive;
		case Mode::TexturedAlphaNz:
			return Mode::TexturedAlpha;
		case Mode::TexturedChromaAlphaNz:
			return Mode::TexturedChroma;
		default:
			return mode;
		}
	}
	switch (mode) // 0x57E182
	{
	case Mode::AlphaTexturedAlpha:
		return Mode::AlphaTexturedAlphaNz;
	case Mode::AlphaTexturedAlphaAdditive:
		return Mode::AlphaTexturedAlphaAdditiveNz;
	case Mode::TexturedAlpha:
	case Mode::Textured:
		return Mode::TexturedAlphaNz;
	case Mode::TexturedChroma:
		return Mode::TexturedChromaAlphaNz;
	default:
		return mode;
	}
}

} // namespace openblack::graphics::render_modes
