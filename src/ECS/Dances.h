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

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::dance
{
struct DanceFile;
}

namespace openblack::ecs
{
class Registry;
}

/// The dances, the same for every dance (a worship site's, a town's or a script's): made about a place, joined and left by
/// dancers, gone when what they are danced about goes
namespace openblack::ecs::dances
{

/// What a new dance is
struct DanceSetup
{
	/// Its row in the dances' table, and whether it starts by itself once it has dancers
	DanceInfo type {DanceInfo::None};
	bool autostart {false};
	map_coords::MapCoords place;
	/// What it is danced for or about, if anything
	entt::entity owner {entt::null};
	/// The turns it dances before stopping to start again; none for no end
	uint32_t duration {0};
	bool madeByScript {false};
	/// The way it faces: a worship site's dance faces the way the site does
	float angle {0.0f};
};

/// A new dance, stopped at a quarter speed, with the groups of its file, each set up by the file's key frames up to
/// the start of its clock; without a file, one group and no key frames. It comes after every dance made before it
entt::entity Create(Registry& registry, const DanceSetup& setup, std::shared_ptr<const dance::DanceFile> file);
/// A dance goes: each of its dancers that is not itself going leaves it and is told it has finished dancing; one that
/// is going is neither
void Destroy(Registry& registry, entt::entity dance, const std::function<void(entt::entity)>& finished);
/// The dances in the order they take their turns: the newest first. A dance that goes leaves the others' order as it was
[[nodiscard]] std::vector<entt::entity> InTurnOrder(const Registry& registry);

/// Whether a thing a dance is danced about is still about: it is not going, and a villager is not dying either (by its
/// final state, `villagerFinalState`; none for anything that isn't a villager)
[[nodiscard]] constexpr bool IsStillAbout(bool going, std::optional<VillagerStates> villagerFinalState)
{
	return !going && villagerFinalState != VillagerStates::Dying;
}

[[nodiscard]] bool IsDance(const Registry& registry, entt::entity thing);
/// The dance a living dances in, or none
[[nodiscard]] entt::entity DanceOf(const Registry& registry, entt::entity living);
[[nodiscard]] uint32_t Size(const Registry& registry, entt::entity dance);

/// A living joins a dance, in the group whose turn it is that takes it; false when none does, and then it dances in
/// no group
bool AddDancer(Registry& registry, entt::entity dance, entt::entity living, uint32_t sex);
/// A dancer leaves its dance
void RemoveDancer(Registry& registry, entt::entity living);
/// The first dancer of the first group with one other than `exclude`, among the groups its file names
[[nodiscard]] entt::entity FirstDancer(const Registry& registry, entt::entity dance, entt::entity exclude);
/// A dance's dancers, group by group
[[nodiscard]] std::vector<entt::entity> Dancers(const Registry& registry, entt::entity dance);

/// What a dance's turn needs to know of the game
struct TurnContext
{
	uint32_t turn {0};
	/// Whether the thing danced about is still about (IsStillAbout)
	std::function<bool(entt::entity)> available;
	/// A dancer told it has finished dancing
	std::function<void(entt::entity)> finished;
	/// A dancer of a group a key frame's action starts a move for plays its clip again, there and then: once for each
	/// such action, in its group's order, before the groups' moves go on
	std::function<void(entt::entity)> playClipAgain;
};

/// A dance's turn: it ends once what it is danced about has gone (a dance that is itself going waits instead); it
/// starts once it has dancers, stops when its time is up to start again on its next turn, and while dancing does its
/// key frames and its clock goes on
void ProcessTurn(Registry& registry, entt::entity dance, const TurnContext& context);

/// Where a dancer is in its dance: the dance, its group, and its place in the group's list
struct DancerPlace
{
	entt::entity dance {entt::null};
	std::size_t group {0};
	std::size_t slot {0};
};
/// The place of a living in its dance's groups; none when it dances in no group
[[nodiscard]] std::optional<DancerPlace> PlaceOf(const Registry& registry, entt::entity living);

/// A dancer more than this many metres from its place walks there round what is in the way
inline constexpr float k_WalkRoundDistance = 4.0f;

/// What a dancer in its dance does this turn
struct DancerTurn
{
	/// Its place in its group's shape, as a walk there would leave it: the walks keep their goals in metres, so the place
	/// is taken through metres, and a dancer that has walked there is there
	map_coords::MapCoords place;
	/// How far it is from its place, in metres: a dancer not there walks to it
	float distance {0.0f};
	/// The game angle it turns to, when its group's move turns it
	std::optional<uint16_t> facing;
};
/// A dancer's turn in its dance, standing at `at` and facing the game angle `facing`: where its place is, how far off,
/// and which way its group's move turns it. None when it dances in no group, and then it does nothing
[[nodiscard]] std::optional<DancerTurn> DancerTurnOf(const Registry& registry, entt::entity living,
                                                     const map_coords::MapCoords& at, uint16_t facing);
/// The new place of a dancer walking to its place, once its group has moved this turn; none when it has not, or when
/// the dancer is in no group
[[nodiscard]] std::optional<map_coords::MapCoords> MovedPlace(const Registry& registry, entt::entity living);

} // namespace openblack::ecs::dances
