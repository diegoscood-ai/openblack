/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameWriters.h"

#include "Audio/Services/LanternSounds.h"
#include "ECS/FireFlies.h"
#include "ECS/Systems/GameStatsSystemInterface.h"
#include "Game/GameStats.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Sound/GameMusic.h"
#include "Worship/FireFlyReward.h"

using namespace openblack::ecs::saved_world_write;

ClassWriters openblack::ecs::saved_world_write::GameWriters()
{
	ClassWriters writers;
	// Each area's registration, one line each; a class with none is written from what its load kept
	RegisterGameObjectWriter(writers, saved_game_save::CurrentValues);
	RegisterForestWriters(writers);
	RegisterRiverAndMistWriters(writers);
	RegisterWeatherWriters(writers);
	RegisterReactionWriters(writers);
	RegisterPlayerWriters(writers, {.values = saved_players::CurrentValues,
	                                .objects = saved_players::ObjectsOf,
	                                .newRecord = saved_players::NewGameRecord});
	RegisterMobileStaticWriters(writers);
	RegisterVortexWriters(writers);
	RegisterFireWriters(writers);
	RegisterFireBallWriters(writers);
	RegisterVillagerWriters(writers);
	RegisterArenaWriters(writers);
	RegisterDataPathWriters(writers);
	RegisterDanceWriters(writers);
	RegisterFlockWriters(writers);
	RegisterAnimalWriters(writers);
	RegisterPlayerInterfaceWriters(writers);
	RegisterCameraWriters(writers);
	RegisterComputerPlayerWriters(writers);
	RegisterParticleWriters(writers);
	RegisterSpellIconWriters(writers);
	RegisterSpellSeedWriters(writers);
	RegisterPuzzleGameWriters(writers);
	RegisterSpellWriters(writers);
	RegisterInfluenceRingWriters(writers);
	RegisterCreatureWriters(writers);
	RegisterFootpathWriters(writers);
	RegisterAbodeWriters(writers);
	RegisterTownDesireFlagWriters(writers);
	RegisterCitadelHeartWriters(writers);
	RegisterWorshipSiteWriters(writers);
	RegisterTownWriters(writers);
	RegisterMarkerWriters(writers);
	RegisterBuildingSiteWriters(writers);
	RegisterTownArtifactWriters(writers);
	RegisterPotWriters(writers);
	RegisterFishFarmWriters(writers);
	RegisterLandFeatureWriters(writers);
	RegisterGameStatsWriters(writers, [](size_t player) {
		const auto name = static_cast<PlayerNames>(player);
		return game_stats::Serialize(Locator::gameStatsSystem::value().PlayerStats().at(player), magic::players::MagicOf(name));
	});
	RegisterFireFlyWriters(writers, [] {
		const auto sums = worship::fire_fly::RewardSums();
		const auto probabilities = worship::fire_fly::RewardProbabilities();
		return FireFlyWorld {.fireFlies = SavedFireFlies(),
		                     .sums = {sums.begin(), sums.end()},
		                     .probabilities = {probabilities.begin(), probabilities.end()},
		                     .maySpawn = FireFliesMaySpawn()};
	});
	RegisterStreetLanternWriters(writers, audio::lantern_sounds::IsOn);
	// the music attached to things, while the game's music runs; without it the list is written as the load kept it
	if (const auto lock = audio::game_music::Lock(); audio::game_music::Get() != nullptr)
	{
		RegisterThingMusicWriters(writers, [] {
			const auto lock = audio::game_music::Lock();
			const auto* music = audio::game_music::Get();
			return music != nullptr ? music->GetThingMusic().GetInfos() : std::vector<audio::ThingMusicInfo> {};
		});
	}
	RegisterGameListWriters(writers);
	return writers;
}

void openblack::ecs::saved_world_write::KeepLoadedSources(const gamesave::WorldRead& world, LoadedObjects& loaded)
{
	KeepPlayerInterfaces(world, loaded);
	KeepCamera(world, loaded);
	LoadComputerPlayers(world, loaded);
}
