/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AudioManager.h"

#include "AudioPlayerInterface.h"
#include "Camera/Camera.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "WaveBuffers.h"

namespace openblack::audio
{

AudioManager::AudioManager()
    : _audioPlayer(new AudioPlayer())
{
	_audioPlayer->Initialize();
	_sampleOutput = std::make_unique<AlSampleOutput>();
}

AudioManager::~AudioManager()
{
	// the channels' sources, then the wave buffers they played (one per sample, shared), before the context
	_sampleOutput.reset();
	wave_buffers::DeleteAll();
}

void AudioManager::Stop()
{
	_sampleOutput->DeleteAll();
}

void AudioManager::UpdateListener()
{
	// fn_004270D0 -> LHListenerUpdate (0x10003960: QSWaveMixSetListenerPosition / Orientation), once a game turn: the
	// camera's position, forward and up; the velocity stays 0 (QSWaveMixSetListenerVelocity(0) at 0x10015C1A)
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	_audioPlayer->UpdateListener(camera.GetOrigin(), glm::vec3(0.0f), camera.GetForward(), camera.GetUp());
}

void AudioManager::Update()
{
	// the 16 channels' finite loops (QMixer counts them as it mixes)
	_sampleOutput->Update();
}

const Sound& AudioManager::GetSound(entt::id_type id)
{
	return Locator::resources::value().GetSounds().Handle(id);
}

void AudioManager::CreateSoundGroup(const std::string& name)
{
	_soundGroups[name] = SoundGroup();
}

void AudioManager::AddToSoundGroup(const std::string& name, entt::id_type id)
{
	_soundGroups[name].sounds.emplace_back(id);
}

const SoundGroup& AudioManager::GetSoundGroup(const std::string& name)
{
	return _soundGroups[name];
}

const std::map<std::string, SoundGroup>& AudioManager::GetSoundGroups()
{
	return _soundGroups;
}
} // namespace openblack::audio
