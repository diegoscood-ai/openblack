/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// A window for the tests: a size and its aspect, no handle, never minimised

#include <cstdint>

#include <glm/vec2.hpp>

#include "Windowing/WindowingInterface.h"

namespace openblack::test
{
/// A window of the size the test gives (640 x 480 unless told otherwise)
class FakeWindowing final: public windowing::WindowingInterface
{
public:
	explicit FakeWindowing(glm::ivec2 size = {640, 480})
	    : _size(size)
	{
	}

	[[nodiscard]] void* GetHandle() const final { return nullptr; }
	[[nodiscard]] NativeHandles GetNativeHandles() const final { return {}; }
	[[nodiscard]] uint32_t GetID() const final { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const final { return _size; }
	[[nodiscard]] float GetAspectRatio() const final { return static_cast<float>(_size.x) / static_cast<float>(_size.y); }
	[[nodiscard]] bool IsMinimised() const final { return false; }
	WindowingInterface& SetDisplayMode(windowing::DisplayMode) final { return *this; }

private:
	glm::ivec2 _size;
};
} // namespace openblack::test
