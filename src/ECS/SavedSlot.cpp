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
#include <utility>

#include <LHVM.h>
#include <MindFile.h>
#include <SaveGameInfo.h>

#include "Common/HelpText.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
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
	// the world file, from the entities and what the last load kept, made whole first: when it cannot be, no folder or
	// file is touched, so the slot's old save stays as it was
	const auto writers = saved_world_write::GameWriters();
	const saved_world_write::WorldSource source {.registry = std::as_const(Locator::entitiesRegistry::value()),
	                                             .landscape = globals.landscapeFile,
	                                             .kept = saves.LoadedWorld(),
	                                             .loaded = saves.LoadedWorldObjects()};
	auto world = saved_world_write::WriteWorld(writers, source);
	if (!world.Complete())
	{
		return {.outcome = SlotSaveOutcome::NotMade, .failure = std::move(world.failure)};
	}
	systems::SaveContents contents {
	    .info = openblack::gamesave::SaveGameInfo {.slot = slot, .date = saved_slot::NowText(), .name = std::move(title)},
	    .pictures = true,
	    .world = std::move(world.bytes),
	    .minds = saved_slot::CreatureMinds(),
	    .helpRecord = [] { openblack::help::profile::KeepInProfile(); },
	    .scriptState =
	        [](const std::filesystem::path& file) {
		        if (!Locator::vm::has_value() || !Locator::filesystem::has_value())
		        {
			        return false;
		        }
		        const auto path = file.is_absolute() ? file : Locator::filesystem::value().GetGamePath() / file;
		        return Locator::vm::value().SaveState(path) == EXIT_SUCCESS;
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

std::u16string saved_slot::DateText(std::u16string_view time, std::u16string_view date)
{
	if (time.size() + 1 + date.size() + 1 >= k_DateRoom)
	{
		return {};
	}
	std::u16string text(time);
	text += u' ';
	text += date;
	return text;
}

std::u16string saved_slot::NowText()
{
#ifdef _WIN32
	// The player's locale, the time as it writes it and its long date, of the time now
	std::array<wchar_t, k_TextRoom> time {};
	std::array<wchar_t, k_TextRoom> date {};
	const auto timeLength = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, nullptr, 0);
	const auto dateLength = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, nullptr, nullptr, nullptr, 0);
	if (timeLength <= 0 || dateLength <= 0 || static_cast<size_t>(timeLength) >= k_TextRoom ||
	    static_cast<size_t>(dateLength) >= k_TextRoom)
	{
		return {};
	}
	GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, time.data(), static_cast<int>(time.size()));
	GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, nullptr, nullptr, date.data(), static_cast<int>(date.size()));
	return DateText(FromWide(time.data()), FromWide(date.data()));
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

saved_slot::SlotSave saved_slot::SaveSlot(uint32_t slot, std::u16string title)
{
	return Guarded([slot, &title] { return MakeAndSave(slot, std::move(title)); });
}
