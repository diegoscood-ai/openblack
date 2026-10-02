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
/// (LoadAllAnimations 0x550180). When the clip time crosses an event (fn_00516510) the key {voice, 2, group, surface,
/// soundId} goes to GAudio::SamplePlayAnimEffect 0x42A4B0 (audio::SamplePlayAnimEffect, milestone B2) with the camera's
/// distance to the animated thing: one sample of the editor.sad row on one of LHaudio's 16 channels, following its
/// owner. Banter (0x92-0x94) comes from VillagersBanter.sad, 0x92 at the villager's house; action 1 stops the list's
/// samples playing for the owner (the saw).
class AnimationSounds
{
public:
	/// fn_00516510: plays the events of the clip (an ANM_ index) with from <= time < to, for that villager or animal (what
	/// it reads of the thing: GameQueries::animatedThing; the clips' names: GameQueries::animationClipName).
	static void Fire(entt::entity entity, int32_t clip, int32_t from, int32_t to);
	/// GAudio::SamplePlayAnimEffect 0x42A4B0 with no animation behind it (Tree::Draw: trees rustling, a tree bent by the
	/// hand): one sample of the editor.sad row the key {voice, 2, group, surface, soundId} picks (-1 = only rows with a
	/// wildcard there), with the camera's distance to `position`, owned by `owner` (its point is the owner's). Nothing
	/// when the row has no samples or the camera is out of range.
	static void PlayFromTable(entt::entity owner, glm::vec3 position, const std::array<int32_t, 5>& key);
	/// Nothing (kept for its caller): the channels follow their owner once a turn (LHSampleUpdate3DChannels 0x10014310
	/// from GAudio::ProcessAudioGameTurn), not every frame.
	static void Update();
	AnimationSounds() = delete;
};

} // namespace openblack::audio
