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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "BankTables.h"
#include "Banks.h"
#include "SamplePlay.h"

// GAudio (runblack.exe, 0x3D4 bytes, ctor 0x426D40): layer 2 of dev\tmp_dis\audio\PLAN.md §2.1. The public face is
// Audio.h; this header is for src/Audio (the services under it and the old sample_play names).

namespace openblack::audio
{
struct GameQueries;

// The banks (BankId, RegisterBank, Bank, FindBank, SampleId...): Banks.h

/// GAudio::IsInsideCitadel 0x429D20, misnamed: the owner is a GameThing (dynamic cast) that is not available
/// (IsAvailable(), vtable +0x2C, returns 0). 0 and -1 (no owner, the atmos mixer) and any other Base are available.
[[nodiscard]] bool OwnerUnavailable(const Owner& owner);

/// The queries audio::Init got (the services of layer 3 read the game through them)
[[nodiscard]] const GameQueries& Queries();

/// fn_00427200's position of a Thing (GameQueries::thingPosition) or Object owner (RegisterObject): nullopt for the
/// other kinds or a gone one
[[nodiscard]] std::optional<glm::vec3> OwnerSoundPosition(const Owner& owner);

/// fn_00427200, the game's 3D function of LHaudio (LHSampleRegister3DObjectFunction 0x426E6B) as the anim effects ask
/// it: no owner = the camera (0x4272F9); the atmos owner gives 0 (0x42726D); a GameThing gives 0 when not available
/// (0x4272AD), else its Get3DSoundPos; any other Base its own Get3DSoundPos (a SoundTag's: SoundTag::Get3DSoundPos
/// 0x71EC90). nullopt = 0 (nothing plays).
[[nodiscard]] std::optional<glm::vec3> Get3DSoundPos(const Owner& owner);
/// The listener's point (GGame::GetCamera / LH3DTech::g_camera), nullopt without a camera
[[nodiscard]] std::optional<glm::vec3> ListenerPoint();
/// GameQueries::landAltitude (LH3DIsland::GetAltitude 0x803090), 0 when unset
[[nodiscard]] float IslandAltitude(float x, float z);

/// GAudio::PlaySoundEffect(LH_SamplePlayOptions*) 0x429E30 on a sound id: nothing without a game (g_game, its
/// HelpSystem g_game+0x25005C: openblack always has them once the audio is initialised); a 3D sample (with a sample
/// number, +0x24) not started when the camera's squared distance to pos + offset is more than the squared max distance
/// (GetGSFXSampleMaxDistance 0x42A430 = the .sad's +0x26C, or the options' +0x58 when that is 0; inside the citadel from
/// LH3DTech::g_camera, 0x429EB1); then the filters by the sample's user parameter (LHSampleGetUserParam, its low 16
/// bits): 1 not while a script holds the wide screen (HelpSystem +0x45E8 and +0x45EC, 0x429F51..0x429F69), only 2 inside
/// the citadel (0x429F6D), only the banks Villagers / HelpSprites (GAudio+0x3C4 / +0x3C0) after SET_GAME_SOUND false
/// (GScript+0x90, 0x429F7C..0x429FA3), not 4 in the interface states 0x10 / 0x16 / 0x17 (GInterface+0x44,
/// 0x429FA5..0x429FB8); and a 3D tracked sample of an unavailable owner (0x429FBA..0x429FD9). Then LHSamplePlay.
/// (The original returns nothing, 0x429FE8; openblack returns the channel.)
Channel PlaySoundEffectOptions(const sample_play::Options& options);

/// SET_GAME_SOUND 0x7100B0: false -> LHSampleStopAll (fn_004287D0) and GScript+0x90 = 1, true -> +0x90 = 0
void SetGameSound(bool enabled);
/// HelpSystem::SetWideScreen 0x5C6AD0 from a script (+0x45E8 with the owning task +0x45EC)
void SetScriptWideScreen(bool on);
/// HelpSystem +0x45E8 && +0x45EC (g_game+0x25005C), as SetScriptWideScreen left it
[[nodiscard]] bool IsScriptWideScreen();
/// GameQueries::insideCitadel (g_game+0x205A28 == 1, 0x4282F0)
[[nodiscard]] bool IsInsideCitadel();
/// GameQueries::videoPlaying (g_game+0x250188)
[[nodiscard]] bool IsVideoPlaying();

} // namespace openblack::audio
