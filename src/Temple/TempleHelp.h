/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "3D/CreatureCaveTargets.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleScrolls.h"
#include "Help/DialogueText.h"

/// What the temple asks of the scripts while the player is inside: the help scripts its rooms, scrolls and places
/// start and stop. Each help script shows its help only the first time by itself, so the temple starts them every time.
/// Free of state, so it is tested on its own with a fake of the scripts.
namespace openblack::temple_help
{

/// The scripts, as the temple's help starts and stops them
class Scripts
{
public:
	virtual ~Scripts() = default;
	/// Starts a script by its name
	virtual void Start(std::string_view name) = 0;
	/// Every help script stops: the world's, the temple's and a multiplayer game's
	virtual void StopHelp() = 0;
	/// Every running script of a name stops
	virtual void Stop(std::string_view name) = 0;
};

/// The help script a room starts as the player comes into it, none for a room without one. The options room shares
/// the main room's.
[[nodiscard]] std::optional<std::string_view> RoomHelp(TempleRoom room);
/// The help script a scroll starts as the camera is sent to look at it, none for a scroll without one
[[nodiscard]] std::optional<std::string_view> ScrollHelp(TempleScrolls::Content content);
/// The scroll help scripts a room's camera stops as the player takes it away from a scroll
[[nodiscard]] std::span<const std::string_view> ScrollHelpsStoppedIn(TempleRoom room);
/// The help script a place in the creature's room starts as the camera zooms to it, none for the others
[[nodiscard]] std::optional<std::string_view> CaveTargetHelp(CreatureCaveTargets::Target target);

/// The player comes into a room, on the way into the temple or from another room: the help scripts running stop, and
/// the room's own starts while the help system is on
void EnterRoom(Scripts& scripts, TempleRoom room, bool helpSystemOn);
/// The camera is sent to look at a scroll: the scroll's help starts
void LookAtScroll(Scripts& scripts, TempleScrolls::Content content);
/// The player takes the room's camera away from the scroll it looks at
void LeaveScroll(Scripts& scripts, TempleRoom room);
/// The creature's room's camera zooms to one of its places: its help starts
void ZoomToCaveTarget(Scripts& scripts, CreatureCaveTargets::Target target);
/// The player leaves the temple: the help scripts running stop
void Leave(Scripts& scripts);

/// The world's dialogue, which the temple keeps while the player is inside and gives back as the player leaves
class Dialogue
{
public:
	virtual ~Dialogue() = default;
	/// Both advisors fly home, the evil one first
	virtual void SendAdvisorsHome() = 0;
	/// Whether a script has the dialogue, or has the cinema bars in
	[[nodiscard]] virtual bool IsControlled() const = 0;
	/// The script task that has the dialogue, 0 for none
	[[nodiscard]] virtual uint32_t GetOwner() const = 0;
	/// Whether a task runs one of the temple's help or special scripts, which go on inside the temple
	[[nodiscard]] virtual bool IsTempleScript(uint32_t task) const = 0;
	/// The task asks for the dialogue: it gets it when no task has it
	virtual void GiveTo(uint32_t task) = 0;
	/// No task has the dialogue now, and the text box closes, keeping its texts
	virtual void TakeAway() = 0;
	[[nodiscard]] virtual help::DialogueText::Held HoldText() const = 0;
	virtual void RestoreText(const help::DialogueText::Held& held) = 0;
	/// The box shows the texts it keeps again, the newest's reading time starting again
	virtual void ShowTextAgain() = 0;
};

/// The world's dialogue the temple keeps while the player is inside
struct HeldDialogue
{
	uint32_t owner {0};
	help::DialogueText::Held text;
};

/// The player goes into the temple: the help scripts stop and the advisors fly home; when a script that is not one of
/// the temple's has the dialogue, the temple keeps its owner and texts, then nobody has the dialogue and the box closes.
/// What is kept, if anything
[[nodiscard]] std::optional<HeldDialogue> PutAwayDialogue(Scripts& scripts, Dialogue& dialogue);
/// The player leaves the temple: when the temple kept the dialogue and nobody has it now, its owner gets it back with its
/// texts and click waits; then, kept or not, the box shows its texts again
void BringBackDialogue(Dialogue& dialogue, std::optional<HeldDialogue>& held);

} // namespace openblack::temple_help
