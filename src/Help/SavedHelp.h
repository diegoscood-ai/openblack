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

#include <array>

// What a saved game keeps of the help system's own state, of the fields openblack models: the message sets' conditions,
// turns and counts, the category turns, the last set and its turn, the banter sets sent, the help level and the help
// switch; and the dialogue: the task that has it, its texts and click waits, and the dialogue the temple keeps while the
// player is inside. The wide screen and its owner are saved apart, with the bars' timer. See
// docs/wiki/engine/saved-games.md#the-end-of-the-game-object.

namespace openblack::help
{

/// A dialogue as the help system's save keeps it: the task that has it (0 for none), the last six texts shown, the
/// newest first (0 for none), and whether the newest waits for a click or takes none
struct SavedDialogue
{
	uint32_t owner {0};
	std::array<uint32_t, 6> texts {};
	bool clickToContinue {false};
	bool noSkip {false};

	bool operator==(const SavedDialogue&) const = default;
};

/// The help system's saved state, as a save writes it and a load puts it back
struct SavedHelp
{
	/// The message sets' conditions, this turn's
	std::array<uint32_t, 18> conditions {};
	/// The turn each message set was last sent
	std::array<uint32_t, 0x98> sentTurn {};
	/// The turn each of the nine categories was last triggered
	std::array<uint32_t, 9> categoryTurns {};
	/// How often each message set was sent
	std::array<uint32_t, 0x98> sentCount {};
	/// The last message set sent, and the turn the last help message started
	uint32_t lastSet {0};
	uint32_t messageTurn {0};
	/// The banter sets sent since the last restart
	uint16_t banterCount {0};
	/// The help level (3 without a profile) and the help switch
	int32_t helpLevel {3};
	uint32_t helpOn {1};
	/// The dialogue as it is
	SavedDialogue dialogue;
	/// The dialogue the temple took from its task as the player went in, and whether it holds it now. Once given back
	/// the flag goes and the dialogue stays as the last one kept
	SavedDialogue kept;
	bool textKept {false};

	bool operator==(const SavedHelp&) const = default;
};

} // namespace openblack::help
