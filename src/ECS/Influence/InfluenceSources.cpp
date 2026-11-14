/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The influence of towns and citadels: a radius each.

#include <array>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/PlayerRivalry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/GameStatsSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Town/TownStats.h"
#include "Game/GameStats.h"
#include "Influence.h"
#include "InfluenceState.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// storyInfluence[land - 1]. Land 6 (one map sets it) reads the float after the array.
float StoryInfluence(const std::array<float, 5>& story, float after, int32_t land)
{
	if (land >= 1 && land <= static_cast<int32_t>(story.size()))
	{
		return story.at(static_cast<size_t>(land - 1));
	}
	return after; // land 6; (inferred) for a land < 0 or >= 7, which no map sets
}

/// The influence a citadel keeps from its first heart: the heart's influence, or the land's story influence, times the
/// size the citadel was made at
float FirstCitadelReach()
{
	const auto& info = Locator::infoConstants::value().citadelHeart;
	const auto land = influence::LandNumber();
	// M2: CREATE_CITADEL passes 1 whatever its size. TODO(influence): a planned citadel's M2 is the plan's scale;
	// openblack does not keep it (CitadelArchetype::CreatePlan) and uses 1 for it too
	constexpr float k_Scale = 1.0f;
	const float story = land != 0 ? StoryInfluence(info.storyInfluence, info.transferedDamageMultiplier, land) : info.influence;
	return k_Scale * story;
}

/// The town's base influence (from the single town info)
float BaseInfluence(const TownInfluence& town)
{
	if (town.noInfluence)
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value().town;
	const auto land = influence::LandNumber();
	return land != 0 ? StoryInfluence(info.storyInfluence, info.maxForTimeWillWorkUntil, land) : info.influence;
}

/// The abode's own info record: its abode number and mesh (a lookup by number alone would take the tribeless ark and
/// totem records that come last), or the tribe's. (inferred): the original reads the abode's own info pointer; openblack
/// keeps none, so this lookup is its own
const GAbodeInfo* AbodeInfoOf(const Abode& abode, entt::id_type mesh, Tribe tribe)
{
	const GAbodeInfo* byTribe = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != abode.type)
		{
			continue;
		}
		if (ecs::town_stats::MeshIdHash(info.meshId) == mesh)
		{
			return &info;
		}
		if (byTribe == nullptr && info.tribeType == tribe)
		{
			byTribe = &info;
		}
	}
	return byTribe;
}

/// An abode's influence: (percent built x scale x life x info.influence) x (adults + children + 1). Fields, the town
/// centre, the totem, the dispenser, the storage pit... are all Abodes; one being built counts by its percent built.
float AbodeInfluence(entt::entity entity, const Abode& abode, Tribe tribe)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* info = AbodeInfoOf(abode, mesh != nullptr ? mesh->id : 0, tribe);
	if (info == nullptr)
	{
		return 0.0f;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	// the object's scale; (inferred): taken as openblack's Transform x scale
	const float scale = transform != nullptr ? transform->scale.x : 1.0f;
	// the percent built times the scale is rounded to a float; the life and the info's influence are multiplied on in
	// the FPU's extended precision and rounded once, as the original's. (approximate) the double here can differ from
	// the extended precision in the last bit of a rare tie
	const float built = abode.percentBuilt * scale;
	const auto fixedInfluence = static_cast<float>(static_cast<double>(ecs::life::LifeOf(entity)) * static_cast<double>(built) *
	                                               static_cast<double>(info->influence));
	uint32_t adults = 0;
	uint32_t children = 0;
	for (const auto villager : abode.inhabitants)
	{
		if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr)
		{
			(v->lifeStage == Villager::LifeStage::Child ? children : adults) += 1;
		}
	}
	return fixedInfluence * static_cast<float>(adults + children + 1);
}
} // namespace

namespace openblack::influence
{
namespace
{
/// (openblack) a world without the town info or the map script's globals (unit tests) keeps the radius as it is
bool HasInfluenceInputs()
{
	return Locator::infoConstants::has_value() && Locator::mapScriptSystem::has_value();
}
} // namespace

float TownBaseInfluence(entt::entity town)
{
	const auto* influence = Locator::entitiesRegistry::value().TryGet<const TownInfluence>(town);
	if (influence == nullptr || !HasInfluenceInputs())
	{
		return 0.0f;
	}
	return BaseInfluence(*influence);
}

void StartTownInfluence(entt::entity town)
{
	// step 3 of the town's turn: the radius is the base influence again
	if (auto* influence = Locator::entitiesRegistry::value().TryGet<TownInfluence>(town);
	    influence != nullptr && HasInfluenceInputs())
	{
		influence->radius = BaseInfluence(*influence);
	}
}

void AddStructureInfluence(entt::entity town, entt::entity structure)
{
	// step 4, after the structure's own turn: its influence, unless the town has none
	auto& registry = Locator::entitiesRegistry::value();
	auto* influence = registry.TryGet<TownInfluence>(town);
	const auto* abode = registry.TryGet<const Abode>(structure);
	const auto* tribe = registry.TryGet<const Tribe>(town);
	if (influence == nullptr || abode == nullptr || tribe == nullptr || influence->noInfluence || !HasInfluenceInputs())
	{
		return;
	}
	influence->radius = influence->radius + AbodeInfluence(structure, *abode, *tribe);
}

void FinishTownInfluence(entt::entity town)
{
	// step 5: x townInfluenceMultiplier. The original multiplies only when the town has a player; (inferred): every
	// town has a player (NEUTRAL included). Then the circles are rebuilt when the radius moved since they were last
	if (auto* influence = Locator::entitiesRegistry::value().TryGet<TownInfluence>(town);
	    influence != nullptr && HasInfluenceInputs())
	{
		influence->radius = influence->radius * TownInfluenceMultiplier();
		NoteInfluence(influence->radius, influence->drawnRadius);
	}
}

void ProcessCitadels()
{
	// The citadel update, the influence part, for the citadel of each player (the first temple, as
	// CitadelInfluenceAt)
	auto& registry = Locator::entitiesRegistry::value();
	std::array<entt::entity, static_cast<size_t>(PlayerNames::_COUNT)> citadels {};
	citadels.fill(entt::null);
	registry.Each<const Temple>([&citadels](entt::entity entity, const Temple& temple) {
		const auto index = static_cast<size_t>(temple.owner);
		if (index < citadels.size() && citadels.at(index) == entt::null)
		{
			citadels.at(index) = entity;
		}
	});
	for (size_t p = 0; p < citadels.size(); ++p)
	{
		const auto citadel = citadels.at(p);
		if (citadel == entt::null)
		{
			continue;
		}
		// (the player's border is shown once the temple is drawn whole: worship::citadel::SetHeartDrawPercent)
		// NoteInfluence on the radius against the last one. CitadelRadius makes the CitadelInfluence on its first call
		const float radius = CitadelRadius(citadel);
		if (const auto* stored = registry.TryGet<const CitadelInfluence>(citadel); stored != nullptr)
		{
			NoteInfluence(radius, stored->drawnRadius);
		}
	}
}

float TownRadius(entt::entity town)
{
	const auto* influence = Locator::entitiesRegistry::value().TryGet<const TownInfluence>(town);
	return influence != nullptr ? influence->radius : 0.0f;
}

float CitadelReach(entt::entity temple)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* stored = registry.Valid(temple) ? registry.TryGet<const CitadelInfluence>(temple) : nullptr;
	return stored != nullptr ? stored->reach : FirstCitadelReach();
}

float CitadelRadius(entt::entity temple)
{
	// playerInfluenceMultiplier x the citadel's stored influence. That is set once, when the citadel's first heart is
	// made: the influence (0 then) + M2 x (land ? heart storyInfluence[land-1] : heart influence), M2 = 1 from
	// CREATE_CITADEL and the planned citadel's scale when it is built. openblack makes the heart with the temple, so the
	// value is fixed the first time it is asked for (the map script sets the land number first).
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Temple>(temple))
	{
		return 0.0f;
	}
	const auto* stored = registry.TryGet<const CitadelInfluence>(temple);
	if (stored == nullptr)
	{
		stored = &registry.Assign<CitadelInfluence>(temple, FirstCitadelReach());
	}
	return PlayerInfluenceMultiplier() * stored->reach;
}

namespace
{
/// The player's citadel: openblack's heart is made with the temple (CitadelArchetype), so a temple of the player is a
/// citadel with a heart (one per player: the first one, as CitadelInfluenceAt)
entt::entity CitadelOf(PlayerNames player)
{
	entt::entity citadel = entt::null;
	Locator::entitiesRegistry::value().Each<const Temple, const Transform>(
	    [&](entt::entity entity, const Temple& temple, const Transform&) {
		    if (citadel == entt::null && temple.owner == player)
		    {
			    citadel = entity;
		    }
	    });
	return citadel;
}

/// The player's influence power as it stands, not kept; the citadel's stored influence is fixed as CitadelRadius does
float PowerOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	// power = 0; the player's citadel with its heart -> its radius
	float power = 0.0f;
	if (const auto citadel = CitadelOf(player); citadel != entt::null)
	{
		power = CitadelRadius(citadel);
	}
	// the player's towns: each radius + power
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		power = TownRadius(town) + power;
	}
	// the rings whose player is this one: radius + power
	for (const auto entity : detail::GlobalsOrDefault().rings)
	{
		const auto* ring = registry.TryGet<const InfluenceRing>(entity);
		if (ring != nullptr && ring->player == player)
		{
			power = ring->radius + power;
		}
	}
	return power;
}

/// The player's running average of the power goes halfway to it (kept by the player service all game)
void NoteAverage(PlayerNames player, float power)
{
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	auto& players = Locator::playerSystem::value();
	auto rivalry = players.Rivalry(player);
	ecs::player_rivalry::NoteInfluencePower(rivalry, power);
	players.SetRivalry(player, rivalry);
}
} // namespace

float CalculateInfluencePower(PlayerNames player)
{
	const float power = PowerOf(player);
	// kept by the player service, across lands; (openblack) a test without it keeps none
	if (static_cast<size_t>(player) < static_cast<size_t>(PlayerNames::_COUNT) && Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().SetInfluencePower(player, power);
	}
	// its running average; the stats history is ecs::player_statistics'
	NoteAverage(player, power);
	return power;
}

void NoteInfluencePowerAfterLoad(PlayerNames player)
{
	const float power = CalculateInfluencePower(player);
	// one sample of it in the player's influence history, as every turn's; (openblack) a test without the game's
	// statistics keeps no history
	if (Locator::gameStatsSystem::has_value())
	{
		game_stats::Of(static_cast<size_t>(player)).influence.Add(power);
	}
}

void CalculateInfluencePowers()
{
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		CalculateInfluencePower(static_cast<PlayerNames>(p));
	}
}

float InfluencePower(PlayerNames player)
{
	if (static_cast<size_t>(player) >= static_cast<size_t>(PlayerNames::_COUNT) || !Locator::playerSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::playerSystem::value().InfluencePower(player);
}

float InfluencePowerRatio(PlayerNames player)
{
	// the active players and the neutral one from the first slot: an inactive player is skipped, the neutral one
	// (slot 7) is always given. The sum starts at 0 and each power is added in float
	float sum = 0.0f;
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		const auto other = static_cast<PlayerNames>(p);
		if (other != PlayerNames::NEUTRAL && magic::players::EntityOf(other) == entt::null)
		{
			continue;
		}
		sum = InfluencePower(other) + sum;
	}
	// own == 0 -> 0; else sum / own
	const float own = InfluencePower(player);
	if (own == 0.0f)
	{
		return 0.0f;
	}
	return sum / own;
}
} // namespace openblack::influence
