/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpSpeechSystem.h"

#include "Audio/Game/BankTables.h"
#include "Audio/Services/Voices.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The second voice is the extra one
bool IsExtra(audio::SpeechVoice voice)
{
	return voice == audio::SpeechVoice::Second;
}

audio::SfxBank BankOf(audio::SpeechBank bank)
{
	switch (bank)
	{
	case audio::SpeechBank::Villagers:
		return audio::SfxBank::Villagers;
	case audio::SpeechBank::HelpSprites:
		return audio::SfxBank::HelpSprites;
	case audio::SpeechBank::Guidance:
		return audio::SfxBank::Guidance;
	}
	return audio::SfxBank::None;
}

} // namespace

void HelpSpeechSystem::Say(uint32_t text, audio::SpeechVoice voice, std::optional<glm::vec3> position)
{
	audio::voices::Say(text, position.has_value(), IsExtra(voice), position.value_or(glm::vec3 {0.0f}));
}

bool HelpSpeechSystem::IsSaying(uint32_t text, audio::SpeechVoice voice)
{
	return audio::voices::IsSaying(IsExtra(voice), text);
}

size_t HelpSpeechSystem::GetSpokenTextCount() const
{
	return audio::voices::Table().GetSpokenCount();
}

void HelpSpeechSystem::Stop(audio::SpeechVoice voice, audio::SpeechBank bank)
{
	audio::voices::Stop(IsExtra(voice), BankOf(bank));
}
