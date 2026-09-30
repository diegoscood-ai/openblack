/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio
{

/// The sounds of the animation clips (docs/bw1-notes/animation.md; research dev\tmp_dis\anim\sounds_props.md): footsteps,
/// axe chops, screams... Data\SmallSounds.SAS gives each clip its sound group and events {ms, soundId, action}
/// (LoadAllAnimations 0x550180). When the clip time crosses an event (fn_00516510) the key {voice, group, surface,
/// soundId} picks a list of editor.sad samples in its LHAudioAnimArrayTable / LHAudioWaveNumTable (LHaudiodllR.dll), one
/// at random, heard only within the sample's max distance of the camera. Banter (0x92-0x94) comes from VillagersBanter.sad,
/// 0x92 at the villager's house; action 1 stops the list's samples playing for the object (the saw).
class AnimationSounds
{
public:
	/// Plays the events of the clip (an ANM_ index) with from <= time < to, for that villager or animal.
	static void Fire(entt::entity entity, int32_t clip, int32_t from, int32_t to);
	/// GAudio::SamplePlayAnimEffect 0x42A4B0 with no animation behind it (trees rustling, a tree rubbed by the hand):
	/// one random sample of the editor.sad row the key {voice, 2, group, surface, soundId} picks (-1 = any), heard at
	/// `position` and following `owner`. Does nothing when the row has no samples or the camera is out of range.
	static void PlayFromTable(entt::entity owner, glm::vec3 position, const std::array<int32_t, 5>& key);
	/// Per frame: the playing samples follow their object (the game's 3D callback 0x427200, Get3DSoundPos).
	static void Update();
	AnimationSounds() = delete;
};

} // namespace openblack::audio
