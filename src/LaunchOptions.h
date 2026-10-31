/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "EngineConfig.h"
#include "Windowing/WindowingInterface.h"

// What the command line's names choose: the graphics interface and the window's mode. Pure: values only.
namespace openblack::launch_options
{

/// The graphics interface drawn through when the command line names none
[[nodiscard]] constexpr GraphicsBackend DefaultBackend()
{
#ifdef _APPLE_
	return GraphicsBackend::Metal;
#else
	return GraphicsBackend::Vulkan;
#endif
}

/// The graphics interface of a name the command line accepts (k_GraphicsBackendStringLookup), none for another
[[nodiscard]] constexpr std::optional<GraphicsBackend> BackendNamed(std::string_view name)
{
	const auto found =
	    std::ranges::find(k_GraphicsBackendStringLookup, name, &std::pair<std::string_view, GraphicsBackend>::first);
	return found != k_GraphicsBackendStringLookup.cend() ? std::optional(found->second) : std::nullopt;
}

/// The window modes by their command line names
inline constexpr std::array<std::pair<std::string_view, windowing::DisplayMode>, 3> k_DisplayModes {{
    {"windowed", windowing::DisplayMode::Windowed},
    {"fullscreen", windowing::DisplayMode::Fullscreen},
    {"borderless", windowing::DisplayMode::Borderless},
}};

/// The window mode of a name, none for another
[[nodiscard]] constexpr std::optional<windowing::DisplayMode> DisplayModeNamed(std::string_view name)
{
	const auto found = std::ranges::find(k_DisplayModes, name, &std::pair<std::string_view, windowing::DisplayMode>::first);
	return found != k_DisplayModes.cend() ? std::optional(found->second) : std::nullopt;
}

} // namespace openblack::launch_options
