/*******************************************************************************
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
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/LandscapeVortex.h"
#include "Enums.h"

/// The vortex objects (In / Out / Volcano; see docs/wiki/world/vortex.md#the-vortex-objects). The end-of-turn part (the
/// contents, the light map's step, the fades' end, the land flattening) is in ProcessAll; the frame part is in
/// UpdateGroundEffects (the decal's state, the ground effects) and UpdateOverLandEffects (the effects over the land, the light
/// map's alpha). What draws them reads the component.
namespace openblack::ecs::vortex
{
/// The player a vortex belongs to: it is made belonging to nothing, so it is the neutral player's. Its object mover
/// and the flock of the animals a way into a land lets out are given this player
inline constexpr PlayerNames k_Player = PlayerNames::NEUTRAL;
/// CHL CREATE Vortex (radius 50): In / Out / Volcano, then the creation setup (the info row, the state from the info,
/// the local particle systems of the info row). entt::null for another type.
entt::entity Create(glm::vec3 position, VortexType type);
/// State FadeIn (2), the turn
void StartFadeIn(entt::entity vortex);
/// VORTEX_FADE_OUT 257: state FadeOut (3), the turn. Whether the thing was a vortex
bool StartFadeOut(entt::entity vortex);
/// VORTEX_PARAMETERS 328: the town and the flock parameters of an Out
void SetParameters(entt::entity vortex, entt::entity town, glm::vec3 position, float a, float b, entt::entity flock);
/// A thrown object that hits an active In and can be sucked into a vortex is taken in from the physics (its
/// velocity and world matrix)
void ReactToPhysicsImpact(entt::entity vortex, entt::entity hitter, glm::vec3 velocity, const glm::mat3& rows,
                          glm::vec3 position);
/// Take an object in (a creature only fizzes); queued for the attract particle rule
void TakeIn(entt::entity vortex, entt::entity object, glm::vec3 velocity, const glm::mat3& rows, glm::vec3 position,
            bool fromPhysics);
/// The attract rule takes the record a TakeIn made for the object, the last one made first; none when there is none
[[nodiscard]] std::optional<components::VortexObjectInfo> TakeQueued(entt::entity object);
/// The attract rule has started carrying an object the In took that can be destroyed: it leaves its town (a villager)
/// or its town and flock (an animal), and its line goes into the In's crossing file
void HandOff(entt::entity vortex, entt::entity object);
/// The crossing file's line a way out of a land standing where the object is would write for it, for the debug
/// window; none for a thing that has no line. It reads and changes nothing
[[nodiscard]] std::optional<std::string> LineWhereItIs(entt::entity object);
/// Once a game turn, at the end of the particle pass: the contents first (In / Out), then the light map's step, then
/// FadeIn -> Active and FadeOut -> deleted after 7 s, then the land under an In / Out flattened to its mean
void ProcessAll(uint32_t turn);
/// Once a frame with the frame's game time, before the other effects' frame steps (the ground effects are drawn before
/// the land): every vortex's decal state, and while it shows (fade not 0) its ground effect moved and stepped
void UpdateGroundEffects(float seconds);
/// Once a frame with the frame's game time, after the other effects' frame steps (they are drawn as the sorted objects
/// are): while a vortex shows, its effect over the land stepped; every light map gets its alpha
void UpdateOverLandEffects(float seconds);
/// The fades, the glow, the hole's threshold, the swirl's depth and the levelled square are the pure rules of
/// Magic/VortexRules.h (openblack::vortex::Openness, LevelAmount, GlowBrightness, GroundHoleThreshold, SwirlDepth,
/// LevelSquare).
/// A 0..1 strength as an effect alpha: x 255, truncated, kept to a byte
[[nodiscard]] uint8_t AlphaByte(float value);
/// One frame of the decal with the fade f: when f differs from the last change, it is made (f not 0) or released (f 0)
[[nodiscard]] components::VortexDecal NextDecal(const components::VortexDecal& decal, float f);
/// Whether a creature's route keeps clear of the vortex: of a way out of a land unless it may be taken there (a script
/// lets it through on the leash, or controls it); of any other vortex always
[[nodiscard]] bool CreatureMustAvoid(entt::entity vortex, entt::entity creature);
/// Any vortex throwing this villager (the villager's thrown animation asks)
[[nodiscard]] bool IsVillagerBeingThrown(entt::entity villager);
/// On deletion (with the In's and Out's own parts): the VortexSave writer / reader, the thrown list, the particle systems
void OnDeleted(entt::entity vortex);
} // namespace openblack::ecs::vortex
