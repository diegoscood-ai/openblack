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

#include "Common/Zoomer.h"

// The missionaries' song karaoke: while one of the song's verses plays as the script's music, the newest dialogue text
// shows a soft box behind the word being sung, which glides from word to word. The verse's markers count the beats of
// the line (the script audio state's music beat), and the word whose index in the text is that count is the sung one.
// Pure logic: the renderer draws the box as the help's glow panel, right before the word.

namespace openblack::help::karaoke
{

/// The script music types of the song's three verses
constexpr int32_t k_FirstVerseMusic = 51;
constexpr int32_t k_LastVerseMusic = 53;

/// Whether the game's language follows the song with the box: every language but 8 (br), 10 (sc), 11 (tc) and those past
/// 12
[[nodiscard]] bool UsesBouncingBall(uint32_t language);

/// Whether the script's music type is one of the song's verses
[[nodiscard]] constexpr bool IsVerseMusic(int32_t scriptMusicType)
{
	return scriptMusicType >= k_FirstVerseMusic && scriptMusicType <= k_LastVerseMusic;
}

/// Whether the word is the sung one: the language uses the box, the text is the newest of the stack, a verse plays, and
/// the word's index (1 for the text's first piece, counting every piece the splitter gives, codes included) is the beat
[[nodiscard]] constexpr bool IsSungWord(bool bouncingBall, bool newestText, bool versePlaying, int32_t wordIndex,
                                        int32_t musicBeat)
{
	return bouncingBall && newestText && versePlaying && wordIndex == musicBeat;
}

/// The box, in whole pixels (inclusive corners as the glow panel takes them), in the word's colour at half its alpha
struct Box
{
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
	std::array<uint8_t, 4> rgba;
};

/// The box's place and width, each moved by its own Zoomer. There is one for the whole game, made with the text display:
/// it starts at the left of the screen's bottom edge with no width
class Highlight
{
public:
	/// The time each step heads for the word in
	static constexpr float k_Seconds = 0.18f;

	explicit Highlight(int32_t screenHeight);

	/// One frame of the box behind the sung word, whose top left is (x, y) and whose width is the word's; lineHeight is
	/// the text's. Each Zoomer is sent to the word in k_Seconds, from where it is, and moved on by the frame's time
	/// (the frame's clamped milliseconds); the box is what they give
	[[nodiscard]] Box Follow(float x, float y, float width, float lineHeight, std::array<uint8_t, 4> wordRgba,
	                         uint32_t frameMs);

	[[nodiscard]] const Zoomer& X() const { return _x; }
	[[nodiscard]] const Zoomer& Y() const { return _y; }
	[[nodiscard]] const Zoomer& Width() const { return _width; }

private:
	Zoomer _x;
	Zoomer _y;
	Zoomer _width;
};

} // namespace openblack::help::karaoke
