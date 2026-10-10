/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedPointer.h"

#include <cstdio>

#include <algorithm>

#include <SDL_mouse.h>
#include <fmt/format.h>
#include <glm/common.hpp>

using namespace openblack;
using namespace openblack::testbed_pointer;

glm::vec2 testbed_pointer::FractionOf(glm::ivec2 pixel, glm::ivec2 windowSize)
{
	return (glm::vec2(pixel) + 0.5f) / glm::vec2(windowSize);
}

glm::ivec2 testbed_pointer::PixelOf(glm::vec2 fraction, glm::ivec2 windowSize)
{
	return glm::ivec2(glm::vec2(windowSize) * fraction);
}

std::string testbed_pointer::MouseAtText(glm::vec2 fraction)
{
	// Nine significant digits read back as the same float
	return fmt::format("{:.9g},{:.9g}", fraction.x, fraction.y);
}

std::optional<glm::vec2> testbed_pointer::ReadMouseAt(const char* text)
{
	if (text == nullptr)
	{
		return std::nullopt;
	}
	glm::vec2 fraction(0.5f);
	// NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion): read as the game reads the fixed cursor
	if (std::sscanf(text, "%f,%f", &fraction.x, &fraction.y) != 2)
	{
		return std::nullopt;
	}
	return fraction;
}

glm::ivec2 testbed_pointer::PixelAtShare(glm::vec2 share, glm::ivec2 windowSize)
{
	return glm::ivec2(glm::round(share * glm::vec2(windowSize)));
}

glm::ivec2 testbed_pointer::ClampToWindow(glm::ivec2 pixel, glm::ivec2 windowSize)
{
	return glm::clamp(pixel, glm::ivec2(0), glm::max(windowSize - 1, glm::ivec2(0)));
}

input::MouseButton testbed_pointer::ButtonOf(size_t number)
{
	switch (number)
	{
	case 1:
		return input::MouseButton::Left;
	case 2:
		return input::MouseButton::Middle;
	case 3:
		return input::MouseButton::Right;
	default:
		return input::MouseButton::Other;
	}
}

input::HandButtons testbed_pointer::HandButtonsOf(const input::MouseButtonsState& buttons)
{
	return {.gripping = input::HandGripping(buttons), .action = input::HandAction(buttons)};
}

Sweep testbed_pointer::StartSweep(glm::vec2 share, float seconds, glm::ivec2 windowSize)
{
	const auto pixels = share * glm::vec2(windowSize);
	if (seconds <= 0.0f)
	{
		return {.pixelsPerSecond = pixels, .secondsLeft = 0.0f};
	}
	return {.pixelsPerSecond = pixels / seconds, .secondsLeft = seconds};
}

SweepStep testbed_pointer::Advance(Sweep& sweep, float seconds)
{
	// A sweep over no time holds its whole move as its speed
	const bool instant = sweep.secondsLeft <= 0.0f;
	const auto step = instant ? 0.0f : std::min(seconds, sweep.secondsLeft);
	const auto exact = (instant ? sweep.pixelsPerSecond : sweep.pixelsPerSecond * step) + sweep.remainder;
	const auto moved = glm::ivec2(exact);
	sweep.remainder = exact - glm::vec2(moved);
	sweep.secondsLeft -= step;
	return {.moved = moved, .done = instant || sweep.secondsLeft <= 0.0f};
}

std::optional<std::string> testbed_pointer::SaveMouseAt(const char* current)
{
	if (current == nullptr)
	{
		return std::nullopt;
	}
	return std::string(current);
}

std::string testbed_pointer::RestoredMouseAt(const std::optional<std::string>& saved)
{
	// An empty fixed cursor is the same as none
	return saved.value_or(std::string {});
}

void Driver::Take()
{
	if (_taken)
	{
		return;
	}
	_taken = true;
	_savedMouseAt = SaveMouseAt(input::MouseAt());
	_position = input::GameCursor();
	_buttons = {};
}

void Driver::MoveTo(glm::ivec2 pixel, glm::ivec2 windowSize)
{
	Take();
	_position = pixel;
	if (windowSize.x > 0 && windowSize.y > 0)
	{
		input::OverrideMouseAt(MouseAtText(FractionOf(pixel, windowSize)));
	}
}

void Driver::Press(input::MouseButton button, bool down)
{
	Take();
	input::ApplyMouseButton(_buttons, {.button = button, .down = down, .position = _position});
}

void Driver::WriteButtons() const
{
	if (_taken)
	{
		input::HandButtonsRef() = HandButtonsOf(_buttons);
	}
}

void Driver::Release()
{
	if (!_taken)
	{
		return;
	}
	input::OverrideMouseAt(RestoredMouseAt(_savedMouseAt));
	// The hand lets go of what the scenario held; the mouse's own buttons come back with its next press or release
	input::HandButtonsRef() = {};
	_taken = false;
	_savedMouseAt.reset();
	_buttons = {};
}

uint32_t Driver::ButtonMask() const
{
	const auto mask = [](bool held, int bit) { return held ? static_cast<uint32_t>(bit) : 0u; };
	return mask(_buttons.left, SDL_BUTTON_LMASK) | mask(_buttons.middle, SDL_BUTTON_MMASK) |
	       mask(_buttons.right, SDL_BUTTON_RMASK);
}
