/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The SETTINGS: switch's file: the quest folder its QUESTDIR line gives, read as the game reads it.
// docs/wiki/project/progress/easter_eggs/launch_switches_and_files.md

#include <string>

#include <gtest/gtest.h>

#include "Game/SettingsFile.h"

using namespace openblack::settings_file;

TEST(SettingsFile, WithoutAQuestFolderLineTheGamesOwnStays)
{
	EXPECT_EQ(QuestFolder(""), k_DefaultQuestFolder);
	EXPECT_EQ(QuestFolder("OTHER .\\Elsewhere\\\n"), k_DefaultQuestFolder);
	EXPECT_EQ(k_DefaultQuestFolder, ".\\Scripts\\Quests\\");
}

TEST(SettingsFile, AQuestFolderLineGivesItsWordAsItIs)
{
	EXPECT_EQ(QuestFolder("QUESTDIR .\\MyQuests\\\r\n"), ".\\MyQuests\\");
	// spaces, commas and tabs split the words, before and after
	EXPECT_EQ(QuestFolder("  QUESTDIR,\tC:\\Mods\\ ignored\n"), "C:\\Mods\\");
	// the last line wins; a line with no second word changes nothing
	EXPECT_EQ(QuestFolder("QUESTDIR a\\\nQUESTDIR b\\\nQUESTDIR\n"), "b\\");
	// the word is in capitals
	EXPECT_EQ(QuestFolder("questdir a\\\n"), k_DefaultQuestFolder);
	// no separator is added before the program's name
	EXPECT_EQ(QuestFolder("QUESTDIR Mine\n") + std::string(k_ChallengeFile), "MineChallenge.chl");
}

TEST(SettingsFile, ALineStartingWithASlashOrWithOneSecondIsAComment)
{
	EXPECT_EQ(QuestFolder("//QUESTDIR a\\\n"), k_DefaultQuestFolder);
	EXPECT_EQ(QuestFolder("/QUESTDIR a\\\n"), k_DefaultQuestFolder);
	EXPECT_EQ(QuestFolder("Q/ x\nQUESTDIR b\\\n"), "b\\");
	EXPECT_EQ(QuestFolder("QUESTDIR b\\\n  // QUESTDIR c\\\n"), "b\\");
}

TEST(SettingsFile, ItIsReadAsATextFile)
{
	// a Ctrl+Z ends the file
	EXPECT_EQ(QuestFolder("QUESTDIR a\\\n\x1AQUESTDIR b\\\n"), "a\\");
	// a carriage return alone is part of the word
	EXPECT_EQ(QuestFolder("QUESTDIR a\r"), "a\r");
}

TEST(SettingsFile, ALongLineIsReadInPieces)
{
	// the second piece of a long line is read as a line of its own
	const std::string longLine(k_LinePiece, 'x');
	EXPECT_EQ(QuestFolder(longLine + "QUESTDIR a\\\n"), "a\\");
	// a word cut at the piece's end ends there
	const std::string cut(k_LinePiece - 9, 'y');
	EXPECT_EQ(QuestFolder("QUESTDIR " + cut + "b\\\n"), cut);
}
