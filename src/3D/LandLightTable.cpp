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

#include "3D/SkyType.h"

namespace openblack
{
namespace
{
constexpr size_t k_PaletteSide = 32;

// The haze distances of fn_00869850 in 1 / distance, the exact floats of runblack.exe
constexpr float k_NearInverse = 0x1.47AE14p-9f;      ///< [0x9A3B18] 0x3B23D70A = 0.0025
constexpr float k_NearInverseDusk = 0x1.EB851Ep-8f;  ///< [0x9A3BDC] 0x3BF5C28F = 0.0075
constexpr float k_FarInverse = 0x1.234568p-10f;      ///< [0x9A3BE0] 0x3A91A2B4 = 1 / 900
constexpr float k_FarInverseDusk = 0x1.234560p-13f;  ///< [0x9A3BD8] 0x3911A2B0 = 0.00013888883
constexpr float k_NearInverseStorm = 0x1.111112p-4f; ///< [0x9A3B70] 0x3D888889 = 1 / 15
constexpr float k_FarInverseStorm = 0x1.767DCEp-9f;  ///< [0x9A3BD4] 0x3B3B3EE7 = 1 / 350

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

void LandLightTable::Build(float skyType, float alignment, float overcast, uint8_t flash) noexcept
{
	if (_palette.empty())
	{
		return;
	}
	// Palette columns: time (0 midnight .. 15 dusk .. 30 noon), 0x86985E..0x8698AD: (2 - T) * 6 * 2.5
	// (sky_type::LightColumn), and alignment (0 good .. 15 neutral .. 30 evil)
	const float timeColumn = sky_type::LightColumn(skyType);
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
	_row6 = colours[6];

	const auto k = static_cast<int>(x * 255.0f);
	uint32_t base = x < 1.0f ? Lerp(colours[0], colours[1], static_cast<uint32_t>(k))
	                         // at X = 1 exactly the original lerps with t = -1 (unsigned wrap): keep it
	                         : Lerp(colours[1], colours[2], static_cast<uint32_t>(k - 256));
	// 0x869ADB: ftol(255 - 96 * overcast), not clamped (the haze below clamps the overcast to 1 afterwards)
	const auto limit = static_cast<int32_t>(255.0f - overcast * 96.0f);
	uint32_t capped = base & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const auto channel = static_cast<int32_t>((base >> shift) & 0xFFu);
		capped |= static_cast<uint32_t>(std::clamp(std::min(channel, limit), 0, 255)) << shift;
	}
	base = capped;
	_base = base;

	// Haze: k from the base colour's luminance, fog colour = base / 3, near / far by sky type (400 -> 900 at noon and
	// midnight, 100 -> 800 at dusk); then the storm and the lightning (0x869DB1..0x869F37)
	{
		const uint32_t r = (base >> 16) & 0xFFu;
		const uint32_t g = (base >> 8) & 0xFFu;
		const uint32_t b = base & 0xFFu;
		_haze.k = static_cast<float>(std::min(255u, (r + 4 * g + 3 * b) / 8 + 8));
		_haze.colour = glm::vec3(static_cast<float>(r / 3), static_cast<float>(g / 3), static_cast<float>(b / 3));
		// 0x869D53..0x869DAB: v2 = sky_type::HazeFactor(T) from [0xFA26BC] (0 by day and in full night, 1 in full
		// dusk); with v2 > 0 the inverses become v2 * c + base, else the bases stay
		const float v2 = sky_type::HazeFactor(skyType);
		float nearInverse = k_NearInverse;
		float farInverse = k_FarInverse;
		if (v2 > 0.0f)
		{
			nearInverse = v2 * k_NearInverseDusk + k_NearInverse;
			farInverse = v2 * k_FarInverseDusk + k_FarInverse;
		}
		if (overcast > 0.0f)
		{
			// the storm: colour -> (c >> 3) + 32, k -> 48, near -> 15 and far -> 350 (in 1 / distance) by the overcast
			// amount, clamped to 1 (0x869DDB writes the clamp back to [0xFA2754])
			const float w = std::min(overcast, 1.0f);
			const glm::vec3 storm(static_cast<float>((r >> 3) + 32), static_cast<float>((g >> 3) + 32),
			                      static_cast<float>((b >> 3) + 32));
			_haze.colour += (storm - _haze.colour) * w;
			const float k = _haze.k;
			_haze.k = k + static_cast<float>(static_cast<int32_t>((48.0f - k) * w)); // __ftol truncates
			nearInverse += (k_NearInverseStorm - nearInverse) * w; // 0x869E71..0x869E86
			farInverse += (k_FarInverseStorm - farInverse) * w;    // 0x869E88..0x869E96
		}
		if (flash != 0)
		{
			// the lightning: colour and k that much of the way to 255
			const float f = static_cast<float>(flash);
			_haze.colour += (glm::vec3(255.0f) - _haze.colour) * f * 0.00390625f;
			const auto k = static_cast<int32_t>(_haze.k);
			_haze.k = static_cast<float>(k + ((255 - k) * static_cast<int32_t>(flash)) / 256);
		}
		_haze.nearDistance = 1.0f / nearInverse;
		_haze.farDistance = 1.0f / farInverse;
	}

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
	if (flash != 0)
	{
		// 0x869C25..0x869CB6: per channel c + (((0xFF - c) * flash) >> 8), alpha 0xFF
		for (auto& c : _table)
		{
			c = Lerp(c, 0xFFFFFFFFu, flash) | 0xFF000000u;
		}
	}
	// the original's single global table at 0xEDD90C (and its base [0xFA26A4] and haze): a copy of the last one built
	if (auto& current = CurrentStorage(); &current != this)
	{
		current._table = _table;
		current._base = _base;
		current._haze = _haze;
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

const LandLightTable& LandLightTable::Current() noexcept
{
	return CurrentStorage();
}

LandLightTable& LandLightTable::CurrentStorage() noexcept
{
	static LandLightTable table = [] {
		LandLightTable t;
		t._table.fill(0xFFFFFFFFu);
		return t;
	}();
	return table;
}

glm::vec3 LandLightTable::GetBaseColour() const noexcept
{
	return glm::vec3((_base >> 16) & 0xFFu, (_base >> 8) & 0xFFu, _base & 0xFFu) / 255.0f;
}

} // namespace openblack
