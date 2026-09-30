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

/// The looping crackle of the street lanterns, the SoundTag of GStreetLantern +0x60
/// (`CallVirtualFunctionsForCreation` 0x734810 -> `SoundTag::Create` 0x71E8C0: sample 0x93
/// LH_SAMPLE_G_LANTERN_01 of Audio\SFX\Game\InGame.sad, 3D, loops -1, play mode 2, offset (0, Object::GetHeight, 0),
/// no sound while the object has the UNAVAILABLE flag). Town lanterns and country ones both get it.
/// Research: dev\tmp_dis\mapa\flecos_lantern-sound.md.

/// fn_007349E0(on), the flag [0xDA0A10]: `SoundTag::SetActive` of every lantern's tag. Called every frame from
/// fn_005E5830 (night_lights::Update) with "it is dark" (the mean of the light-table base colour under 120).
/// Turning it off stops the samples at once (StopPlayingSoundEffect).
void SetOn(bool on);

/// `SoundTag::ProcessSoundTags` 0x71E5F0 for the lanterns, once per game turn (GGame::EndTurn): each active tag
/// calls `GAudio::PlaySoundEffect`, which only starts the sample when the camera is within the sample's max distance
/// (5 units) of the lantern's point plus its height; play mode 2 leaves the channel alone while it is playing. A
/// lantern that is gone releases its loop (LHSampleReleaseLoop): the current pass finishes and the tag dies.
void ProcessTurn();

/// InitStaticsValues 0x54A84F: no tags and the flag off (a new map)
void Clear();

} // namespace openblack::audio::lantern_sounds
