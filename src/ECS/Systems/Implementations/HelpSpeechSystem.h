/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HelpSpeechSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our help texts' voices (Audio/Services/Voices.h); it keeps nothing of its own. The first voice is the narration's,
/// the second the extra one.
class HelpSpeechSystem final: public HelpSpeechSystemInterface
{
public:
	void Say(uint32_t text, audio::SpeechVoice voice, std::optional<glm::vec3> position) final;
	[[nodiscard]] bool IsSaying(uint32_t text, audio::SpeechVoice voice) final;
	[[nodiscard]] size_t GetSpokenTextCount() const final;
	void Stop(audio::SpeechVoice voice, audio::SpeechBank bank) final;
};

} // namespace openblack::ecs::systems
