/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedSlot.h"

#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

#include <LHVM.h>
#include <MindFile.h>
#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <fmt/format.h>

#include "Common/HelpText.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/SaveEvents.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/SavedScriptObjects.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Help/HelpProfileFile.h"
#include "Locator.h"

#ifdef _WIN32
// clang-format off
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// clang-format on
#endif

using namespace openblack::ecs;
using openblack::Locator;

namespace
{
/// The most characters, its end included, the locale's time or date text is read into
constexpr size_t k_TextRoom = 64;

#ifdef _WIN32
std::u16string FromWide(const wchar_t* text)
{
	std::u16string out;
	for (; *text != L'\0'; ++text)
	{
		out.push_back(static_cast<char16_t>(*text));
	}
	return out;
}

/// One of the locale's texts of the time now through its wide call: the count it asks for, and the text when it is
/// less than k_TextRoom characters (one of k_TextRoom or more is not read, its count kept)
template <typename Call>
saved_slot::LocaleText ReadWide(Call call)
{
	const auto length = call(nullptr, 0);
	if (length <= 0 || static_cast<size_t>(length) >= k_TextRoom)
	{
		return {.length = static_cast<size_t>(std::max(length, 0))};
	}
	std::array<wchar_t, k_TextRoom> text {};
	const auto written = call(text.data(), length + 1);
	return {.text = FromWide(text.data()), .length = static_cast<size_t>(std::max(written, 0))};
}

/// The same through its narrow call, read into the one narrow buffer both texts share (as the game's, so a call that
/// writes nothing leaves the other text there), the text made wide in the system's code page
template <typename Call>
saved_slot::LocaleText ReadNarrow(Call call, std::array<char, k_TextRoom>& narrow)
{
	const auto length = call(nullptr, 0);
	if (length < 0 || static_cast<size_t>(length) >= k_TextRoom)
	{
		return {.length = static_cast<size_t>(std::max(length, 0))};
	}
	const auto written = call(narrow.data(), length + 1);
	std::array<wchar_t, k_TextRoom> text {};
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, narrow.data(), -1, text.data(), static_cast<int>(text.size()));
	return {.text = FromWide(text.data()), .length = static_cast<size_t>(std::max(written, 0))};
}
#endif

/// The save made from the game and written, through the Locator's services
saved_slot::SlotSave MakeAndSave(uint32_t slot, std::u16string title)
{
	using saved_slot::SlotSaveOutcome;
	if (!Locator::saveGame::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::mapScriptSystem::has_value())
	{
		return {.outcome = SlotSaveOutcome::NoSlot};
	}
	auto& saves = Locator::saveGame::value();
	const auto& globals = Locator::mapScriptSystem::value().Globals();
	const auto land = static_cast<uint32_t>(std::max(0, globals.landNumber));

	// with no profile kept (a run of the checks) there is no slot, and nothing is made
	if (!saves.SlotFolder(slot, land).has_value())
	{
		return {.outcome = SlotSaveOutcome::NoSlot};
	}
	// the "Saving..." banner, shown before anything is written
	events::Publish(events::GameSaving {.slot = slot});
	// the world file, from the entities and what the last load kept, made whole first: when it cannot be, no folder or
	// file is touched, so the slot's old save stays as it was
	const auto writers = saved_world_write::GameWriters();
	// a crossing file's save names its copy beside the world file as the game names it, from the game's folder with back
	// slashes
	std::string crossingCopy;
	if (const auto copy = saves.CrossingCopy(slot, land); copy.has_value())
	{
		constexpr char k_Backslash = 0x5C;
		crossingCopy = copy->string();
		std::ranges::replace(crossingCopy, '/', k_Backslash);
	}
	const saved_world_write::WorldSource source {.registry = std::as_const(Locator::entitiesRegistry::value()),
	                                             .landscape = globals.landscapeFile,
	                                             .kept = saves.LoadedWorld(),
	                                             .loaded = saves.LoadedWorldObjects(),
	                                             .crossingCopy = std::move(crossingCopy)};
	auto world = saved_world_write::WriteWorld(writers, source);
	if (!world.Complete())
	{
		return {.outcome = SlotSaveOutcome::NotMade, .failure = std::move(world.failure)};
	}
	systems::SaveContents contents {
	    .info = openblack::gamesave::SaveGameInfo {.slot = slot, .date = saved_slot::NowText(), .name = std::move(title)},
	    .pictures = true,
	    .world = std::move(world.bytes),
	    .crossingFiles = saved_world_write::CrossingFiles(writers, source),
	    .minds = saved_slot::CreatureMinds(),
	    .helpRecord = [] { openblack::help::profile::KeepInProfile(); },
	    .scriptState =
	        [](const std::filesystem::path& file) {
		        if (!Locator::vm::has_value() || !Locator::filesystem::has_value())
		        {
			        return false;
		        }
		        const auto path = file.is_absolute() ? file : Locator::filesystem::value().GetGamePath() / file;
		        // the machine's file names each object by its place in the scripts' object table the world file holds;
		        // the running machine keeps its own values
		        auto& vm = Locator::vm::value();
		        const auto places = saved_script_objects::Current();
		        if (!places.has_value())
		        {
			        return false;
		        }
		        const auto values = vm.ObjectValues();
		        vm.MapObjects([&places](uint32_t value) { return saved_script_objects::PlaceOf(*places, value); });
		        const bool saved = vm.SaveState(path) == EXIT_SUCCESS;
		        size_t next = 0;
		        vm.MapObjects([&values, &next](uint32_t /*place*/) { return values.at(next++); });
		        return saved;
	        },
	};
	switch (saves.Save(slot, land, contents))
	{
	case systems::SaveWrite::Written:
		return {.outcome = SlotSaveOutcome::Written};
	case systems::SaveWrite::NotOpened:
		return {.outcome = SlotSaveOutcome::NotOpened, .failure = "the world file's temporary file did not open"};
	case systems::SaveWrite::Failed:
		break;
	}
	return {.outcome = SlotSaveOutcome::WriteFailed, .failure = "a file of the save could not be written"};
}

/// The save, whatever stops it on the way: what it throws before any file is written is a save not made
template <typename Save>
saved_slot::SlotSave Guarded(Save&& save)
{
	try
	{
		return std::forward<Save>(save)();
	}
	catch (const std::exception& error)
	{
		return {.outcome = saved_slot::SlotSaveOutcome::NotMade, .failure = error.what()};
	}
}
} // namespace

saved_slot::LocaleText saved_slot::ChooseLocaleText(const LocaleText& wide, const LocaleText& narrow)
{
	return wide.length != 0 ? wide : narrow;
}

std::u16string saved_slot::DateText(const LocaleText& time, const LocaleText& date)
{
	if (time.length + date.length >= k_DateRoom)
	{
		return {};
	}
	std::u16string text(time.text);
	text += u' ';
	text += date.text;
	return text;
}

std::u16string saved_slot::DateText(std::u16string_view time, std::u16string_view date)
{
	return DateText(LocaleText {.text = std::u16string(time), .length = time.size() + 1},
	                LocaleText {.text = std::u16string(date), .length = date.size() + 1});
}

std::u16string saved_slot::NowText()
{
#ifdef _WIN32
	// The player's locale, the time as it writes it and its long date, of the time now; where the wide calls give
	// nothing, the narrow ones: the time as it writes it and its short date
	std::array<char, k_TextRoom> narrow {};
	const auto wideDate = [](wchar_t* text, int room) {
		return GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, nullptr, nullptr, text, room);
	};
	const auto narrowDate = [](char* text, int room) {
		return GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, nullptr, nullptr, text, room);
	};
	const auto wideTime = [](wchar_t* text, int room) {
		return GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, text, room);
	};
	const auto narrowTime = [](char* text, int room) {
		return GetTimeFormatA(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, text, room);
	};
	const auto longDate = ReadWide(wideDate);
	const auto date = ChooseLocaleText(longDate, longDate.length != 0 ? LocaleText {} : ReadNarrow(narrowDate, narrow));
	const auto wideTimeText = ReadWide(wideTime);
	const auto time = ChooseLocaleText(wideTimeText, wideTimeText.length != 0 ? LocaleText {} : ReadNarrow(narrowTime, narrow));
	return DateText(time, date);
#else
	// Elsewhere the C library's texts of the locale's time and date
	const auto now = std::time(nullptr);
	std::tm local {};
	localtime_r(&now, &local);
	std::array<char, k_TextRoom> time {};
	std::array<char, k_TextRoom> date {};
	std::strftime(time.data(), time.size(), "%X", &local);
	std::strftime(date.data(), date.size(), "%x", &local);
	const auto widen = [](const char* text) {
		std::u16string out;
		for (; *text != '\0'; ++text)
		{
			out.push_back(static_cast<char16_t>(static_cast<unsigned char>(*text)));
		}
		return out;
	};
	return DateText(widen(time.data()), widen(date.data()));
#endif
}

std::u16string saved_slot::TitleOf(uint32_t slot, uint32_t land)
{
	const auto text = slot < k_SlotTitleTexts.size() ? k_SlotTitleTexts.at(slot) : 0;
	return helptext::Format(text, static_cast<double>(land));
}

std::vector<entt::entity> saved_slot::CreaturesInListOrder(const Registry& registry)
{
	std::vector<std::pair<uint32_t, entt::entity>> creatures;
	registry.Each<const components::Creature>([&registry, &creatures](entt::entity entity, const components::Creature&) {
		const auto* index = registry.TryGet<const components::ObjectCreationIndex>(entity);
		creatures.emplace_back(index != nullptr ? index->value : 0, entity);
	});
	// the newest at the head; the same number, the entity's own order
	std::ranges::sort(creatures, [](const auto& left, const auto& right) {
		return left.first != right.first ? left.first > right.first
		                                 : entt::to_integral(left.second) < entt::to_integral(right.second);
	});
	std::vector<entt::entity> ordered;
	ordered.reserve(creatures.size());
	for (const auto& [index, entity] : creatures)
	{
		ordered.push_back(entity);
	}
	return ordered;
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> saved_slot::CreatureMinds()
{
	std::vector<std::pair<uint32_t, std::vector<uint8_t>>> minds;
	if (!Locator::entitiesRegistry::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return minds;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* physiology =
	    Locator::creaturePhysiologySystem::has_value() ? &Locator::creaturePhysiologySystem::value() : nullptr;
	const auto* leash = Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr;
	const auto creatures = CreaturesInListOrder(registry);
	for (uint32_t place = 0; place < creatures.size(); ++place)
	{
		if (const auto mind = player_creature::ToMindFile(registry, Locator::creatureMindSystem::value(), physiology, leash,
		                                                  creatures.at(place));
		    mind.has_value())
		{
			minds.emplace_back(place, creaturemind::Write(*mind));
		}
	}
	return minds;
}

saved_slot::SlotSave saved_slot::InstantSave(uint32_t slot)
{
	return Guarded([slot] {
		const auto land = Locator::mapScriptSystem::has_value()
		                      ? static_cast<uint32_t>(std::max(0, Locator::mapScriptSystem::value().Globals().landNumber))
		                      : 0U;
		return SaveSlot(slot, TitleOf(slot, land));
	});
}

saved_slot::SlotSave saved_slot::RoomSave(uint32_t slot, std::u16string_view name)
{
	return Guarded([slot, name] {
		auto saved = SaveSlot(slot, std::u16string(name));
		// a save the room made counts in the profile's statistics
		if (saved.Written() && Locator::playerProfileSystem::has_value())
		{
			Locator::playerProfileSystem::value().CountRoomSave();
		}
		return saved;
	});
}

namespace
{
/// The folder the save game room lists a slot's save in and deletes it from, none with no profile kept
std::optional<std::filesystem::path> RoomFolder(uint32_t slot, uint32_t land)
{
	if (!Locator::saveGame::has_value())
	{
		return std::nullopt;
	}
	const auto landFolder = openblack::temple::save_game_room::LandFolderOf(slot, land);
	auto folder = Locator::saveGame::value().SlotFolder(slot, landFolder.value_or(0));
	// a slot with a folder for each land, read without one, is the slot's own folder
	if (folder.has_value() && !landFolder.has_value() && openblack::gamesave::HasLandFolder(slot))
	{
		folder = folder->parent_path();
	}
	return folder;
}
} // namespace

std::vector<openblack::temple::save_game_room::Entry> saved_slot::RoomList(uint32_t land)
{
	std::vector<temple::save_game_room::Entry> list;
	if (!Locator::filesystem::has_value())
	{
		return list;
	}
	auto& fileSystem = Locator::filesystem::value();
	for (const auto slot : temple::save_game_room::FolderOrder())
	{
		const auto folder = RoomFolder(slot, land);
		if (!folder.has_value())
		{
			continue;
		}
		try
		{
			if (!fileSystem.Exists(*folder / gamesave::VanillaWorldFileName(slot)))
			{
				continue;
			}
			const auto bytes = fileSystem.ReadUserFile(*folder / gamesave::k_InfoFileName);
			const auto info = bytes.has_value() ? gamesave::ReadSaveGameInfo(*bytes) : std::nullopt;
			if (info.has_value())
			{
				list.push_back({.slot = info->slot, .date = info->date, .name = info->name});
			}
		}
		catch (const std::exception&)
		{
			// a slot whose files cannot be read is not listed
		}
	}
	return list;
}

bool saved_slot::DeleteRoomSlot(uint32_t slot, uint32_t land)
{
	const auto folder = RoomFolder(slot, land);
	if (!folder.has_value() || !Locator::filesystem::has_value())
	{
		return false;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		// the folder's files go, the folders in it stay, as the room's delete leaves them
		std::vector<std::filesystem::path> files;
		fileSystem.Iterate(*folder, false, [&files](const std::filesystem::path& entry) {
			std::error_code error;
			if (!std::filesystem::is_directory(entry, error))
			{
				files.push_back(entry);
			}
		});
		for (const auto& file : files)
		{
			std::ignore = fileSystem.Remove(file);
		}
		// and the folder, when nothing is left in it
		std::ignore = fileSystem.Remove(*folder);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

saved_slot::SlotSave saved_slot::SaveSlot(uint32_t slot, std::u16string title)
{
	return Guarded([slot, &title] { return MakeAndSave(slot, std::move(title)); });
}

std::vector<std::string> saved_slot::GroupRefusals(const std::vector<saved_world_write::Refusal>& refusals)
{
	// the numbers of the objects (entities, ids) differ from one object to the next: read as N, a reason is one line
	static const std::regex k_Numbers("[0-9]+");
	struct Group
	{
		size_t count {0};
		/// The first number of the group's first refusal (the object it names), to find that object by
		std::string first;
		/// The object whose record named the group's first refusal's
		std::string namedBy;
	};
	std::map<std::pair<std::string, std::string>, Group> groups;
	for (const auto& refusal : refusals)
	{
		// a refusal of an object a record of another class names is told apart by that class
		const auto namer = refusal.namedBy.empty() ? std::string {} : " (named by " + refusal.namedBy + ")";
		auto& group = groups[{refusal.type, std::regex_replace(refusal.reason + namer, k_Numbers, "N")}];
		if (group.count++ == 0)
		{
			if (std::smatch number; std::regex_search(refusal.reason, number, k_Numbers))
			{
				group.first = number.str();
			}
			group.namedBy = refusal.namedBy;
		}
	}
	std::vector<std::pair<size_t, std::string>> lines;
	for (const auto& [key, group] : groups)
	{
		const auto namer = group.namedBy.empty() ? std::string {} : ", named by " + group.namedBy;
		const auto first =
		    group.first.empty() ? std::string {} : fmt::format(" (first: {} {}{})", key.first, group.first, namer);
		lines.emplace_back(group.count, fmt::format("{} x{}{}: {}", key.first, group.count, first, key.second));
	}
	std::ranges::stable_sort(lines, std::ranges::greater {}, &std::pair<size_t, std::string>::first);
	std::vector<std::string> out;
	out.reserve(lines.size());
	for (auto& [count, line] : lines)
	{
		out.push_back(std::move(line));
	}
	return out;
}

std::vector<std::string> saved_slot::RefusalLines()
{
	if (!Locator::saveGame::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::mapScriptSystem::has_value())
	{
		return {};
	}
	auto& saves = Locator::saveGame::value();
	const auto writers = saved_world_write::GameWriters();
	const saved_world_write::WorldSource source {.registry = std::as_const(Locator::entitiesRegistry::value()),
	                                             .landscape = Locator::mapScriptSystem::value().Globals().landscapeFile,
	                                             .kept = saves.LoadedWorld(),
	                                             .loaded = saves.LoadedWorldObjects()};
	return GroupRefusals(saved_world_write::ListRefusals(writers, source));
}
