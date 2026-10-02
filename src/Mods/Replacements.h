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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

namespace openblack::mods
{
class ModRegistry;
}

namespace openblack::mods::replace
{

/// What the active mods replace, from the "replace" object of their mod.json (mod-library.md, "Reemplazar"):
///
///   "replace": {
///     "meshes":   { "AnimalBat1": "meshes/bat.l3d" },          a mesh of AllMeshes.g3d by name (k_MeshNames) or "#12"
///     "textures": { "pack:47": "textures/47.png",               a texture of AllMeshes.g3d by its id (hex)
///                   "raw:ATMOS": "textures/atmos.png" },        a Data/Textures/*.raw by name (png or raw)
///     "objects":  { "feature": { "Rock1": { "woodValue": 5, "meshId": "AnimalBat1" } } }
///                 or "objects": "data/objects.json" (the same object in a file)
///   }
///
/// plus the folder replace/ of the mod: files there replace the game's files with the same path (Data/..., Scripts/...).
/// With two mods replacing the same thing, the later in load order wins (the log says so).
/// Paths are relative to the mod's folder.

/// Reads the "replace" of every active mod (call after ModRegistry::ApplyAll, before the game data loads)
void Collect(const ModRegistry& registry);
/// Forget everything (tests)
void Clear();

/// The file to load instead of mesh `index` of AllMeshes.g3d (its name in k_MeshNames)
[[nodiscard]] std::optional<std::filesystem::path> Mesh(size_t index);
/// The image to load instead of the AllMeshes.g3d texture with this id
[[nodiscard]] std::optional<std::filesystem::path> PackTexture(uint32_t id);
/// The file to load instead of Data/Textures/<stem>.raw (a .png or a .raw); case does not matter
[[nodiscard]] std::optional<std::filesystem::path> RawTexture(std::string_view stem);
/// The names of the raw textures replaced (some may not exist in the game: they are added)
[[nodiscard]] std::vector<std::string> RawTextureNames();
/// The replace/ folders of the active mods, in load order (mounted after the data mods)
[[nodiscard]] const std::vector<std::filesystem::path>& Folders();

/// Some active mod changes info.dat objects
[[nodiscard]] bool HasObjectPatches();
/// Applies the "objects" of the active mods to info.dat (before it is published, Game.cpp). Returns how many fields
/// changed
size_t PatchObjects(InfoConstants& info);

/// The tables of info.dat "objects" can change, and the fields of an object (for the wiki and the tests)
[[nodiscard]] std::vector<std::string_view> ObjectTables();
[[nodiscard]] std::vector<std::string_view> ObjectFields();

} // namespace openblack::mods::replace
