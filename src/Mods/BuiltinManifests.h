/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <string_view>

namespace openblack::mods::builtin
{

/// A mod.json that comes with openblack: assets/mods/<folder>/mod.json, compiled in by CMake
/// (src/CMakeLists.txt writes include/generated/mods/BuiltinManifests.cpp at configure time), so the mods that ship with
/// openblack are there even when Mods/ has only their settings.cfg. A mod.json on disk with the same id replaces it.
struct Manifest
{
	std::string_view folder;
	std::string_view json;
};

[[nodiscard]] std::span<const Manifest> Manifests();

} // namespace openblack::mods::builtin
