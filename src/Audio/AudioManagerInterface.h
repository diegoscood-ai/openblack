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
#include <string>

#include <entt/fwd.hpp>

#include "AudioDecoderInterface.h"
#include "AudioPlayerInterface.h"
#include "SampleOutput.h"
#include "Sound.h"
#include "SoundGroup.h"

namespace openblack
{

namespace audio
{

/// The audio device of openblack (Locator::audio): the OpenAL context, the listener, the 16 channels' sources and the
/// list of the registered banks. It plays nothing itself: every sample goes through audio:: (Audio.h) onto the
/// channels of audio::sample_play, the music through audio::music (MusicStream.h). Since audio milestone B5 there is no
/// emitter left: no AudioEmitter component, no CreateEmitter / PlayEmitter / PlaySound / PlayMusic.
class AudioManagerInterface
{
public:
	/// The channels' sources released (Locator shutting the audio down)
	virtual void Stop() = 0;
	/// Once a frame: the channels' finite loops (QMixer counts them as it mixes)
	virtual void Update() = 0;
	/// LHListenerUpdate from fn_004270D0, once a game turn: QMixer's listener goes to the camera (position, forward, up)
	virtual void UpdateListener() = 0;
	virtual const Sound& GetSound(entt::id_type id) = 0;
	/// The samples of each registered .sad bank by its file name (the debug panel's list)
	virtual void CreateSoundGroup(const std::string& name) = 0;
	virtual void AddToSoundGroup(const std::string& name, entt::id_type id) = 0;
	virtual const SoundGroup& GetSoundGroup(const std::string& name) = 0;
	virtual const std::map<std::string, SoundGroup>& GetSoundGroups() = 0;
	/// The 16 sample channels' device side (audio::sample_play), on this manager's OpenAL context
	[[nodiscard]] virtual SampleOutput& GetSampleOutput() = 0;
};
} // namespace audio
} // namespace openblack
