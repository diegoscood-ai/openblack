/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundAction.h"

#include <map>
#include <memory>

#include <EnumHeader.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::psys;

namespace
{
const EnumNames& Loaded()
{
	bool loadedNow = false;
	const auto& names = EnumHeaderNames("SoundAction.h", &loadedNow);
	if (loadedNow)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "PSys: {} sound actions", names.byName.size());
	}
	return names;
}
} // namespace

std::shared_ptr<EnumNames> openblack::psys::ReadEnumHeader(const std::string& file)
{
	auto names = std::make_shared<EnumNames>();
	auto& fileSystem = openblack::Locator::filesystem::value();
	try
	{
		const auto& bytes = openblack::resources::LoadBlob(openblack::Locator::resources::value().GetBlobs(),
		                                                   fileSystem.GetPath<openblack::filesystem::Path::Data>() / file);
		const std::string text(bytes.begin(), bytes.end());
		names->byName = ParseEnumHeader(text);
		// each value's first name, in name order: in Data\SoundAction.h, the one read by value, every value has one name
		for (const auto& [name, value] : names->byName)
		{
			names->byValue.try_emplace(value, name);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: cannot read Data\\{}: {}", file, e.what());
	}
	return names;
}

const EnumNames& openblack::psys::EnumHeaderNames(const std::string& file, bool* loadedNow)
{
	// without a file system (some unit tests) the table is empty
	static const EnumNames k_Empty;
	if (!openblack::Locator::filesystem::has_value() || !openblack::Locator::resources::has_value())
	{
		return k_Empty;
	}
	auto& headers = openblack::Locator::resources::value().GetEnumHeaders();
	const auto id = entt::hashed_string(("psys/enum/" + file).c_str()).value();
	if (!headers.Contains(id))
	{
		headers.Load(id, openblack::resources::EnumHeaderLoader::FromDiskTag {}, file);
		if (loadedNow != nullptr)
		{
			*loadedNow = true;
		}
	}
	return *headers.Handle(id);
}

int32_t openblack::psys::SoundActionByName(std::string_view name)
{
	const auto& names = Loaded().byName;
	const auto it = names.find(name);
	return it == names.end() ? -1 : it->second;
}

std::string openblack::psys::SoundActionName(int32_t value)
{
	const auto& names = Loaded().byValue;
	const auto it = names.find(value);
	return it == names.end() ? std::string("?") : it->second;
}

SoundAction openblack::psys::ReadSoundAction(const Object& object, std::string_view key)
{
	SoundAction result;
	// a missing property reads as NO_SOUND with every switch off: the defaults
	const auto sound = object.Sound(key);
	// LOOPING bit 0, (ONLYONE only read), SOFTRELEASE bit 2, USESURFACE bit 3; the other bits are kept
	result.flags = static_cast<uint8_t>((sound.looping ? SoundAction::k_Looping : 0) |
	                                    (sound.softRelease ? SoundAction::k_SoftRelease : 0) |
	                                    (sound.useSurface ? SoundAction::k_UseSurface : 0));
	result.action = sound.sound == "NO_SOUND" ? -1 : SoundActionByName(sound.sound);
	return result;
}
