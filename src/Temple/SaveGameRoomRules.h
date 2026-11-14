/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The rules of the temple's save game room: a wall of 21 pictures, one for each slot of the saved games, each with a
/// button to save, a button to load and a button to delete. The buttons show only while the camera is all the way close
/// to a picture it was sent to. See docs/wiki/interface/temple-interior.md#the-save-game-room.
namespace openblack::temple::save_game_room
{

/// How many pictures the room's walls have, one for each slot
inline constexpr uint32_t k_Slots = 21;
/// The last slot that the player can save into
inline constexpr uint32_t k_LastPlayerSlot = 13;
/// How far in front of a picture, along its frame, the camera looks at it from
inline constexpr float k_LookDistance = 20.0f;
/// The longest name that the save's name box takes
inline constexpr size_t k_NameLength = 32;

/// The help texts of the room's questions, asked in the game's question box with its Yes and No
inline constexpr uint32_t k_LoadQuestionText = 0x141C;
inline constexpr uint32_t k_OverwriteQuestionText = 0x141D;
inline constexpr uint32_t k_DeleteQuestionText = 0x141E;

/// The parts of the room's mesh that are controls
enum class Part : uint8_t
{
	/// The slot's picture, which a click looks at closer and, once close, loads or saves
	Picture,
	Save,
	Load,
	Delete,
};

/// A control of the room's mesh and its slot, from 0
struct Control
{
	Part part;
	uint32_t slot;

	bool operator==(const Control&) const = default;
};

/// The control that a submesh of the room's mesh is, by its name, if any: "LH_PICTURE_savegame_01" is the picture of
/// slot 0, "LH_BUTTON_SAVE00" its save button, "LH_BUTTON_LOAD00" and "LH_BUTTON_DELETE00" its others. The case of the
/// letters is not important.
[[nodiscard]] std::optional<Control> ParseControl(std::string_view subMeshName);

/// What the room's controls depend on
struct View
{
	/// The room's camera is all the way to the part it looks at (its blend exactly 1)
	bool closeUp {false};
	/// The camera was last sent to a picture rather than the scroll
	bool pictureLookedAt {false};
	/// The game is over
	bool gameOver {false};
	/// The player is kept in a room of the temple, as at the end of the game
	bool keptInRoom {false};

	bool operator==(const View&) const = default;
};

/// Whether a control is drawn: the pictures always; while the camera is close to a picture it was sent to, the save
/// buttons of the slots the player can save into (not once the game is over, nor while the player is kept in a room),
/// and the load and delete buttons of the slots that have a save
[[nodiscard]] bool IsShown(Control control, const View& view, bool hasSave);

/// What a click on a control does
enum class Click : uint8_t
{
	Nothing,
	/// The camera goes to look at the picture from close by
	LookCloser,
	/// The player is asked whether to load the slot's save
	AskLoad,
	/// The player is asked whether to save over the slot's save, then for a name
	AskOverwrite,
	/// The player is asked for a name for a new save in the slot
	AskName,
	/// The player is asked whether to delete the slot's save
	AskDelete,
};
/// A click on a control shown. Away from the pictures, a click on one looks at it closer. Close to a picture, a click
/// on a picture loads the slot's save, or saves into an empty slot the player can save into while the game is not over.
/// The save button saves over a save after a question, or into an empty slot; the load and delete buttons ask first.
[[nodiscard]] Click Clicked(Control control, const View& view, bool hasSave);

/// The question a click asks first, as its help text, none for a click that goes on at once
[[nodiscard]] std::optional<uint32_t> QuestionOf(Click click);

/// What a click does once its question is answered, or at once when it asks none
enum class Step : uint8_t
{
	Nothing,
	/// The save's name box opens; its OK saves into the slot
	AskName,
	Load,
	Delete,
};
/// The step after a click, with the player's answer to its question: no does nothing
[[nodiscard]] Step StepAfter(Click click, bool yes);

/// The name the name box starts with: a new save's empty, a save over another the other's name
[[nodiscard]] std::u16string NameBoxStart(Click click, std::optional<std::u16string_view> oldName);

/// A load from the room. Loading the previous land's slot first saves the land into the next land's slot, and loading
/// the next land's slot first saves it into the previous land's, each under that slot's own label as its name; both
/// are the room's saves.
struct LoadSteps
{
	/// The slot saved into first, if any
	std::optional<uint32_t> saveFirstInto;
	uint32_t load;

	bool operator==(const LoadSteps&) const = default;
};
[[nodiscard]] LoadSteps LoadStepsOf(uint32_t slot);

/// The land's subfolder of the slot's folder that the room lists the slot's save from and a delete removes, none for the
/// slot's folder itself: the land's own for the autosaves, the land before for the previous land's slot past the first
/// land, the land after for the next land's slot on the lands 1 to 4
[[nodiscard]] std::optional<uint32_t> LandFolderOf(uint32_t slot, uint32_t land);

/// A saved game that the room lists
struct Entry
{
	/// The slot, as the save's own file numbers it
	uint32_t slot {0};
	std::u16string date;
	std::u16string name;

	bool operator==(const Entry&) const = default;
};
/// Whether an entry is the save of a slot: its own number counts by its last two digits
[[nodiscard]] bool IsOfSlot(const Entry& entry, uint32_t slot);
/// Whether a slot has a save in the list
[[nodiscard]] bool HasSave(std::span<const Entry> list, uint32_t slot);
/// The save of a slot that the name box starts with, the last of the slot in the list
[[nodiscard]] const Entry* SaveOf(std::span<const Entry> list, uint32_t slot);
/// Whether a save in the list carries the slot's own number whole, not only by its last two digits: what a picture's
/// tooltip close up goes by
[[nodiscard]] bool HasSaveNumbered(std::span<const Entry> list, uint32_t slot);
/// The help texts of the room's tooltips, by their numbers
inline constexpr uint32_t k_SaveText = 0xE9C;
inline constexpr uint32_t k_LoadText = 0xE9D;
inline constexpr uint32_t k_DeleteText = 0xE9E;
inline constexpr uint32_t k_ZoomInText = 0xE75;
inline constexpr uint32_t k_ZoomInNamedText = 0xEE6;
inline constexpr uint32_t k_ZoomOutText = 0xE8B;
/// The name of a save as a tooltip shows it: a name of 16 characters or more is cut to 13 characters and three dots
[[nodiscard]] std::u16string ToolTipName(std::u16string_view name);
/// What a picture's tooltip depends on
struct PictureToolTipInput
{
	uint32_t slot;
	bool hovered;
	/// How close the camera is to the part it looks at, from 0 to 1
	float zoom;
	/// The camera was last sent to a picture rather than the scroll
	bool pictureLookedAt;
	/// The slot of the picture the camera was last sent to
	std::optional<uint32_t> lookedAt;
	/// A button before this picture, in the order of the mesh, has the cursor
	bool buttonHovered;
	/// The name of the first save of the slot in the list, if it has one
	std::optional<std::u16string_view> saveName;
	/// A save in the list carries the slot's own number whole (HasSaveNumbered)
	bool saveNumbered;
	bool gameOver;
};
/// The tooltip that a picture chooses, if it chooses one
struct PictureToolTip
{
	uint32_t text;
	/// The name the tooltip shows, for the named zoom. A picture's tooltips have no arrows.
	std::u16string name;
};
/// The tooltip of a picture. Hovered from afar, or close while the camera looks at the scroll, it zooms in, with the
/// save's name if the slot has a named save. Hovered close up as the picture looked at, it loads the slot's save, or
/// saves into an empty slot that the player can save into while the game is not over. Not hovered while the camera
/// looks at it close up, it zooms out.
[[nodiscard]] std::optional<PictureToolTip> PictureToolTipOf(const PictureToolTipInput& input);
/// Whether a picture's tooltip, as above, is one of its zooms in, which hold back the scroll's zoom out
[[nodiscard]] bool PictureZoomsIn(const PictureToolTipInput& input);
/// The order the room finds the slots' folders in, by their names: 0, 1, 10 to 19, 2, 20, 3 to 9
[[nodiscard]] std::vector<uint32_t> FolderOrder();
/// The list after a save written into a slot: a save over another keeps its place with its new date and name, a new one
/// goes last
void AfterSave(std::vector<Entry>& list, Entry saved, bool overAnother);
/// The list after the slot's save is deleted: every save of the slot leaves it
void AfterDelete(std::vector<Entry>& list, uint32_t slot);
/// Whether a save written starts the room's load help: only a save into a slot that had none
[[nodiscard]] constexpr bool StartsLoadHelp(bool overAnother) noexcept
{
	return !overAnother;
}
/// The temple script a new save starts
inline constexpr std::string_view k_LoadHelpScript = "CitadelSaveGameRoomLoadHelp";

/// The part of a texture a picture is drawn with, from 0 to 1: its low corner, then its high one
struct UvRect
{
	float u0;
	float v0;
	float u1;
	float v1;

	bool operator==(const UvRect&) const = default;
};
/// What a slot's picture is drawn with: the empty picture (Data/Textures/PictureTexture.raw), a file of small pictures,
/// or the file of large pictures of the slot shown in high detail
enum class PictureSheet : uint8_t
{
	Empty,
	Small,
	Large,
};
struct SlotPicture
{
	PictureSheet sheet;
	/// The file of small pictures (slot / 8) or of large ones (slot / 2); 0 for the empty picture
	uint32_t file;
	UvRect uv;

	bool operator==(const SlotPicture&) const = default;
};
/// The half of the empty picture a slot with no save shows: its right half for a player who is good or neither, its
/// left half for an evil player
[[nodiscard]] UvRect EmptyPicture(float alignment);
/// The picture a slot shows. A slot whose save the room lists, while the save game room is the room the player is in,
/// shows its own: the large picture while it is the slot shown in high detail (the picture the camera was last sent
/// to since the player came into the room), else its small one, each its cell of its shared file. Any other shows the
/// empty picture's half by the player's alignment
[[nodiscard]] SlotPicture PictureOf(uint32_t slot, bool hasSave, bool inTheRoom, std::optional<uint32_t> highDetail,
                                    float alignment);
/// The pictures the room reads again after a save into a slot: the small ones always, the large one only when the
/// slot is the one shown in high detail
struct PicturesToRead
{
	bool smallPictures;
	bool largePicture;

	bool operator==(const PicturesToRead&) const = default;
};
[[nodiscard]] PicturesToRead PicturesAfterSave(uint32_t slot, std::optional<uint32_t> highDetail);

} // namespace openblack::temple::save_game_room
