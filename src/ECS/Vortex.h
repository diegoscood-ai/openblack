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

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/LandscapeVortex.h"
#include "Enums.h"

/// The vortex objects (In / Out / Volcano; see docs/bw1-notes/vortex.md). The end-of-turn part (the contents, the light
/// map's step, the fades' end, the land flattening) is in ProcessAll; the frame part is in UpdateGroundEffects (the
/// decal's state, the ground effects) and UpdateOverLandEffects (the effects over the land, the light map's alpha).
/// What draws them reads the component.
namespace openblack::ecs::vortex
{
/// CHL CREATE Vortex (radius 50): In / Out / Volcano, then the creation setup (the info row, the state from the info,
/// the local particle systems of the info row). entt::null for another type.
entt::entity Create(glm::vec3 position, VortexType type);
/// State FadeIn (2), the turn
void StartFadeIn(entt::entity vortex);
/// VORTEX_FADE_OUT 257: state FadeOut (3), the turn
void StartFadeOut(entt::entity vortex);
/// VORTEX_PARAMETERS 328: the town and the flock parameters of an Out
void SetParameters(entt::entity vortex, entt::entity town, glm::vec3 position, float a, float b, entt::entity flock);
/// A thrown object that hits an active In and can be sucked into a vortex is taken in from the physics (its
/// velocity and world matrix)
void ReactToPhysicsImpact(entt::entity vortex, entt::entity hitter, glm::vec3 velocity, const glm::mat3& rows,
                          glm::vec3 position);
/// Take an object in (a creature only fizzes); queued for the attract particle rule
void TakeIn(entt::entity vortex, entt::entity object, glm::vec3 velocity, const glm::mat3& rows, glm::vec3 position,
            bool fromPhysics);
/// Once a game turn, at the end of the particle pass: the contents first (In / Out), then the light map's step, then
/// FadeIn -> Active and FadeOut -> deleted after 7 s, then the land under an In / Out flattened to its mean
void ProcessAll(uint32_t turn);
/// Once a frame with the frame's game time, before the other effects' frame steps (the ground effects are drawn before
/// the land): every vortex's decal state, and while it shows (fade not 0) its ground effect moved and stepped
void UpdateGroundEffects(float seconds);
/// Once a frame with the frame's game time, after the other effects' frame steps (they are drawn as the sorted objects
/// are): while a vortex shows, its effect over the land stepped; every light map gets its alpha
void UpdateOverLandEffects(float seconds);
/// The fade value f of a state after e seconds in it: 0 / 1; FadeIn 0 for 2 s then smooth((e - 2) / 5); FadeOut
/// smooth(1 - e / 5) for 5 s then 0; smooth(x) = ((3 - 2x) x) x
[[nodiscard]] float FadeValue(VortexStateType state, float e);
/// q = 1 - (1 - s)^2, s = FadeValue (1 in FadeOut)
[[nodiscard]] float LandFactorValue(VortexStateType state, float e);
/// With the retail spline (every y 0): b x q, b = mean - alt0 within 50 m, (56 - r)(mean - alt0) / 6 to 56 m, 0
/// beyond
[[nodiscard]] float LandOffset(glm::vec2 d, float alt0, float mean, float q);
/// The light map's strength L after e seconds in a state: Inactive 0, Active 0.6; FadeIn e / 2 for 2 s, then from 1
/// down to 0.6 over 5 s; FadeOut from 0.6 up to 1 over 5 s, then down to 0 over 2 s. A flash, then a rest at 0.6
[[nodiscard]] float LightMapAlphaValue(VortexStateType state, float e);
/// A 0..1 strength as an effect alpha: x 255, truncated, kept to a byte
[[nodiscard]] uint8_t AlphaByte(float value);
/// The height of the ground effect over the land: from 2.5 m under it at q = 0 to 0.3 m under it at q = 1
[[nodiscard]] float PreLandscapeHeight(float ground, float q);
/// The decal hole's alpha reference for a fade f: f x 255 truncated, at most 235
[[nodiscard]] uint8_t DecalAlphaRef(float f);
/// One frame of the decal with the fade f: when f differs from the last change, it is made (f not 0) or released (f 0)
[[nodiscard]] components::VortexDecal NextDecal(const components::VortexDecal& decal, float f);
/// Any vortex throwing this villager (the villager's thrown animation asks)
[[nodiscard]] bool IsVillagerBeingThrown(entt::entity villager);
/// On deletion (with the In's and Out's own parts): the VortexSave writer / reader, the thrown list, the particle systems
void OnDeleted(entt::entity vortex);
} // namespace openblack::ecs::vortex
