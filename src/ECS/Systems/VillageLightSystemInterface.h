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

#include <chrono>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::night_lights
{
struct State;
} // namespace openblack::night_lights

namespace openblack::ecs::systems
{
/// The lights villages keep at night: the hand light, the street lanterns and the campfires, their flames and glows
/// (Locator::villageLightSystem)
class VillageLightSystemInterface
{
public:
	virtual ~VillageLightSystemInterface() = default;

	/// Lights the land under the hand and the village lights, flickers them and plays their flames by the game time
	/// that has passed, which is 0 while the game is paused. They only light and show while the land is dark enough
	/// for them. The renderer calls it once a frame, once the frame's land light has been built and stamped.
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// A street lantern or one of the Norse Gate's lamps was made: its light, made at once, dark or not, with its six
	/// draws of the C runtime's stream (night_lights::MakeLight). The lights already made are left as they are.
	/// type 0 = a town light, 1 = a country lantern.
	virtual void AddLight(entt::entity lantern, const glm::vec3& position, uint8_t type) = 0;
	/// Whether the land was dark enough for the lights at the last update; the street lanterns crackle while it is
	[[nodiscard]] virtual bool IsDark() const = 0;

	/// The night lights' images, textures, lights, clocks and cell luminosities
	[[nodiscard]] virtual openblack::night_lights::State& GetState() noexcept = 0;
};
} // namespace openblack::ecs::systems
