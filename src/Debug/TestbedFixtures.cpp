/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedFixtures.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <exception>
#include <numbers>
#include <span>
#include <utility>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Creature/LeashRules.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Trees.h"
#include "ECS/Weather/Storms.h"
#include "InfoConstants.h"
#include "LHScriptX/VillagerCommands.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/DispenserRules.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/SpellDispenser.h"

using namespace openblack;
using namespace openblack::testbed_fixtures;

namespace
{

/// The turn a seed gives the ring of huts, as a share of the angle between two huts: the golden ratio's fraction, so
/// that seeds next to each other turn it far apart
constexpr float k_SeedTurn = 0.618034f;
/// How many different turns the seed picks from
constexpr uint32_t k_SeedTurns = 16;
/// The golden angle, which spreads things evenly from a middle outwards
constexpr float k_GoldenAngle = std::numbers::pi_v<float> * (3.0f - 2.236068f);
/// Villagers stand this share of the way from the village's middle to their hut
constexpr float k_VillagerShare = 0.75f;
/// A grown villager's age
constexpr uint32_t k_VillagerAge = 20;
/// The jobs the villagers take, in turn
constexpr std::array k_VillagerJobs {VillagerNumber::Housewife, VillagerNumber::Farmer, VillagerNumber::Forester};
/// A dispenser's town as a map script names none: the nearest one
constexpr int k_NearestTown = -1;
/// A forest with no id given takes the next free one
constexpr uint32_t k_NextForest = 0;

std::string Describe(const Where& where)
{
	if (const auto* press = std::get_if<DemoPress>(&where))
	{
		return fmt::format("press {} of the hand demo", press->press);
	}
	const auto& offset = std::get<glm::vec2>(where);
	return fmt::format("({}, {}) from the middle", offset.x, offset.y);
}

/// The problem of a fixture that stands at a press, if it has one
void CheckWhere(const Where& where, const std::string& what, const Fixtures& fixtures, std::optional<size_t> pressCount,
                std::vector<std::string>& problems)
{
	const auto* press = std::get_if<DemoPress>(&where);
	if (press == nullptr)
	{
		return;
	}
	if (!fixtures.handDemo.has_value())
	{
		problems.push_back(fmt::format("{} stands at {}, but the scenario has no hand demo.", what, Describe(where)));
	}
	else if (pressCount.has_value() && press->press >= *pressCount)
	{
		problems.push_back(
		    fmt::format("{} stands at {}, but the hand demo has only {} presses.", what, Describe(where), *pressCount));
	}
}

/// The problem of a miracle given by a dispenser or put in the hand, if it has one
void CheckMagic(MagicType magic, const std::string& what, std::vector<std::string>& problems)
{
	if (magic == MagicType::None)
	{
		problems.push_back(fmt::format("{} has no miracle.", what));
		return;
	}
	const auto dispensable = magic::DispensableMiracles();
	if (std::ranges::find(dispensable, magic) == dispensable.end())
	{
		problems.push_back(fmt::format("{} has miracle {}, which has no seed to give.", what, static_cast<int>(magic)));
	}
}

void CheckLightning(const std::optional<glm::vec2>& seconds, const std::string& what, std::vector<std::string>& problems)
{
	if (!seconds.has_value())
	{
		return;
	}
	if (seconds->x < 0.0f || seconds->y < 0.0f)
	{
		problems.push_back(fmt::format("{} has a negative time between strikes.", what));
	}
	if (seconds->x > seconds->y)
	{
		problems.push_back(
		    fmt::format("{} strikes at least every {} seconds but at most every {}.", what, seconds->x, seconds->y));
	}
}

// ---- setting out ------------------------------------------------------------------------------------------------

/// What Place needs from the game, and its log
struct Placer
{
	glm::vec2 middle;
	std::span<const glm::vec2> presses;
	const std::function<std::optional<entt::entity>(size_t)>& objectAt;
	const std::function<void(std::string)>& log;
	const std::function<void(entt::entity)>& placedCreature;

	void Log(std::string line) const
	{
		if (log)
		{
			log(std::move(line));
		}
	}

	/// The point of a Where on the ground (x, ground height, z); none, with a line in the log, for a press the demo
	/// does not have
	[[nodiscard]] std::optional<glm::vec3> Ground(const Where& where, std::string_view what) const
	{
		const auto point = Resolve(where, middle, presses);
		if (!point.has_value())
		{
			Log(fmt::format("{} not placed: {} is not one of the hand demo's", what, Describe(where)));
			return std::nullopt;
		}
		return glm::vec3(point->x, Locator::terrainSystem::value().GetHeightAt(*point), point->y);
	}

	void Placed(std::string_view what, glm::vec3 at) const
	{
		Log(fmt::format("placed {} at ({:.1f}, {:.1f})", what, at.x, at.z));
	}
};

/// The services every step needs; a line in the log for the first one missing
bool HasCoreServices(const Placer& placer)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		placer.Log("fixtures not placed: there is no entity registry");
		return false;
	}
	if (!Locator::terrainSystem::has_value())
	{
		placer.Log("fixtures not placed: there is no land");
		return false;
	}
	if (!Locator::infoConstants::has_value())
	{
		placer.Log("fixtures not placed: the game's tables are not loaded");
		return false;
	}
	return true;
}

/// The tribe's abode of that number; none when the tables have no such abode (their lookup throws then)
AbodeInfo AbodeOf(Tribe tribe, AbodeNumber number)
{
	try
	{
		return GAbodeInfo::Find(tribe, number);
	}
	catch (const std::exception&)
	{
		return AbodeInfo::None;
	}
}

/// The seed of a miracle and the level it is powered up to for it, as a dispenser's bubble has them; none when no
/// seed has that miracle
std::optional<std::pair<SpellSeedType, int>> SeedOf(MagicType magic)
{
	const auto& tables = Locator::infoConstants::value();
	const auto seed = magic::GetSpellSeedOfMagicInfo(tables, magic);
	if (static_cast<int>(seed) < 0)
	{
		return std::nullopt;
	}
	return std::make_pair(seed, magic::GetPowerUpGesture(magic::GetSpellSeedInfo(tables, seed), magic).level);
}

/// The town a dispenser joins when a map script names none: the first player's first town, else the nearest town
entt::entity DispenserTown(glm::vec3 at)
{
	const auto towns = ecs::map_cells::TownsOf(PlayerNames::PLAYER_ONE);
	if (!towns.empty())
	{
		return towns.front();
	}
	return ecs::map_cells::FindNearestTownInList(map_coords::FromMetres(glm::vec2(at.x, at.z)));
}

void PlaceDispenser(const Placer& placer, const Dispenser& dispenser, size_t index)
{
	const auto what = fmt::format("dispenser {}", index);
	const auto at = placer.Ground(dispenser.at, what);
	if (!at.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = DispenserTown(*at);
	if (town == entt::null)
	{
		placer.Log(fmt::format("{} not placed: it needs a town, so the scenario needs a village fixture", what));
		return;
	}
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto abode = AbodeOf(tribe != nullptr ? *tribe : Tribe::CELTIC, AbodeNumber::SpellDispenser);
	if (abode == AbodeInfo::None)
	{
		placer.Log(fmt::format("{} not placed: its town's tribe has no spell dispenser", what));
		return;
	}
	// As the map script's spell dispenser command: the dispenser, then its miracle and period, which makes its first
	// bubble at once (a period of 0 leaves it with that one only)
	const auto entity = worship::dispenser::Create(*at, abode, k_NearestTown, glm::radians(dispenser.yawDegrees), 1.0f);
	if (entity == entt::null)
	{
		placer.Log(fmt::format("{} not placed: the dispenser could not be made", what));
		return;
	}
	worship::dispenser::SetMagicAndPeriod(entity, dispenser.magic, dispenser.periodTurns);
	placer.Placed(fmt::format("{} of miracle {}", what, static_cast<int>(dispenser.magic)), *at);
}

void PlaceStorm(const Placer& placer, const Storm& storm, size_t index)
{
	const auto what = fmt::format("storm {}", index);
	const auto at = placer.Ground(storm.at, what);
	if (!at.has_value())
	{
		return;
	}
	// The climates' own storms would come and go with the random numbers: off, so that the run is the same each time
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetStormCreationEnabled(false);
	}
	else
	{
		placer.Log("there is no weather system: the climates' own storms were not turned off");
	}
	weather::storms::StormDescriptor descriptor;
	descriptor.position = *at;
	descriptor.innerRadius = storm.innerRadius;
	descriptor.outerRadius = storm.outerRadius;
	descriptor.lifeTime = storm.lifeSeconds;
	// The weather bytes as the script's weather properties command scales them: whole degrees, and percentages
	descriptor.weather.temperature = static_cast<int8_t>(static_cast<int32_t>(storm.temperature));
	descriptor.weather.rain = static_cast<int8_t>(static_cast<int32_t>(storm.rain * 100.0f));
	descriptor.weather.snow = static_cast<int8_t>(static_cast<int32_t>(storm.snow * 100.0f));
	if (storm.forkSeconds.has_value())
	{
		descriptor.forkMin = storm.forkSeconds->x;
		descriptor.forkMax = storm.forkSeconds->y;
	}
	if (storm.sheetSeconds.has_value())
	{
		descriptor.sheetMin = storm.sheetSeconds->x;
		descriptor.sheetMax = storm.sheetSeconds->y;
	}
	weather::storms::Create(descriptor);
	placer.Placed(what, *at);
}

/// Where a player's built temple keeps its creature, found as the game finds it each turn (FollowTemplePens): the
/// temple mesh's pen point; none without such a temple
std::optional<glm::vec3> TemplePenOf(PlayerNames owner)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::optional<glm::vec3> pen;
	registry.Each<const ecs::components::Temple, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Temple& temple, const ecs::components::Transform& transform) {
		    if (pen.has_value() || temple.owner != owner)
		    {
			    return;
		    }
		    if (const auto* build = registry.TryGet<const ecs::components::CitadelPartBuild>(entity);
		        build != nullptr && (build->buildFlags & ecs::components::CitadelPartBuild::k_Built) == 0)
		    {
			    return;
		    }
		    const auto* mesh = registry.TryGet<const ecs::components::Mesh>(entity);
		    if (mesh != nullptr && meshes.Contains(mesh->id))
		    {
			    pen = ecs::player_creature::TemplePenPoint(transform, meshes.Handle(mesh->id)->GetExtraMetrics());
		    }
	    });
	return pen;
}

void PlaceCreature(const Placer& placer, const Creature& creature, size_t index)
{
	const auto what = fmt::format("creature {}", index);
	auto at = placer.Ground(creature.at, what);
	if (!at.has_value())
	{
		return;
	}
	if (creature.hold == Hold::TemplePen)
	{
		// Set out at its temple's pen point, on the ground; the fixture's point is used when there is none
		if (const auto pen = TemplePenOf(creature.owner); pen.has_value())
		{
			const glm::vec2 penXZ(pen->x, pen->z);
			at = glm::vec3(penXZ.x, Locator::terrainSystem::value().GetHeightAt(penXZ), penXZ.y);
		}
		else
		{
			placer.Log(fmt::format("{}: player {} has no built temple with a pen point, so it stands at its own point", what,
			                       static_cast<int>(creature.owner)));
		}
	}
	const glm::vec2 xz(at->x, at->z);
	entt::entity entity = entt::null;
	if (creature.file.empty())
	{
		// The profile's creature, as a land's script loads the player's own
		if (creature.owner != PlayerNames::PLAYER_ONE)
		{
			placer.Log(fmt::format("{}: the profile's creature is always the first player's", what));
		}
		entity = ecs::player_creature::LoadMyCreature(xz);
	}
	else
	{
		// The script's species is a row of the creature tables, which start with the Giant Ape
		const auto row = creature.species == CreatureType::GiantApe ? 0 : static_cast<int32_t>(creature.species);
		entity = ecs::player_creature::ScriptLoadCreature(row, creature.file, creature.owner, xz);
	}
	if (entity == entt::null)
	{
		placer.Log(fmt::format("{} not placed: no creature could be loaded from {}", what,
		                       creature.file.empty() ? std::string_view("the profile") : creature.file));
		return;
	}
	placer.Placed(what, *at);
	if (placer.placedCreature)
	{
		placer.placedCreature(entity);
	}
	if (!creature.knows.empty())
	{
		if (Locator::leashSystem::has_value())
		{
			for (const auto type : creature.knows)
			{
				Locator::leashSystem::value().SetKnown(entity, type, true);
			}
			placer.Log(fmt::format("{} knows {} more leashes", what, creature.knows.size()));
		}
		else
		{
			placer.Log(fmt::format("{} knows no more leashes: there is no leash system", what));
		}
	}
	if (creature.hold == Hold::Free)
	{
		return;
	}
	if (creature.hold == Hold::TemplePen)
	{
		// Free: each turn the game makes its temple's pen point its home and draws it at the pen's size near it
		placer.Log(fmt::format("{} in its temple's pen", what));
		return;
	}
	if (!Locator::leashSystem::has_value())
	{
		placer.Log(fmt::format("{} is free: there is no leash system", what));
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	if (creature.hold == Hold::Leashed)
	{
		leashes.SetKnown(entity, creature.leash, true);
		leashes.SetLeashable(entity, true);
		placer.Log(fmt::format("{} {}", what, leashes.PutOn(entity, creature.leash) ? "leashed" : "could not be leashed"));
		return;
	}
	// Penned: its home where it stands, on the ground as a script's home is, and kept near it as the leash keeps a
	// young creature
	ecs::player_creature::SetHome(leashes, Locator::entitiesRegistry::value(), entity,
	                              ecs::player_creature::HomeOnGround(*at, at->y));
	leashes.ConfineToHome(entity, creature_leash::k_HomeConfinement);
	placer.Log(fmt::format("{} penned within {} m", what, creature_leash::k_HomeConfinement));
}

/// A town id no town has yet: one past the highest
int NewTownId(ecs::Registry& registry)
{
	int highest = -1;
	registry.Each<const ecs::components::Town>([&highest](entt::entity, const ecs::components::Town& town) {
		highest = std::max(highest, static_cast<int>(town.id));
	});
	return highest + 1;
}

/// A villager at its hut, as the map script's villager-at-abode helper adds it: the hut while it has room, else the
/// abode of its town with the most space, else the town's homeless
void AddVillager(entt::entity town, entt::entity hut, entt::entity villager)
{
	if (ecs::abode_villagers::GetRoomLeftForAdults(hut) > 0)
	{
		ecs::abode_villagers::AddVillagerToAbode(hut, villager);
		return;
	}
	if (const auto abode = ecs::town_villagers::FindAbodeWithSpaceInTown(town, villager, 0.0f); abode != entt::null)
	{
		ecs::abode_villagers::AddVillagerToAbode(abode, villager);
		return;
	}
	ecs::town_villagers::AddVillagerToTown(town, villager);
}

void PlaceVillage(const Placer& placer, const Village& village, size_t index)
{
	const auto what = fmt::format("village {}", index);
	const auto at = placer.Ground(village.at, what);
	if (!at.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& terrain = Locator::terrainSystem::value();
	const auto onGround = [&terrain, &at](glm::vec2 offset) {
		const glm::vec2 xz = glm::vec2(at->x, at->z) + offset;
		return glm::vec3(xz.x, terrain.GetHeightAt(xz), xz.y);
	};

	// The town as the map script's town command makes it: the town, then the creation numbers its desire flags take
	const int townId = NewTownId(registry);
	const auto town = ecs::archetypes::TownArchetype::Create(townId, *at, village.owner, village.tribe);
	ecs::object_index::Skip(7);
	if (town == entt::null)
	{
		placer.Log(fmt::format("{} not placed: the town could not be made", what));
		return;
	}
	const auto townKey = static_cast<uint32_t>(townId);
	placer.Placed(fmt::format("{} town {}", what, townId), *at);

	// The huts, as the map script's abode command makes them, with no food or wood of their own
	const auto hutInfo = AbodeOf(village.tribe, AbodeNumber::A);
	std::vector<std::pair<entt::entity, glm::vec2>> huts;
	if (hutInfo == AbodeInfo::None)
	{
		placer.Log(fmt::format("{}: its tribe has no hut", what));
	}
	else
	{
		for (const auto offset : HutOffsets(village))
		{
			const auto point = onGround(offset);
			const auto hut = ecs::archetypes::AbodeArchetype::Create(townKey, point, hutInfo, 0.0f, 1.0f, 0, 0);
			if (hut != entt::null)
			{
				huts.emplace_back(hut, offset);
			}
		}
		placer.Log(fmt::format("placed {} huts of {}", huts.size(), what));
	}

	if (village.storagePit)
	{
		const auto pitInfo = AbodeOf(village.tribe, AbodeNumber::StoragePit);
		const auto pit = pitInfo == AbodeInfo::None
		                     ? entt::null
		                     : ecs::archetypes::AbodeArchetype::Create(townKey, *at, pitInfo, 0.0f, 1.0f,
		                                                               static_cast<uint32_t>(std::max(village.food, 0)),
		                                                               static_cast<uint32_t>(std::max(village.wood, 0)));
		if (pit != entt::null)
		{
			placer.Placed(fmt::format("{} storage pit with {} food and {} wood", what, village.food, village.wood), *at);
		}
		else
		{
			placer.Log(fmt::format("{}: its storage pit could not be made", what));
		}
	}

	if (village.villagers == 0)
	{
		return;
	}
	if (huts.empty())
	{
		placer.Log(fmt::format("{}: no villagers placed, it has no huts", what));
		return;
	}
	size_t made = 0;
	for (size_t i = 0; i < village.villagers; ++i)
	{
		const auto job = k_VillagerJobs.at(i % k_VillagerJobs.size());
		const auto info = lhscriptx::villager_commands::FindVillagerInfo(village.tribe, job);
		if (!info.has_value())
		{
			continue;
		}
		const auto& [hut, offset] = huts.at(i % huts.size());
		const auto villager = ecs::archetypes::VillagerArchetype::Create(onGround(offset), onGround(offset * k_VillagerShare),
		                                                                 *info, k_VillagerAge, false);
		if (villager == entt::null)
		{
			continue;
		}
		AddVillager(town, hut, villager);
		++made;
	}
	placer.Log(fmt::format("placed {} villagers of {}", made, what));
}

void PlaceTemple(const Placer& placer, const Temple& temple, size_t index)
{
	const auto what = fmt::format("temple {}", index);
	const auto at = placer.Ground(temple.at, what);
	if (!at.has_value())
	{
		return;
	}
	// As the map script's citadel command: built at once, turned about the vertical, its leash posts made with it
	const glm::mat4 rotation(affine::AngleY(static_cast<float>(temple.rotation) * 0.001f));
	if (ecs::archetypes::CitadelArchetype::Create(*at, temple.owner, rotation, glm::vec3(1.0f)) == entt::null)
	{
		placer.Log(fmt::format("{} not placed: the temple could not be made", what));
		return;
	}
	placer.Placed(fmt::format("{} of player {}", what, static_cast<int>(temple.owner)), *at);
}

void PlaceFire(const Placer& placer, const Fire& fire, size_t index)
{
	const auto what = fmt::format("fire {}", index);
	if (!Locator::fireSystem::has_value())
	{
		placer.Log(fmt::format("{} not lit: there is no fire system", what));
		return;
	}
	entt::entity target = entt::null;
	glm::vec3 at {0.0f};
	if (const auto* object = std::get_if<size_t>(&fire.what))
	{
		const auto found = placer.objectAt ? placer.objectAt(*object) : std::nullopt;
		if (!found.has_value() || *found == entt::null)
		{
			placer.Log(fmt::format("{} not lit: the scenario has no object {}", what, *object));
			return;
		}
		target = *found;
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(target))
		{
			at = transform->position;
		}
	}
	else
	{
		const auto point = placer.Ground(std::get<Where>(fire.what), what);
		if (!point.has_value())
		{
			return;
		}
		at = *point;
		target = ecs::archetypes::BonfireArchetype::Create(at, 0.0f, 1.0f);
		if (target == entt::null)
		{
			placer.Log(fmt::format("{} not lit: the bonfire could not be made", what));
			return;
		}
	}
	Locator::fireSystem::value().SetOnFire(target, fire.speed);
	placer.Placed(what, at);
}

void PlaceTrees(const Placer& placer, const Trees& trees, size_t index)
{
	const auto what = fmt::format("trees {}", index);
	const auto at = placer.Ground(trees.at, what);
	if (!at.has_value())
	{
		return;
	}
	const auto& terrain = Locator::terrainSystem::value();
	// A forest for them, then each tree as the map script's tree command makes it: nothing on top of another object,
	// a tree that is not scenery, grown to its full size
	const auto forest = ecs::CreateForest(k_NextForest, *at);
	const auto forestId = ecs::ResolveForestId(static_cast<int32_t>(forest));
	size_t made = 0;
	for (const auto offset : SpreadOffsets(trees.count, trees.spread))
	{
		const glm::vec2 xz = glm::vec2(at->x, at->z) + offset;
		const glm::vec3 point(xz.x, terrain.GetHeightAt(xz), xz.y);
		if (!ecs::map_collide::IsOkToCreateAtPos(point, "CREATE_TREE"))
		{
			continue;
		}
		if (ecs::archetypes::TreeArchetype::Create(forestId, point, trees.type, true, 0.0f, trees.scale, trees.scale) !=
		    entt::null)
		{
			++made;
		}
	}
	placer.Placed(fmt::format("{} of {} trees in forest {}", what, made, forest), *at);
}

void PlacePile(const Placer& placer, const Pile& pile, size_t index)
{
	const auto what = fmt::format("pile {}", index);
	const auto at = placer.Ground(pile.at, what);
	if (!at.has_value())
	{
		return;
	}
	// As the map script's pot command: nothing on top of another object or without an amount, and the pot spreads its
	// reaction as it is made
	if (!ecs::map_collide::IsOkToCreateAtPos(*at, "CREATE_POT") || pile.amount <= 0)
	{
		placer.Log(fmt::format("{} not placed: something stands there, or it has no amount", what));
		return;
	}
	ecs::animal_ai::SetupPotReaction(ecs::archetypes::PotArchetype::Create(*at, 0.0f, pile.type, pile.amount));
	placer.Placed(what, *at);
}

void PlaceHighlight(const Placer& placer, const Highlight& highlight, size_t index)
{
	const auto what = fmt::format("highlight {}", index);
	const auto at = placer.Ground(highlight.at, what);
	if (!at.has_value())
	{
		return;
	}
	// As the map script's highlight command: at the point, with no challenge, unturned and at full size
	const auto row = static_cast<uint32_t>(highlight.info);
	if (ecs::script_highlight::Create(*at, row, 0, 0.0f, 1.0f) == entt::null)
	{
		placer.Log(fmt::format("{} not placed: no highlight has row {}", what, row));
		return;
	}
	placer.Placed(fmt::format("{} of row {}", what, row), *at);
}

void PlaceHandSeed(const Placer& placer, MagicType magic)
{
	if (!Locator::handSystem::has_value())
	{
		placer.Log("hand seed not given: there is no hand");
		return;
	}
	if (Locator::handSystem::value().GetHeldObject().has_value())
	{
		placer.Log("hand seed not given: the hand is not free");
		return;
	}
	const auto seed = SeedOf(magic);
	if (!seed.has_value())
	{
		placer.Log(fmt::format("hand seed not given: no seed has miracle {}", static_cast<int>(magic)));
		return;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().LocalPlayer() : PlayerNames::PLAYER_ONE;
	if (magic::one_off::CreateSpellIntoHand(player, seed->first, seed->second, 1.0f) == entt::null)
	{
		placer.Log(fmt::format("hand seed not given: the hand did not take seed {}", static_cast<int>(seed->first)));
		return;
	}
	placer.Log(fmt::format("placed the seed of miracle {} in the hand", static_cast<int>(magic)));
}

/// Runs one step, its exceptions a line in the log
template <typename Step>
void Guarded(const Placer& placer, std::string_view what, Step&& step)
{
	try
	{
		step();
	}
	catch (const std::exception& error)
	{
		placer.Log(fmt::format("{} not placed: {}", what, error.what()));
	}
	catch (...)
	{
		placer.Log(fmt::format("{} not placed: an unknown error", what));
	}
}

} // namespace

bool testbed_fixtures::Empty(const Fixtures& fixtures)
{
	return fixtures.dispensers.empty() && fixtures.storms.empty() && fixtures.creatures.empty() && fixtures.villages.empty() &&
	       fixtures.temples.empty() && fixtures.fires.empty() && fixtures.trees.empty() && fixtures.piles.empty() &&
	       fixtures.highlights.empty() && !fixtures.handSeed.has_value() && !fixtures.handDemo.has_value() &&
	       !fixtures.writesGameData;
}

std::vector<std::string> testbed_fixtures::Problems(const Fixtures& fixtures, std::optional<size_t> pressCount)
{
	std::vector<std::string> problems;
	for (size_t i = 0; i < fixtures.dispensers.size(); ++i)
	{
		const auto& dispenser = fixtures.dispensers[i];
		const auto what = fmt::format("Dispenser {}", i);
		CheckMagic(dispenser.magic, what, problems);
		CheckWhere(dispenser.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.storms.size(); ++i)
	{
		const auto& storm = fixtures.storms[i];
		const auto what = fmt::format("Storm {}", i);
		if (storm.innerRadius > storm.outerRadius)
		{
			problems.push_back(fmt::format("{} has an inner radius of {}, larger than its outer radius of {}.", what,
			                               storm.innerRadius, storm.outerRadius));
		}
		if (storm.lifeSeconds <= 0.0f)
		{
			problems.push_back(fmt::format("{} lasts {} seconds; it must last longer than 0.", what, storm.lifeSeconds));
		}
		if (storm.rain < 0.0f || storm.rain > 1.0f)
		{
			problems.push_back(fmt::format("{} has rain {}, outside 0 to 1.", what, storm.rain));
		}
		if (storm.snow < 0.0f || storm.snow > 1.0f)
		{
			problems.push_back(fmt::format("{} has snow {}, outside 0 to 1.", what, storm.snow));
		}
		CheckLightning(storm.forkSeconds, what + "'s fork lightning", problems);
		CheckLightning(storm.sheetSeconds, what + "'s sheet lightning", problems);
		CheckWhere(storm.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.creatures.size(); ++i)
	{
		const auto& creature = fixtures.creatures[i];
		const auto what = fmt::format("Creature {}", i);
		if (creature.hold == Hold::Leashed && creature.leash == LeashType::None)
		{
			problems.push_back(fmt::format("{} is leashed, but with no leash.", what));
		}
		if (std::ranges::find(creature.knows, LeashType::None) != creature.knows.end())
		{
			problems.push_back(fmt::format("{} knows a leash of no kind.", what));
		}
		if (creature.hold == Hold::TemplePen && std::ranges::none_of(fixtures.temples, [&creature](const Temple& temple) {
			    return temple.owner == creature.owner;
		    }))
		{
			problems.push_back(fmt::format("{} is held in its temple's pen, but player {} has no temple.", what,
			                               static_cast<int>(creature.owner)));
		}
		CheckWhere(creature.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.villages.size(); ++i)
	{
		const auto& village = fixtures.villages[i];
		const auto what = fmt::format("Village {}", i);
		if (village.huts == 0 && village.villagers > 0)
		{
			problems.push_back(fmt::format("{} has {} villagers but no huts for them.", what, village.villagers));
		}
		else if (village.huts == 0)
		{
			problems.push_back(fmt::format("{} has no huts.", what));
		}
		CheckWhere(village.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.temples.size(); ++i)
	{
		const auto& temple = fixtures.temples[i];
		const auto what = fmt::format("Temple {}", i);
		if (temple.owner >= PlayerNames::NEUTRAL)
		{
			problems.push_back(fmt::format("{} belongs to no player.", what));
		}
		else if (std::ranges::any_of(std::span(fixtures.temples).first(i),
		                             [&temple](const Temple& other) { return other.owner == temple.owner; }))
		{
			problems.push_back(
			    fmt::format("{} is a second temple of player {}; a player has one.", what, static_cast<int>(temple.owner)));
		}
		CheckWhere(temple.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.fires.size(); ++i)
	{
		if (const auto* where = std::get_if<Where>(&fixtures.fires[i].what))
		{
			CheckWhere(*where, fmt::format("Fire {}", i), fixtures, pressCount, problems);
		}
	}
	for (size_t i = 0; i < fixtures.trees.size(); ++i)
	{
		const auto& trees = fixtures.trees[i];
		const auto what = fmt::format("Trees {}", i);
		if (trees.count == 0)
		{
			problems.push_back(fmt::format("{} has a count of 0.", what));
		}
		CheckWhere(trees.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.piles.size(); ++i)
	{
		const auto& pile = fixtures.piles[i];
		const auto what = fmt::format("Pile {}", i);
		if (pile.amount <= 0)
		{
			problems.push_back(fmt::format("{} has an amount of {}; it must be more than 0.", what, pile.amount));
		}
		CheckWhere(pile.at, what, fixtures, pressCount, problems);
	}
	for (size_t i = 0; i < fixtures.highlights.size(); ++i)
	{
		CheckWhere(fixtures.highlights[i].at, fmt::format("Highlight {}", i), fixtures, pressCount, problems);
	}
	if (fixtures.handSeed.has_value())
	{
		CheckMagic(*fixtures.handSeed, "The hand seed", problems);
	}
	if (fixtures.handDemo.has_value() && fixtures.handDemo->name.empty())
	{
		problems.emplace_back("The hand demo has no name.");
	}
	return problems;
}

std::optional<glm::vec2> testbed_fixtures::Resolve(const Where& where, glm::vec2 middle, std::span<const glm::vec2> presses)
{
	if (const auto* press = std::get_if<DemoPress>(&where))
	{
		if (press->press >= presses.size())
		{
			return std::nullopt;
		}
		return presses[press->press] + press->nudge;
	}
	return middle + std::get<glm::vec2>(where);
}

std::vector<glm::vec2> testbed_fixtures::HutOffsets(const Village& village)
{
	std::vector<glm::vec2> offsets;
	if (village.huts == 0)
	{
		return offsets;
	}
	offsets.reserve(village.huts);
	const auto count = static_cast<float>(village.huts);
	const float step = 2.0f * std::numbers::pi_v<float> / count;
	// Huts on a circle k_HutSpacing apart along their chord
	const float fitting = village.huts > 1 ? k_HutSpacing / (2.0f * std::sin(step * 0.5f)) : 0.0f;
	const float radius = std::max(k_MinHutRing, fitting);
	const float turnShare = std::fmod(static_cast<float>(village.seed % k_SeedTurns) * k_SeedTurn, 1.0f);
	const float turn = turnShare * step;
	for (size_t i = 0; i < village.huts; ++i)
	{
		// Clockwise from north (y) towards east (x)
		const float angle = turn + step * static_cast<float>(i);
		offsets.emplace_back(radius * std::sin(angle), radius * std::cos(angle));
	}
	return offsets;
}

std::vector<glm::vec2> testbed_fixtures::SpreadOffsets(size_t count, float spread)
{
	std::vector<glm::vec2> offsets;
	offsets.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		if (spread <= 0.0f || count == 1)
		{
			offsets.emplace_back(0.0f);
			continue;
		}
		// The sunflower's pattern: each a golden angle on from the last, at the root of its share of the circle
		const float radius = spread * std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(count));
		const float angle = k_GoldenAngle * static_cast<float>(i);
		offsets.emplace_back(radius * std::sin(angle), radius * std::cos(angle));
	}
	return offsets;
}

void testbed_fixtures::Place(const Fixtures& fixtures, glm::vec2 middle, std::span<const glm::vec2> presses,
                             const std::function<std::optional<entt::entity>(size_t)>& objectAt,
                             const std::function<void(std::string)>& log,
                             const std::function<void(entt::entity)>& placedCreature)
{
	const Placer placer {
	    .middle = middle, .presses = presses, .objectAt = objectAt, .log = log, .placedCreature = placedCreature};
	bool ready = false;
	Guarded(placer, "fixtures", [&] { ready = HasCoreServices(placer); });
	if (!ready)
	{
		return;
	}
	// Villages first: the dispensers join their towns
	for (size_t i = 0; i < fixtures.villages.size(); ++i)
	{
		Guarded(placer, fmt::format("village {}", i), [&] { PlaceVillage(placer, fixtures.villages[i], i); });
	}
	for (size_t i = 0; i < fixtures.temples.size(); ++i)
	{
		Guarded(placer, fmt::format("temple {}", i), [&] { PlaceTemple(placer, fixtures.temples[i], i); });
	}
	for (size_t i = 0; i < fixtures.dispensers.size(); ++i)
	{
		Guarded(placer, fmt::format("dispenser {}", i), [&] { PlaceDispenser(placer, fixtures.dispensers[i], i); });
	}
	for (size_t i = 0; i < fixtures.storms.size(); ++i)
	{
		Guarded(placer, fmt::format("storm {}", i), [&] { PlaceStorm(placer, fixtures.storms[i], i); });
	}
	for (size_t i = 0; i < fixtures.creatures.size(); ++i)
	{
		Guarded(placer, fmt::format("creature {}", i), [&] { PlaceCreature(placer, fixtures.creatures[i], i); });
	}
	for (size_t i = 0; i < fixtures.trees.size(); ++i)
	{
		Guarded(placer, fmt::format("trees {}", i), [&] { PlaceTrees(placer, fixtures.trees[i], i); });
	}
	for (size_t i = 0; i < fixtures.piles.size(); ++i)
	{
		Guarded(placer, fmt::format("pile {}", i), [&] { PlacePile(placer, fixtures.piles[i], i); });
	}
	for (size_t i = 0; i < fixtures.highlights.size(); ++i)
	{
		Guarded(placer, fmt::format("highlight {}", i), [&] { PlaceHighlight(placer, fixtures.highlights[i], i); });
	}
	// Fires last, so that they can be set to what the other fixtures made
	for (size_t i = 0; i < fixtures.fires.size(); ++i)
	{
		Guarded(placer, fmt::format("fire {}", i), [&] { PlaceFire(placer, fixtures.fires[i], i); });
	}
	if (fixtures.handSeed.has_value())
	{
		Guarded(placer, "hand seed", [&] { PlaceHandSeed(placer, *fixtures.handSeed); });
	}
}
