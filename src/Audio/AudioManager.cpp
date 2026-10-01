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

#include <cstdlib>
#include <fstream>

#include <PackFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "AudioPlayerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"
#include "MpegAudioDecoder.h"
#include "Resources/Resources.h"
#include "WavAudioDecoder.h"

using namespace openblack::ecs::components;

namespace openblack::audio
{

namespace
{
/// OPENBLACK_AUDIO_TRACE=1: every PlayAt (start or cull) and, twice a second, every playing emitter (AL pitch, camera
/// distance, distance gain, muted)
bool AudioTrace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

/// The channel's distance gain in QMixer (0x1800ACDF .. 0x1800AE1A with the channel flags LHSamplePlay sets, 0x103 /
/// 0x111: neither 0x8 "clamp at max" nor 0x1000 "linear"; 0x1802CE50): 1 up to min (or with scale 0),
/// min / (min + scale * (d - min)) up to max, 0 beyond max
float MappingGain(const Sound& sound, float d)
{
	if (d > sound.mappingMaxDistance)
	{
		return 0.0f;
	}
	if (d <= sound.minDistance || sound.scale == 0.0f)
	{
		return 1.0f;
	}
	return sound.minDistance / (sound.minDistance + sound.scale * (d - sound.minDistance));
}

/// QSWaveMixSetFrequency(rate * percent / 100) as a ratio of the sample's rate (the integer division of 0x10012820)
float FrequencyRatio(int sampleRate, int percent)
{
	if (sampleRate <= 0)
	{
		return static_cast<float>(percent) / 100.0f;
	}
	const auto frequency = static_cast<uint32_t>(sampleRate) * static_cast<uint32_t>(percent) / 100u;
	return static_cast<float>(frequency) / static_cast<float>(sampleRate);
}

/// Which side of the listener a world point is heard on, as OpenAL builds it: the listener's right is at x up of the
/// swapped (x <-> z) vectors AudioPlayer::UpdateListener sends. > 0 = right.
float AlSide(glm::vec3 point)
{
	const auto& camera = Locator::camera::value();
	const auto swap = [](glm::vec3 v) { return glm::vec3(v.z, v.y, v.x); };
	const glm::vec3 right = glm::cross(swap(camera.GetForward()), swap(camera.GetUp()));
	return glm::dot(swap(point - camera.GetOrigin()), right);
}
} // namespace

AudioManager::AudioManager()
    : _audioPlayer(new AudioPlayer())
{
	_audioPlayer->Initialize();
}

AudioManager::~AudioManager()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Transform, AudioEmitter>([this](entt::entity entity, const Transform&, const AudioEmitter& emitter) {
		DestroyEmitter(entity);
		auto sound = Locator::resources::value().GetSounds().Handle(emitter.soundId);
		_audioPlayer->DeleteBuffer(sound->bufferId);
	});

	if (registry.Valid(_musicEntity))
	{
		DestroyEmitter(_musicEntity);
	}
}

void AudioManager::Stop()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Transform, AudioEmitter>(
	    [this](entt::entity entity, const Transform&, const AudioEmitter&) { DestroyEmitter(entity); });
	StopMusic();
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
	_listenerPosition = camera.GetOrigin();
	_audioPlayer->UpdateListener(_listenerPosition, glm::vec3(0.0f), camera.GetForward(), camera.GetUp());
}

void AudioManager::Update()
{
	// QMixer mixes every channel against the listener of the last game turn (UpdateListener)
	const auto pos = _listenerPosition;
	auto& registry = Locator::entitiesRegistry::value();
	static uint32_t s_Frame = 0;
	const bool traceFrame = AudioTrace() && (s_Frame++ % 30) == 0;
	registry.Each<Transform, AudioEmitter>(
	    [this, pos, traceFrame](entt::entity entity, const Transform& transform, const AudioEmitter& emitter) {
		    auto volume = _globalVolume * emitter.volume;
		    // QMixer 0x1800ADDF: a channel farther than the max of its distance mapping is silent (it keeps playing)
		    const float distance =
		        emitter.relative ? glm::length(transform.position) : glm::distance(transform.position, pos);
		    const bool muted = emitter.cutDistance > 0.0f && distance > emitter.cutDistance;
		    if (muted)
		    {
			    volume = 0.0f;
		    }
		    if (traceFrame && entity != _musicEntity)
		    {
			    const auto& sound = Locator::resources::value().GetSounds().Handle(emitter.soundId);
			    SPDLOG_LOGGER_INFO(spdlog::get("audio"),
			                       "Audio trace: {} {} at ({:.1f}, {:.1f}, {:.1f}) distance {:.1f} pitch {:.3f} gain {:.3f}{}",
			                       sound->name, emitter.relative ? "relative" : "world", transform.position.x,
			                       transform.position.y, transform.position.z, distance,
			                       _audioPlayer->GetSourcePitch(emitter.sourceId), muted ? 0.0f : MappingGain(*sound, distance),
			                       muted ? " MUTED (beyond max)" : "");
		    }
		    if (entity == _musicEntity)
		    {
			    volume *= _musicVolume;
		    }
		    else
		    {
			    volume *= _sfxVolume;
		    }
		    _audioPlayer->UpdateSource(emitter.sourceId, transform.position, volume, emitter.loop == PlayType::Repeat);
		    auto audioStatus = _audioPlayer->GetStatus(emitter.sourceId);
		    if (audioStatus == AudioStatus::Stopped)
		    {
			    if (AudioTrace() && entity != _musicEntity)
			    {
				    SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio trace: {} ended",
				                       Locator::resources::value().GetSounds().Handle(emitter.soundId)->name);
			    }
			    DestroyEmitter(entity);
		    }
	    });
}

BufferId AudioManager::CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate)
{
	return _audioPlayer->CreateBuffer(layout, buffer, sampleRate);
}

void AudioManager::PlayEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& emitterComponent = registry.Get<AudioEmitter>(emitter);
	auto& transform = registry.Get<Transform>(emitter);
	_audioPlayer->PlaySource(emitterComponent.sourceId, transform.position, 1.f, emitterComponent.loop == PlayType::Repeat);
}

void AudioManager::PauseEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	_audioPlayer->PauseSource(component.sourceId);
}

void AudioManager::SetEmitterPitch(entt::entity emitter, float percent)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(emitter) || !registry.AnyOf<AudioEmitter>(emitter))
	{
		return;
	}
	// LHSampleSetPitch 0x10013520: QSWaveMixSetFrequency(rate * p / 100), integers
	const auto& emitterComponent = registry.Get<AudioEmitter>(emitter);
	const auto& sound = Locator::resources::value().GetSounds().Handle(emitterComponent.soundId);
	_audioPlayer->SetSourcePitch(emitterComponent.sourceId,
	                             FrequencyRatio(sound->sampleRate, std::max(static_cast<int>(percent), 1)));
}

void AudioManager::StopEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	_audioPlayer->StopSource(component.sourceId);
}

void AudioManager::DestroyEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	_audioPlayer->DeleteSource(component.sourceId);
	registry.Destroy(emitter);
}

entt::entity AudioManager::CreateEmitter(entt::id_type id, PlayType playType, glm::vec3 position, glm::vec3 direction,
                                         glm::vec2 radius, float volume, AudioStatus status, bool relative)
{
	auto sound = Locator::resources::value().GetSounds().Handle(id);
	auto& registry = Locator::entitiesRegistry::value();
	auto entity = registry.Create();
	// the start of a sample (LHaudiodllR 0x1001278B..0x1001283B, unsigned integers): p = pitch (0 -> 100),
	// d = deviation * p / 100, p = p - d + rand() * 2d / 32767 (0 -> 100); QSWaveMixSetFrequency(rate * p / 100)
	uint32_t pitch = static_cast<uint32_t>(sound->pitch > 0 ? sound->pitch : 100);
	const uint32_t d = static_cast<uint32_t>(std::max(sound->pitchDeviation, 0)) * pitch / 100;
	pitch -= d;
	pitch += static_cast<uint32_t>(Locator::rng::value().NextValue<int32_t>(0, 32767)) * (2 * d) / 32767;
	if (pitch == 0)
	{
		pitch = 100;
	}
	auto sourceId = _audioPlayer->CreateSource(FrequencyRatio(sound->sampleRate, static_cast<int>(pitch)), relative);
	// QSWaveMixSetDistanceMapping {min, max, scale} of the channel (LHaudiodllR 0x10012159)
	_audioPlayer->SetSourceDistance(sourceId, sound->minDistance, sound->mappingMaxDistance, sound->scale);
	if (!sound->buffer.empty())
	{
		CreateBuffer(sound);
	}
	_audioPlayer->QueueBuffer(sourceId, sound->bufferId);
	registry.Assign<AudioEmitter>(entity, sourceId, id, 0, position, direction, radius, volume, playType, status, relative,
	                              sound->mappingMaxDistance);
	registry.Assign<Transform>(entity, glm::zero<glm::vec3>(), glm::one<glm::mat4>(), glm::one<glm::vec3>());
	return entity;
}

void AudioManager::CreateBuffer(Sound& sound)
{
	std::vector<int16_t> decodeBuffer;
	for (auto& buffer : sound.buffer)
	{
		// a .sad sample is a RIFF wave (LHaudio opens it with QSWaveMixOpenWaveEx); trying MPEG first let dr_mp3 find
		// frame syncs inside some waves (G_BigSplash_03 decoded to a fraction of a second)
		const bool riff = buffer.size() >= 4 && buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F';
		bool success = false;
		std::vector<int16_t> decoded;
		if (!riff)
		{
			auto decoder = audio::MpegAudioDecoder();
			success = decoder.Open(buffer);
			if (success)
			{
				decoder.Read(decoded);
				sound.channelLayout = decoder.GetChannelLayout();
			}
		}
		if (!success)
		{
			auto decoder = audio::WavAudioDecoder();
			success = decoder.Open(buffer);
			if (success)
			{
				decoder.Read(decoded);
				sound.channelLayout = decoder.GetChannelLayout();
			}
		}
		if (success)
		{
			decodeBuffer.insert(decodeBuffer.end(), decoded.begin(), decoded.end());
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Unable to decode sound");
		}
	}
	sound.bufferId = CreateBuffer(sound.channelLayout, decodeBuffer, sound.sampleRate);
	sound.duration = _audioPlayer->GetDuration(sound.bufferId);
	sound.sizeInBytes = decodeBuffer.size() * sizeof(decodeBuffer[0]);
}

bool AudioManager::EmitterExists(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(emitter) && registry.AnyOf<AudioEmitter>(emitter);
}

float AudioManager::GetProgress(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(entity));
	auto& emitter = registry.Get<AudioEmitter>(entity);
	auto sizeInBytes = Locator::resources::value().GetSounds().Handle(emitter.soundId)->sizeInBytes;
	return _audioPlayer->GetProgress(sizeInBytes, emitter.sourceId);
}

AudioStatus AudioManager::GetStatus(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	return _audioPlayer->GetStatus(component.sourceId);
}

const Sound& AudioManager::GetSound(entt::id_type id)
{
	return Locator::resources::value().GetSounds().Handle(id);
}

void AudioManager::PlaySound(entt::id_type id, PlayType playType)
{
	// a 2D sample (is3D 0): on the listener, no distance mapping
	auto position = glm::zero<glm::vec3>();
	auto direction = glm::zero<glm::vec3>();
	auto radius = glm::zero<glm::vec3>();
	auto sound = Locator::resources::value().GetSounds().Handle(id);
	auto entity = CreateEmitter(id, playType, position, direction, radius, sound->volume, AudioStatus::Playing, true);
	PlayEmitter(entity);
}

entt::entity AudioManager::PlayAt(entt::id_type id, glm::vec3 position)
{
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id) || !Locator::camera::has_value())
	{
		return entt::null;
	}
	const auto sound = sounds.Handle(id);
	// GAudio::PlaySoundEffect 0x429E30: GetGSFXSampleMaxDistance (.sad +0x26C, raw), or the options' max when that is 0;
	// skipped when the camera is farther than it
	const float maxDistance = sound->maxDistance != 0.0f ? sound->maxDistance : sound->mappingMaxDistance;
	const float distance = glm::distance(position, Locator::camera::value().GetOrigin());
	if (distance > maxDistance)
	{
		if (AudioTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio trace: PlayAt {} at ({:.1f}, {:.1f}, {:.1f}) culled: camera {:.1f} > max {:.1f}",
			                   sound->name, position.x, position.y, position.z, distance, maxDistance);
		}
		return entt::null;
	}
	const auto entity =
	    CreateEmitter(id, PlayType::Once, position, glm::zero<glm::vec3>(), glm::zero<glm::vec2>(), sound->volume,
	                  AudioStatus::Playing, false);
	Locator::entitiesRegistry::value().Get<Transform>(entity).position = position;
	PlayEmitter(entity);
	if (AudioTrace())
	{
		const auto& emitter = Locator::entitiesRegistry::value().Get<AudioEmitter>(entity);
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Audio trace: PlayAt {} at ({:.1f}, {:.1f}, {:.1f}): camera {:.1f} (max {:.1f}), pitch {:.3f} "
		                   "(.sad {} +-{}%), mapping min {:.0f} max {:.0f} scale {:.1f} -> gain {:.3f}, volume {:.3f}, {} "
		                   "(AL side {:.1f}, camera right {:.1f})",
		                   sound->name, position.x, position.y, position.z, distance, maxDistance,
		                   _audioPlayer->GetSourcePitch(emitter.sourceId), sound->pitch, sound->pitchDeviation,
		                   sound->minDistance, sound->mappingMaxDistance, sound->scale, MappingGain(*sound, distance),
		                   sound->volume, AlSide(position) >= 0.0f ? "right" : "left", AlSide(position),
		                   glm::dot(position - Locator::camera::value().GetOrigin(), Locator::camera::value().GetRight()));
	}
	return entity;
}

void AudioManager::CreateSoundGroup(const std::string& name)
{
	_soundGroups[name] = SoundGroup();
}

void AudioManager::AddMusicEntry(const std::string& name)
{
	_music.emplace_back(name);
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

void AudioManager::PlayMusic(const std::string& packPath, PlayType type)
{
	StopMusic();
	const entt::id_type id = entt::hashed_string(fmt::format("{}", packPath).c_str());
	if (!Locator::resources::value().GetSounds().Contains(packPath))
	{
		pack::PackFile soundPack;
		soundPack.Open(packPath);
		const auto& audioHeaders = soundPack.GetAudioSampleHeaders();
		const auto& audioData = soundPack.GetAudioSamplesData();
		Locator::resources::value().GetSounds().Load(id, resources::SoundLoader::FromBufferTag {}, audioHeaders[0], audioData);
	}
	auto sound = Locator::resources::value().GetSounds().Handle(id);
	auto position = glm::one<glm::vec3>();
	auto direction = glm::zero<glm::vec3>();
	auto radius = glm::zero<glm::vec3>();
	_musicEntity = CreateEmitter(id, type, position, direction, radius, sound->volume, AudioStatus::Playing, true);
	PlayEmitter(_musicEntity);
}

void AudioManager::StopMusic()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!EmitterExists(_musicEntity))
	{
		return;
	}
	auto& emitter = registry.Get<AudioEmitter>(_musicEntity);
	// Clean up the audio player's music resources
	_audioPlayer->StopSource(emitter.sourceId);
	_audioPlayer->DeleteSource(emitter.sourceId);
	[[maybe_unused]] auto music = Locator::resources::value().GetSounds().Handle(emitter.soundId);
	//	Erase the music resource as it is no longer being played
	Locator::resources::value().GetSounds().Erase(emitter.soundId);
	//	Remove the entity
	registry.Destroy(_musicEntity);
	_musicEntity = entt::null;
}
} // namespace openblack::audio
