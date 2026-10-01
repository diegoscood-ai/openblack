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
#include <vector>

#include <glm/vec3.hpp>

namespace openblack
{

/// The original's landscape light table (0xEDD90C, built every frame by fn_00869850): 256 colours indexed by the cell
/// luminosity, taken from Data\WeatherSystem\palette.raw (32x32 RGBA; rows 0..2 = good / neutral / evil land colour by
/// time of day, rows 3..7 = colours by alignment). The land vertex diffuse is table[luminosity], the reflected land
/// uses half of it and the sea overlay uses table[255].
class LandLightTable
{
public:
	static constexpr size_t k_Size = 256;

	/// @param palette the 4096 bytes of palette.raw
	bool Load(const std::vector<uint8_t>& palette) noexcept;

	/// @param skyType openblack's sky type (0 night .. 2 day; the original's Time2SkyType is 2 - skyType)
	/// @param alignment -1 evil .. 1 good (the original's X = 1 - alignment: 0 good, 2 evil)
	/// @param weather overcast amount 0..1 (caps the base colour at 255 - 96 * weather)
	void Build(float skyType, float alignment, float weather) noexcept;

	[[nodiscard]] bool IsLoaded() const noexcept { return !_palette.empty(); }
	/// RGBA8 texels of the current table
	[[nodiscard]] const std::array<uint32_t, k_Size>& GetTexels() const noexcept { return _texels; }
	[[nodiscard]] glm::vec3 GetColour(size_t index) const noexcept;
	/// table[index] as the original's D3DCOLOR 0xAARRGGBB (the clouds read table[255], [0xEDDD08], as integers)
	[[nodiscard]] uint32_t GetRaw(size_t index) const noexcept { return _table[index]; }
	/// The base colour of this frame ([0xFA26A4], after the overcast cap), 0..1
	[[nodiscard]] glm::vec3 GetBaseColour() const noexcept;

	/// Software distance haze of this frame (fn_00869850 0x869CB8..0x869F78 -> fn_007FEAA0 / fn_007FEAD0): at view
	/// depth z, t = clamp((z - near) / (far - near), 0, 1); the diffuse is scaled by (256 - trunc((256 - k) * t)) / 256
	/// and colour * t (0..255) is added to the specular.
	struct Haze
	{
		float nearDistance {400.0f};
		float farDistance {900.0f};
		float k {256.0f};
		glm::vec3 colour {0.0f}; ///< 0..255
	};
	[[nodiscard]] const Haze& GetHaze() const noexcept { return _haze; }
	/// Palette row 5 at the alignment column ([0xFA26DC]): the moon's colour, 0..1
	[[nodiscard]] glm::vec3 GetMoonColour() const noexcept { return _moonColour; }

	/// The base colour [0xFA26A4] (0xAARRGGBB) and the haze of the last Build of any table (the renderer's, once a
	/// frame), for the code outside the renderer that reads them: the PSys mists (RenderParticleMist::DrawAt 0x67A6C7)
	/// and the storm puffs (GWeather::DrawClouds 0x83FF56). White and the default haze before the first Build.
	[[nodiscard]] static uint32_t LastBuiltBase() noexcept;
	[[nodiscard]] static Haze LastBuiltHaze() noexcept;

private:
	static uint32_t s_lastBase;
	static Haze s_lastHaze;
	std::vector<uint32_t> _palette;     ///< 0xAARRGGBB, like the D3DCOLORs of the original
	std::array<uint32_t, k_Size> _table {}; ///< 0xAARRGGBB
	uint32_t _base {0xFFFFFFFFu};          ///< 0xAARRGGBB
	std::array<uint32_t, k_Size> _texels {}; ///< same colours as little-endian RGBA8
	Haze _haze;
	glm::vec3 _moonColour {1.0f};
};

} // namespace openblack
