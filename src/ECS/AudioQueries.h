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

// The ECS side of the audio (audio session, milestone B11b): src/Audio includes no ECS component, so what GAudio and
// its callers read of the ECS things (the animated villagers and animals, the street lanterns, the map's surface, the
// weather) is answered here, through audio::GameQueries (src/Audio/GameQueries.h), which Game.cpp gives to audio::Init.
// docs/bw1-notes/audio.md, "Arquitectura".

namespace openblack::audio
{
struct GameQueries;
}

namespace openblack::ecs::audio_queries
{

/// Fills the queries that read the ECS registry and its systems: surfaceType (ecs::sea_cells::GetSurfaceType, GSoundMap::
/// GetSurfaceType 0x71D8E0), weatherSmooth (weather::atmos::GetWeatherSmooth 0x835180), animatedThing (fn_00516510's
/// reads of a villager or animal), animationClipName (the clips of the resources, LoadAllAnimations 0x550180) and
/// streetLanterns (the list g_game+0x205C34 with Object::GetHeight 0x638120)
void Fill(audio::GameQueries& queries);

/// (openblack test hooks, audio session) once a game turn, after audio::ProcessTurn:
///  - OPENBLACK_AUDIO_TEST_VIEW="turn,n[,distance]" flies the camera to look at the n-th villager from that distance (4)
///    at that game turn, and OPENBLACK_AUDIO_TEST_ANIM="clip" plays that clip in a loop on every villager from the same
///    turn (after the Land 1 intro has given the camera back);
///  - OPENBLACK_AUDIO_TEST_LANTERN="turn[,distance]": at that turn (counted by these calls since the start) the camera
///    flies to look at the first lantern's top from that distance (3: inside the sample's 5)
void RunTestHooks(uint32_t turn);

} // namespace openblack::ecs::audio_queries
