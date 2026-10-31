/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameRewardOpeningWorld.h"

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "Particles/Utility.h"
#include "Worship/SpellSeedGraphic.h"
#include "Worship/TownCentreSpellIcon.h"
#include "Worship/TownMagic.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

bool GameCoinIsHeads()
{
	return game_random::GameRand(2) != 0;
}
} // namespace

GameRewardOpeningWorld::GameRewardOpeningWorld(entt::entity chest)
    : _chest(chest)
{
}

PlayerNames GameRewardOpeningWorld::Player() const
{
	// Only the chest's own player can tap it open, so an opening chest always has one; a chest with none, which no one
	// can open, would be the neutral player's
	const auto* chest = Locator::entitiesRegistry::value().TryGet<const Reward>(_chest);
	return chest != nullptr && chest->player.has_value() ? *chest->player : PlayerNames::NEUTRAL;
}

entt::entity GameRewardOpeningWorld::Town() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* chest = registry.TryGet<const Reward>(_chest);
	return chest != nullptr && chest->town != entt::null && registry.Valid(chest->town) &&
	               registry.AllOf<components::Town>(chest->town)
	           ? chest->town
	           : entt::null;
}

glm::vec3 GameRewardOpeningWorld::Position() const
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(_chest);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

void GameRewardOpeningWorld::Pour(ResourceType type, uint32_t amount)
{
	// Poured by the player at this computer, as the game pours it
	if (Locator::resourceStoreSystem::has_value())
	{
		Locator::resourceStoreSystem::value().PourAt(type, Position(), amount, false, LocalPlayer(), false);
	}
}

bool GameRewardOpeningWorld::HasTown() const
{
	return Town() != entt::null;
}

void GameRewardOpeningWorld::GiveTownBelief(float belief)
{
	// The sparkle of a magic object made, on the chest for its time
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().StartSpotVisual(SpotVisualType::MagicObjectCreated, Position(),
		                                                 reward::k_BeliefSparkleTurns, entt::null, 1.0f);
	}
	// Given with no thing to rise from, so no belief symbol shows and no sound plays
	town_stores::AddToBelief(Town(), Player(), belief, entt::null, true, static_cast<int>(GuidanceAlignment::Good));
}

void GameRewardOpeningWorld::MakeObject(MobileStaticInfo object)
{
	// Where the chest stands, unturned and at its own size
	const auto made = archetypes::MobileStaticArchetype::Create(Position(), object, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	// a rock is the chest's player's (the neutral player's when the chest has none); other mobile statics keep no player
	if (object_physics::IsRockObject(made))
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto* chest = registry.TryGet<const Reward>(_chest);
		registry.Get<MobileStatic>(made).owner =
		    chest != nullptr && chest->player.has_value() ? *chest->player : PlayerNames::NEUTRAL;
	}
}

void GameRewardOpeningWorld::MakeScaffold()
{
	// Only for a town with a workshop: a scaffold where the chest stands for the first workshop of the town's list, unturned
	// and at its own size, already worth one, and the workshop takes it as one of its own
	const auto town = Town();
	const auto* held = town != entt::null ? Locator::entitiesRegistry::value().TryGet<const components::Town>(town) : nullptr;
	if (held == nullptr || held->workshops.empty())
	{
		return;
	}
	const auto workshop = held->workshops.front();
	const auto scaffold = scaffolds::Create(map_coords::FromWorld(Position()), town, workshop, 0.0f, 1.0f);
	if (scaffold == entt::null)
	{
		return;
	}
	scaffolds::SetValue(scaffold, 1);
	workshops::AddScaffold(workshop, scaffold);
}

void GameRewardOpeningWorld::FindTownForMiracle()
{
	// The player's towns in their own list's order: the first whose village centre shows fewer than six miracles
	for (const auto town : map_cells::TownsOf(Player()))
	{
		const auto centre = worship::town::TownCentreOf(town);
		if (centre != entt::null &&
		    static_cast<size_t>(worship::town_centre::SpellCount(centre)) < reward::k_CentreMostMiracles)
		{
			Locator::entitiesRegistry::value().Get<Reward>(_chest).town = town;
			return;
		}
	}
}

std::vector<reward::ProgressEntry> GameRewardOpeningWorld::ProgressList() const
{
	std::vector<reward::ProgressEntry> list;
	if (!Locator::infoConstants::has_value())
	{
		return list;
	}
	const auto& info = Locator::infoConstants::value();
	const float alignment =
	    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(Player()) : 0.0f;
	const auto add = [&list](const auto& entries) {
		for (const auto& entry : entries)
		{
			list.push_back({.magic = entry.magicType, .onLand = entry.onLand});
		}
	};
	if (reward::TakesGoodList(alignment, GameCoinIsHeads))
	{
		add(info.rewardProgressGood);
	}
	else
	{
		add(info.rewardProgressEvil);
	}
	return list;
}

MagicType GameRewardOpeningWorld::NextMiracle()
{
	return reward::NextMiracle(ProgressList(), influence::LandNumber(), [this](MagicType type) { return PlayerMay(type); });
}

MagicType GameRewardOpeningWorld::RandomMiracle()
{
	return reward::RandomMiracle(ProgressList(), influence::LandNumber(), [](uint32_t n) { return game_random::GameRand(n); });
}

SpellSeedType GameRewardOpeningWorld::FirstSeedOf(MagicType type) const
{
	if (!Locator::infoConstants::has_value())
	{
		return SpellSeedType::None;
	}
	return magic::GetFirstSpellSeedForMagicType(Locator::infoConstants::value(), type);
}

int GameRewardOpeningWorld::LevelCount(SpellSeedType seed) const
{
	if (!Locator::infoConstants::has_value() || seed == SpellSeedType::None)
	{
		return 0;
	}
	return magic::GetNumPowerUpLevels(magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed));
}

MagicType GameRewardOpeningWorld::MagicAt(SpellSeedType seed, int level) const
{
	if (!Locator::infoConstants::has_value() || seed == SpellSeedType::None)
	{
		return MagicType::None;
	}
	const auto& row = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	return reward::MagicAtLevel(row.magicTypes, row.powerUpGestures, level);
}

bool GameRewardOpeningWorld::PlayerMay(MagicType type) const
{
	return magic::players::IsMagicTypeEnabled(Player(), type);
}

void GameRewardOpeningWorld::PlayerLearns(MagicType type)
{
	// One more holder of it for good, and ever enabled
	magic::players::SetMagicTypeEnabled(Player(), type, true);
}

bool GameRewardOpeningWorld::TownLearns(MagicType type)
{
	return worship::town::AddMagicTypesHeld(Town(), type);
}

void GameRewardOpeningWorld::ShowTownSeed(SpellSeedType seed)
{
	// The town's new miracle spins above the chest, smaller than on an icon, in the chest's player's colours, with no
	// power-up band, until the chest goes
	auto& registry = Locator::entitiesRegistry::value();
	if (seed == SpellSeedType::None || !registry.Valid(_chest) || !registry.AllOf<Reward>(_chest))
	{
		return;
	}
	const auto graphic = worship::seed_graphic::Create(Position() + glm::vec3(0.0f, reward::k_TownSeedAbove, 0.0f), seed,
	                                                   Player(), reward::k_TownSeedScale, -1);
	registry.Get<Reward>(_chest).townSeed = graphic;
}

void GameRewardOpeningWorld::OneShotGlobe(SpellSeedType seed)
{
	if (seed != SpellSeedType::None && Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().CreateOneOffSeed(Position() + glm::vec3(0.0f, reward::k_GlobeAbove, 0.0f), seed,
		                                               magic::k_PlainMiracleLevel, 1.0f);
	}
}

void GameRewardOpeningWorld::ShowGesture(MagicType type)
{
	if (type == MagicType::None || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto gesture = magic::GetMagicInfo(Locator::infoConstants::value(), type).gestureType;
	if (gesture != GestureType::None)
	{
		// Shown for the interface that tapped it, its own player's
		particles::utility::ShowGestureOnLand(gesture, Position(), reward::k_GestureSize, Player(), Player() == LocalPlayer());
	}
}
