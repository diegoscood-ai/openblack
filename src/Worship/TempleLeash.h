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

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{
class LeashSystemInterface;
}

// A temple's three leash posts in the game: made with the temple's heart, one entity each (components::LeashPost),
// destroyed with the heart and never saved; and the leash picked at the temple, kept on the heart
// (components::TempleLeash). The rules they follow are worship::leash_posts. (wiki: creature.md, "The temple's leash
// posts")

namespace openblack::worship::temple_leash
{
/// A random number in [low, high): the game's CRT stream in play, a fake in tests
using Draw = std::function<float(float low, float high)>;

/// The heart's three posts, post 0 first: each stands at its special point on the temple, follows the land, and takes
/// its four draws in the order of leash_posts::k_Draws before the next post is made. Nothing is picked yet. Nothing for
/// a heart that has its posts already or for an entity that is no temple heart
void CreatePosts(entt::entity heart, const Draw& draw);

/// The heart's picked post, 0..2, or leash_posts::k_NoPick when nothing is picked or the heart has no posts
[[nodiscard]] int32_t Pick(entt::entity heart);

/// The leash picked at the heart, none with nothing picked
[[nodiscard]] LeashType PickedType(entt::entity heart);

/// Picks the post of a leash at the heart (leash_posts::PickAfterSet: any value that is no leash leaves the pick)
void SetPick(entt::entity heart, LeashType type);

/// The pick at the player's temple: none when the player has no temple heart, else Pick of the heart
[[nodiscard]] std::optional<int32_t> PickOf(PlayerNames player);

/// The heart's posts that show this frame, post 0 first (leash_posts::Shown): none unless the temple is fully built and
/// its player has a creature, then each post whose leash that creature knows (`leash`'s PlayersCreature and Knows).
/// The draw shows them and the hand feels them; the local player's picked post also waits on the hand being shown
[[nodiscard]] std::vector<entt::entity> ShownPosts(entt::entity heart, const ecs::systems::LeashSystemInterface& leash);

/// Where the post stands this frame: its temple's special point read again through the temple's matrix and on the land,
/// as when it was made; none for an entity that is no post of a temple heart
[[nodiscard]] std::optional<glm::vec3> PointNow(entt::entity post);

/// A post the hand can pick this frame, at its point
struct HandPost
{
	entt::entity post;
	glm::vec3 point;
};
/// Every temple's posts the hand feels this frame, each temple's post 0 first: the posts shown (ShownPosts) at their
/// point read again now (PointNow), all but the local player's picked post while the hand is hidden
[[nodiscard]] std::vector<HandPost> HandPosts(const ecs::systems::LeashSystemInterface& leash, PlayerNames local,
                                              bool handHidden);

/// Whether the hand may tap the post: only the local player's own posts (leash_posts::ValidToTap)
[[nodiscard]] bool InterfaceValidToTap(entt::entity post);

/// The hand's tap on a post (hand_tap), in leash_posts::Tap's order: nothing for a post that is not the local
/// player's; the picked post is unpicked; any other post plays the click for the local interface (`myInterface`), is
/// picked at once and sends its leash to the next turn (game_packets::Type::LeashType). Returns 1
uint32_t InterfaceTap(entt::entity post, bool myInterface);

/// The turn's side of a leash picked at the temple, for `player`: refused while the player's creature is under the
/// compassionate or angry spell (leash_posts::LeashRefused); otherwise their creature's leash becomes it
/// (LeashSystemInterface::ChangeType, which also sets the creature's moods from the leash while it is on), and their
/// temple's pick takes it (SetPick). No service, no creature: only the pick
void ApplyLeashType(PlayerNames player, LeashType type, ecs::systems::LeashSystemInterface* leash);

/// Makes ApplyLeashType the handler of the leash's packet (Game::LoadMap, after the hand's handlers)
void RegisterPacketHandler();

/// Game::LoadMap calls it right before Registry::Reset, so that the clear does not destroy posts out of turn: the
/// listener that destroys a heart's posts with it is connected again by the next CreatePosts
void DisconnectDeletionListener();
} // namespace openblack::worship::temple_leash
