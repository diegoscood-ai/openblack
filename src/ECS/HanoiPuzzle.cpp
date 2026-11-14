/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HanoiPuzzle.h"

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/Voices.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NoShadowOnTexture.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/HanoiRules.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

namespace openblack::ecs::hanoi_puzzle
{
namespace
{
using namespace components;

/// A piece the puzzle makes again plays one of nine sounds of the editor's bank, heard everywhere
constexpr int k_PieceSound = 413;
constexpr int k_PieceSounds = 9;
/// The hand that takes the largest ring says one of these lines
constexpr uint32_t k_LargestRingLine = 5267;
constexpr int32_t k_LargestRingLines = 8;
/// The slots of the pieces in the kept states: the bases first, then the rings
constexpr size_t k_FirstRingSlot = hanoi::k_Bases;
constexpr size_t k_Slots = hanoi::k_Bases + hanoi::k_Rings;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// Whether a piece is there for the puzzle: made, and not on its way out
bool IsThere(entt::entity piece)
{
	return piece != entt::null && Entities().Valid(piece) && IsAvailable(piece);
}

MobileObjectInfo KindOf(entt::entity piece)
{
	const auto* mobile = IsThere(piece) ? Entities().TryGet<const MobileObject>(piece) : nullptr;
	return mobile != nullptr ? mobile->type : MobileObjectInfo::None;
}

HanoiBlock* BlockOf(entt::entity piece)
{
	return IsThere(piece) ? Entities().TryGet<HanoiBlock>(piece) : nullptr;
}

entt::entity& PieceAt(PuzzleGame& game, size_t slot)
{
	return slot < k_FirstRingSlot ? game.bases.at(slot) : game.rings.at(slot - k_FirstRingSlot);
}

MobileObjectInfo KindOfSlot(size_t slot)
{
	return slot < k_FirstRingSlot ? hanoi::k_BaseKind : hanoi::k_RingKinds.at(slot - k_FirstRingSlot);
}

/// A piece of the puzzle at its rest place, with no links: indestructible, and a base immovable
entt::entity MakePiece(entt::entity puzzle, MobileObjectInfo kind, const map_coords::MapCoords& home)
{
	auto& registry = Entities();
	const auto piece = archetypes::MobileObjectArchetype::Create(map_coords::ToWorld(home), kind, 0.0f, 1.0f);
	registry.AssignState<HanoiBlock>(
	    piece, HanoiBlock {.puzzle = puzzle, .home = home, .placedAt = registry.Get<const Transform>(piece).position});
	registry.Assign<Indestructible>(piece);
	if (kind == hanoi::k_BaseKind)
	{
		object_flags::SetMoveable(piece, false);
	}
	return piece;
}

/// A piece leaves the world as the puzzle takes it away: its ghost stays a moment where it was
void TakeAway(entt::entity piece)
{
	object_ghosts::Add(piece);
	Entities().RemoveState<HanoiBlock>(piece);
	ToBeDeleted(piece);
}

/// The puzzle makes a piece again at its rest place from the piece's last state, with the same links, under the scripts'
/// control, and a sound plays
void MakeAgain(entt::entity puzzle, PuzzleGame& game, size_t slot, const HanoiBlock& last)
{
	auto& registry = Entities();
	const auto piece = MakePiece(puzzle, KindOfSlot(slot), last.home);
	auto& block = registry.Get<HanoiBlock>(piece);
	block.below = last.below;
	block.above = last.above;
	if (auto* above = BlockOf(block.above))
	{
		above->below = piece;
	}
	if (auto* below = BlockOf(block.below))
	{
		below->above = piece;
	}
	PieceAt(game, slot) = piece;
	game.kept.at(slot) = block;
	script_held::SetControlledByScript(piece, true);
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::Editor), audio::tags::RandomSample(k_PieceSound, k_PieceSounds)};
	options.track = false;
	audio::PlaySoundEffect(options);
}

/// A piece that is still there is taken away and made again at its rest place from its own state
void ReplaceAndMakeAgain(entt::entity puzzle, PuzzleGame& game, size_t slot)
{
	const auto piece = PieceAt(game, slot);
	const auto last = Entities().Get<const HanoiBlock>(piece);
	TakeAway(piece);
	MakeAgain(puzzle, game, slot, last);
}

/// Whether the piece is where the puzzle put it
bool AtPlace(entt::entity piece)
{
	const auto& registry = Entities();
	const auto& block = registry.Get<const HanoiBlock>(piece);
	const auto* transform = registry.TryGet<const Transform>(piece);
	return transform != nullptr && transform->position == block.placedAt;
}

bool InAHand(entt::entity piece)
{
	return Locator::handSystem::has_value() && Locator::handSystem::value().IsInMagicHand(piece);
}

/// The top piece of the first stack the search meets: in nine map cells spiralling out from the centre's cell, the
/// first piece within reach of `near` that is not the ring, followed up its stack. A stack that holds the ring does not
/// count
entt::entity FindStack(map_coords::MapCoords centre, const map_coords::MapCoords& near, entt::entity ring)
{
	map_coords::Spiral spiral;
	for (int32_t cell = 0; cell < hanoi::k_SettleCells; ++cell)
	{
		const auto at = map_coords::Cell(centre);
		for (auto thing = map_cells::FindType(at, ObjectType::Any); thing != entt::null;
		     thing = map_cells::FindType(at, ObjectType::Any, thing))
		{
			if (thing == ring || BlockOf(thing) == nullptr ||
			    game_math::GetDistanceInMetres(object::MapCoordsOf(thing), near) > hanoi::k_SettleReach)
			{
				continue;
			}
			auto top = thing;
			bool holdsRing = false;
			for (auto next = BlockOf(thing)->above; next != entt::null;)
			{
				if (next == ring)
				{
					holdsRing = true;
					break;
				}
				top = next;
				const auto* block = BlockOf(next);
				next = block != nullptr ? block->above : entt::null;
			}
			if (!holdsRing)
			{
				return top;
			}
		}
		map_coords::AddCells(centre, spiral.Next());
	}
	return entt::null;
}

/// The links of the puzzle's pieces for the test of a solution: a ring that is not there rests on nothing it knows
hanoi::Stacks<entt::entity> StacksOf(const PuzzleGame& game)
{
	hanoi::Stacks<entt::entity> stacks {.bases = game.bases, .rings = game.rings};
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto* block = BlockOf(game.rings.at(ring));
		stacks.below.at(ring) = block != nullptr ? block->below : entt::tombstone;
	}
	return stacks;
}

/// The last state of each piece that is there
void KeepStates(PuzzleGame& game)
{
	for (size_t slot = 0; slot < k_Slots; ++slot)
	{
		if (const auto* block = BlockOf(PieceAt(game, slot)))
		{
			game.kept.at(slot) = *block;
		}
	}
}

PuzzleGame* GameOf(entt::entity puzzle)
{
	auto& registry = Entities();
	return puzzle != entt::null && registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr;
}
} // namespace

void Start(entt::entity puzzle)
{
	auto* game = GameOf(puzzle);
	if (game == nullptr)
	{
		return;
	}
	const auto spaced = hanoi::Spaced(hanoi::StepsOf(static_cast<uint16_t>(game->angle), game->scale));
	game->cosineStep = spaced.cosine;
	game->sineStep = spaced.sine;
	for (size_t base = 0; base < hanoi::k_Bases; ++base)
	{
		const auto piece =
		    MakePiece(puzzle, hanoi::k_BaseKind, hanoi::BasePlace(game->place, spaced, static_cast<int32_t>(base)));
		game = GameOf(puzzle);
		game->bases.at(base) = piece;
	}
	const auto places = hanoi::StartRingPlaces(hanoi::BasePlace(game->place, spaced, 0));
	auto below = game->bases.front();
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto piece = MakePiece(puzzle, hanoi::k_RingKinds.at(ring), places.at(ring));
		game = GameOf(puzzle);
		game->rings.at(ring) = piece;
		if (!hanoi::StartRingCastsShadowOnTexture(ring))
		{
			Entities().Assign<NoShadowOnTexture>(piece);
		}
		Entities().Get<HanoiBlock>(piece).below = below;
		Entities().Get<HanoiBlock>(below).above = piece;
		below = piece;
	}
	KeepStates(*game);
}

bool TestSolution(entt::entity puzzle)
{
	auto* game = GameOf(puzzle);
	if (game == nullptr)
	{
		return false;
	}
	if (game->played)
	{
		return true;
	}
	if (!hanoi::Solved(StacksOf(*game)))
	{
		return false;
	}
	for (const auto ring : game->rings)
	{
		if (IsThere(ring))
		{
			object_flags::SetPickupable(ring, false);
			object_flags::SetMoveable(ring, false);
		}
	}
	game->played = true;
	return true;
}

void Process(entt::entity puzzle)
{
	auto* game = GameOf(puzzle);
	if (game == nullptr || game->played)
	{
		return;
	}
	auto& registry = Entities();
	const hanoi::Steps steps {.cosine = game->cosineStep, .sine = game->sineStep};
	for (size_t base = 0; base < hanoi::k_Bases; ++base)
	{
		const auto piece = game->bases.at(base);
		if (piece == entt::null)
		{
			continue;
		}
		const auto place = hanoi::BasePlace(game->place, steps, static_cast<int32_t>(base));
		if (IsThere(piece) && !(registry.Get<const HanoiBlock>(piece).home == place && AtPlace(piece)))
		{
			// a base off its place goes, and a new base stands on the place, with no links
			TakeAway(piece);
			const auto made = MakePiece(puzzle, hanoi::k_BaseKind, place);
			game = GameOf(puzzle);
			game->bases.at(base) = made;
			script_held::SetControlledByScript(made, true);
		}
		if (!IsThere(game->bases.at(base)))
		{
			MakeAgain(puzzle, *game, base, game->kept.at(base));
			game = GameOf(puzzle);
		}
	}
	// once a ring of this turn has gone onto a stack, every other ring off its place goes back to its place
	bool settled = false;
	bool calm = true;
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto slot = k_FirstRingSlot + ring;
		if (game->rings.at(ring) != entt::null && !IsThere(game->rings.at(ring)))
		{
			MakeAgain(puzzle, *game, slot, game->kept.at(slot));
			game = GameOf(puzzle);
		}
		const auto piece = game->rings.at(ring);
		if (piece == entt::null || AtPlace(piece) || InAHand(piece))
		{
			continue;
		}
		const auto kind = KindOf(piece);
		const auto at = object::MapCoordsOf(piece);
		const auto top = FindStack(at, at, piece);
		bool remake = false;
		if (top != entt::null && hanoi::MayRestOn(kind, KindOf(top)))
		{
			// the ring goes onto the stack, higher by the height of the top piece
			auto& block = registry.Get<HanoiBlock>(piece);
			auto& topBlock = registry.Get<HanoiBlock>(top);
			block.below = top;
			topBlock.above = piece;
			block.home = topBlock.home;
			block.home.altitude = hanoi::HeightOf(KindOf(top)) + block.home.altitude;
			settled = true;
			game->begun = true;
			remake = true;
		}
		else if (settled)
		{
			remake = true;
		}
		else
		{
			calm = false;
			game->calmTurns = hanoi::CalmAfterRestlessTurn(game->calmTurns);
			if (game->calmTurns < 0)
			{
				// after a while the ring goes back onto the stack at its rest place, whatever its size
				auto& block = registry.Get<HanoiBlock>(piece);
				const auto back = FindStack(block.home, block.home, piece);
				if (auto* backBlock = BlockOf(back))
				{
					backBlock->above = piece;
				}
				block.above = entt::null;
				block.below = back;
				remake = true;
			}
		}
		if (remake)
		{
			ReplaceAndMakeAgain(puzzle, *game, slot);
			game = GameOf(puzzle);
		}
	}
	if (calm)
	{
		game->calmTurns = hanoi::CalmAfterCalmTurn(game->calmTurns);
	}
	KeepStates(*game);
}

bool RefusesPickUp(entt::entity thing)
{
	const auto* block = BlockOf(thing);
	if (block == nullptr)
	{
		return false;
	}
	const auto* game = GameOf(block->puzzle);
	return hanoi::CannotBePickedUp({
	    .base = KindOf(thing) == hanoi::k_BaseKind,
	    .covered = block->above != entt::null,
	    .hasPuzzle = game != nullptr,
	    .calm = game != nullptr ? game->calmTurns : hanoi::k_CalmTop,
	});
}

void TakenInHand(entt::entity thing)
{
	auto* block = BlockOf(thing);
	if (block == nullptr)
	{
		return;
	}
	if (auto* below = BlockOf(block->below))
	{
		below->above = entt::null;
	}
	block->above = entt::null;
	block->below = entt::null;
	if (KindOf(thing) == hanoi::k_RingKinds.front())
	{
		audio::voices::Say(k_LargestRingLine + game_random::LocalRand(k_LargestRingLines), false, false, glm::vec3(0.0f));
	}
}

std::optional<bool> CanBecomePhysicsObject(entt::entity thing)
{
	const auto* block = thing != entt::null && Entities().Valid(thing) ? Entities().TryGet<const HanoiBlock>(thing) : nullptr;
	if (block == nullptr)
	{
		return std::nullopt;
	}
	return KindOf(thing) != hanoi::k_BaseKind && block->below == entt::null;
}

void Forget(entt::entity puzzle)
{
	Entities().Each<HanoiBlock>([puzzle](entt::entity piece, HanoiBlock& block) {
		if (block.puzzle == puzzle && KindOf(piece) != hanoi::k_BaseKind)
		{
			block.puzzle = entt::null;
		}
	});
}

} // namespace openblack::ecs::hanoi_puzzle
