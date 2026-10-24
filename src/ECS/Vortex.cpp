/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Vortex.h"

#include <cmath>

#include <algorithm>
#include <bit>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Services/VortexSound.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Events/Publish.h"
#include "ECS/Flocks.h"
#include "ECS/LandReseat.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptTypes.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/VortexSave.h"
#include "ECS/VortexSaveLines.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/VortexCrossingRules.h"
#include "Magic/VortexPullRules.h"
#include "Magic/VortexRules.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleTypes.h"
#include "Particles/Rules/VortexAttract.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct VortexState
{
	/// the vortexes in creation order (the newest first, inferred from the list insert)
	std::vector<entt::entity> vortexes {};
};

VortexState& VortexData()
{
	return openblack::Locator::scriptState::value().Get<VortexState>();
}

LandscapeVortex* Find(entt::entity vortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(vortex) ? registry.TryGet<LandscapeVortex>(vortex) : nullptr;
}

/// The seconds in the state, 0 when Inactive
float StateSeconds(const LandscapeVortex& v)
{
	if (v.state == VortexStateType::Inactive)
	{
		return 0.0f;
	}
	// the zero-extended turn difference, + the turn fraction, x the ms per turn, x 0.001
	const auto turns = static_cast<float>(static_cast<int64_t>(game_clock::Turn() - v.stateTurn));
	const float t = turns + game_clock::TurnFraction();
	return t * static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
}

/// The vortex's levelling amount (openblack::vortex::LevelAmount)
float LandFactor(const LandscapeVortex& v)
{
	return openblack::vortex::LevelAmount(v.state, StateSeconds(v));
}

/// An In or Out pulls the 11 x 11 cells under it to their mean as it fades in (openblack::vortex::LevelSquare)
void FlattenLand(LandscapeVortex& v)
{
	namespace rules = openblack::vortex;
	if (!rules::LevelsGround(v.type) || !Locator::terrainSystem::has_value()) // not the volcano
	{
		return;
	}
	const float q = LandFactor(v);
	if (!(q > v.landQ))
	{
		return;
	}
	v.landQ = q;
	auto& island = Locator::terrainSystem::value();
	const glm::vec2 centre(v.position.x, v.position.z);
	const auto centreCell = rules::CentreCell(centre); // truncated toward zero
	const auto altitudeAt = [&island](glm::ivec2 cell) -> uint8_t {
		if (cell.x < 0 || cell.x > 0x1FF || cell.y < 0 || cell.y > 0x1FF || !island.HasBlockAt(glm::u16vec2(cell)))
		{
			return 0;
		}
		return static_cast<uint8_t>(island.GetCellAltitude(island.GetCell(glm::u16vec2(cell))) & 0xFF);
	};
	if (v.savedAltitudes.empty())
	{
		for (size_t i = 0; i < rules::k_LevelCellCount; ++i)
		{
			v.savedAltitudes.push_back(altitudeAt(rules::SquareCell(centreCell, i)));
		}
		v.meanAltitude = rules::AverageAltitude(v.savedAltitudes);
	}
	// (openblack) the ground under the things that follow the land, read before the land changes (ecs::land_reseat)
	const int half = rules::k_LevelHalfSide;
	const glm::ivec2 minCorner(std::max(centreCell.x - half, 0), std::max(centreCell.y - half, 0));
	const glm::ivec2 maxCorner(std::min(centreCell.x + half, 0x1FF), std::min(centreCell.y + half, 0x1FF));
	auto grounds = land_reseat::RecordGroundsUnder(minCorner, maxCorner);
	const auto heights = rules::LevelSquare(centre, v.savedAltitudes, v.meanAltitude, q);
	bool changed = false;
	for (size_t i = 0; i < heights.size(); ++i)
	{
		const auto cell = rules::SquareCell(centreCell, i);
		const auto alt = heights.at(i);
		// the original writes every cell; openblack's island has none off the map (as CitadelArchetype)
		if (cell.x >= 0 && cell.y >= 0 && cell.x <= 0x1FF && cell.y <= 0x1FF && alt != altitudeAt(cell))
		{
			island.SetCellAltitude(glm::u16vec2(cell), static_cast<uint16_t>(alt));
			changed = true;
		}
	}
	if (changed)
	{
		island.CommitAltitudeChanges(); // (openblack) the land follows the new altitudes, only when a cell changed
		events::Publish(
		    events::LandAltitudesChanged {.minCorner = minCorner, .maxCorner = maxCorner, .groundsBefore = std::move(grounds)});
	}
}

/// The land's height under a point, 0 without a land
float GroundAltitude(glm::vec3 position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
}

/// One of the vortex's effects of its info row at a point, 0 for none: local unless said (only the object mover is
/// synchronised)
uint32_t StartEffect(ParticleType type, glm::vec3 at,
                     game_random::particles::NetGameType net = game_random::particles::NetGameType::Local)
{
	const auto file = particles::ParticleTypeFile(type);
	if (type == ParticleType::None || file.empty())
	{
		return 0;
	}
	// no direction, magnitude 1, no spell to report to
	return particles::manager::StartForSpell(std::string(file), at, glm::vec3(0.0f), 1.0f, nullptr, net);
}

/// The vortex's effects are deleted at once (not closed down) and its decal released
void ReleaseEffects(LandscapeVortex& v)
{
	for (auto* id : {&v.objectMover, &v.preLandscape, &v.postLandscape, &v.lightMap})
	{
		if (*id != 0)
		{
			particles::manager::Delete(*id);
			*id = 0;
		}
	}
	v.decal.made = false;
}

/// A step with the strength `power`, enabled; the id is cleared once the effect is finished
void StepEffect(uint32_t& id, float power, float seconds)
{
	if (id == 0)
	{
		return;
	}
	const particles::ProcessInfo info {
	    .power = power,
	    .enabled = true,
	};
	if (!particles::manager::ProcessForSpell(id, info, seconds))
	{
		id = 0;
	}
}

/// An object can when it can become a physics object. A creature can when a script has let it through and it is on
/// the leash, or whenever a script controls it (openblack::vortex::CreatureMayBeTaken)
bool CanBeSuckedIntoVortex(entt::entity object)
{
	if (const auto* creature = Locator::entitiesRegistry::value().TryGet<const Creature>(object); creature != nullptr)
	{
		const bool leashed = Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(object);
		return openblack::vortex::CreatureMayBeTaken(creature->mayGoThroughVortex, leashed,
		                                             script_held::IsControlledByScript(object));
	}
	return Locator::dynamicsSystem::value().CanBecomeAPhysicsObject(object);
}

/// Get2DRadius x 0.7: the radius of the vortex's own cylinder, never drawn
float SuckRadius(entt::entity vortex)
{
	return object::Get2DRadius(vortex) * openblack::vortex::k_CreatureTakenShare;
}

/// The In's contents
void ProcessIn(entt::entity entity, LandscapeVortex& vortex, uint32_t turn)
{
	if (vortex.state != VortexStateType::Active)
	{
		return;
	}
	// the whole radius on every 3rd turn, half otherwise
	const float r = turn % 3 == 0 ? vortex.radius : vortex.radius * 0.5f;
	auto cell = map_coords::CellOf(vortex.position);
	glm::ivec2 offset(0); // the spiral's whole-cell offset (stepping the MapCoords adds only the high words)
	map_coords::Spiral spiral;
	for (int remaining = 9999; remaining > 0; --remaining) // 9999 cells
	{
		if (!map_coords::InBounds(cell))
		{
			// off the map, straight to the next spiral step
		}
		else
		{
			// A walked cell farther than R ends the walk. The walked MapCoords keeps the vortex's fractions, so the distance is
			// exactly the whole-cell offset's: Hypotenuse(dx << 16, dz << 16) in metres
			const float distance = gutils::ConvertWholeDistanceToMeters(
			    gutils::Hypotenuse(static_cast<int32_t>(static_cast<uint32_t>(offset.x) << 16),
			                       static_cast<int32_t>(static_cast<uint32_t>(offset.y) << 16)));
			if (distance > r)
			{
				return;
			}
			for (const auto object : map_cells::ObjectsInCell(cell))
			{
				auto& registry = Locator::entitiesRegistry::value();
				if (!registry.Valid(object))
				{
					continue;
				}
				// taken at rest, only in the cell of its own MapCoords, so an object over several cells is taken once
				if (map_coords::Cell(object::MapCoordsOf(object)) == cell && CanBeSuckedIntoVortex(object))
				{
					const auto& transform = registry.Get<const Transform>(object);
					ecs::vortex::TakeIn(entity, object, glm::vec3(0.0f), transform.rotation, transform.position, false);
					continue;
				}
				// else a pile, in any of its cells, gives up to 1000 into a new pot of its kind, sized at (GameFloatRand(0.3)
				// x 0.4 + 0.3) of its own size, which is taken the same way
				if (!object_resources::IsPileResource(object))
				{
					continue;
				}
				const auto piece = object_resources::SplitOffPile(object, openblack::vortex::crossing::k_PileTake);
				if (piece == entt::null)
				{
					continue;
				}
				const float share = openblack::vortex::crossing::PileShare(
				    game_random::GameFloatRand(openblack::vortex::crossing::k_PileShareRandom));
				auto& transform = registry.Get<Transform>(piece);
				transform.scale *= share;
				map_cells::InsertMapObject(piece);
				ecs::vortex::TakeIn(entity, piece, glm::vec3(0.0f), transform.rotation, transform.position, false);
			}
		}
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
		offset += glm::ivec2(step.x, step.z);
	}
}

/// The Out hands an arriving object over
void HandOver(entt::entity entity, LandscapeVortex& vortex, entt::entity object, float angle)
{
	// already being thrown -> nothing
	if (std::ranges::find(vortex.thrownVillagers, object) != vortex.thrownVillagers.end())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const Villager>(object); villager != nullptr)
	{
		if (vortex.town != entt::null)
		{
			// out of its old town, into the Out's; with a script's flock too it comes as a disciple from the vortex
			if (villager->town != entt::null && registry.Valid(villager->town))
			{
				town_villagers::RemoveVillager(villager->town, object);
			}
			town_villagers::AddVillagerToTown(vortex.town, object);
			if (vortex.flock != entt::null)
			{
				ecs::villager::SetVillagerDisciple(object, entt::null, VillagerDisciple::FromVortex, 0);
			}
		}
		// a villager joins the script's flock, town or not; a flock a script holds hands it to the script, which takes
		// control of it
		if (vortex.flock != entt::null)
		{
			flocks::AddMember(vortex.flock, object);
			if (script_held::IsInScript(vortex.flock) && Locator::scriptObjects::has_value() &&
			    Locator::scriptObjects::value().Register(object, false))
			{
				Locator::scriptObjects::value().AddReference(object);
				script_held::SetControlledByScript(object, true);
			}
		}
	}
	if (const auto* animal = registry.TryGet<const Animal>(object); animal != nullptr)
	{
		// an animal is pointed at the Out's animal flock, made at the first one at the script's flock position (a new
		// one when the flock's first member is of another kind); being pointed at it does not make it a member, so the
		// flock stays empty in play and every animal shares it. It also takes the Out's town
		if (vortex.animalFlock != entt::null)
		{
			const auto members = flocks::MembersFromHead(vortex.animalFlock);
			const auto* first =
			    !members.empty() && registry.Valid(members.front()) ? registry.TryGet<const Animal>(members.front()) : nullptr;
			if (first != nullptr && first->type != animal->type)
			{
				flocks::RemoveLiving(vortex.animalFlock, object, true);
				vortex.animalFlock = entt::null;
			}
		}
		if (vortex.animalFlock == entt::null)
		{
			// the flock creators' own info, the vortex's player and the script flocks' id
			vortex.animalFlock = flocks::Create(vortex.flockPosition, flocks::k_DefaultFlockInfo, ecs::vortex::k_Player,
			                                    flocks::k_ScriptFlockId);
			if (auto* flock = registry.TryGet<Flock>(vortex.animalFlock); flock != nullptr)
			{
				// the flock parameters truncated into its words: the distance, then the radius
				flock->flockDistance = static_cast<uint16_t>(map_coords::FtoL(vortex.flockA));
				flock->domainRadius = static_cast<uint16_t>(map_coords::FtoL(vortex.flockB));
			}
		}
		flocks::SetFlock(object, vortex.animalFlock);
		animal_ai::SetTown(object, vortex.town);
	}
	// A Dove (pending: whether the citadel and spell doves are doves too) ends here. Anything else goes to the front
	// of the thrown list, is flung (ecs::physics::particle_carried_objects::Fling) and cannot be destroyed nor eaten until
	// the Out goes
	if (registry.AllOf<Animal>(object) && registry.Get<const Animal>(object).type == AnimalInfo::Dove)
	{
		return;
	}
	vortex.thrownVillagers.insert(vortex.thrownVillagers.begin(), object);
	ecs::physics::particle_carried_objects::Fling(object, entity, angle, 1.0f); // the player argument is not read
	registry.AssignOrReplace<Indestructible>(object);
}

/// The Out's contents: the town, the thrown list, the emission on even turns (the file's objects, then 30 new
/// villagers) and the hand-over with its fling. (pending) the land script's last created object
void ProcessOut(entt::entity entity, LandscapeVortex& vortex, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (vortex.animalFlock != entt::null && !ecs::IsAvailable(vortex.animalFlock))
	{
		vortex.animalFlock = entt::null;
	}
	if (vortex.flock != entt::null && !ecs::IsAvailable(vortex.flock))
	{
		vortex.flock = entt::null;
	}
	if (vortex.town == entt::null) // the nearest town within 200 m of its own position
	{
		vortex.town = map_cells::GetNearestTown(map_coords::FromWorld(vortex.position), 200.0f);
	}
	// The thrown list: the gone ones leave it; a villager in it stays alive (its life set to 1.0)
	std::erase_if(vortex.thrownVillagers, [](entt::entity v) { return !ecs::IsAvailable(v); });
	for (const auto thrown : vortex.thrownVillagers)
	{
		if (registry.AllOf<Villager>(thrown))
		{
			life::SetLife(thrown, 1.0f);
		}
	}
	// while it still has something to emit (its file, or fewer than 30 new villagers) the state's turn follows the game
	// turn, so its fade does not run out while it emits
	if (vortex.reader != nullptr || vortex.statVillagers < 30)
	{
		vortex.stateTurn = game_clock::Turn();
	}
	// the land script's last created object is forgotten once it is not available
	if (const auto last = lhscriptx::Script::LastCreated();
	    last != entt::null && !ecs::physics::particle_carried_objects::IsAvailable(last))
	{
		lhscriptx::Script::SetLastCreated(entt::null);
	}
	(void)registry;
	(void)entity;
	if ((turn & 1) != 0) // only on even turns
	{
		return;
	}
	// the emission cell, 16 m away at a random angle; each axis' cell is (cell x 10 +- 16 trig) / 10 truncated toward zero, and
	// only the high word changes (the fraction of the vortex's own MapCoords is kept)
	const float angle = game_random::GameFloatRand(std::bit_cast<float>(0x40C90FDBu)); // 2 pi
	auto at = map_coords::FromWorld(vortex.position);
	const auto cellX = static_cast<float>(static_cast<uint32_t>(at.x) >> 16);
	const auto cellZ = static_cast<float>(static_cast<uint32_t>(at.z) >> 16);
	const float dx = static_cast<float>(std::sin(static_cast<double>(angle))) * 16.0f;  // sin unrounded, x 16
	const float dz = static_cast<float>(std::cos(static_cast<double>(angle))) * -16.0f; // cos, x -16
	const auto newX = static_cast<uint32_t>(static_cast<int32_t>((cellX * 10.0f + dx) / 10.0f)) & 0xFFFFu;
	const auto newZ = static_cast<uint32_t>(static_cast<int32_t>((cellZ * 10.0f + dz) / 10.0f)) & 0xFFFFu;
	at.x = static_cast<int32_t>((newX << 16) | (static_cast<uint32_t>(at.x) & 0xFFFFu));
	at.z = static_cast<int32_t>((newZ << 16) | (static_cast<uint32_t>(at.z) & 0xFFFFu));
	// With its VortexSave reader, the next line of vortex.txt run as a land-script command at `at` (every A position +=
	// at); the end of the file deletes the reader
	if (vortex.reader != nullptr)
	{
		const auto read = vortex_save::ReadNext(*vortex.reader, at);
		if (!read.has_value())
		{
			vortex.reader.reset();
			return;
		}
		// the created thing as an object; none -> nothing more this turn.
		// (approximate) an Object = a valid entity with a Transform
		const auto object = *read;
		if (object != entt::null && registry.Valid(object) && registry.AllOf<Transform>(object))
		{
			// the statistics, which only a saved game reads: one more object, the wood and the food it holds (each kind
			// as the thing answers for it), and one more villager when it is one
			vortex.statObjects += 1;
			vortex.statResources += object_resources::GetResource(object, ResourceType::Wood);
			vortex.statResources += object_resources::GetResource(object, ResourceType::Food);
			vortex.statVillagers += registry.AllOf<Villager>(object) ? 1 : 0; // IsVillager
			HandOver(entity, vortex, object, angle);
		}
		return;
	}
	// no file left: up to 30 new villagers, one per even turn
	if (vortex.statVillagers >= 30)
	{
		return;
	}
	vortex.statVillagers += 1;
	// the town's tribe, 7 without a town (Tribe::NORSE; inferred: the same enum, as TownDesire.h)
	auto tribe = Tribe::NORSE;
	if (vortex.town != entt::null)
	{
		if (const auto* t = registry.TryGet<const Tribe>(vortex.town))
		{
			tribe = *t;
		}
	}
	// GameFloatRand(1.0) < 0.5 -> the housewife (row tribe x 7), else GameRand(5) + 1 (forester .. leader)
	int row = static_cast<int>(tribe) * 7;
	if (!(game_random::GameFloatRand(1.0f) < 0.5f))
	{
		row += static_cast<int>(game_random::GameRand(5)) + 1;
	}
	// every 5th one (the new count % 5 == 0) is GameRand(9) + 1 years old, the others GameRand(6) + 16
	const uint32_t age = vortex.statVillagers % 5 == 0 ? game_random::GameRand(9) + 1 : game_random::GameRand(6) + 16;
	const auto position = map_coords::ToWorld(at);
	const auto villager = archetypes::VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(row), age, false);
	if (villager != entt::null)
	{
		HandOver(entity, vortex, villager, angle);
	}
}
} // namespace

entt::entity ecs::vortex::Create(glm::vec3 position, VortexType type)
{
	auto& vortexState = VortexData();
	if (type != VortexType::In && type != VortexType::Out && type != VortexType::Volcano)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// The base: a static object with scale 1.0; the info row, a constant per class (In 0, Out 1, Volcano 2); the
	// radius, Create's 50.0
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& vortex = registry.Assign<LandscapeVortex>(entity);
	vortex.type = type;
	vortex.info = static_cast<int32_t>(type);
	vortex.radius = openblack::vortex::k_PullRadius;
	vortex.position = position;
	// a static thing of the map, in its cells as the others are (a way out of a land is struck there by what flies)
	map_cells::InsertMapObject(entity);
	// the volcano's rumble, a loop on the vortex for as long as it stands; the In and the Out are silent
	if (const auto tag = audio::vortex_sound::CreationTag(type, entity))
	{
		audio::sound_tags::Create(*tag);
	}
	if (type == VortexType::In)
	{
		// its writer of vortex.txt, which it empties: none in a run of the checks
		vortex.writer = vortex_save::OpenWriter();
	}
	if (type == VortexType::Out)
	{
		vortex.flockPosition = position; // the Out's flock position: its own
		// the vortex.txt reader, then the land script's last created object cleared
		vortex.reader = vortex_save::OpenReader();
		lhscriptx::Script::SetLastCreated(entt::null);
	}
	// The creation setup with the info.dat vortex row: the state, then its particle systems. The object mover (the
	// only synchronised one: it carries what an In takes), the effect over the land and the light map stand at the
	// vortex's point on the land (the script's vectors come with y = 0), the ground effect on the land under it.
	// (pending) the depth box scaled by the base scale, the spline
	if (Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value().vortex.at(static_cast<size_t>(type));
		vortex.state = info.initialState;
		const glm::vec3 onLand(position.x, GroundAltitude(position), position.z);
		vortex.objectMover = StartEffect(info.particleTypeObjectMover, onLand, game_random::particles::NetGameType::Synced);
		// the object mover is given the vortex's player
		if (auto* mover = vortex.objectMover != 0 ? particles::manager::Find(vortex.objectMover) : nullptr; mover != nullptr)
		{
			mover->SetPlayer(static_cast<int>(ecs::vortex::k_Player));
		}
		vortex.preLandscape = StartEffect(info.particleTypePreLandscape, onLand);
		vortex.postLandscape = StartEffect(info.particleTypePostLandscape, onLand);
		vortex.lightMap = StartEffect(info.particleTypeLightMap, onLand);
		// the ground and over-the-land effects are stepped every frame; the light map once a turn
		particles::manager::SetPerFrame(vortex.preLandscape);
		particles::manager::SetPerFrame(vortex.postLandscape);
	}
	vortexState.vortexes.insert(vortexState.vortexes.begin(), entity);
	if (vortex.state == VortexStateType::FadeIn)
	{
		StartFadeIn(entity); // the fade's turn
	}
	return entity;
}

void ecs::vortex::StartFadeIn(entt::entity entity)
{
	if (auto* v = Find(entity))
	{
		v->state = VortexStateType::FadeIn;
		v->stateTurn = game_clock::Turn();
	}
}

bool ecs::vortex::StartFadeOut(entt::entity entity)
{
	if (auto* v = Find(entity))
	{
		v->state = VortexStateType::FadeOut;
		v->stateTurn = game_clock::Turn();
		return true;
	}
	return false;
}

void ecs::vortex::SetParameters(entt::entity entity, entt::entity town, glm::vec3 position, float a, float b,
                                entt::entity flock)
{
	// the town and the flock parameters are the Out's; (inferred) the others' do nothing
	if (auto* v = Find(entity); v != nullptr && v->type == VortexType::Out)
	{
		v->town = town;
		v->flockPosition = position;
		v->flockA = a;
		v->flockB = b;
		v->flock = flock;
	}
}

void ecs::vortex::ReactToPhysicsImpact(entt::entity entity, entt::entity hitter, glm::vec3 velocity, const glm::mat3& rows,
                                       glm::vec3 position)
{
	const auto* v = Find(entity);
	// active, a hitter, CanBeSuckedIntoVortex and IN_PHYSICS
	if (v == nullptr || hitter == entt::null || !Locator::entitiesRegistry::value().Valid(hitter) ||
	    !openblack::vortex::TakesWhatStrikesIt(v->type, v->state, Locator::dynamicsSystem::value().IsFlying(hitter),
	                                           CanBeSuckedIntoVortex(hitter)))
	{
		return;
	}
	TakeIn(entity, hitter, velocity, rows, position, true);
}

void ecs::vortex::TakeIn(entt::entity entity, entt::entity object, glm::vec3 velocity, const glm::mat3& rows,
                         glm::vec3 position, bool fromPhysics)
{
	auto* v = Find(entity);
	if (v == nullptr || !ecs::IsAvailable(object))
	{
		return;
	}
	// A creature is never carried in: closer to the middle than SuckRadius it sparkles away for good (its fizz to 1 over
	// 3 seconds, then its removal, which keeps it for the next land when it is the player's own); farther, nothing
	if (Locator::entitiesRegistry::value().AllOf<Creature>(object))
	{
		const float distance = gutils::GetDistanceInMetres(object::MapCoordsOf(entity), object::MapCoordsOf(object));
		if (distance < SuckRadius(entity) && Locator::creatureFizzSystem::has_value())
		{
			Locator::creatureFizzSystem::value().SetFizz(object, openblack::vortex::k_CreatureSparkleTarget,
			                                             openblack::vortex::k_CreatureSparkleSeconds, true);
		}
		return;
	}
	// Anything else that CanBecomeAPhysicsObject, when the vortex has its object mover: a record for the mover's rule
	// (UR_VortexAttract, Particles/Rules/VortexAttract.cpp), which takes it at the mover's next step
	auto* mover = v->objectMover != 0 ? particles::manager::Find(v->objectMover) : nullptr;
	if (!Locator::dynamicsSystem::value().CanBecomeAPhysicsObject(object) || mover == nullptr)
	{
		return;
	}
	VortexObjectInfo info {
	    .object = object,
	    .fromPhysics = fromPhysics,
	    .velocity = velocity,
	    .rows = rows,
	    .position = position,
	    .indestructible = Locator::entitiesRegistry::value().AllOf<ecs::components::Indestructible>(object),
	    .vortex = entity,
	};
	v->queue.push_back(info);
	mover->AddTarget(object);
}

std::optional<VortexObjectInfo> ecs::vortex::TakeQueued(entt::entity object)
{
	for (const auto entity : VortexData().vortexes)
	{
		auto* v = Find(entity);
		if (v == nullptr)
		{
			continue;
		}
		// the last record of it, as the effect hands its targets back last first
		const auto found = std::find_if(v->queue.rbegin(), v->queue.rend(),
		                                [object](const VortexObjectInfo& record) { return record.object == object; });
		if (found != v->queue.rend())
		{
			const auto record = *found;
			v->queue.erase(std::next(found).base());
			return record;
		}
	}
	return std::nullopt;
}

namespace
{
/// What the crossing file's lines read from the rest of the game, now
vortex_save_lines::World SaveLinesWorld()
{
	const auto* info = &Locator::infoConstants::value();
	auto* registry = &Locator::entitiesRegistry::value();
	return vortex_save_lines::World {
	    .turn = game_clock::Turn(),
	    .villagerRow = [](entt::entity villager) { return static_cast<int32_t>(script_type::SubtypeOf(villager)); },
	    .potResource =
	        [info](PotInfo type) {
		        const auto row = static_cast<size_t>(type);
		        return row < info->pot.size() ? static_cast<int32_t>(info->pot.at(row).resourceType) : 0;
	        },
	    .life = [](entt::entity thing) { return life::LifeOf(thing); },
	    // the things written with a player (the dead trees) name the one they belong to
	    .player = [registry](entt::entity thing) { return vortex_save_lines::OwnerOf(*registry, thing); },
	    .ofAStoragePit = [](entt::entity pile) { return StoragePitStore::OwnerOf(pile) != entt::null; },
	    .magicName =
	        [info](SpellSeedType seed, int powerUp) {
		        const auto& record = magic::MagicInfoForPowerUpLevel(*info, magic::GetSpellSeedInfo(*info, seed), powerUp);
		        // the name fills its field up to the first nought, if it has one
		        const auto& name = magic::GetMagicEffectInfo(*info, record.magicType).debugString;
		        return std::string(name.begin(), std::ranges::find(name, '\0'));
	        },
	    .placeOf = [](entt::entity thing) { return object::MapCoordsOf(thing); },
	    .lyingOnTheLand =
	        [registry](entt::entity thing) {
		        const bool held = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld() == thing;
		        return !held && !registry->AnyOf<components::InPhysics, components::CarriedByTornado>(thing);
	        },
	};
}
} // namespace

void ecs::vortex::HandOff(entt::entity entity, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* v = Find(entity);
	if (v == nullptr || !registry.Valid(object))
	{
		return;
	}
	// it leaves the land for good: a villager its town, an animal its town and its flock (so a villager's line names no
	// town, and an animal's no flock and no town, as the original's do)
	if (const auto* villager = registry.TryGet<const Villager>(object);
	    villager != nullptr && villager->town != entt::null && registry.Valid(villager->town))
	{
		town_villagers::RemoveVillager(villager->town, object);
	}
	if (registry.AllOf<Animal>(object))
	{
		animal_ai::DeleteDependencies(object);
	}
	// then its line goes into the crossing file, placed relative to the vortex: each thing once
	if (v->writer == nullptr || !v->writer->written.insert(object).second)
	{
		return;
	}
	const auto world = SaveLinesWorld();
	auto vortexPlace = map_coords::FromMetres({v->position.x, v->position.z});
	vortexPlace.altitude = 0.0f;
	if (const auto line =
	        vortex_save_lines::LineFor(std::as_const(registry), object, object::MapCoordsOf(object), vortexPlace, world))
	{
		if (const auto text = vortex_save_lines::Text(*line))
		{
			vortex_save::Write(*v->writer, *text);
			// a static thing's or a thing lying about's fellows lying just where it stood count as written with it
			std::vector<entt::entity> fellows;
			if (std::holds_alternative<openblack::vortex::crossing::MobileStaticLine>(*line))
			{
				fellows = vortex_save_lines::IdenticalStatics(std::as_const(registry), object, world);
			}
			else if (std::holds_alternative<openblack::vortex::crossing::MobileObjectLine>(*line))
			{
				fellows = vortex_save_lines::IdenticalMobileObjects(std::as_const(registry), object, world);
			}
			v->writer->written.insert(fellows.begin(), fellows.end());
		}
	}
}

std::optional<std::string> ecs::vortex::LineWhereItIs(entt::entity object)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (object == entt::null || !registry.Valid(object))
	{
		return std::nullopt;
	}
	auto place = object::MapCoordsOf(object);
	auto vortexPlace = place;
	vortexPlace.altitude = 0.0f;
	const auto line = vortex_save_lines::LineFor(registry, object, place, vortexPlace, SaveLinesWorld());
	return line.has_value() ? vortex_save_lines::Text(*line) : std::nullopt;
}

void ecs::vortex::ProcessAll(uint32_t turn)
{
	auto& vortexState = VortexData();
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(vortexState.vortexes, [&registry](entt::entity e) { return !registry.Valid(e); });
	const auto vortexes = vortexState.vortexes; // ToBeDeleted below
	for (const auto entity : vortexes)
	{
		auto* v = Find(entity);
		if (v == nullptr)
		{
			continue;
		}
		// First the contents (In / Out; the Volcano has none)
		if (v->type == VortexType::In)
		{
			ProcessIn(entity, *v, turn);
		}
		else if (v->type == VortexType::Out)
		{
			ProcessOut(entity, *v, turn);
		}
		// then the light map, then the object mover (which takes what the contents handed it), both at full strength
		// with the turn's length; a vortex already being deleted has lost its effects
		if (!ecs::IsAvailable(entity))
		{
			ReleaseEffects(*v);
		}
		const float turnSeconds = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
		StepEffect(v->lightMap, 1.0f, turnSeconds);
		StepEffect(v->objectMover, 1.0f, turnSeconds);
		const bool done = StateSeconds(*v) > openblack::vortex::k_FadeOverSeconds;
		if (v->state == VortexStateType::FadeIn && done)
		{
			v->state = VortexStateType::Active;
		}
		else if (v->state == VortexStateType::FadeOut && done)
		{
			v->state = VortexStateType::Inactive;
			OnDeleted(entity);
			ecs::ToBeDeleted(entity);
			continue;
		}
		FlattenLand(*v);
	}
}

void ecs::vortex::UpdateGroundEffects(float seconds)
{
	// The ground effects are drawn before the land: moved to the ground under the vortex, raised as the land under it
	// settles, and stepped with the fade as their strength, only while the vortex shows
	const auto vortexes = VortexData().vortexes;
	for (const auto entity : vortexes)
	{
		auto* v = Find(entity);
		if (v == nullptr)
		{
			continue;
		}
		if (!ecs::IsAvailable(entity))
		{
			ReleaseEffects(*v);
			continue;
		}
		const float f = openblack::vortex::Openness(v->state, StateSeconds(*v));
		v->decal = NextDecal(v->decal, f);
		if (f != 0.0f && v->preLandscape != 0)
		{
			const float height = GroundAltitude(v->position) - openblack::vortex::SwirlDepth(LandFactor(*v));
			particles::manager::SetOrigin(v->preLandscape, glm::vec3(v->position.x, height, v->position.z));
			StepEffect(v->preLandscape, f, seconds);
		}
	}
}

void ecs::vortex::UpdateOverLandEffects(float seconds)
{
	// Each one's effect over the land, stepped the same way where it was made, and the light map's alpha. (pending)
	// the original does this only for a vortex in the land's object draw list (docs/wiki/world/vortex.md#the-z-sorted-part)
	const auto vortexes = VortexData().vortexes;
	for (const auto entity : vortexes)
	{
		auto* v = Find(entity);
		if (v == nullptr || !ecs::IsAvailable(entity))
		{
			continue;
		}
		const float e = StateSeconds(*v);
		const float f = openblack::vortex::Openness(v->state, e);
		if (f != 0.0f)
		{
			StepEffect(v->postLandscape, f, seconds);
		}
		if (auto* effect = particles::manager::Find(v->lightMap); effect != nullptr)
		{
			effect->SetGlobalAlpha(static_cast<float>(AlphaByte(openblack::vortex::GlowBrightness(v->state, e))));
		}
	}
	// what the object movers carry is drawn where their particles are
	particles::vortex_attract::UpdateCarriedObjects();
}

uint8_t ecs::vortex::AlphaByte(float value)
{
	return static_cast<uint8_t>(static_cast<uint32_t>(static_cast<int32_t>(value * 255.0f)) & 0xFFu);
}

VortexDecal ecs::vortex::NextDecal(const VortexDecal& decal, float f)
{
	if (f == decal.lastFade)
	{
		return decal;
	}
	VortexDecal next = decal;
	next.lastFade = f;
	next.made = f != 0.0f;
	next.alphaRef = next.made ? openblack::vortex::GroundHoleThreshold(f) : 0;
	return next;
}

bool ecs::vortex::CreatureMustAvoid(entt::entity vortex, entt::entity creature)
{
	const auto* v = Find(vortex);
	// a way out of a land: a creature keeps clear unless it may be taken; any other vortex, and with no creature: yes
	if (v == nullptr || v->type != VortexType::In || creature == entt::null)
	{
		return true;
	}
	return !CanBeSuckedIntoVortex(creature);
}

bool ecs::vortex::IsVillagerBeingThrown(entt::entity villager)
{
	for (const auto entity : VortexData().vortexes)
	{
		// only the Out looks in its thrown list, the others say no
		if (const auto* v = Find(entity); v != nullptr && v->type == VortexType::Out &&
		                                  std::ranges::find(v->thrownVillagers, villager) != v->thrownVillagers.end())
		{
			return true;
		}
	}
	return false;
}

void ecs::vortex::OnDeleted(entt::entity entity)
{
	auto* v = Find(entity);
	if (v == nullptr)
	{
		return;
	}
	// In: its VortexSave writer goes (the file is closed) and its records with it; Out: its VortexSave reader and the
	// thrown villagers' list, everything in it destructible again, whoever made it so
	v->writer.reset();
	v->queue.clear();
	v->reader.reset();
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto thrown : v->thrownVillagers)
	{
		if (registry.Valid(thrown))
		{
			registry.Remove<Indestructible>(thrown);
		}
	}
	v->thrownVillagers.clear();
	// its particle systems and its decal. (pending) the depth box
	ReleaseEffects(*v);
	std::erase(VortexData().vortexes, entity);
}
