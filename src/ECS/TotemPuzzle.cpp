/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TotemPuzzle.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <utility>

#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/TotemPuzzle.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/TotemPuzzleRules.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The totems' row and size: the Japanese totem, at its own size whatever the puzzle's
constexpr auto k_TotemInfo = AbodeInfo::JapaneseTotem;
constexpr float k_TotemScale = 1.0f;

TotemPuzzle* ArcOf(entt::entity puzzle)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(puzzle) ? registry.TryGet<TotemPuzzle>(puzzle) : nullptr;
}

/// A sparkle over a place: the creature's target visual at the totem puzzle's magnitude, for its own life or for some
/// turns; none without the particles
entt::entity Sparkle(const map_coords::MapCoords& at, std::optional<int> turns)
{
	if (!Locator::particleSystem::has_value())
	{
		return entt::null;
	}
	return Locator::particleSystem::value().StartSpotVisual(SpotVisualType::CreatureTarget, map_coords::ToWorld(at), turns,
	                                                        entt::null, totem_puzzle::k_SparkleMagnitude);
}

/// A totem of the puzzle, made sunk and unmovable at a place on the map, its highest step and its height given, at no
/// angle. The puzzle keeps the place: the totem never moves off it
entt::entity MakeTotem(TotemPuzzle& arc, size_t index, const map_coords::MapCoords& at, int32_t maxHeight, int32_t height)
{
	const auto totem = archetypes::PuzzleTotemArchetype::Create(map_coords::ToWorld(at), k_TotemInfo, 0.0f, k_TotemScale);
	arc.totems.at(index) = totem;
	if (totem == entt::null)
	{
		return totem;
	}
	auto& piece = Locator::entitiesRegistry::value().Get<PuzzleTotem>(totem);
	piece.maxHeight = maxHeight;
	piece.height = height;
	object_flags::SetMoveable(totem, false);
	arc.places.at(index) = at;
	arc.maxHeights.at(index) = maxHeight;
	arc.seenHeights.at(index) = height;
	return totem;
}

/// A totem that has gone is made again where it stood, on the land, with the highest step and height it had, held by
/// the scripts and at no angle (not turned from the middle as at the start)
void RemakeGone(TotemPuzzle& arc, size_t index)
{
	const auto old = arc.totems.at(index);
	if (old == entt::null || IsAvailable(old))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* piece = registry.Valid(old) ? registry.TryGet<const PuzzleTotem>(old) : nullptr; piece != nullptr)
	{
		arc.maxHeights.at(index) = piece->maxHeight;
		arc.seenHeights.at(index) = piece->height;
	}
	const auto& stood = arc.places.at(index);
	const auto totem = MakeTotem(arc, index, {.x = stood.x, .z = stood.z, .altitude = 0.0f}, arc.maxHeights.at(index),
	                             arc.seenHeights.at(index));
	if (totem != entt::null)
	{
		script_held::SetControlledByScript(totem, true);
	}
}
} // namespace

void totem_puzzle_game::Start(entt::entity puzzle, const map_coords::MapCoords& place, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& arc = registry.Assign<TotemPuzzle>(puzzle);
	arc.place = place;
	arc.scale = scale;
	// every kept height 0; a totem at each step of the arc, its highest 4 and its height its kept one, then turned from
	// the middle by a quarter turn less than the way to it
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		const auto at = totem_puzzle::ArcPlace(place, scale, index);
		const auto totem = MakeTotem(arc, index, at, totem_puzzle::k_Highest, arc.heights.at(index));
		if (totem == entt::null)
		{
			continue;
		}
		const auto angles = placed_turn::AnglesAfterScriptSet(placed_turn::AnglesOf(registry, totem), placed_turn::Axis::Y,
		                                                      totem_puzzle::FacingFromMiddle(place, at));
		if (angles.has_value() && placed_turn::SetAngles(registry, totem, *angles))
		{
			map_cells::OnAnglesOrScaleChanged(totem);
		}
	}
}

void totem_puzzle_game::Process(entt::entity puzzle)
{
	auto* arc = ArcOf(puzzle);
	if (arc == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the first totem, in order, whose height is not the kept one is the move; the totems before it that still blend
	// from an earlier move are put at their height
	std::optional<size_t> moved;
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		RemakeGone(*arc, index);
		const auto totem = arc->totems.at(index);
		auto* piece = totem != entt::null ? registry.TryGet<PuzzleTotem>(totem) : nullptr;
		if (piece == nullptr)
		{
			continue;
		}
		arc->maxHeights.at(index) = piece->maxHeight;
		arc->seenHeights.at(index) = piece->height;
		if (piece->height != arc->heights.at(index))
		{
			moved = index;
			break;
		}
		if (piece->blendCountdown != 0)
		{
			archetypes::PuzzleTotemArchetype::Place(totem);
		}
	}
	if (!moved.has_value())
	{
		return;
	}
	arc->started = true;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Totem puzzle: totem {} moved to {}", *moved,
	                    registry.Get<const PuzzleTotem>(arc->totems.at(*moved)).height);
	const auto& movedPiece = registry.Get<const PuzzleTotem>(arc->totems.at(*moved));
	arc->heights = totem_puzzle::AfterMove(arc->heights, *moved, movedPiece.height);
	// each totem goes to its kept height, from the one it had; all but the moved one blend to it
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		const auto totem = arc->totems.at(index);
		auto* piece = totem != entt::null ? registry.TryGet<PuzzleTotem>(totem) : nullptr;
		if (piece == nullptr)
		{
			continue;
		}
		piece->previousHeight = piece->height;
		piece->height = arc->heights.at(index);
		if (index != *moved)
		{
			piece->blendCountdown = puzzle_totem::k_BlendReads;
		}
		archetypes::PuzzleTotemArchetype::Place(totem);
		arc->seenHeights.at(index) = piece->height;
	}
	// the sparkles go, and a new one stands over each totem at its highest
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		if (const auto sparkle = std::exchange(arc->sparkles.at(index), entt::null);
		    sparkle != entt::null && registry.Valid(sparkle))
		{
			ToBeDeleted(sparkle);
		}
		if (arc->heights.at(index) == totem_puzzle::k_Highest)
		{
			arc->sparkles.at(index) = Sparkle(totem_puzzle::ArcPlace(arc->place, arc->scale, index), std::nullopt);
		}
	}
}

bool totem_puzzle_game::TestSolution(entt::entity puzzle)
{
	auto* arc = ArcOf(puzzle);
	if (arc == nullptr || !totem_puzzle::Solved(arc->heights))
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// solved: the sparkles go, and a sparkle of three turns stands over each totem's place
	for (auto& kept : arc->sparkles)
	{
		if (const auto sparkle = std::exchange(kept, entt::null); sparkle != entt::null && registry.Valid(sparkle))
		{
			ToBeDeleted(sparkle);
		}
	}
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		static_cast<void>(Sparkle(totem_puzzle::ArcPlace(arc->place, arc->scale, index), totem_puzzle::k_SolvedSparkleTurns));
	}
	registry.Get<PuzzleGame>(puzzle).played = true;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Totem puzzle: every totem at its highest, solved");
	return true;
}

bool totem_puzzle_game::Started(entt::entity puzzle)
{
	const auto* arc = ArcOf(puzzle);
	return arc != nullptr && arc->started;
}
