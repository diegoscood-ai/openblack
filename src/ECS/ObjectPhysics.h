/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{
struct PhysicsEntry;

/// What rocks do in the physics and in the hand. The Rock class of the original: mobile statics whose info.dat
/// mobileType is 2 (Rock, Boulder* .. Squarerock*, Meteor). Taller rocks break in two when tapped (TapRock ->
/// SplitRock), and hard knocks wear a rock away until it breaks (KnockRock).
namespace object_physics
{
[[nodiscard]] bool IsRock(entt::entity entity);
/// Get2DRadius (a rock does not override it): max(half size x, half size z) of the mesh box x
/// scale (ecs::object::Get2DRadius).
[[nodiscard]] float RockRadius2D(entt::entity entity);
/// GetHeight: 2 x half size y of the mesh box x scale (ecs::object::GetHeight).
[[nodiscard]] float RockHeight(entt::entity entity);
/// Rocks with a 2D radius over 3.6 cannot be lifted.
[[nodiscard]] bool RockValidForPlaceInHand(entt::entity entity);
/// Taller than 0.7 (physics::objects::RockBreaksWhenTapped).
[[nodiscard]] bool CanTapRock(entt::entity entity);
/// Splits the rock and plays LH_SAMPLE_G_ROCKTAP_01..04 in turn at the hand.
std::array<entt::entity, 2> TapRock(entt::entity entity, glm::vec3 handPosition);
/// Two rocks of the same type, scale x 0.7935, the Y angle only, at
/// Pos +- (cos a, 0, sin a) x 0.7935 x radius 2D (a random); the original is deleted. Both halves enter physics
/// with the given velocity and angular momentum (a flying rock's, halved). Returns them.
std::array<entt::entity, 2> SplitRock(entt::entity entity, glm::vec3 velocity = glm::vec3(0.0f),
                                      glm::vec3 angularMomentum = glm::vec3(0.0f));
/// A rock's reaction to a knock of `g` G: not from another rock, harder than 4 G and taller than 0.7, it loses
/// (g - 4) x 0.005 of its life, and below 0.01 it splits in two (SplitRock), the halves carrying on with its velocity
/// and half its angular momentum when it flies. False when it split
bool KnockRock(entt::entity entity, PhysicsEntry& po, float g);
/// A thing at the end of its physics, back into the map: put down gently from a hand by a player (not the neutral one),
/// a kind worth making an artefact of and no script's, it becomes an artefact of the nearest building's town or of the
/// nearest worship site within reach. One already worth enough impresses a town or site it didn't last belong to
void ConsiderArtefact(const PhysicsEntry& entry, entt::entity object);
/// A hand takes a thing: an artefact leaves its town and site, and the hand's player owns it
void ArtefactTaken(entt::entity object, PlayerNames player);
} // namespace object_physics
} // namespace openblack::ecs
