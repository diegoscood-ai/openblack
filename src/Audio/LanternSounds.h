/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::audio::lantern_sounds
{

/// The looping crackle of the street lanterns: the SoundTag of GStreetLantern +0x60 (audio::tags, milestone B3)
/// made by `CallVirtualFunctionsForCreation` 0x734810 -> fn_0071E8C0(lantern, (0, Object::GetHeight, 0), 0x93
/// LH_SAMPLE_G_LANTERN_01 of InGame.sad, track 0, mode 2, loops -1, +0x40 0, 3D, AUDIO_SFX_BANK_TYPE_IN_GAME, delay 0),
/// then `SetActive([0xDA0A10])` (0x734965), unless the lantern has the UNAVAILABLE flag (openblack's lanterns have
/// none). Town lanterns and country ones both get it. The tag replays it while active and the camera is within the
/// sample's max distance (5); a gone lantern's tag releases its loop (CreateSoundTagForDeadObject 0x71ECD0).
/// Research: dev\tmp_dis\mapa\flecos_lantern-sound.md.

/// fn_007349E0(on), the flag [0xDA0A10]: `SoundTag::SetActive` of every lantern's tag. Called every frame from
/// fn_005E5830 (night_lights::Update) with "it is dark" (the mean of the light-table base colour under 120).
/// Turning it off stops the samples at once (StopPlayingSoundEffect).
void SetOn(bool on);

/// Before SoundTag::ProcessSoundTags 0x71E5F0, once per game turn: the lanterns made since the last turn get their tag
/// (openblack has no CallVirtualFunctionsForCreation hook: inferred to be the same, a turn later at most), the gone
/// ones are forgotten (their tag goes on its own); the tags themselves are processed with the others (audio::tags).
void ProcessTurn();

/// InitStaticsValues 0x54A84F: no tags and the flag off (a new map; the tags go with tags::Clear)
void Clear();

} // namespace openblack::audio::lantern_sounds
