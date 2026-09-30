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
#include <unordered_map>
#include <vector>

namespace openblack::resources
{

/// Mod graphics.hd-tweaks: pack textures replaced by bigger images from the mod's folder. Its textures.cfg lists
/// `<texture id (hex)> = <FNV-1a of the pack's DDS data>`, and the image is textures/<id>.png. The hash ties each image
/// to the texture it was made from, so a different AllMeshes.g3d (another mod pack) keeps its own textures.
class HdTextures
{
public:
	HdTextures() = default;
	explicit HdTextures(const std::filesystem::path& modDirectory);

	/// The image that replaces this pack texture, or an empty path.
	[[nodiscard]] std::filesystem::path Find(uint32_t id, const std::vector<uint8_t>& ddsData) const;

	/// Where the image of this texture id is (it may not exist)
	[[nodiscard]] std::filesystem::path ImagePath(uint32_t id) const;

	/// The pack textures this list replaces (the villagers' ones)
	[[nodiscard]] std::vector<uint32_t> Ids() const;

	[[nodiscard]] static uint32_t Hash(const std::vector<uint8_t>& data) noexcept;

private:
	std::filesystem::path _directory;
	std::unordered_map<uint32_t, uint32_t> _sourceHashes;
};

} // namespace openblack::resources
