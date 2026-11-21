/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// The ECS side of the audio: src/Audio includes no ECS component, so what the audio engine and its callers read of
// the ECS things (the animated villagers and animals, the street lanterns, the map's surface, the weather) is answered
// here, through audio::GameQueries (src/Audio/GameQueries.h), which Game.cpp gives to audio::Init.
// docs/wiki/audio/engine.md#openblack-audio-architecture.

namespace openblack
{
class LandIslandInterface;
}

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
/// The name of the profile the player plays as, for the night's voices; none without one
[[nodiscard]] std::u16string ProfileName();

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

/// Where a creature's dance chant is placed: the creature's map coordinates as a world point, x and z cut to 16.16 and
/// y the land's height there plus the creature's height above it
[[nodiscard]] glm::vec3 CreatureDanceChantPoint(const LandIslandInterface* island, glm::vec3 position);

/// The interface's creature fight state, which plays the fight music wherever the camera is: the interface active, the
/// player's own camera watching a fight, the local player's creature in its duel and the hand holding nothing. (The
/// original enters it from an idle hand and leaves it as soon as the camera or the duel ends; a press in it, which starts
/// the fight's own gestures, is not ported.)
[[nodiscard]] bool DirectsFight(bool interfaceActive, bool cameraWatchesFight, bool creatureDuelling, bool handHoldsNothing);
/// The interface's state number for the audio's filters: 0x10 in the creature fight state, else 0
[[nodiscard]] int InterfaceState(bool directsFight);
/// The same read from the game: the interface, the camera's fight watch, the fight system and the hand
[[nodiscard]] bool LocalPlayerDirectsFight();

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

/// A worship site the chant music can hear: one with dancers, its tribe, its dancer count and its dance centre (the
/// mesh's dance centre point as MapCoords, zero without one)
struct ChantSiteCandidate
{
	entt::entity site {entt::null};
	int tribe {0};
	uint32_t dancers {0};
	map_coords::MapCoords centre {};
};

/// A player's citadel as the chant music looks at it: its MapCoords and its six site slots, each holding a site with
/// dancers or nothing
struct ChantCitadelCandidate
{
	entt::entity citadel {entt::null};
	map_coords::MapCoords position {};
	std::array<std::optional<ChantSiteCandidate>, 6> sites {};
};

/// The citadels the chant music looks at, every player's and the neutral one's in player order, read from the game
[[nodiscard]] std::vector<ChantCitadelCandidate> ChantCandidates();

/// The site whose chant is heard, by index into the citadels and their slots, with the two distances that chose it
struct ChantPick
{
	size_t citadel {0};
	size_t slot {0};
	float citadelDistance {0.0f};
	float siteDistance {0.0f};
};

/// The chant's site: the citadel strictly nearest the camera's MapCoords within 150 metres (the first of equals), then
/// of its slots in order the site with dancers whose dance centre is strictly nearest within 100 metres; none when the
/// nearest citadel has no such site, even if a farther one has
[[nodiscard]] std::optional<ChantPick> PickChantSite(const map_coords::MapCoords& camera,
                                                     std::span<const ChantCitadelCandidate> citadels);

/// (openblack's own) the turns a land run of a --scenarios list gives the hook, in place of the variable's, for this
/// game only (they are kept in the debug hooks' store, which a new game makes again)
void SetCitadelTestTurns(CitadelTestTurns turns);

} // namespace openblack::ecs::audio_queries
