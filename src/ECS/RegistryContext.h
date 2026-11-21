/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"
#include "Common/LogOnce.h"
#include "Components/Stream.h"
#include "CreationDraws.h"
#include "Creature/CreatureExplorationMap.h"

namespace openblack::ecs
{
struct RegistryContext
{
	std::unordered_map<components::Stream::Id, entt::entity> streams;
	std::unordered_map<uint32_t, entt::entity> towns;
	/// (openblack) the last Town::ownerListStamp given (town_queries::NextOwnerListStamp): 0 again with every land
	uint32_t ownerListCounter {0};
	/// (openblack) the last Town::creationStamp given (TownArchetype::Create): 0 again with every land
	uint32_t townCreationCounter {0};
	/// (openblack) the last Footpath::creationStamp given (footpaths::Create): 0 again with every land
	uint32_t footpathCreationCounter {0};
	/// (openblack) the last FootpathFinder::creationStamp given (footpaths::StartFootpathFinder): 0 again with every land
	uint32_t footpathFinderCreationCounter {0};
	/// (openblack) the last Temple::creationStamp given (CitadelArchetype::CreateHeart): 0 again with every land
	uint32_t heartCreationCounter {0};
	/// (openblack) the last Flock::creationStamp given (flocks::Create): 0 again with every land
	uint32_t flockCreationCounter {0};
	/// (openblack) the last Mist::creationStamp given (MistArchetype::Create): 0 again with every land
	uint32_t mistCreationCounter {0};
	/// The creation draws a saved game's load holds back until every object is made (ecs::creation_draws)
	creation_draws::Queue creationDraws;
	/// (openblack) the last Stream::creationStamp given (CREATE_STREAM and a load's streams): 0 again with every land
	uint32_t streamCreationCounter {0};
	/// (openblack) the last ScriptMarker::creationStamp given (ScriptMarkerArchetype::Create): 0 again with every land
	uint32_t markerCreationCounter {0};
	/// (openblack) the last BuildingSite::creationStamp given (building_sites::Create): 0 again with every land
	uint32_t siteCreationCounter {0};
	/// (openblack) the last PuzzleGame::creationStamp given (CreatePuzzleGame): 0 again with every land
	uint32_t puzzleGameCreationCounter {0};
	/// CREATE_FLOCK's flocks by their script id. The original's list is newest first, so a repeated id finds the last
	/// one made.
	std::unordered_map<int32_t, entt::entity> flocks;
	/// (openblack) the script opcodes that have logged "Object no longer valid" on this land: the first time is an
	/// error, the repeats only debug lines
	LogOnce objectNoLongerValid;
	/// The game has ended for the local player, whose temple was destroyed: its script has started. Cleared with the map
	bool gameOver {false};
	/// The game is a skirmish: set as the skirmish box starts its land, before the land's script. Cleared with the map,
	/// as the game clears it at each game's start
	bool skirmish {false};
	/// START_CAMERA_POS's place in map units, all zero until the land's script gives one: a game that starts with it
	/// still at the origin starts with the camera at the player's temple (start_camera::StartsAtTemple)
	map_coords::MapCoords startCameraPlace {};
	/// Once a script or the hand has forced an action on a creature, a free creature always agrees to play, to be friends
	/// or to be shown something when another creature asks it, until the land changes
	bool partnersAlwaysAgree {false};
	/// The regions of the land that creatures find places in, made on the game's first land and kept for the whole game:
	/// unlike the rest of the context, Game carries them over each land's reset
	creature_exploration::GameRegions explorationRegions;
	/// The game turn on which any creature last cast each of the miracles that impress, in the order of
	/// creature_mind::k_ImpressiveMiracles. A new land starts with all at 0.
	std::array<uint32_t, 4> impressiveMiracleCastTurn {};
	/// The names of the game's table of named people already given to a villager on this land
	std::array<bool, 0x30> namedVillagersUsed {};
	/// The game turn whose football match has been played: the matches take their turn once a game turn, whichever
	/// pitch asks first. The game keeps it from land to land (Game carries it over each land's reset)
	uint32_t footballTurn {0};
};
} // namespace openblack::ecs
