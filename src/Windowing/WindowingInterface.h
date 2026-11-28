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

#include <glm/vec2.hpp>

namespace openblack::windowing
{

enum class DisplayMode : std::uint8_t
{
	Windowed,
	Fullscreen,
	Borderless
};

class WindowingInterface
{
public:
	virtual ~WindowingInterface() = default;

	struct NativeHandles
	{
		void* nativeWindow;
		void* nativeDisplay;
	};

	[[nodiscard]] virtual void* GetHandle() const = 0;
	[[nodiscard]] virtual NativeHandles GetNativeHandles() const = 0;
	[[nodiscard]] virtual uint32_t GetID() const = 0;
	[[nodiscard]] virtual glm::ivec2 GetSize() const = 0;
	[[nodiscard]] virtual float GetAspectRatio() const = 0;
	[[nodiscard]] virtual bool IsMinimised() const = 0;
	/// The window is the active one, the one the keyboard and the mouse go to
	[[nodiscard]] virtual bool HasInputFocus() const { return true; }

	virtual WindowingInterface& SetDisplayMode(DisplayMode mode) = 0;
};

} // namespace openblack::windowing
