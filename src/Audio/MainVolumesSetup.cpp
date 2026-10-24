/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MainVolumesSetup.h"

#include "Common/MachineSetup.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

namespace openblack::audio::main_volumes
{

std::optional<std::filesystem::path> Folder()
{
	if (!Locator::playerProfileSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::playerProfileSystem::value().ProfilesFolder();
}

void Read()
{
	if (!Locator::filesystem::has_value() || !Locator::config::has_value())
	{
		return;
	}
	const auto setup = machine_setup::Load(Locator::filesystem::value(), Folder());
	auto& config = Locator::config::value();
	machine_setup::ApplyVolumes(setup, config.audioSampleMainVolume, config.audioMusicMainVolume);
}

bool Keep()
{
	if (!Locator::filesystem::has_value() || !Locator::config::has_value())
	{
		return false;
	}
	const auto& config = Locator::config::value();
	return machine_setup::Save(Locator::filesystem::value(), Folder(),
	                           machine_setup::FromVolumes(config.audioSampleMainVolume, config.audioMusicMainVolume));
}

} // namespace openblack::audio::main_volumes
