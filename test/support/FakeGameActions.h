/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The game's actions as a test sets them: the keys held, the mouse, its delta and its buttons, and the hands' places

#include <cstdint>

#include <array>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Input/GameActionMapInterface.h"
#include "Input/MouseButtons.h"

namespace openblack::test
{
/// The keys and buttons held, the mouse and the hands, as the test sets them; nothing changes or repeats by itself
class FakeGameActions final: public input::GameActionInterface
{
public:
	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const final
	{
		return (held & static_cast<uint64_t>(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindable(input::UnbindableActionMap action) const final
	{
		return (heldUnbindable & static_cast<uint8_t>(action)) != 0;
	}
	[[nodiscard]] bool GetBindableChanged(input::BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableChanged(input::UnbindableActionMap) const final { return false; }
	[[nodiscard]] bool GetBindableRepeat(input::BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(input::UnbindableActionMap) const final { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const final { return mouse; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const final { return mouseDelta; }
	[[nodiscard]] float GetMouseWheelDelta() const final { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final { return hands; }
	[[nodiscard]] bool IsMouseButtonHeld(input::MouseButton button) const final
	{
		return (buttons & (1u << static_cast<uint8_t>(button))) != 0;
	}
	void Frame() final {}
	void ProcessEvent(const SDL_Event&) final {}

	/// The bindable actions held (their bits)
	uint64_t held {0};
	/// The unbindable actions held (their bits)
	uint8_t heldUnbindable {0};
	glm::uvec2 mouse {0, 0};
	glm::ivec2 mouseDelta {0, 0};
	/// The mouse buttons held, one bit each (1 << MouseButton)
	uint8_t buttons {0};
	std::array<std::optional<glm::vec3>, 2> hands {};
};
} // namespace openblack::test
