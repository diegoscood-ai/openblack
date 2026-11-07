/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our help system's texts (Help/HelpSystem.h): it keeps the interface's text database, owns the dialogue the help
/// system makes at its start and takes away at its shutdown, and holds the advisors' voices for as long as it lives;
/// the rest of the help system lives in the script state. Before the start a text shows nothing and is never read.
class HelpTextSystem final: public HelpTextSystemInterface
{
public:
	HelpTextSystem();
	HelpTextSystem(const HelpTextSystem&) = delete;
	HelpTextSystem& operator=(const HelpTextSystem&) = delete;
	HelpTextSystem(HelpTextSystem&&) = delete;
	HelpTextSystem& operator=(HelpTextSystem&&) = delete;
	~HelpTextSystem() override = default;

	[[nodiscard]] bool IsStarted() const final { return _dialogue != nullptr; }
	void SetTexts(const gui::TextDatabase* texts) final { _texts = texts; }
	[[nodiscard]] const gui::TextDatabase* GetTexts() const final { return _texts; }
	void Start(std::unique_ptr<help::DialogueText> dialogue) final { _dialogue = std::move(dialogue); }
	void Shutdown() final { _dialogue.reset(); }
	[[nodiscard]] help::DialogueText* GetDialogue() final { return _dialogue.get(); }
	[[nodiscard]] const help::DialogueText* GetDialogue() const final { return _dialogue.get(); }
	[[nodiscard]] help::AdvisorVoices& GetVoices() final { return _voices; }
	[[nodiscard]] const help::AdvisorVoices& GetVoices() const final { return _voices; }
	void InitVoices(uint16_t bank) final;
	bool RunText(bool singleLine, uint32_t text, int32_t withInteraction) final;
	[[nodiscard]] bool IsTextRead() const final;
	void ClearAllText() final;
	void CloseDialogue() final;
	void InterruptAdvisors() final;
	[[nodiscard]] int32_t GetNarrator(uint32_t text) const final;
	[[nodiscard]] const help::TextFrame* Layout(glm::ivec2 screen, int barPixels, const help::WidthFn& widthFn) const final;
	void SetSong(help::SongInputs song) final;
	[[nodiscard]] std::optional<help::karaoke::Box> FollowSong(const help::TextFrame& frame, uint32_t frameMs) final;

private:
	/// The voices' sounds and clock, from the game's audio: the advisors' bank on the sample player with the advisors'
	/// owner, its play position the mixer's
	[[nodiscard]] help::AdvisorVoices::Audio VoiceAudio();
	/// A line's gesture tags and a stopped line's flush, given to the advisors' control
	[[nodiscard]] static help::AdvisorVoices::Hooks VoiceHooks();

	const gui::TextDatabase* _texts {nullptr};
	/// The advisors' bank, given by InitVoices
	uint16_t _voicesBank {0};
	help::AdvisorVoices _voices;
	std::unique_ptr<help::DialogueText> _dialogue;
};

} // namespace openblack::ecs::systems
