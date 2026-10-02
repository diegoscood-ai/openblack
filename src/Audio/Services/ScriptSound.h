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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Audio.h"

// The script's sound effects (milestone B6 of dev\tmp_dis\audio\PLAN.md, dev\tmp_dis\audio\script.md §2.2): what the CHL
// functions of GScript do between their POPs and GAudio. CHLApi pops the arguments in the original's order and calls
// these. The bank argument is an AUDIO_SFX_BANK_TYPE (0x9CB3F8, chlasm AudioSFX.h) that the original indexes
// GAudio+0x3A8 with unchecked: (approximated) a value outside 0..10 plays / stops nothing here.

namespace openblack::audio::script_sound
{

/// PLAY_SOUND_EFFECT (043) 0x70F7F0: a default LH_SamplePlayOptions (ctor 0x10010E90) with bank +0x04 =
/// GAudio+0x3A8 + 4 * bank, sample +0x24, the owner +0x20 = the sample number itself (Owner::Key), is3D +0x08 = withPos,
/// track +0x0C = 0, the point +0x30 (copied even when withPos is 0), +0x164 = 1 (keep the PCM); then
/// GAudio::PlaySoundEffect 0x429E30 with no field of the caller's mask set (+0x1C = 0: the .sad decides). Returns the
/// channel (the original returns nothing).
Channel PlaySoundEffect(int sample, int bank, glm::vec3 position, bool withPosition);

/// STOP_SOUND_EFFECT (424) 0x70FA50: not isSay -> GAudio::StopPlayingSoundEffect(id, owner id, bank) 0x42A210 (the
/// channel of PLAY_SOUND_EFFECT). isSay: id is a HELP_TEXT and `bank` is not used; the narrator of
/// HelpTextDatabase[0 < id < count ? id : 0] (0x70FA8D..0x70FAAD) and the voice {bank, sample} of the say table
/// 0x942B38 (+4 / +8, id unchecked); the good spirit (narrator 2) -> stop(sample, 0x270C, bank), else stop(sample,
/// 0x270E, bank) and stop(sample, 0x270D, bank). The 2D voice of RUN_TEXT / SAY without alt (0x270F) is never stopped.
void StopSoundEffect(bool isSay, uint32_t id, int bank);

/// GAME_SOUND_PLAYING (450) 0x710230: fn_0042A280(owner = sample, sample, bank) -> LHSampleIsPlaying(bank, owner,
/// sample) 0x10013ED0
[[nodiscard]] bool GameSoundPlaying(int sample, int bank);

/// ATTACH_SOUND_TAG (447) 0x710150: SoundTag::Create(thing, sample, track = threeD != 0, mode 2, loops 0, +0x40 0,
/// is3D = threeD, bank, delay 0) 0x71E840 (no offset). Nothing for no thing (GetScriptGameThing 0x70D220 gives null).
tags::TagId AttachSoundTag(bool threeD, int sample, int bank, entt::entity thing);

/// DETACH_SOUND_TAG (448) 0x7101D0: SoundTag::Remove(thing, sample, bank) 0x71EBE0 (every matching tag goes, a playing
/// loop finishes its pass). Nothing for no thing.
void DetachSoundTag(int sample, int bank, entt::entity thing);

/// The AUDIO_SFX_BANK_TYPE of a script value, SfxBank::None outside 1..10 (approximated, see above)
[[nodiscard]] SfxBank BankType(int bank);

} // namespace openblack::audio::script_sound
