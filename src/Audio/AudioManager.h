/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <string>

#include <glm/vec3.hpp>

#include "AudioManagerInterface.h"
#include "AlSampleOutput.h"
#include "AudioPlayer.h"
#include "SoundGroup.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::audio
{

class AudioManager final: public AudioManagerInterface
{
public:
	AudioManager();
	~AudioManager();
	const Sound& GetSound(entt::id_type id) override;
	void Stop() override;
	void Update() override;
	void UpdateListener() override;
	void CreateSoundGroup(const std::string& name) override;
	[[nodiscard]] SampleOutput& GetSampleOutput() override { return *_sampleOutput; }
	void AddToSoundGroup(const std::string& name, entt::id_type id) override;
	const SoundGroup& GetSoundGroup(const std::string& name) override;
	const std::map<std::string, SoundGroup>& GetSoundGroups() override;

private:
	/// The OpenAL device and context, and the listener
	std::unique_ptr<AudioPlayerInterface> _audioPlayer;
	/// The 16 sample channels' OpenAL sources (made after the context, deleted before it)
	std::unique_ptr<AlSampleOutput> _sampleOutput;
	/// The samples of each bank
	std::map<std::string, SoundGroup> _soundGroups;
};

} // namespace openblack::audio
