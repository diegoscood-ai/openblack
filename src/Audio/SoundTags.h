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

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio::sound_tags
{

/// The original's SoundTag (SoundTag.cpp 0x71E300..0x71ED90, `new(0x54)`, linked in g_game+0x205C1C): a looping 3D
/// sample tied to a thing or a fixed point, (re)started by `ProcessSoundTags` every game turn. Only what the ported
/// users need: play mode 2 (a playing channel is left alone), 3D, not tracked, the InGame bank, no delay. The sample
/// plays on one of LHaudio's 16 channels (audio::sample_play) with the tag itself as the channel's owner (0x71E6F1).
/// Research: dev\tmp_dis\mapa\flecos_lantern-sound.md (SoundTag, GAudio::PlaySoundEffect) and dev\tmp_dis\agua\audio.md
/// §6 (the designed waterfall's tag).
using TagId = uint32_t;
inline constexpr TagId k_NoTag = 0;

struct TagDesc
{
	/// The sample (an InGame.sad SoundId)
	entt::id_type sample {0};
	/// The tag's thing (+0xC). entt::null: a fixed point (a ScriptMarker; `point` is where it sounds)
	entt::entity thing {entt::null};
	/// With a thing: added to its position (+0x1C, e.g. (0, Object::GetHeight, 0)). Without: the absolute point.
	/// (ScriptMarker = GameThingWithPos::Get3DSoundPos 0x56FE20: (x, altitude + y above the land, z); a marker made
	/// with MapCoords(LHPoint) keeps y - altitude, so it sounds at the LHPoint's y itself.)
	glm::vec3 point {0.0f};
	/// loops (+0x3C): -1 = for ever
	bool loop {true};
	/// +0x4C: SoundTag::SetActive
	bool active {true};
};

/// SoundTag::Create 0x71E840 / 0x71E8C0
TagId Create(const TagDesc& desc);

/// SoundTag::SetActive 0x71E640: turning it off stops the sample at once (GAudio::StopPlayingSoundEffect)
void SetActive(TagId tag, bool active);

/// SoundTag::ToBeDeleted 0x71ECB0 -> CreateSoundTagForDeadObject: a playing loop is released (LHSampleReleaseLoop, it
/// ends with the current pass) and the tag dies when it stops; one that is not playing goes at once.
void Delete(TagId tag);

/// SoundTag::ProcessSoundTags 0x71E5F0, once per game turn from GGame::EndTurn. Per tag (fn_0071E680): a thing that is
/// gone deletes the tag; an active tag calls GAudio::PlaySoundEffect 0x42A100, which starts the sample only when the
/// camera is within the sample's max distance (.sad +0x26C) of the point, and does nothing while it plays (mode 2).
void ProcessTurn();

/// Every tag stopped and forgotten (a new map: the registry reset destroys the emitters, so their AL sources go first)
void Clear();

} // namespace openblack::audio::sound_tags
