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

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/ext/vector_uint2_sized.hpp>

namespace openblack
{

/// The sky's clouds (CloudInSky::Open 0x5E23F0, per frame fn_005E25C0, "Clouds" detail key): 70 mist puffs, plus
/// two huge ones pinned at the horizon, drifting with the wind and fading at the ends of their track.
class Clouds
{
public:
	struct Cloud
	{
		glm::vec3 local; ///< position along the wind track (x in -8000..8000), before the rotation to the world
		float size;      ///< +0x88
		float k;         ///< +0x8C: edge-on shrink factor
		bool pinned;
	};

	Clouds();

	/// Moves the clouds by the game time step (ms); nothing moves while the game is paused
	void Update(float milliseconds);

	[[nodiscard]] const std::vector<Cloud>& GetClouds() const noexcept { return _clouds; }
	/// World position of a cloud: its track rotated by the wind angle (3 pi / 4) about the island centre (1280, 1280)
	[[nodiscard]] static glm::vec3 WorldPosition(const Cloud& cloud);
	/// Edge alpha 0..255: fades in over the first 2000 units of the track and out over the last 2000
	[[nodiscard]] static float EdgeAlpha(const Cloud& cloud);
	/// Animation frame 0..15 of the mist texture atlas
	[[nodiscard]] int GetFrame() const noexcept { return (_counter / 20) & 15; }

	/// Cloud shadows ("CloudShadows" key, fn_0086CFF0 -> fn_0086D360 / fn_00878C70): every cloud stamps
	/// Data\Textures\sclouds.raw (40 x 40, one texel per 10-unit cell, placed by its top-left corner at the cloud's
	/// cell) into a luminosity cap, cap = min(cap, max(48, 255 - (255 - s) * alpha / 255)); the landscape and the models
	/// use min(cell luminosity, cap).
	/// @param origin world x/z of the map's first cell, @param size map size in cells, @param alpha per cloud 0..255
	void BuildShadowCap(const std::vector<uint8_t>& shadowImage, glm::vec2 origin, glm::u16vec2 size,
	                    const std::vector<float>& alpha, std::vector<uint8_t>& cap) const;

private:
	std::vector<Cloud> _clouds;
	int _counter {0};
	float _counterRemainder {0.0f};
};

} // namespace openblack
