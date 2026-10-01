/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLightTable.h"

#include <algorithm>

namespace openblack
{
namespace
{
constexpr size_t k_PaletteSide = 32;

/// Per-channel integer lerp of fn_00869850, t in 1/256; alpha from b
uint32_t Lerp(uint32_t a, uint32_t b, uint32_t t)
{
	const uint32_t r = (((((b & 0xFF0000u) - (a & 0xFF0000u)) * t) >> 8) + (a & 0xFFFF0000u)) & 0xFF0000u;
	const uint32_t g = (((((b & 0xFF00u) - (a & 0xFF00u)) * t) >> 8) + (a & 0xFFFFFF00u)) & 0xFF00u;
	const uint32_t bl = (((((b & 0xFFu) - (a & 0xFFu)) * t) >> 8) + a) & 0xFFu;
	return r | g | bl | (b & 0xFF000000u);
}

/// fn_00869790: min(255, (a * (255 - t) + b * t) / D), D = 200 - 130 * LightBoost (registry, default 0); alpha from a
uint32_t Ramp(uint32_t a, uint32_t b, uint32_t t)
{
	constexpr uint32_t k_Divisor = 200;
	uint32_t result = a & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const uint32_t value = (((a >> shift) & 0xFFu) * (255 - t) + ((b >> shift) & 0xFFu) * t) / k_Divisor;
		result |= std::min(255u, value) << shift;
	}
	return result;
}
} // namespace

bool LandLightTable::Load(const std::vector<uint8_t>& palette) noexcept
{
	if (palette.size() != k_PaletteSide * k_PaletteSide * 4)
	{
		return false;
	}
	_palette.resize(k_PaletteSide * k_PaletteSide);
	for (size_t i = 0; i < _palette.size(); ++i)
	{
		// R, G, B, A bytes; the loader (0x835CEC) swaps them into a D3DCOLOR
		_palette[i] = static_cast<uint32_t>(palette[i * 4 + 0]) << 16 | static_cast<uint32_t>(palette[i * 4 + 1]) << 8 |
		              static_cast<uint32_t>(palette[i * 4 + 2]) | static_cast<uint32_t>(palette[i * 4 + 3]) << 24;
	}
	return true;
}

void LandLightTable::Build(float skyType, float alignment, float weather) noexcept
{
	if (_palette.empty())
	{
		return;
	}
	// Palette columns: time (0 midnight .. 15 dusk .. 30 noon) and alignment (0 good .. 15 neutral .. 30 evil)
	const float timeColumn = std::clamp(skyType, 0.0f, 2.0f) * 6.0f * 2.5f;
	const float x = std::clamp(1.0f - alignment, 0.0f, 2.0f);
	const float alignColumn = x * 15.0f;

	std::array<uint32_t, 8> colours {};
	for (size_t row = 0; row < colours.size(); ++row)
	{
		const float column = row < 3 ? timeColumn : alignColumn;
		const auto index = std::min(static_cast<size_t>(column), k_PaletteSide - 2);
		const auto t = static_cast<uint32_t>((column - static_cast<float>(index)) * 256.0f);
		colours[row] = Lerp(_palette[row * k_PaletteSide + index], _palette[row * k_PaletteSide + index + 1], t);
	}

	_moonColour = glm::vec3((colours[5] >> 16) & 0xFFu, (colours[5] >> 8) & 0xFFu, colours[5] & 0xFFu) / 255.0f;

	const auto k = static_cast<int>(x * 255.0f);
	uint32_t base = x < 1.0f ? Lerp(colours[0], colours[1], static_cast<uint32_t>(k))
	                         // at X = 1 exactly the original lerps with t = -1 (unsigned wrap): keep it
	                         : Lerp(colours[1], colours[2], static_cast<uint32_t>(k - 256));
	const auto limit = static_cast<uint32_t>(255.0f - std::clamp(weather, 0.0f, 1.0f) * 96.0f);
	uint32_t capped = base & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		capped |= std::min((base >> shift) & 0xFFu, limit) << shift;
	}
	base = capped;
	_base = base;

	// Haze (clear weather, no lightning): k from the base colour's luminance, fog colour = base / 3, near / far by sky
	// type (400 -> 900 at noon and midnight, 100 -> 800 at dusk)
	{
		const uint32_t r = (base >> 16) & 0xFFu;
		const uint32_t g = (base >> 8) & 0xFFu;
		const uint32_t b = base & 0xFFu;
		_haze.k = static_cast<float>(std::min(255u, (r + 4 * g + 3 * b) / 8 + 8));
		_haze.colour = glm::vec3(static_cast<float>(r / 3), static_cast<float>(g / 3), static_cast<float>(b / 3));
		const float v = 1.0f - std::abs(std::clamp(skyType, 0.0f, 2.0f) - 1.0f); // 0 day / night, 1 dusk
		_haze.nearDistance = 1.0f / (0.0025f + 0.0075f * v * v);
		_haze.farDistance = 1.0f / (0.00111111f + 0.000138889f * v * v);
	}
	s_lastBase = _base; // [0xFA26A4] for the readers outside the renderer (LastBuiltBase)
	s_lastHaze = _haze;

	const uint32_t n = (((base >> 8) & 0xFFu) * 48) >> 8;
	for (uint32_t i = 0; i < n; ++i)
	{
		_table[i] = Ramp(colours[3], base, (i * 256) / n);
	}
	for (uint32_t i = n; i < 48; ++i)
	{
		_table[i] = Ramp(base, colours[6], ((i - n) * 256) / (48 - n));
	}
	for (uint32_t i = 48; i < k_Size; ++i)
	{
		_table[i] = Ramp(colours[3], base, i);
	}

	for (size_t i = 0; i < k_Size; ++i)
	{
		const uint32_t c = _table[i];
		_texels[i] = ((c >> 16) & 0xFFu) | (c & 0xFF00u) | ((c & 0xFFu) << 16) | 0xFF000000u;
	}
}

glm::vec3 LandLightTable::GetColour(size_t index) const noexcept
{
	const uint32_t c = _table.at(index);
	return glm::vec3((c >> 16) & 0xFFu, (c >> 8) & 0xFFu, c & 0xFFu) / 255.0f;
}

glm::vec3 LandLightTable::GetBaseColour() const noexcept
{
	return glm::vec3((_base >> 16) & 0xFFu, (_base >> 8) & 0xFFu, _base & 0xFFu) / 255.0f;
}

uint32_t LandLightTable::s_lastBase = 0xFFFFFFFFu;
LandLightTable::Haze LandLightTable::s_lastHaze {};

uint32_t LandLightTable::LastBuiltBase() noexcept
{
	return s_lastBase;
}

LandLightTable::Haze LandLightTable::LastBuiltHaze() noexcept
{
	return s_lastHaze;
}

} // namespace openblack
