/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>

/// The settings file the game's SETTINGS: launch switch names, read once as the game starts. Its one setting is the
/// folder the challenge scripts are read from, so a changed challenge program can be played without replacing the
/// game's own. Pure: the file's text in, the folder out.
namespace openblack::settings_file
{

/// The quest folder without a settings file, or when the file gives none, as the game writes it
inline constexpr std::string_view k_DefaultQuestFolder = ".\\Scripts\\Quests\\";
/// The challenge program's name, put right after the quest folder as it is written (no separator is added)
inline constexpr std::string_view k_ChallengeFile = "Challenge.chl";
/// The setting's word
inline constexpr std::string_view k_QuestFolderWord = "QUESTDIR";
/// How much of a line one read takes: a longer line is read in pieces of this many characters, each read as a line
inline constexpr size_t k_LinePiece = 4095;

/// The quest folder the settings file's text gives, read as a text file: a Ctrl+Z ends it and each "\r\n" is a line's
/// end. On each line the words are split by spaces, commas, tabs and the line's end; a line whose first word starts
/// with "/" or has "/" as its second character is a comment. A line whose first word is QUESTDIR (in capitals) and
/// has a second word sets the folder to that word, as it is; the last such line wins. With none, the default folder
[[nodiscard]] std::string QuestFolder(std::string_view text);

} // namespace openblack::settings_file
