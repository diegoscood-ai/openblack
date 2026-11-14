/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveGameRoomRules.h"

#include <cctype>
#include <charconv>

#include <algorithm>
#include <array>
#include <ranges>
#include <string>
#include <utility>

#include <SaveSlots.h>

using namespace openblack::temple;

namespace
{
/// A save's name as a tooltip shows it: whole under 16 characters, else its first 13 and three dots
constexpr size_t k_ToolTipNameLength = 16;
constexpr size_t k_ToolTipNameDots = 13;

struct PartName
{
	std::string_view prefix;
	save_game_room::Part part;
	/// The number after the name of the first slot's control
	uint32_t first;
};
/// The controls by the start of their names, each followed by two digits: the pictures from 01, the buttons from 00
constexpr std::array k_PartNames {
    PartName {.prefix = "LH_PICTURE_savegame_", .part = save_game_room::Part::Picture, .first = 1},
    PartName {.prefix = "LH_BUTTON_SAVE", .part = save_game_room::Part::Save, .first = 0},
    PartName {.prefix = "LH_BUTTON_LOAD", .part = save_game_room::Part::Load, .first = 0},
    PartName {.prefix = "LH_BUTTON_DELETE", .part = save_game_room::Part::Delete, .first = 0},
};

bool StartsWithIgnoringCase(std::string_view text, std::string_view prefix)
{
	return text.size() >= prefix.size() && std::ranges::equal(text.substr(0, prefix.size()), prefix, [](char a, char b) {
		       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	       });
}

} // namespace

std::optional<save_game_room::Control> save_game_room::ParseControl(std::string_view subMeshName)
{
	for (const auto& [prefix, part, first] : k_PartNames)
	{
		if (!StartsWithIgnoringCase(subMeshName, prefix))
		{
			continue;
		}
		const auto digits = subMeshName.substr(prefix.size());
		if (digits.size() != 2 || !std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; }))
		{
			return std::nullopt;
		}
		uint32_t number = 0;
		std::from_chars(digits.data(), digits.data() + digits.size(), number);
		if (number < first || number - first >= k_Slots)
		{
			return std::nullopt;
		}
		return Control {.part = part, .slot = number - first};
	}
	return std::nullopt;
}

bool save_game_room::IsShown(Control control, const View& view, bool hasSave)
{
	const bool close = view.closeUp && view.pictureLookedAt;
	switch (control.part)
	{
	case Part::Picture:
		return true;
	case Part::Save:
		return !view.gameOver && control.slot <= k_LastPlayerSlot && !view.keptInRoom && close;
	case Part::Load:
	case Part::Delete:
		return hasSave && close;
	}
	return false;
}

save_game_room::Click save_game_room::Clicked(Control control, const View& view, bool hasSave)
{
	switch (control.part)
	{
	case Part::Picture:
		if (!view.closeUp || !view.pictureLookedAt)
		{
			return Click::LookCloser;
		}
		// Close up, a picture loads its slot's save, or saves into an empty slot of the player's while the game is not
		// over, even while the player is kept in a room
		if (hasSave)
		{
			return Click::AskLoad;
		}
		return control.slot <= k_LastPlayerSlot && !view.gameOver ? Click::AskName : Click::Nothing;
	case Part::Save:
		return hasSave ? Click::AskOverwrite : Click::AskName;
	case Part::Load:
		return hasSave ? Click::AskLoad : Click::Nothing;
	case Part::Delete:
		return Click::AskDelete;
	}
	return Click::Nothing;
}

std::optional<uint32_t> save_game_room::QuestionOf(Click click)
{
	switch (click)
	{
	case Click::AskLoad:
		return k_LoadQuestionText;
	case Click::AskOverwrite:
		return k_OverwriteQuestionText;
	case Click::AskDelete:
		return k_DeleteQuestionText;
	default:
		return std::nullopt;
	}
}

save_game_room::Step save_game_room::StepAfter(Click click, bool yes)
{
	switch (click)
	{
	case Click::AskName:
		return Step::AskName;
	case Click::AskOverwrite:
		return yes ? Step::AskName : Step::Nothing;
	case Click::AskLoad:
		return yes ? Step::Load : Step::Nothing;
	case Click::AskDelete:
		return yes ? Step::Delete : Step::Nothing;
	default:
		return Step::Nothing;
	}
}

std::u16string save_game_room::NameBoxStart(Click click, std::optional<std::u16string_view> oldName)
{
	if (click != Click::AskOverwrite || !oldName.has_value())
	{
		return {};
	}
	return std::u16string(oldName->substr(0, k_NameLength));
}

save_game_room::LoadSteps save_game_room::LoadStepsOf(uint32_t slot)
{
	return {.saveFirstInto = gamesave::SaveBeforeRoomLoad(slot), .load = slot};
}

std::optional<uint32_t> save_game_room::LandFolderOf(uint32_t slot, uint32_t land)
{
	// the room's folder rule is the saved games' one (gamesave::RoomFolderOf): a land's folder in the slot's, or none
	const auto room = gamesave::RoomFolderOf(std::to_string(slot), land);
	if (!room.has_value() || !room->folder.has_parent_path())
	{
		return std::nullopt;
	}
	const auto name = room->folder.filename().string();
	uint32_t landFolder = 0;
	std::from_chars(name.data(), name.data() + name.size(), landFolder);
	return landFolder;
}

bool save_game_room::IsOfSlot(const Entry& entry, uint32_t slot)
{
	return entry.slot % 100 == slot;
}

bool save_game_room::HasSave(std::span<const Entry> list, uint32_t slot)
{
	return std::ranges::any_of(list, [slot](const Entry& entry) { return IsOfSlot(entry, slot); });
}

bool save_game_room::HasSaveNumbered(std::span<const Entry> list, uint32_t slot)
{
	return std::ranges::any_of(list, [slot](const Entry& entry) { return entry.slot == slot; });
}

std::u16string save_game_room::ToolTipName(std::u16string_view name)
{
	if (name.size() < k_ToolTipNameLength)
	{
		return std::u16string(name);
	}
	auto shown = std::u16string(name.substr(0, k_ToolTipNameDots));
	shown.append(k_ToolTipNameLength - k_ToolTipNameDots, u'.');
	return shown;
}

bool save_game_room::PictureZoomsIn(const PictureToolTipInput& input)
{
	return input.hovered && (input.zoom == 0.0f || (input.zoom == 1.0f && !input.pictureLookedAt));
}

std::optional<save_game_room::PictureToolTip> save_game_room::PictureToolTipOf(const PictureToolTipInput& input)
{
	if (PictureZoomsIn(input))
	{
		if (input.saveName.has_value() && !input.saveName->empty())
		{
			return PictureToolTip {.text = k_ZoomInNamedText, .name = ToolTipName(*input.saveName)};
		}
		return PictureToolTip {.text = k_ZoomInText, .name = {}};
	}
	const bool closeUp = input.zoom == 1.0f && input.lookedAt == input.slot && !input.buttonHovered;
	if (input.hovered && closeUp)
	{
		if (input.saveNumbered)
		{
			return PictureToolTip {.text = k_LoadText, .name = {}};
		}
		if (input.slot <= k_LastPlayerSlot && !input.gameOver)
		{
			return PictureToolTip {.text = k_SaveText, .name = {}};
		}
		return std::nullopt;
	}
	if (!input.hovered && closeUp && input.pictureLookedAt)
	{
		return PictureToolTip {.text = k_ZoomOutText, .name = {}};
	}
	return std::nullopt;
}

const save_game_room::Entry* save_game_room::SaveOf(std::span<const Entry> list, uint32_t slot)
{
	const Entry* found = nullptr;
	for (const auto& entry : list)
	{
		if (IsOfSlot(entry, slot))
		{
			found = &entry;
		}
	}
	return found;
}

std::vector<uint32_t> save_game_room::FolderOrder()
{
	std::vector<uint32_t> slots(k_Slots);
	for (uint32_t slot = 0; slot < k_Slots; ++slot)
	{
		slots[slot] = slot;
	}
	std::ranges::sort(slots, {}, [](uint32_t slot) { return std::to_string(slot); });
	return slots;
}

void save_game_room::AfterSave(std::vector<Entry>& list, Entry saved, bool overAnother)
{
	if (overAnother)
	{
		for (auto& entry : list | std::views::reverse)
		{
			if (IsOfSlot(entry, saved.slot))
			{
				entry.date = std::move(saved.date);
				entry.name = std::move(saved.name);
				return;
			}
		}
	}
	list.push_back(std::move(saved));
}

void save_game_room::AfterDelete(std::vector<Entry>& list, uint32_t slot)
{
	std::erase_if(list, [slot](const Entry& entry) { return IsOfSlot(entry, slot); });
}

save_game_room::UvRect save_game_room::EmptyPicture(float alignment)
{
	return alignment >= 0.0f ? UvRect {.u0 = 0.5f, .v0 = 0.0f, .u1 = 1.0f, .v1 = 1.0f}
	                         : UvRect {.u0 = 0.0f, .v0 = 0.0f, .u1 = 0.5f, .v1 = 1.0f};
}

save_game_room::SlotPicture save_game_room::PictureOf(uint32_t slot, bool hasSave, bool inTheRoom,
                                                      std::optional<uint32_t> highDetail, float alignment)
{
	if (!hasSave || !inTheRoom)
	{
		return {.sheet = PictureSheet::Empty, .file = 0, .uv = EmptyPicture(alignment)};
	}
	if (highDetail == slot)
	{
		// The pair's file of large pictures, the slot's half of it
		const float v = static_cast<float>(slot & 1u) * 0.5f;
		return {.sheet = PictureSheet::Large, .file = slot >> 1u, .uv = {.u0 = 0.0f, .v0 = v, .u1 = 1.0f, .v1 = v + 0.5f}};
	}
	// Eight slots share a file of small pictures, in four rows of two
	const float u = static_cast<float>(slot & 1u) * 0.5f;
	const float v = static_cast<float>((slot & 7u) >> 1u) * 0.25f;
	return {.sheet = PictureSheet::Small, .file = slot >> 3u, .uv = {.u0 = u, .v0 = v, .u1 = u + 0.5f, .v1 = v + 0.25f}};
}

save_game_room::PicturesToRead save_game_room::PicturesAfterSave(uint32_t slot, std::optional<uint32_t> highDetail)
{
	return {.smallPictures = true, .largePicture = highDetail == slot};
}
