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

#include "3D/FrameAnim.h"

namespace openblack
{

/// The sky's alignment [0xBF3378] (0 good .. 1 neutral .. 2 evil in the original, kept here in openblack's units:
/// alignment = 1 - X, so -1 evil .. 1 good). GLandAlignement::DrawSky 0x5E2160 moves it every frame towards the
/// target [0xBF337C] by g_game_time_inc * 0.01 * 0.1 (0.001 per ms, good to evil in 2 s) and snaps it when it passes;
/// the clouds (fn_005E1DE0), the land light table and the sky all read the moved value. It starts neutral and a new
/// land does not reset it (GLandAlignement::Open 0x5E1D10 only reads it).
class SkyAlignment
{
public:
	/// @param target -1 evil .. 1 good, @param milliseconds the game time step (0 while paused)
	void Update(float target, float milliseconds) noexcept;
	[[nodiscard]] float Get() const noexcept { return _value; }

private:
	float _value {0.0f};
};

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
		int counter {0};                ///< +0x84: the mist's own animation counter 0..900 (fn_007FA300)
		float counterRemainder {0.0f};
	};

	/// CloudInSky::Open, run by GLandAlignement::Open on every GLandscape::Open: a new layout for every land
	Clouds();

	/// GLandscape::Open: the next Renderer::UpdateClouds builds a new layout
	static void OnLandscapeOpened() noexcept;
	/// Bumped by OnLandscapeOpened
	[[nodiscard]] static uint32_t GetLandscapeGeneration() noexcept;

	/// The target of the sky's alignment (fn_0064AC30, from GPlayer::ProcessPlayers 0x64A697, fn_005E2240 stores
	/// (1 - clamp((v + 1) / 2, 0, 1)) * 2): v = GetAlignmentValue of CalculateMostInfluentialPlayer at the interface
	/// position (GInterface+0x39C -> +0xB0), the value GAlignment::Update 0x414410 keeps for every player; -1 evil .. 1
	/// good: ecs::effects::alignment::GetInterfaceAlignment() x 2 - 1 (unless the test hook or the debug "Sky
	/// alignment" slider moved off 0 say otherwise).
	[[nodiscard]] static float InfluentialPlayerAlignment() noexcept;
	/// The overcast amount at the camera, 0..1 (GWeather / LH3DAtmos: fn_00869850 caps the light table's base colour at
	/// 255 - 96 * overcast, and so the clouds through table[255]): GCamera::Update 0x4426BA, the overcast byte of
	/// weather::atmos::GetWeatherSmooth(camera) x 0.01 (it can pass 1: a byte of up to 127); 0 with a clear sky (a storm
	/// of the weather miracle gives 0.8).
	[[nodiscard]] static float WeatherOvercastAtCamera() noexcept;

	/// fn_005E1DE0 -> [0xBF3398]: the clouds' 0xAARRGGBB. Integer lerp of good 0x00FFFFFF / neutral 0xC8FFFFFF / evil
	/// 0xFFAAA066 (0xBF339C) by the sky alignment X (i = trunc(X), f = trunc((X - i) * 256)), each channel times light
	/// table[255] (c * t >> 8), then c + floor((8960 - 70 c) / 256) (255 -> 220); the alpha byte is the lerped one.
	/// @param alignment -1 evil .. 1 good (X = 1 - alignment), @param table255 [0xEDDD08] as 0xAARRGGBB
	[[nodiscard]] static uint32_t Colour(float alignment, uint32_t table255) noexcept;

	/// Moves the clouds by the game time step (ms); nothing moves while the game is paused
	void Update(float milliseconds);
	/// fn_007FA300 (the mist's Draw): the cloud's own animation counter += ftol(g_game_time_inc * 0.255), modulo 900.
	/// Only a cloud that LH3DMist::AddDrawing 0x7FA7F0 sends to the Z-sorter (its sphere touches the screen) is drawn,
	/// so only its counter advances, and the clouds drift out of step with each other.
	void AdvanceAnimation(size_t index, float milliseconds);

	[[nodiscard]] const std::vector<Cloud>& GetClouds() const noexcept { return _clouds; }
	/// World position of a cloud: its track rotated by the wind angle (3 pi / 4) about the island centre (1280, 1280)
	[[nodiscard]] static glm::vec3 WorldPosition(const Cloud& cloud);
	/// Edge alpha 0..255 (fistp, rounded): fades in over the first 2000 units of the track and out over the last 2000
	[[nodiscard]] static int EdgeAlpha(const Cloud& cloud);
	/// Animation frame 0..15 of the mist texture atlas: counter * 45 / 900 in integers, & 15. A whole cell each time:
	/// the original sets one UV offset (vt+0xE8, 0x7F9B70) and draws once, so frames switch without a blend
	[[nodiscard]] static int GetFrame(const Cloud& cloud) noexcept { return graphics::frame_anim::MistCell(cloud.counter); }

	/// Cloud shadows ("CloudShadows" key, fn_0086CFF0 -> fn_0086D360 / fn_00878C70): every cloud stamps
	/// Data\Textures\sclouds.raw (40 x 40, one texel per 10-unit cell, placed by its top-left corner at the cloud's
	/// cell) into a luminosity cap, cap = min(cap, max(48, 255 - (255 - s) * alpha / 255)); the landscape and the models
	/// use min(cell luminosity, cap).
	/// @param origin world x/z of the map's first cell, @param size map size in cells, @param alpha per cloud 0..255
	void BuildShadowCap(const std::vector<uint8_t>& shadowImage, glm::vec2 origin, glm::u16vec2 size,
	                    const std::vector<float>& alpha, std::vector<uint8_t>& cap) const;

private:
	std::vector<Cloud> _clouds;
};

} // namespace openblack
