/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace openblack::audio
{

/// The sounds of the animation clips (docs/bw1-notes/animation.md; research dev\tmp_dis\anim\sounds_props.md): footsteps,
/// axe chops, screams... Data\SmallSounds.SAS gives each clip its sound group and events {ms, soundId, action}
/// (LoadAllAnimations 0x550180). When the clip time crosses an event (fn_00516510) the key {voice, group, surface,
/// soundId} picks a list of editor.sad samples in its LHAudioAnimArrayTable / LHAudioWaveNumTable (LHaudiodllR.dll), one
/// at random, heard only within the sample's max distance of the camera.
class AnimationSounds
{
public:
	/// Plays the events of the clip (an ANM_ index) with from <= time < to, for that villager or animal.
	static void Fire(entt::entity entity, int32_t clip, int32_t from, int32_t to);
	AnimationSounds() = delete;
};

} // namespace openblack::audio
