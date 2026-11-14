/*******************************************************************************
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
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// What an advisor's voice gives its body: the gesture tags of the line it says and its mouth's lip sync
/// (help::AdvisorVoices gives them, help::spirits reads them). The parser's bodies are in Help/Spirits.cpp.
namespace openblack::help::spirits
{

/// One audio tag (a 20-byte record): time, who, action, index, value
struct AudioTag
{
	float time {0.0f};
	int32_t who {0};    ///< 0 the speaker (T), 1 the other (O, !), 2 the good one (G), 3 the evil one (E), 4 both (B, *)
	int32_t action {0}; ///< 1 A play, 2 AS loop, 3 AR / R release, 4 E emotion, 5 L / 6 LS look mode, 7 LR restore
	int32_t index {0};  ///< anim (prefix match), emotion or look mode
	int32_t value {100};
};
/// The audio tag builder's per-label loop: only the text from a '[' is read, each group up to its ']', ';' or '/', and
/// the search goes on after it; in each group one tag is parsed at a time, "<talker><type> <name>[digits]" entries.
/// The first tag gets the cue's time, the ones after it 0 (they fire with the first, which the walker reaches first). `errors`
/// counts the "unrecognised ..." cases and an unclosed '['. `lastWord` is the parser's word buffer, which keeps its word from
/// one call to the next: a tag with no name takes the last one read.
[[nodiscard]] std::vector<AudioTag> ParseAudioTags(std::string_view label, float time, std::string& lastWord,
                                                   int* errors = nullptr);

/// One cue label of a voice recording and its time in seconds from the start
struct TagLabel
{
	std::string text;
	float time;
};
/// A sentence's tags from all its labels in order; any error in any label leaves it with none
[[nodiscard]] std::vector<AudioTag> BuildSentenceTags(std::span<const TagLabel> labels, std::string& lastWord);

/// What the lip sync left for the mouth
struct LipSyncFrame
{
	/// (the clock - the sentence's start tick) x 0.001, then the play position x 0.001 when it is >= 0. The tag
	/// walker runs on it in both cases, so the tags fire on the clock's time while the play position is still -1 (Say's
	/// delay of up to 500 ms)
	float time {0.0f};
	/// The play position was >= 0: the vowels run
	bool playing {false};
	/// The key the last lip sync key step left (stale when it was skipped, no recording)
	std::array<float, 3> weights {};
};

} // namespace openblack::help::spirits
