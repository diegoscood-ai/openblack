/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The towns' artifacts' records of a saved game's world file, in the original's order: the object that is the
// artefact, its town, its player, its worth, its dance and its magic effect, whether its dance has started and the
// turn it did (docs/wiki/engine/saved-games.md#writing-the-world-file, the towns' artifacts). openblack keeps an
// artefact on its object (components::TownArtefact), so the object's entity stands for both, and an artefact is named
// by its class.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <optional>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorshipRecords.h>

#include "ECS/Components/TownArtefact.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
[[nodiscard]] gamesave::ObjectNumber ReferenceOrNone(WriteContext& context, entt::entity entity)
{
	return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
}

/// An artefact: its object, its town (kept while the town lasts, after the artefact leaves it), its player, its worth,
/// its dance, its magic effect, whether its dance has started and the turn it did
std::optional<gamesave::SavedRecord> WriteArtifact(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& artefact = registry.Get<const components::TownArtefact>(entity);
	gamesave::TownArtifactRecord record;
	record.objects.at(0).number = context.Reference(entity);
	record.objects.at(1).number = ReferenceOrNone(context, artefact.town);
	record.objects.at(2).number = artefact.player.has_value() ? PlayerReference(context, *artefact.player) : 0;
	record.word = std::bit_cast<uint32_t>(artefact.value);
	record.others.at(0).number = ReferenceOrNone(context, artefact.dance);
	record.others.at(1).number = ReferenceOrNone(context, artefact.effect);
	record.words.at(0) = artefact.dancing ? 1 : 0;
	record.words.at(1) = artefact.danceStarted;
	return record;
}
} // namespace

std::vector<gamesave::ObjectNumber> openblack::ecs::saved_world_write::TownArtifactsOf(WriteContext& context, entt::entity town)
{
	const auto& registry = context.GetRegistry();
	std::vector<std::pair<uint64_t, entt::entity>> found;
	registry.Each<const components::TownArtefact>([&found, town](entt::entity entity, const components::TownArtefact& each) {
		if (each.inTown && each.town == town)
		{
			found.emplace_back(each.joined, entity);
		}
	});
	// a town looks after its newest first
	std::ranges::sort(found, std::ranges::greater {}, &std::pair<uint64_t, entt::entity>::first);
	std::vector<gamesave::ObjectNumber> numbers;
	for (const auto& [joined, entity] : found)
	{
		numbers.push_back(context.Reference(entity, gamesave::TownArtifactRecord::k_Type));
	}
	return numbers;
}

void openblack::ecs::saved_world_write::RegisterTownArtifactWriters(ClassWriters& writers)
{
	// an artefact is named only by its class: its object's entity is claimed by the object's own class
	writers.Register(gamesave::TownArtifactRecord::k_Type, {}, WriteArtifact);
}
