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

#include <memory>
#include <optional>

#include <glm/vec2.hpp>

#include "Help/HelpTextDisplay.h"

namespace openblack::gui
{
class TextDatabase;
}

namespace openblack::help
{
class DialogueText;
class AdvisorVoices;
} // namespace openblack::help

namespace openblack::ecs::systems
{

/// The dialogue the scripts show and have spoken: the help texts at the bottom of the screen, the advisors' and the
/// narrators' voices saying them, and the player clicking them on
class HelpTextSystemInterface
{
public:
	virtual ~HelpTextSystemInterface() = default;

	/// The help texts have been started
	[[nodiscard]] virtual bool IsStarted() const = 0;

	/// The interface's text database the help texts are read from, kept while the interface lives; none takes it away.
	/// Without one they are read from the help text cache
	virtual void SetTexts(const gui::TextDatabase* texts) = 0;
	[[nodiscard]] virtual const gui::TextDatabase* GetTexts() const = 0;

	/// The help system's start hands over the dialogue it made (help::Start), kept until the help system's shutdown
	/// takes it away (help::Shutdown)
	virtual void Start(std::unique_ptr<help::DialogueText> dialogue) = 0;
	virtual void Shutdown() = 0;
	/// The texts shown, nothing before the start
	[[nodiscard]] virtual help::DialogueText* GetDialogue() = 0;
	[[nodiscard]] virtual const help::DialogueText* GetDialogue() const = 0;
	/// The advisors' voices, kept as long as the slot
	[[nodiscard]] virtual help::AdvisorVoices& GetVoices() = 0;
	[[nodiscard]] virtual const help::AdvisorVoices& GetVoices() const = 0;
	/// The advisors' voices are given their bank (an audio::BankId) and read its number of lines, when the help system
	/// is made (help::AdvisorVoices::Init)
	virtual void InitVoices(uint16_t bank) = 0;

	/// A script shows a help text and has it said; false for an invalid text, which shows the first
	virtual bool RunText(bool singleLine, uint32_t text, int32_t withInteraction) = 0;
	/// The text the scripts last showed has been read
	[[nodiscard]] virtual bool IsTextRead() const = 0;
	/// Every text is taken away
	virtual void ClearAllText() = 0;
	/// The texts are taken away and their box closed
	virtual void CloseDialogue() = 0;
	/// Both advisors are cut short in what they say
	virtual void InterruptAdvisors() = 0;
	/// Who says a help text, a narrator of the help texts' script; a number past the last gives the first text's
	[[nodiscard]] virtual int32_t GetNarrator(uint32_t text) const = 0;

	/// The box and words to draw this frame for a screen with cinema bars of `barPixels`, nothing before the start
	[[nodiscard]] virtual const help::TextFrame* Layout(glm::ivec2 screen, int barPixels,
	                                                    const help::WidthFn& widthFn) const = 0;
	/// This frame's song, for the newest text's sung word (help::karaoke); nothing before the start
	virtual void SetSong(help::SongInputs /*song*/) {}
	/// The box behind the sung word of `frame`, moved on once this frame by its clamped ms; none before the start or with
	/// no sung word
	[[nodiscard]] virtual std::optional<help::karaoke::Box> FollowSong(const help::TextFrame& /*frame*/, uint32_t /*frameMs*/)
	{
		return std::nullopt;
	}
};

} // namespace openblack::ecs::systems
