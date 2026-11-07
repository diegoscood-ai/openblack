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

#include <array>
#include <optional>

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
/// Whether a thing is a rock itself, with the rock's own rules for the hand, the tap and the knocks: a mobile static
/// whose row's mobileType is 2. Not the bonfire, a fragment or a dead tree, which have rules of their own
[[nodiscard]] bool IsRockObject(entt::entity entity);
/// Whether a thing answers that it is a rock: a rock (as IsRockObject, but not the plain base the game makes of the
/// singing stone base's row), the bonfire, or a piece knocked off a building. Not a dead or felled tree
[[nodiscard]] bool IsRock(entt::entity entity);
/// The bonfire: the mobile static of the bonfire's row. It is never placed in the hand, never tapped, never becomes a
/// physics object and is not in the way of what flies
[[nodiscard]] bool IsBonfire(entt::entity entity);
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
/// A creature's blow lands on the rock: one of the four smashing sounds of a creature in turn plays at the rock, kept
/// with the creature, then the rock splits in two as it lies or flies (SplitRockAsItGoes)
std::array<entt::entity, 2> SmashRock(entt::entity entity, entt::entity creature);
/// Splits the rock in two (SplitRock), the halves flying on with its velocity and half its angular momentum when it
/// flies, else at rest
std::array<entt::entity, 2> SplitRockAsItGoes(entt::entity entity);
/// What a split thing's halves are made as: the row they are made from, and the player they take
struct RockHalves
{
	/// The thing's own row; a piece knocked off a building has the rock's
	MobileStaticInfo row {MobileStaticInfo::Rock};
	/// A rock's halves keep its player, a fragment's take the neutral player; the bonfire's are made as bonfires,
	/// which take none
	std::optional<PlayerNames> owner;
};
/// The halves of a thing that answers it is a rock (IsRock); nullopt for anything else
[[nodiscard]] std::optional<RockHalves> HalvesOf(entt::entity entity);
/// Two things of the split thing's row (HalvesOf: a rock's or the bonfire's own, a fragment's the rock's), made as
/// the mobile statics of that row are, scale x 0.7935, the Y angle only, at
/// Pos +- (cos a, 0, sin a) x 0.7935 x radius 2D (a random); the original is deleted. Both halves enter physics
/// with the given velocity and angular momentum (a flying rock's, halved) when they can (a bonfire's can't). Returns
/// them; a half that could not be made is null, and nothing is split for a thing that is not a rock.
std::array<entt::entity, 2> SplitRock(entt::entity entity, glm::vec3 velocity = glm::vec3(0.0f),
                                      glm::vec3 angularMomentum = glm::vec3(0.0f));
/// A rock's reaction to a knock of `g` G: not from another rock, harder than 4 G and taller than 0.7, it loses
/// (g - 4) x 0.005 of its life, and below 0.01 it splits in two (SplitRock), the halves carrying on with its velocity
/// and half its angular momentum when it flies. False when it split
bool KnockRock(entt::entity entity, PhysicsEntry& po, float g);
/// The wood a thing that answers it is a rock holds (IsRock: a rock, the bonfire, or a piece knocked off a building,
/// which has the Rock row). Its scale times its row's wood value, truncated (store_rules::RockWood); nullopt for
/// anything else
[[nodiscard]] std::optional<uint32_t> RockWood(entt::entity entity);
/// A thing that answers it is a rock (IsRock) asked for `amount` when it holds `have` of the type asked (its wood, and nothing
/// of any other type): asked for all it has or more, it goes and gives what it had; else it shrinks to the share of its wood
/// left, its height above the ground in proportion, and gives `amount`. 0 for anything else
uint32_t TakeFromRock(entt::entity entity, uint32_t have, uint32_t amount);
/// A thing at the end of its physics, back into the map: put down gently from a hand by a player (not the neutral one),
/// a kind worth making an artefact of and no script's, it becomes an artefact of the nearest building's town or of the
/// nearest worship site within reach. One already worth enough impresses a town or site it didn't last belong to
void ConsiderArtefact(const PhysicsEntry& entry, entt::entity object);
/// A hand takes a thing: an artefact leaves its town and site, and the hand's player owns it
void ArtefactTaken(entt::entity object, PlayerNames player);
} // namespace object_physics
} // namespace openblack::ecs
