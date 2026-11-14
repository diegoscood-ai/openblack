/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleSpellFile.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "Common/ProcessLogOnce.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::particles;
using namespace openblack::psys;

std::optional<File> File::Parse(std::string_view text, std::string name)
{
	auto parsed = ParticleFile::Parse(text);
	if (!parsed.has_value())
	{
		return std::nullopt;
	}
	return File {std::move(*parsed), std::move(name)};
}

std::shared_ptr<const File> File::Load(const std::string& name)
{
	// (openblack guard) without resources (the unit tests) the file is read uncached; with no file system either it
	// cannot be read: none, with one log line, rather than a null dereference
	if (!Locator::resources::has_value() && !Locator::filesystem::has_value())
	{
		if (auto logger = spdlog::get("game"); logger != nullptr && openblack::log_once::First("game/psys-file-no-filesystem"))
		{
			SPDLOG_LOGGER_WARN(logger, "PSys: {} cannot be read: no file system", name);
		}
		return nullptr;
	}
	if (!Locator::resources::has_value())
	{
		return resources::ParticleFileLoader {}(resources::ParticleFileLoader::FromDiskTag {}, name);
	}
	// once into the PSys file cache; a file that cannot be read is cached empty, so it is tried once. Only the main
	// thread loads effects
	auto& files = Locator::resources::value().GetParticleFiles();
	const auto id = entt::hashed_string(("psys/" + name).c_str()).value();
	if (!files.Contains(id))
	{
		files.Load(id, resources::ParticleFileLoader::FromDiskTag {}, name);
	}
	return files.Handle(id).handle();
}
