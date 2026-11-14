/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <SDL_keyboard.h>

namespace openblack::input
{
/// The keys down the game takes in one frame, in the order they came: a key down with the same key and the same Shift,
/// Ctrl and Alt as the last one taken this frame is not taken again (so two of a held key's repeats in one frame act
/// once), and no more than k_Capacity are taken a frame. Cleared at the start of each frame
class KeyBuffer
{
public:
	static constexpr std::size_t k_Capacity = 128;

	/// Shift, Ctrl and Alt, the key of either side, one bit each; no other modifier counts
	[[nodiscard]] static constexpr uint8_t ModifiersOf(uint16_t sdlModifiers) noexcept
	{
		return static_cast<uint8_t>(((sdlModifiers & KMOD_SHIFT) != 0 ? 1U : 0U) | ((sdlModifiers & KMOD_CTRL) != 0 ? 2U : 0U) |
		                            ((sdlModifiers & KMOD_ALT) != 0 ? 4U : 0U));
	}

	/// Whether the key down is taken, and then kept as the frame's last
	[[nodiscard]] bool Take(SDL_Scancode key, uint16_t sdlModifiers) noexcept
	{
		const Key down {.key = key, .modifiers = ModifiersOf(sdlModifiers)};
		if (_last.has_value() && *_last == down)
		{
			return false;
		}
		if (_count >= k_Capacity)
		{
			return false;
		}
		++_count;
		_last = down;
		return true;
	}

	void Clear() noexcept
	{
		_count = 0;
		_last.reset();
	}

	[[nodiscard]] std::size_t Count() const noexcept { return _count; }

private:
	struct Key
	{
		SDL_Scancode key;
		uint8_t modifiers;
		bool operator==(const Key&) const = default;
	};
	std::size_t _count = 0;
	std::optional<Key> _last;
};
} // namespace openblack::input
