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
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The creature's web page, the "Creature Cave" page the game writes about the player's own creature into the
/// profile's folder each time it saves the creature's mind. The page is the game's template, Scripts/creature_html.tmpl,
/// with each [$key$] in it replaced by the creature's value for that key. Everything here is pure: the values, the
/// template filled in and the page's file name.
namespace openblack::creature_web_page
{

/// The template, in the game's Scripts folder
inline constexpr std::string_view k_TemplateFile = "creature_html.tmpl";
/// The folder of the profile's folder the pages go in
inline constexpr std::string_view k_Folder = "html";

/// The creature's mind is saved, and its page written, each time it has lived this many more turns
inline constexpr uint32_t k_TurnsBetweenSaves = 600;
/// Whether the turn the creature has just lived up to is one its mind is saved at: every so many turns, never before
/// its first
[[nodiscard]] constexpr bool IsSaveTurn(uint32_t turnsLived)
{
	return turnsLived > 0 && turnsLived % k_TurnsBetweenSaves == 0;
}

/// The kinds of thing a creature keeps beliefs about, each a value of the page
inline constexpr size_t k_BeliefKinds = 24;
/// The miracles that can be cast on a creature
inline constexpr size_t k_ReceivedSpells = 16;
/// The game's texts that "how_nice_player_has_been" and "how_much_attention_player_has_given_me" give the number of:
/// the first of the seven alignment words (from the most evil) and of the five words for how much attention
inline constexpr int32_t k_FirstNiceText = 0x46A;
inline constexpr int32_t k_FirstAttentionText = 0x471;

/// What the page tells of the creature, as plain data gathered from its components
struct Facts
{
	std::u16string name;
	/// Its life, 0 to 1
	float health {1.0f};
	/// -1 evil to 1 good
	float alignment {0.0f};
	/// In its hours of game time, which the page calls its age
	uint32_t age {0};
	float energy {1.0f};
	float strength {0.5f};
	float fatness {0.5f};
	float exhaustion {0.0f};
	float dehydration {0.0f};
	float itchiness {0.0f};
	float poo {0.0f};
	/// People, animals and creatures killed, battles fought, battles won, poos and mushrooms eaten
	std::array<uint32_t, 7> tallies {};
	/// Each desire in the order of the game's tables: its name in the tables, and how much it is felt
	struct Desire
	{
		std::string name;
		float level {0.0f};
	};
	std::vector<Desire> desires;
	/// How many beliefs it holds of each kind
	std::array<uint32_t, k_BeliefKinds> beliefs {};
	/// The first of the miracles on it now (cast and waiting, starting, holding or finishing), by its place in the
	/// game's tables; none when no miracle is on it
	std::optional<uint32_t> spellReceived;
	/// What it thinks of its god, -1 to 1
	float attitudeToPlayer {0.0f};
	/// The game turns its god looked at it, and the turns it has lived
	uint32_t turnsLookedAt {0};
	uint32_t turnsLived {0};
};

/// One value of the page, by the key the template names it with
struct Field
{
	std::string key;
	std::string value;
};

/// The page's values, in the order the game sets them: name, health, alignment, age, energy, strength, fatness,
/// exhaustion, dehydration, itchiness, amount-of-poo, the seven tallies, a desire-<name> for each desire, beliefs_<n>
/// for each kind, spell_received_<text> for the miracle on it, how_nice_player_has_been and
/// how_much_attention_player_has_given_me. The values with a fraction have two digits after the point, as the game's C
/// library writes them (fixed_format::Fixed); the counts are whole numbers
[[nodiscard]] std::vector<Field> FieldsOf(const Facts& facts);

/// The template filled in, as the game writes the page from it: read as a text file (a carriage return before a line
/// feed is dropped, and the text ends at the first Ctrl+Z), line by line in pieces of at most 2047 characters, each
/// "[$key$]" replaced by the key's value (the last one given of that key). A key that has no value ends the piece of
/// line there, and a piece ends at a zero byte. Written as a text file again: each line feed as a carriage return and a
/// line feed
[[nodiscard]] std::string Fill(std::string_view templateText, std::span<const Field> fields);

/// A name in the game's 8-bit text, converted as the game converts it: up to its first zero, through the machine's own
/// code page, with Windows' best fit and default character for what the code page lacks; nothing when it takes 0x3FF
/// bytes or more. Off Windows, the Western code page (1252): the characters it has as their byte, any other as "?"
[[nodiscard]] std::string Narrow(std::u16string_view text);

/// The page's file in the profile's folder: html/<the creature's name>.html
[[nodiscard]] std::filesystem::path PagePath(std::u16string_view creatureName);

} // namespace openblack::creature_web_page
