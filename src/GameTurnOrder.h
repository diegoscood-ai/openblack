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

#include <array>
#include <optional>
#include <span>

/// The order of the steps of one game turn. The game does its turn in a fixed order of stages, and many systems depend
/// on that order: what one stage changes, the next stages see in the same turn. Each step of openblack's turn has the
/// stage of the game that it does. A step whose stage is not known keeps its place between its neighbours. The order
/// keeps no game state, so that the game and the tests use it both.
namespace openblack::game_turn
{
/// The stages of the game's turn, in the game's order
enum class Stage : uint8_t
{
	/// The player's input of the turn, before the turn count moves on
	Input,
	/// The turn count moves on
	StartTurn,
	/// The storms move on, and the snow on the land lies and melts
	Atmosphere,
	/// Each player in turn: its temple, its towns, its computer mind, its influence and its alignment
	Players,
	Dances,
	/// The fields, the fish farms, the things that walk along tracks and the footpaths that are found
	Lists,
	/// The forests' trees grow and spread
	Forests,
	/// The living: creatures, villagers and animals
	Living,
	Fires,
	Reactions,
	/// The miracles: the spell icons charge, the upkeep is paid, and each miracle takes its turn
	Spells,
	/// The particle effects that are not part of a miracle
	Particles,
	Fireflies,
	/// The bodies that fly, collide and come to rest
	Physics,
	/// The end of the turn of the particle effects, which also moves the vortices on
	ParticleEnd,
	/// The scripts, then the script fade
	Scripts,
	/// The advisors, the tooltips and the banter of the help
	Help,
	/// The help's count of what the player did
	HelpProfile,
	/// The time of day
	TimeOfDay,
	/// The weather things follow their storms
	WeatherThings,
	/// The scrolls and signs of the scripts
	Highlights,
	/// The climates
	Climates,
	/// What the hand holds
	Hand,
	/// The reward chests
	Rewards,
	/// The guidance of the advisors
	Guidance,
	/// The advisors confirm what the player does, as when they cheer the player's turning of the camera
	Confirmations,
	/// The camera's stacked views let go of things that are not there now
	CameraChecks,
	/// The pieces broken off buildings
	Fragments,
	/// The moods of the custom music
	MusicMoods,
	/// The end of the turn: the sounds, the game's end and the statistics
	EndTurn,
	/// After the game's code
	AfterTurn,
};

/// The steps of openblack's turn. Several of them do what the game splits into more steps (the turn start of the
/// atmosphere, the players and the dances is one step here, as the magic system does it)
enum class Step : uint8_t
{
	/// The packets the last flush passed on
	PlayerCommands,
	/// The hand's part of the interface's process
	HandInterface,
	/// The interface's pump with the power-up system
	PowerUpInputs,
	/// The turn number goes up, and the time since the last turn is taken
	Clock,
	/// The map grid's acceleration structure is rebuilt
	MapCells,
	/// The reactions' clock for the whole turn, and the reactions whose initiator went
	ReactionClock,
	Sharks,
	/// The atmosphere, the influence rings, the players (their towns' turn) and the dances
	TurnStart,
	Fields,
	FishFarms,
	WalkPaths,
	/// The scripts' mist fades
	MistFades,
	PuzzleGames,
	/// The footpaths being planned
	Footpaths,
	ForestGrowth,
	/// The living list, then the creatures, the creature help and the souls of the dead
	Living,
	/// The fire, the balls, the reactions and the spells
	Miracles,
	/// The particle effects' and the magic's test hooks
	DebugHooks,
	Particles,
	Fireflies,
	Physics,
	/// The particle system's end of loop: the exploded meshes' queue and the particle sounds
	ParticleEnd,
	/// The scripts, the script objects, the script fade, the help, the help profile and the time of day
	Scripts,
	WeatherThings,
	ScriptHighlights,
	Climates,
	/// The towns' belief
	TownBelief,
	HeldThing,
	/// The dead list's pass
	DeadList,
	Rewards,
	/// The spooky voices, the help sprites' moon phase check, the town desires' sounds and the confirmations
	Guidance,
	/// The influence circles rebuilt when a radius moved
	InfluenceBorders,
	CameraChecks,
	BuildingPieces,
	/// The sound map, the sound tags, then the audio's turn: its music, atmos, channels and listener
	Sounds,
	/// The audio's test views
	AudioTestHooks,
	/// The autosave, when the options' autosave is on and the game is not over
	AutoSave,
	/// The game's end for the local player whose temple is being destroyed
	GameOver,
	/// The reactions' end of turn
	ReactionsEnd,
	GlobalStatistics,

	_Count
};

/// A step of the turn and the stage of the game that it does. No stage: the place of the step in the game's turn is not
/// known, and the step keeps its place between its neighbours.
struct Place
{
	Step step;
	std::optional<Stage> stage;
};

/// The turn, step by step, in openblack's order (docs/wiki/engine/loop.md#the-turn-call-by-call)
inline constexpr std::array k_Turn = std::to_array<Place>({
    // The game's inputs come before the game's code, so before the turn number goes up
    {.step = Step::PlayerCommands, .stage = Stage::Input},
    {.step = Step::HandInterface, .stage = Stage::Input},
    {.step = Step::PowerUpInputs, .stage = Stage::Input},
    {.step = Step::Clock, .stage = Stage::StartTurn},
    {.step = Step::MapCells, .stage = Stage::StartTurn},
    {.step = Step::ReactionClock, .stage = Stage::StartTurn},
    {.step = Step::Sharks, .stage = Stage::StartTurn},
    {.step = Step::TurnStart, .stage = Stage::Atmosphere},
    {.step = Step::Fields, .stage = Stage::Lists},
    {.step = Step::FishFarms, .stage = Stage::Lists},
    {.step = Step::WalkPaths, .stage = Stage::Lists},
    {.step = Step::MistFades, .stage = Stage::Lists},
    {.step = Step::PuzzleGames, .stage = Stage::Lists},
    {.step = Step::Footpaths, .stage = Stage::Lists},
    {.step = Step::ForestGrowth, .stage = Stage::Forests},
    // (approximate) the creatures take their turn after the living list rather than inside it
    {.step = Step::Living, .stage = Stage::Living},
    {.step = Step::Miracles, .stage = Stage::Fires},
    {.step = Step::DebugHooks, .stage = std::nullopt},
    {.step = Step::Particles, .stage = Stage::Particles},
    {.step = Step::Fireflies, .stage = Stage::Fireflies},
    {.step = Step::Physics, .stage = Stage::Physics},
    {.step = Step::ParticleEnd, .stage = Stage::ParticleEnd},
    {.step = Step::Scripts, .stage = Stage::Scripts},
    {.step = Step::WeatherThings, .stage = Stage::WeatherThings},
    {.step = Step::ScriptHighlights, .stage = Stage::Highlights},
    {.step = Step::Climates, .stage = Stage::Climates},
    {.step = Step::TownBelief, .stage = std::nullopt},
    {.step = Step::HeldThing, .stage = Stage::Hand},
    {.step = Step::DeadList, .stage = std::nullopt},
    {.step = Step::Rewards, .stage = Stage::Rewards},
    {.step = Step::Guidance, .stage = Stage::Guidance},
    {.step = Step::InfluenceBorders, .stage = std::nullopt},
    {.step = Step::CameraChecks, .stage = Stage::CameraChecks},
    {.step = Step::BuildingPieces, .stage = Stage::Fragments},
    {.step = Step::Sounds, .stage = Stage::EndTurn},
    {.step = Step::AudioTestHooks, .stage = std::nullopt},
    {.step = Step::AutoSave, .stage = Stage::EndTurn},
    {.step = Step::GameOver, .stage = Stage::EndTurn},
    {.step = Step::ReactionsEnd, .stage = std::nullopt},
    {.step = Step::GlobalStatistics, .stage = Stage::EndTurn},
});

/// True when no step of the turn comes before a step of an earlier stage. The steps with no stage are not checked.
[[nodiscard]] constexpr bool InGameOrder(std::span<const Place> turn) noexcept
{
	std::optional<Stage> last;
	for (const auto& place : turn)
	{
		if (!place.stage.has_value())
		{
			continue;
		}
		if (last.has_value() && *place.stage < *last)
		{
			return false;
		}
		last = place.stage;
	}
	return true;
}

/// Does each step of the turn, in the order of the turn
template <typename Do>
void PlayTurn(Do&& step)
{
	for (const auto& place : k_Turn)
	{
		step(place.step);
	}
}

static_assert(k_Turn.size() == static_cast<size_t>(Step::_Count), "Each step has one place in the turn");
static_assert(InGameOrder(k_Turn), "The steps of the turn follow the game's stages");

} // namespace openblack::game_turn
