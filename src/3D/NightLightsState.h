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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"
#include "3D/NightLights.h"
#include "Graphics/GraphicsHandle.h"

namespace openblack::night_lights
{
/// The cell luminosities of the island with the cap layout (0 where there is no block: the original skips those)
struct LuminosityCache
{
	glm::ivec2 firstCell {0};
	glm::ivec2 size {0};
	std::vector<uint8_t> values;
};

/// A village light: a street lantern (type 0) or a campfire (type 1)
struct VillageLight
{
	entt::entity owner {entt::null}; ///< the lantern (or the gate's lamp) it was made for
	glm::vec3 position {0.0f};
	int type {0};
	float timer {0.0f};
	glm::vec2 offset {0.0f};
	std::array<entt::entity, 3> sprites {entt::null, entt::null, entt::null}; // 2 flames, 1 glow
	std::array<float, 2> flameSize {1.0f, 1.0f};
	float glowSize {3.0f};
};

/// A new village light, made when its lantern is created, whether it is dark or not. Six draws, in this order: its
/// flicker clock Random(0, 30), then for each of its three sprites (two flames, the glow) the flame's size
/// Random(-0.1, 0.1) + 1 (at least 1e-4; flames only) and the sprite's start Random(0, 31), written into the start
/// table every light shares. Its sprites are made by the next update.
[[nodiscard]] VillageLight MakeLight(entt::entity owner, const glm::vec3& position, int type,
                                     graphics::frame_anim::LanternStarts& starts, const Draw& random);

/// The lights' flicker, newest first (from the end of the list), only while it is dark. Each light's clock takes the
/// frame's whole milliseconds; past 30 it wraps and draws three numbers: the light's x and z jitter Random(-0.5, 0.5)
/// and its glow's size Random(-0.1, 0.1) + 3, at least 1e-4. While it is light the clocks stand still and nothing is
/// drawn.
void Flicker(std::vector<VillageLight>& lights, bool dark, uint32_t wholeMs, const Draw& random);

/// The night lights' state (ecs::systems::VillageLightSystemInterface owns it): their images and textures, the village
/// lights, their clocks and the cached cell luminosities
struct State
{
	bool loaded {false};
	night_lights::LightImage hand;    // light_hand.raw
	night_lights::LightImage village; // village_diffuse.raw
	graphics::TextureHandle fire {};  // S_Firea
	graphics::TextureHandle glow {};  // smokea
	std::vector<VillageLight> lights;
	float rescanMs {0.0f};
	int flameMs {0}; // one clock for every light (frame_anim::LanternAdvance)
	// the start of each sprite's cells, one table for every light, rewritten by every new light
	// (frame_anim::LanternStarts); kept from land to land as the original's global
	graphics::frame_anim::LanternStarts flameStarts {graphics::frame_anim::k_LanternFileStarts};
	LuminosityCache luminosity;
};
} // namespace openblack::night_lights
