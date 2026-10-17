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

#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The ECS side of the audio: src/Audio includes no ECS component, so what the audio engine and its callers read of
// the ECS things (the animated villagers and animals, the street lanterns, the map's surface, the weather) is answered
// here, through audio::GameQueries (src/Audio/GameQueries.h), which Game.cpp gives to audio::Init.
// docs/wiki/audio/engine.md#openblack-audio-architecture.

namespace openblack::audio
{
struct GameQueries;
}

namespace openblack::ecs::audio_queries
{

/// Fills the queries that read the ECS registry and its systems: surfaceType (ecs::sea_cells::GetSurfaceType),
/// weatherSmooth (weather::atmos::GetWeatherSmooth), cameraAlignment (the audio's alignment value, from
/// ecs::effects::alignment::GetInterfaceAlignment), animatedThing (what the animation sounds read of a villager or
/// animal), animationClipName (the clips of the resources) and streetLanterns (the lanterns with their heights)
void Fill(audio::GameQueries& queries);

/// The audio's alignment value from x ((alignment + 1) / 2): x clamped to 0..1 (a NaN is 0), then 2 - 2 (1 - x) - 1
/// in float steps
[[nodiscard]] float AudioAlignmentValue(float x);

/// (openblack test hooks) once a game turn, after audio::ProcessTurn:
///  - OPENBLACK_AUDIO_TEST_VIEW="turn,n[,distance]" flies the camera to look at the n-th villager from that distance (4)
///    at that game turn, and OPENBLACK_AUDIO_TEST_ANIM="clip" plays that clip in a loop on every villager from the same
///    turn (after the Land 1 intro has given the camera back);
///  - OPENBLACK_AUDIO_TEST_LANTERN="turn[,distance]": at that turn (counted by these calls since the start) the camera
///    flies to look at the first lantern's top from that distance (3: inside the sample's 5)
///  - OPENBLACK_AUDIO_TEST_CITADEL (RunCitadelTestHook, below)
void RunTestHooks(uint32_t turn);

/// (openblack test hook) OPENBLACK_AUDIO_TEST_CITADEL="<in>[,<out>]": at those hook turns the temple interior is entered
/// / left, as ENTER_EXIT_CITADEL(1) / (0) would (the citadel's music and filters). The hook turns are counted by the
/// world's turns (RunTestHooks) and, while the world is paused inside the temple, by the temple's own turns (Game), so
/// the way out comes as many 100 ms turns after the way in as before. The turns are the game's own: read from the
/// variable as the game's debug hooks' store is made (nothing changes the variable while the program runs), or given by
/// SetCitadelTestTurns
void RunCitadelTestHook();

/// The hook turns RunCitadelTestHook enters and leaves the temple at; 0 never
struct CitadelTestTurns
{
	uint32_t in {0};
	uint32_t out {0};
};

/// OPENBLACK_AUDIO_TEST_CITADEL's value as the hook reads it, "<in>[,<out>]" through sscanf: none for no value or one
/// that does not start with a number
[[nodiscard]] std::optional<CitadelTestTurns> ParseCitadelTestTurns(const char* value);

/// An arena of a fight now on, as the fight music measures it: its two fighters, and its middle at the land's height
struct FightArena
{
	entt::entity maker {entt::null};
	entt::entity opponent {entt::null};
	glm::vec3 middle {0.0f};
};

/// The fight music's arena test: the creature is one of the two fighters of an arena whose middle lies under 100 from
/// the camera, the distance summed depth first, sqrt((dz^2 + dy^2) + dx^2) with d the middle minus the camera
[[nodiscard]] bool FightsNearCamera(entt::entity creature, std::span<const FightArena> arenas, const glm::vec3& camera);

/// A player's creature as the heart beat looks at it: its player, and the owner of the town nearest it with that
/// whole-cell distance (no owner without a town)
struct CreatureNearTown
{
	PlayerNames player {PlayerNames::NEUTRAL};
	std::optional<PlayerNames> townOwner;
	uint32_t distance {0};
};

/// The distances the heart beat adds a term for, in order: each creature of another player than `local` whose nearest
/// town is `local`'s
[[nodiscard]] std::vector<uint32_t> EnemyCreatureDistances(std::span<const CreatureNearTown> creatures, PlayerNames local);

/// (openblack's own) the turns a land run of a --scenarios list gives the hook, in place of the variable's, for this
/// game only (they are kept in the debug hooks' store, which a new game makes again)
void SetCitadelTestTurns(CitadelTestTurns turns);

} // namespace openblack::ecs::audio_queries
