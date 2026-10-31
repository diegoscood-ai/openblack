/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpTextSystem.h"

#include <cstdint>

#include <optional>
#include <utility>
#include <vector>

#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"
#include "Audio/Device/WaveBuffers.h"
#include "Audio/Game/Banks.h"
#include "Audio/WaveCueLabels.h"
#include "Common/HelpText.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "Help/Spirits.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

HelpTextSystem::HelpTextSystem()
    : _voices(VoiceAudio(), VoiceHooks())
{
}

help::AdvisorVoices::Audio HelpTextSystem::VoiceAudio()
{
	// Every line of the advisors plays with the advisors' owner
	const auto owner = []() { return audio::Owner::Key(audio::k_OwnerAdvisor); };
	help::AdvisorVoices::Audio voice;
	voice.lineCount = [this]() { return static_cast<uint32_t>(audio::BankSampleCount(_voicesBank)); };
	voice.start = [this, owner](uint32_t line) {
		const audio::sample_play::Options options {
		    .sound = audio::SampleId(_voicesBank, static_cast<int>(line)),
		    .is3D = false,
		    .owner = owner(),
		    .keepPcm = true,
		};
		// The advisor's position is not set: a 2D channel does not use it
		return audio::sample_play::Start(options) != audio::k_NoChannel;
	};
	voice.isPlaying = [this, owner](uint32_t line) {
		return audio::sample_play::IsPlaying(audio::SampleId(_voicesBank, static_cast<int>(line)), owner());
	};
	voice.percentageDone = [this, owner](uint32_t line) {
		return audio::sample_play::PercentageDone(audio::SampleId(_voicesBank, static_cast<int>(line)), owner());
	};
	voice.stop = [this, owner](uint32_t line) {
		audio::sample_play::Stop(audio::SampleId(_voicesBank, static_cast<int>(line)), owner());
	};
	voice.tickMs = []() { return audio::TickCount(); };
	voice.localRand = [](int32_t n) { return static_cast<uint32_t>(audio::tags::RandomSample(0, n)); };
	voice.recording = [this](uint32_t line) -> std::optional<help::AdvisorVoices::Recording> {
		const auto* sound = audio::sample_play::GetSound(audio::SampleId(_voicesBank, static_cast<int>(line)));
		if (sound == nullptr)
		{
			return std::nullopt;
		}
		// The PCM the channel's buffer was just made from, else (the buffer was made before) decoded once more, the
		// same PCM
		audio::wave_buffers::Pcm pcm;
		if (!audio::wave_buffers::TakeDecoded(*sound, pcm) && !audio::wave_buffers::Decode(*sound, pcm))
		{
			return std::nullopt;
		}
		help::AdvisorVoices::Recording recording;
		recording.channels = pcm.layout == audio::ChannelLayout::Stereo ? 2 : 1;
		recording.sampleRate = pcm.sampleRate > 0 ? pcm.sampleRate : sound->sampleRate;
		recording.samples = std::move(pcm.samples);
		// The labels of the wave's cue points, which the line's gesture tags are read from: the wave's own bytes, read
		// once more from its bank when only its place there is kept
		std::vector<uint8_t> wave;
		if (sound->buffer.empty() && !audio::banks::ReadWave(*sound, wave))
		{
			return recording;
		}
		std::vector<help::spirits::TagLabel> labels;
		for (auto& label : audio::ReadWaveCueLabels(sound->buffer.empty() ? wave : sound->buffer.front()))
		{
			labels.push_back({.text = std::move(label.text), .time = label.time});
		}
		recording.labels = std::move(labels);
		return recording;
	};
	// The mixer's position of the bank's first channel with the advisors' owner, the line not compared, as the original
	// reads it
	voice.playPositionMs = [this, owner](uint32_t) { return audio::sample_play::PlayPosition(_voicesBank, owner()); };
	voice.textLine = [](uint32_t text) { return audio::voices::Table().Get(text).sample; };
	voice.insideCitadel = []() { return audio::IsInsideCitadel(); };
	return voice;
}

help::AdvisorVoices::Hooks HelpTextSystem::VoiceHooks()
{
	// A line's tags go to the advisor that says it, and a stopped line's flush to the same advisor, once the advisors
	// are loaded
	help::AdvisorVoices::Hooks hooks;
	hooks.setTags = [](int advisor, std::vector<help::spirits::AudioTag> tags) {
		if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
		{
			Locator::advisorSystem::value().GetController().SetSentenceTags(advisor, std::move(tags));
		}
	};
	hooks.stopTags = [](int advisor, float before) {
		if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
		{
			Locator::advisorSystem::value().GetController().StopSentence(advisor, before);
		}
	};
	return hooks;
}

void HelpTextSystem::InitVoices(uint16_t bank)
{
	_voicesBank = bank;
	_voices.Init();
}

bool HelpTextSystem::RunText(bool singleLine, uint32_t text, int32_t withInteraction)
{
	if (_dialogue != nullptr)
	{
		static_cast<void>(_dialogue->RunText(singleLine, text, withInteraction));
	}
	return text < helptext::k_TextCount;
}

bool HelpTextSystem::IsTextRead() const
{
	return _dialogue != nullptr && _dialogue->IsTextRead();
}

void HelpTextSystem::ClearAllText()
{
	if (_dialogue != nullptr)
	{
		_dialogue->ClearAllText();
	}
}

void HelpTextSystem::CloseDialogue()
{
	if (_dialogue != nullptr)
	{
		_dialogue->CloseDialogue();
	}
}

void HelpTextSystem::InterruptAdvisors()
{
	// The good advisor first, each cut as a click cuts it
	_voices.Interrupt(help::AdvisorVoices::k_GoodSpirit, 1);
	_voices.Interrupt(help::AdvisorVoices::k_EvilSpirit, 1);
}

int32_t HelpTextSystem::GetNarrator(uint32_t text) const
{
	return helptext::GetEntry(text < helptext::k_TextCount ? text : 0).narrator;
}

const help::TextFrame* HelpTextSystem::Layout(glm::ivec2 screen, int barPixels, const help::WidthFn& widthFn) const
{
	if (_dialogue == nullptr)
	{
		return nullptr;
	}
	// laid out again only when the texts, their animation, the flags or the screen changed
	return &_dialogue->Layout(screen.x, screen.y, barPixels, widthFn);
}

void HelpTextSystem::SetSong(help::SongInputs song)
{
	if (_dialogue != nullptr)
	{
		_dialogue->Display().SetSong(song);
	}
}

std::optional<help::karaoke::Box> HelpTextSystem::FollowSong(const help::TextFrame& frame, uint32_t frameMs)
{
	return _dialogue != nullptr ? _dialogue->Display().FollowSong(frame, frameMs) : std::nullopt;
}
