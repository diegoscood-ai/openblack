/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HdTextures.h"

#include <fstream>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace openblack::resources
{

HdTextures::HdTextures(const std::filesystem::path& modDirectory)
    : _directory(modDirectory / "textures")
{
	std::ifstream file(modDirectory / "textures.cfg");
	if (!file)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "HD textures: no {}", (modDirectory / "textures.cfg").string());
		return;
	}
	std::string line;
	while (std::getline(file, line))
	{
		line = line.substr(0, line.find('#'));
		const auto equals = line.find('=');
		if (equals == std::string::npos)
		{
			continue;
		}
		try
		{
			const auto id = static_cast<uint32_t>(std::stoul(line.substr(0, equals), nullptr, 16));
			const auto hash = static_cast<uint32_t>(std::stoul(line.substr(equals + 1), nullptr, 16));
			_sourceHashes[id] = hash;
		}
		catch (const std::exception&)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "HD textures: bad line in textures.cfg: {}", line);
		}
	}
}

std::filesystem::path HdTextures::Find(uint32_t id, const std::vector<uint8_t>& ddsData) const
{
	const auto entry = _sourceHashes.find(id);
	if (entry == _sourceHashes.end())
	{
		return {};
	}
	const auto image = ImagePath(id);
	if (Hash(ddsData) != entry->second)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "HD textures: {:#x} was made from another texture, not used", id);
		return {};
	}
	if (!std::filesystem::exists(image))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "HD textures: missing {}", image.string());
		return {};
	}
	return image;
}

std::filesystem::path HdTextures::ImagePath(uint32_t id) const
{
	return _directory / fmt::format("{:x}.png", id);
}

std::vector<uint32_t> HdTextures::Ids() const
{
	std::vector<uint32_t> ids;
	ids.reserve(_sourceHashes.size());
	for (const auto& [id, hash] : _sourceHashes)
	{
		ids.push_back(id);
	}
	return ids;
}

uint32_t HdTextures::Hash(const std::vector<uint8_t>& data) noexcept
{
	uint32_t hash = 0x811C9DC5u;
	for (const auto byte : data)
	{
		hash = (hash ^ byte) * 0x01000193u;
	}
	return hash;
}

} // namespace openblack::resources
