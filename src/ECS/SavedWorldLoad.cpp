/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedWorldLoad.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <iterator>
#include <list>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <MindFile.h>
#include <SaveSlots.h>
#include <SavedRecords.h>
#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/CameraTrack.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Services/LanternSounds.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureScriptAgendas.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TownDesireFlagArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillageTotemArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAutoscale.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSizeLimits.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/ShepherdWork.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TreePuzzle.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreationDraws.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Footpaths.h"
#include "ECS/HanoiRules.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/LivingFootpath.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/ObjectResources.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PlayerCreature.h"
#include "ECS/PotResource.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ResourceStores.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadThingMusic.h"
#include "ECS/SavedWorld/LoadWorkshops.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorld/WriteArenas.h"
#include "ECS/SavedWorld/WriteDataPaths.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Wonders.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/TreePuzzleRules.h"
#include "ECS/Villager/VillagerScriptMemory.h"
#include "ECS/VillagerAge.h"
#include "ECS/Vortex.h"
#include "ECS/VortexSave.h"
#include "ECS/WalkGoal.h"
#include "ECS/WallHugBookkeeping.h"
#include "ECS/WallHugRules.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/WeatherThing.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Hand/HandMagicFX.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/Spells/SpellFlock.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/ParticleTypes.h"
#include "Particles/Rules/Explosion.h"
#include "Particles/Rules/Lightning.h"
#include "Particles/TownBelief.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Citadel.h"
#include "Worship/FireFlyReward.h"
#include "Worship/SpecialPoints.h"
#include "Worship/SpellDispenser.h"
#include "Worship/SpellSeedGraphic.h"
#include "Worship/TownCentreSpellIcon.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack::ecs;

namespace
{
/// The numbers of the first tree row and the first big forest row; the others of each follow it downwards
constexpr uint32_t k_FirstTreeRow = 2115;
constexpr uint32_t k_FirstBigForestRow = 369;
constexpr uint32_t k_FirstPotRow = 1876;
constexpr uint32_t k_FirstAbodeRow = 2394;
/// The citadel heart's row, the one heart row the game's tables hold, which a planned citadel heart is of
constexpr uint32_t k_CitadelHeartInfoRow = 2411;
constexpr uint32_t k_FirstMobileStaticRow = 1937;
constexpr uint32_t k_FirstVillagerRow = 2247;
constexpr uint32_t k_FirstAnimalRow = 2548;
/// The creature rows: the 17 species of the game's creature tables
constexpr uint32_t k_FirstCreatureRow = 2517;
constexpr uint32_t k_CreatureRows = 17;
/// The tribe rows, read before the other tables: the first tribe's number, the others following it downwards
constexpr uint32_t k_FirstTribeRow = 742;
/// A living thing's status bit of its way along a footpath, towards the footpath's head
constexpr uint16_t k_LivingFootpathReverse = 0x0008;
/// The spell seed rows: the first seed's number, the others following it downwards
constexpr uint32_t k_FirstSpellSeedRow = 2578;
/// The totem statue rows, one a tribe: the first tribe's number, the others following it downwards
constexpr uint32_t k_FirstTotemStatueRow = 378;
/// The moving object rows: the first's number, the others following it downwards (a creature's poo is row 5, 1965, and a
/// one-off spell's orb row 25, 1945, in the saves read)
constexpr uint32_t k_FirstMobileObjectRow = 1970;
/// The places in a spell's seed's record of its objects (its icon, its creator and its interface's status), its bytes,
/// its words and its last words, as its writer puts them
constexpr size_t k_SeedIcon = 0;
constexpr size_t k_SeedCreator = 1;
constexpr size_t k_SeedStatus = 3;
constexpr size_t k_SeedHasCast = 0;
constexpr size_t k_SeedLearnedFrom = 1;
constexpr size_t k_SeedChantStore = 0;
constexpr size_t k_SeedChantStoreCopy = 1;
constexpr size_t k_SeedStoredChants = 2;
constexpr size_t k_SeedStoredAge = 3;
constexpr size_t k_SeedStoredMaxObjects = 4;
constexpr size_t k_SeedPower = 5;
constexpr size_t k_SeedCastMultiplier = 6;
constexpr size_t k_SeedTurnsInHand = 0;
constexpr size_t k_SeedLastMagic = 1;
/// The spell seed rows the game has
constexpr size_t k_SpellSeedCount = 30;
/// A loaded villager's state counter when its states load nothing: 1 to 20
constexpr uint32_t k_LoadedCounterRange = 20;
/// A storage pit's first food pile and its five other piles
constexpr size_t k_PitWoodPiles = 5;
/// The game object's lists of every forest and of every town, among its global lists
constexpr size_t k_ForestList = 4;
constexpr size_t k_TownList = 31;
constexpr size_t k_FootpathList = 28;
constexpr size_t k_ClimateList = 38;
constexpr auto k_FlockList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Flocks);
/// The game turn, the third word of the game's data
constexpr size_t k_GameTurnWord = 2;
/// The turns of a year, by which an animal's age counts from its birth turn
constexpr int32_t k_TurnsPerYear = 1500;
/// The status bits an animal's brain keeps of its living part's
constexpr uint16_t k_AnimalStatus = 0x00B1;
/// The animal rows from which the animals are a puzzle game's pieces (those before them from the spell dove are a spell's)
constexpr auto k_FirstPuzzleAnimal = openblack::AnimalInfo::PuzzleLion;
constexpr auto k_ReactionList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Reactions);
constexpr auto k_WeatherThingList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::WeatherThings);
constexpr auto k_FireList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::FireEffects);
constexpr auto k_FireBallList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Fireballs);
/// A fireball's ball's row of the game's tables runs down from this one, by its row of the fireball's three
constexpr uint32_t k_FirstFireBallRow = 316;
constexpr uint32_t k_FireBallRows = 3;
constexpr auto k_RewardList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Rewards);
constexpr auto k_BallList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Balls);
/// A reward chest's row of the reward table runs down from this one, the table read right after the fireballs'
constexpr uint32_t k_FirstRewardRow = 313;
constexpr uint32_t k_RewardRows = static_cast<uint32_t>(openblack::RewardObjectInfo::RewardObjectToyDie) + 1;
/// The places in a reward chest's record of its player and its town (the first names the miracle above an opened one,
/// a class openblack does not read)
constexpr size_t k_RewardPlayer = 1;
constexpr size_t k_RewardTown = 2;
/// The places in a reward chest's record of its words: whether it is open, its state, whether it has landed, whether
/// it goes into the map's cells at the next turn, its clock and its dust time
constexpr size_t k_RewardOpened = 1;
constexpr size_t k_RewardState = 3;
constexpr size_t k_RewardLanded = 4;
constexpr size_t k_RewardGoesIntoMap = 5;
constexpr size_t k_RewardClock = 6;
constexpr size_t k_RewardDust = 9;
constexpr auto k_ArenaList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Arenas);
constexpr auto k_TownArtifactList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::TownArtifacts);
constexpr auto k_DanceList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Dances);
constexpr auto k_HighlightList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::ScriptHighlights);
constexpr auto k_FireFlyList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Fireflies);
constexpr auto k_InfluenceRingList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::InfluenceRings);
constexpr auto k_ThingsWithPositionList =
    static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::ThingsWithPosition);
constexpr auto k_WaypointList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::Waypoints);
/// The key frame and the action of a dance's part number: the key frame in the high half
constexpr uint32_t k_KeyFrameShift = 16;
constexpr uint32_t k_FirstClimateRow = 327;
constexpr uint32_t k_ClimateRows = 7;
/// A thing that is out of play (held, or in a script's hands) or flown by the particles
constexpr uint8_t k_ThingUnavailable = 1U << 0U;
constexpr uint8_t k_ThingFlownByParticles = 1U << 4U;
/// A thing whose name is hidden for good (one a script made)
constexpr uint8_t k_ThingNameHidden = 1U << 6U;
/// A thing whose state may not change (held), and one the physics moves
constexpr uint16_t k_PositionUnavailable = 1U << 2U;
constexpr uint16_t k_PositionInPhysics = 1U << 6U;

/// An object the load puts on the map: one that is not held or in flight
template <typename Record>
bool StandsOnTheMap(const Record& object)
{
	return (object.thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
	       (object.base.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) == 0;
}

/// Whether a vortex's object mover carries the animal, as its record's marks say: flown by the particles and in the
/// physics, not held, and named by a mover the load makes again
bool CarriedByAMover(const openblack::ecs::saved_world_load::PlannedAnimal& planned)
{
	return planned.carriedByParticles && (planned.thing.flags & k_ThingFlownByParticles) != 0 &&
	       (planned.thing.flags & k_ThingUnavailable) == 0 && (planned.position.flags & k_PositionUnavailable) == 0 &&
	       (planned.position.flags & k_PositionInPhysics) != 0;
}

/// Whether a living thing the physics moved at the save goes back into the physics with its saved body: one in the
/// physics that is neither held nor flown by the particles, whose record keeps its body
bool LivingReturnsToThePhysics(const openblack::gamesave::ThingPart& thing, const openblack::gamesave::PositionPart& position,
                               const openblack::gamesave::ObjectPart& object)
{
	return (thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
	       (position.flags & k_PositionUnavailable) == 0 && (position.flags & k_PositionInPhysics) != 0 &&
	       object.physics.has_value();
}

/// The row of a table whose first row has that number: the rows are numbered downwards
template <typename Type>
std::optional<Type> RowOf(uint32_t info, uint32_t firstRow)
{
	const auto rows = static_cast<uint32_t>(Type::_COUNT);
	if (info > firstRow || info + rows <= firstRow)
	{
		return std::nullopt;
	}
	return static_cast<Type>(firstRow - info);
}

/// The marks its saved place's flags give an object: held by a script, under a script's control, immovable, not to be
/// picked up and indestructible
void RestorePlaceMarks(entt::entity entity, uint16_t flags)
{
	namespace components = openblack::ecs::components;
	namespace place = openblack::ecs::saved_world_write::position_flags;
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto mark = [&registry, entity, flags]<typename Mark>(uint16_t bit) {
		if ((flags & bit) != 0)
		{
			registry.AssignOrReplace<Mark>(entity);
		}
	};
	mark.template operator()<components::InScript>(place::k_InScript);
	mark.template operator()<components::ScriptControlled>(place::k_ControlledByScript);
	mark.template operator()<components::Immovable>(place::k_Immovable);
	mark.template operator()<components::CannotBePickedUp>(place::k_CannotBePickedUp);
	mark.template operator()<components::Indestructible>(place::k_Indestructible);
}

/// The states that save what the villager works on, the thing it fishes at, fells or looks at
bool TargetsAThing(openblack::gamesave::VillagerStateSave save)
{
	using openblack::gamesave::VillagerStateSave;
	return save == VillagerStateSave::Fishing || save == VillagerStateSave::Foresting ||
	       save == VillagerStateSave::InspectObject || save == VillagerStateSave::Breeder;
}

/// Whether a loaded villager's state values are put back: none to put back, the dance states' two words, which
/// nothing of those states reads (kept with the record, and written back as loaded), a dead villager's cause, or the
/// footpath state's footpath and node, the living part's again (put back with its footpath)
bool StateValuesLoaded(uint8_t state, const std::vector<openblack::gamesave::StateValue>& values, uint8_t discipleType)
{
	using openblack::gamesave::VillagerStateSave;
	const auto save = openblack::gamesave::VillagerStateSaveOf(state);
	return values.empty() || openblack::gamesave::StateValueSizes(save, discipleType).empty() ||
	       save == VillagerStateSave::Dance || save == VillagerStateSave::Worship || save == VillagerStateSave::Dead ||
	       save == VillagerStateSave::FootPath || save == VillagerStateSave::Farming || save == VillagerStateSave::Shepherd ||
	       save == VillagerStateSave::Building || save == VillagerStateSave::InScript || save == VillagerStateSave::ScriptPos ||
	       save == VillagerStateSave::Fire || save == VillagerStateSave::InHand || TargetsAThing(save);
}

/// The made object keeps its saved creation number, its life (a Life only when not full), its place's marks and its
/// record
template <typename Record>
void KeepSaved(entt::entity entity, const Record& record)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto& object = record.base.object;
	RestorePlaceMarks(entity, record.base.position.flags);
	registry.AssignOrReplace<openblack::ecs::components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<openblack::ecs::components::Life>(entity, object.life);
	}
	registry.Assign<openblack::ecs::components::LoadedRecord<Record>>(entity, record);
}

openblack::map_coords::MapCoords CoordsOf(const openblack::gamesave::MapCoords& coords)
{
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

/// An animal's age in years at the turn, by its birth turn
uint32_t AgeAt(uint32_t turn, uint32_t birthTurn)
{
	const int32_t turns = static_cast<int32_t>(turn) - std::bit_cast<int32_t>(birthTurn);
	return turns > 0 ? static_cast<uint32_t>(turns / k_TurnsPerYear) : 0;
}

/// The player the game is played by, whose creature's mind is the profile's
constexpr auto k_LocalPlayer = openblack::PlayerNames::PLAYER_ONE;
/// The game's leash byte: the evil, rope and good leashes the local player's creature knows
constexpr uint8_t k_EvilLeashBit = 1U << 0U;
constexpr uint8_t k_RopeLeashBit = 1U << 1U;
constexpr uint8_t k_GoodLeashBit = 1U << 2U;
/// A creature's index that names no place in the list of creatures
constexpr uint8_t k_NoCreatureIndex = 0xFF;

/// The mind file a loaded creature reads: the profile's for the local player's creature, else the one its place in the
/// list of creatures names in the slot's folder; none when there is no such file to name
std::optional<std::filesystem::path> MindFileOf(const openblack::gamesave::CreatureFields& fields,
                                                const std::filesystem::path& slotFolder)
{
	using namespace openblack;
	const auto profile = fields.owner == static_cast<uint8_t>(k_LocalPlayer)
	                         ? ecs::player_creature::ProfileMindPath(ecs::player_creature::ProfileCreatureFile(),
	                                                                 ecs::player_creature::CurrentMindFolder())
	                         : std::nullopt;
	return ecs::saved_world_load::CreatureMindFileOf(fields, slotFolder, profile);
}

/// A mind file read through the mind cache, none when it does not open or is not a mind file
std::optional<openblack::creaturemind::MindFileData> ReadMindFile(const std::filesystem::path& path)
{
	using namespace openblack;
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& minds = Locator::resources::value().GetCreatureMinds();
	const auto loaded = minds.Load(path.generic_string(), resources::CreatureMindLoader::FromDiskTag {}, path);
	const auto handle = minds.Handle(loaded.first->first);
	if (!handle || !handle->Loaded())
	{
		return std::nullopt;
	}
	return handle->data;
}

/// A town's belief as its saved block holds it, the block's two leading words skipped: each player's belief, the recent
/// belief, the turn belief was last added, the cap, what was added this period, the reduction's accumulator, the
/// pending belief, the boredom of each reaction and each desire's threshold, in that order
void ReadBeliefBlock(const std::array<uint8_t, 464>& block, openblack::ecs::components::TownBelief& belief)
{
	size_t at = 2 * sizeof(uint32_t);
	const auto take = [&block, &at](auto& values) {
		std::memcpy(values.data(), block.data() + at, sizeof(values));
		at += sizeof(values);
	};
	take(belief.belief);
	take(belief.recent);
	take(belief.lastAddedTurn);
	take(belief.cap);
	take(belief.addedThisPeriod);
	take(belief.reduceAccumulator);
	take(belief.pending);
	take(belief.boredom);
	take(belief.desireThreshold);
}

/// Reads values one after another out of a block the original writes whole from its memory
class BlockReader
{
public:
	explicit BlockReader(std::span<const uint8_t> block)
	    : _block(block)
	{
	}

	template <typename T>
	void Take(T& value)
	{
		std::memcpy(&value, _block.data() + _at, sizeof(T));
		_at += sizeof(T);
	}

	template <typename T, size_t N>
	void TakeEach(std::array<T, N>& values)
	{
		for (auto& value : values)
		{
			Take(value);
		}
	}

	/// Passes over bytes openblack does not keep
	void Skip(size_t bytes) { _at += bytes; }

private:
	std::span<const uint8_t> _block;
	size_t _at {0};
};

/// A town's figures as its saved block holds them, in the original's order after the block's two words: its counts,
/// its deaths, the food and wood it used and its injured; the counts openblack does not keep are passed over
void ReadFiguresBlock(const std::array<uint8_t, 280>& block, openblack::ecs::components::Town& town,
                      openblack::ecs::components::TownDeaths& died)
{
	auto& stats = town.stats;
	BlockReader in(block);
	in.Skip(2 * sizeof(uint32_t));
	in.Take(stats.adults);
	in.Take(stats.children);
	in.Take(stats.abodesWithPlaces);
	in.Skip(4); // the planned buildings
	in.Skip(4); // the building sites
	in.Take(stats.civicBuildings);
	in.Skip(4); // the civic building sites
	in.Take(stats.civicPlans);
	in.Skip(4); // the planned wonders
	in.Skip(4); // the wonder sites
	in.Skip(4); // the total space left in the abodes
	in.Take(stats.adultPlaces);
	in.Take(died.total38);
	in.Skip(4); // the children added
	in.Take(stats.childPlaces);
	in.Skip(4); // the abodes added
	in.Skip(4); // the wonders
	in.Take(stats.freeAdultPlaces);
	in.Skip(4); // the child space left in the abodes
	in.Take(stats.males);
	in.Take(stats.females);
	in.TakeEach(died.total5C);
	in.TakeEach(died.byReason);
	in.TakeEach(died.byPlayer);
	in.TakeEach(stats.disciples);
	in.Skip(3); // the padding after the disciples
	in.Take(died.lastDeathTurn);
	in.Take(died.count);
	in.Skip(4); // the turn the last abode was added
	in.Take(stats.foodForDinner);
	in.Take(town.foodUsed);
	in.Take(town.woodUsedForBuilding);
	in.Skip(4); // the average food used
	in.Skip(4); // the average wood used
	in.Take(stats.foodCarried);
	in.Take(stats.woodCarried);
	in.Take(stats.woodAtSites);
	in.Take(town.injured);
	in.TakeEach(stats.abodesByNumber);
}
} // namespace

std::optional<openblack::TreeInfo> saved_world_load::TreeTypeOfInfo(uint32_t info)
{
	return RowOf<TreeInfo>(info, k_FirstTreeRow);
}

std::optional<openblack::BigForestInfo> saved_world_load::BigForestTypeOfInfo(uint32_t info)
{
	return RowOf<BigForestInfo>(info, k_FirstBigForestRow);
}

uint32_t saved_world_load::TreeInfoRowOf(TreeInfo type)
{
	return k_FirstTreeRow - static_cast<uint32_t>(type);
}

uint32_t saved_world_load::BigForestInfoRowOf(BigForestInfo type)
{
	return k_FirstBigForestRow - static_cast<uint32_t>(type);
}

std::optional<openblack::PotInfo> saved_world_load::PotTypeOfInfo(uint32_t info)
{
	return RowOf<PotInfo>(info, k_FirstPotRow);
}

std::optional<openblack::MobileObjectInfo> saved_world_load::MobileObjectTypeOfInfo(uint32_t info)
{
	return RowOf<MobileObjectInfo>(info, k_FirstMobileObjectRow);
}

std::optional<openblack::AbodeInfo> saved_world_load::AbodeTypeOfInfo(uint32_t info)
{
	return RowOf<AbodeInfo>(info, k_FirstAbodeRow);
}

std::optional<openblack::VillagerInfo> saved_world_load::VillagerTypeOfInfo(uint32_t info)
{
	return RowOf<VillagerInfo>(info, k_FirstVillagerRow);
}

std::optional<openblack::AnimalInfo> saved_world_load::AnimalTypeOfInfo(uint32_t info)
{
	return RowOf<AnimalInfo>(info, k_FirstAnimalRow);
}

std::optional<openblack::CreatureType> saved_world_load::CreatureSpeciesOfInfo(uint32_t info)
{
	if (info > k_FirstCreatureRow || info + k_CreatureRows <= k_FirstCreatureRow)
	{
		return std::nullopt;
	}
	// the tables start with the Giant Ape, then follow the species in their order
	const auto row = k_FirstCreatureRow - info;
	return row == 0 ? CreatureType::GiantApe : static_cast<CreatureType>(row);
}

bool saved_world_load::VillagerCounterDrawnOnLoad(uint8_t topState, uint8_t finalState)
{
	using gamesave::VillagerStateSave;
	return gamesave::VillagerStateSaveOf(topState) == VillagerStateSave::None &&
	       gamesave::VillagerStateSaveOf(finalState) == VillagerStateSave::None;
}

std::optional<openblack::MobileStaticInfo> saved_world_load::MobileStaticTypeOfInfo(uint32_t info)
{
	return RowOf<MobileStaticInfo>(info, k_FirstMobileStaticRow);
}

uint32_t saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo type)
{
	return k_FirstMobileStaticRow - static_cast<uint32_t>(type);
}

uint32_t saved_world_load::PotInfoRowOf(PotInfo type)
{
	return k_FirstPotRow - static_cast<uint32_t>(type);
}

uint32_t saved_world_load::AbodeInfoRowOf(AbodeInfo type)
{
	return k_FirstAbodeRow - static_cast<uint32_t>(type);
}

saved_world_load::WorldPlan saved_world_load::PlanOf(const gamesave::WorldRead& world)
{
	WorldPlan plan {.census = world.Census()};
	const auto recordOf = [&world](gamesave::ObjectNumber number) -> const gamesave::SavedRecord* {
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			return nullptr;
		}
		return &*world.objects[number - 1];
	};
	// a rock's (and a bonfire's) player: its player record's number
	const auto rockPlayer = [&recordOf](gamesave::ObjectNumber number) -> std::optional<PlayerNames> {
		const auto* ownerRecord = recordOf(number);
		if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
		    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
		{
			return static_cast<PlayerNames>(owner->playerNumber);
		}
		return std::nullopt;
	};
	// what the players' hands hold, as their interfaces' statuses name it: a thing held by a script has the same marks
	// and no status names it
	for (const auto& object : world.objects)
	{
		const auto* status = object.has_value() ? std::get_if<gamesave::InterfaceStatusRecord>(&*object) : nullptr;
		if (status == nullptr)
		{
			continue;
		}
		for (const auto held : status->hand.held.numbers)
		{
			if (held != 0)
			{
				plan.inHand.insert(held);
			}
		}
	}
	// the player a player's record or an interface's status names, none for any other object
	const auto playerOf = [&recordOf](gamesave::ObjectNumber object) -> std::optional<PlayerNames> {
		const auto* saved = recordOf(object);
		auto number = static_cast<uint8_t>(PlayerNames::_COUNT);
		if (const auto* player = saved != nullptr ? std::get_if<gamesave::PlayerRecord>(saved) : nullptr)
		{
			number = player->playerNumber;
		}
		else if (const auto* status = saved != nullptr ? std::get_if<gamesave::InterfaceStatusRecord>(saved) : nullptr)
		{
			number = status->playerNumber;
		}
		return number < static_cast<uint8_t>(PlayerNames::_COUNT) ? std::optional {static_cast<PlayerNames>(number)}
		                                                          : std::nullopt;
	};
	// an object put on the map, in a hand or in the physics: one the physics moves (not one flown by the particles nor
	// one held) with its body, which goes back into the physics once made
	const auto madeHere = [&plan](gamesave::ObjectNumber number, const auto& object) {
		if (StandsOnTheMap(object) || plan.inHand.contains(number))
		{
			return true;
		}
		const auto& physics = object.base.object.physics;
		if ((object.thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
		    (object.base.position.flags & k_PositionUnavailable) == 0 &&
		    (object.base.position.flags & k_PositionInPhysics) != 0 && physics.has_value())
		{
			plan.inPhysics.insert_or_assign(number, *physics);
			return true;
		}
		return false;
	};

	// The forests: the game's list holds them newest first, and a load keeps its order
	std::vector<gamesave::ObjectNumber> listed;
	std::vector<gamesave::ObjectNumber> listedTowns;
	std::vector<gamesave::ObjectNumber> listedFootpaths;
	if (const auto* first = recordOf(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			listed = game->lists[k_ForestList].numbers;
			listedTowns = game->lists[k_TownList].numbers;
			listedFootpaths = game->lists[k_FootpathList].numbers;
			// the climates as the list holds them, the newest first
			for (const auto number : game->lists[k_ClimateList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* climate = record != nullptr ? std::get_if<gamesave::ClimateRecord>(record) : nullptr;
				    climate != nullptr)
				{
					plan.climates.push_back({.number = number, .record = *climate});
				}
			}
			// the reactions as the list holds them, the newest first, each with its player's number
			for (const auto number : game->lists[k_ReactionList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* reaction = record != nullptr ? std::get_if<gamesave::ReactionRecord>(record) : nullptr;
				if (reaction == nullptr)
				{
					continue;
				}
				PlannedReaction planned {.number = number, .record = *reaction};
				const auto* initiator = recordOf(reaction->object.number);
				if (const auto* status =
				        initiator != nullptr ? std::get_if<gamesave::InterfaceStatusRecord>(initiator) : nullptr;
				    status != nullptr && status->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.statusPlayer = static_cast<PlayerNames>(status->playerNumber);
				}
				const auto* playerRecord = recordOf(reaction->third.number);
				if (const auto* player = playerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(playerRecord) : nullptr;
				    player != nullptr && player->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.player = static_cast<PlayerNames>(player->playerNumber);
				}
				plan.reactions.push_back(planned);
			}
			// the things of the weather as the list holds them, the newest first
			for (const auto number : game->lists[k_WeatherThingList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* thing = record != nullptr ? std::get_if<gamesave::WeatherThingRecord>(record) : nullptr;
				    thing != nullptr)
				{
					plan.weatherThings.push_back({.number = number, .record = *thing});
				}
			}
			// the fires as the list holds them, the newest first, each with its player
			// the dances as the list holds them, the newest first, each with its groups
			for (const auto number : game->lists[k_DanceList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* dance = record != nullptr ? std::get_if<gamesave::DanceRecord>(record) : nullptr;
				if (dance == nullptr)
				{
					continue;
				}
				PlannedDance planned {.number = number, .record = *dance};
				for (const auto group : dance->group.danceGroups.numbers)
				{
					const auto* groupRecord = recordOf(group);
					if (const auto* saved =
					        groupRecord != nullptr ? std::get_if<gamesave::DanceGroupRecord>(groupRecord) : nullptr)
					{
						planned.groups.emplace_back(group, *saved);
					}
				}
				for (const auto frame : dance->group.keyFrames.numbers)
				{
					const auto* frameRecord = recordOf(frame);
					if (const auto* saved =
					        frameRecord != nullptr ? std::get_if<gamesave::DanceKeyFrameRecord>(frameRecord) : nullptr)
					{
						plan.keyFrames.emplace(frame, *saved);
					}
				}
				plan.dances.push_back(std::move(planned));
			}
			for (const auto number : game->lists[k_FireList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* fire = record != nullptr ? std::get_if<gamesave::FireEffectRecord>(record) : nullptr;
				if (fire == nullptr)
				{
					continue;
				}
				PlannedFire planned {.number = number, .record = *fire};
				const auto* ownerRecord = recordOf(fire->objects[2].number);
				if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
				    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.player = static_cast<PlayerNames>(owner->playerNumber);
				}
				plan.fires.push_back(planned);
			}
			// the fireballs' balls as the list holds them, the newest first; one of no fireball's row is not made
			for (const auto number : game->lists[k_FireBallList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* ball = record != nullptr ? std::get_if<gamesave::MagicFireBallRecord>(record) : nullptr;
				const auto row = ball != nullptr ? static_cast<int64_t>(k_FirstFireBallRow) - ball->object.info : -1;
				if (row >= 0 && row < static_cast<int64_t>(k_FireBallRows))
				{
					plan.fireBalls.push_back({.number = number, .infoRow = static_cast<int>(row), .record = *ball});
				}
			}
			// the reward chests as the list holds them, the newest first, made the oldest first; one of no row of the
			// reward table, or one the physics moves or the particles fly, is not made
			const auto& rewardNumbers = game->lists[k_RewardList].numbers;
			for (auto number = rewardNumbers.rbegin(); number != rewardNumbers.rend(); ++number)
			{
				const auto* record = recordOf(*number);
				const auto* chest = record != nullptr ? std::get_if<gamesave::RewardRecord>(record) : nullptr;
				if (chest == nullptr || (chest->thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) != 0 ||
				    (chest->base.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) != 0)
				{
					continue;
				}
				const auto row = static_cast<int64_t>(k_FirstRewardRow) - static_cast<int64_t>(chest->base.object.info);
				if (row >= 0 && row < static_cast<int64_t>(k_RewardRows))
				{
					plan.rewards.push_back({.number = *number,
					                        .type = static_cast<RewardObjectInfo>(row),
					                        .player = rockPlayer(chest->objects[k_RewardPlayer].number),
					                        .record = *chest});
				}
			}
			// the footballs as the list holds them, the newest first, made the oldest first; one the physics moves,
			// held or flown by the particles is not made
			const auto& ballNumbers = game->lists[k_BallList].numbers;
			for (auto number = ballNumbers.rbegin(); number != ballNumbers.rend(); ++number)
			{
				const auto* record = recordOf(*number);
				const auto* ball = record != nullptr ? std::get_if<gamesave::BallRecord>(record) : nullptr;
				if (ball != nullptr && (ball->thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
				    (ball->base.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) == 0)
				{
					plan.balls.push_back({.number = *number, .record = *ball});
				}
			}
			// the fireflies as the list holds them, its head first
			for (const auto number : game->lists[k_FireFlyList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* fly = record != nullptr ? std::get_if<gamesave::FireFlyRecord>(record) : nullptr;
				    fly != nullptr)
				{
					plan.fireFlies.emplace_back(number, *fly);
				}
			}
			// the scripts' highlights as the list holds them, the newest first
			for (const auto number : game->lists[k_HighlightList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* highlight = record != nullptr ? std::get_if<gamesave::ScriptHighlightRecord>(record) : nullptr;
				    highlight != nullptr)
				{
					plan.highlights.push_back({.number = number, .record = *highlight});
				}
			}
			// the influence rings as the list holds them, the newest first
			for (const auto number : game->lists[k_InfluenceRingList].numbers)
			{
				const auto* record = recordOf(number);
				const auto* ring = record != nullptr ? std::get_if<gamesave::InfluenceRingRecord>(record) : nullptr;
				if (ring == nullptr)
				{
					continue;
				}
				PlannedInfluenceRing planned {.number = number, .record = *ring};
				const auto* ownerRecord = recordOf(ring->player.number);
				if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
				    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.player = static_cast<PlayerNames>(owner->playerNumber);
				}
				plan.rings.push_back(planned);
			}
			// the scripts' markers as the list of things with a place holds them, the newest first
			for (const auto number : game->lists[k_ThingsWithPositionList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* marker = record != nullptr ? std::get_if<gamesave::ScriptMarkerRecord>(record) : nullptr;
				    marker != nullptr)
				{
					plan.markers.push_back({.number = number, .record = *marker});
				}
			}
			// the creature's drinking waypoints as the list holds them, the newest first
			for (const auto number : game->lists[k_WaypointList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* waypoint = record != nullptr ? std::get_if<gamesave::WaypointRecord>(record) : nullptr;
				    waypoint != nullptr)
				{
					plan.waypoints.push_back({.number = number, .record = *waypoint});
				}
			}
			// the towns' artifacts as the game's list holds them, the newest first; each in its town's list, the newest
			// first, takes its place there
			std::unordered_map<gamesave::ObjectNumber, uint64_t> joinedOf;
			uint64_t joinedCount = 0;
			for (const auto town : game->lists[k_TownList].numbers)
			{
				const auto* record = recordOf(town);
				const auto* fields = record != nullptr ? std::get_if<gamesave::TownRecord>(record) : nullptr;
				if (fields == nullptr)
				{
					continue;
				}
				for (const auto artifact : fields->artifacts.numbers | std::views::reverse)
				{
					joinedOf.try_emplace(artifact, ++joinedCount);
				}
			}
			const auto& artifacts = game->lists[k_TownArtifactList].numbers;
			for (size_t i = 0; i < artifacts.size(); ++i)
			{
				const auto* record = recordOf(artifacts[i]);
				const auto* artifact = record != nullptr ? std::get_if<gamesave::TownArtifactRecord>(record) : nullptr;
				if (artifact == nullptr)
				{
					continue;
				}
				const auto joined = joinedOf.find(artifacts[i]);
				plan.artifacts.push_back({.number = artifacts[i],
				                          .record = *artifact,
				                          .player = playerOf(artifact->objects[2].number),
				                          .inTown = joined != joinedOf.end(),
				                          .joined = joined != joinedOf.end() ? joined->second : 0,
				                          .made = static_cast<uint64_t>(artifacts.size() - i)});
			}
			// the arenas as the list holds them, the newest first
			for (const auto number : game->lists[k_ArenaList].numbers)
			{
				const auto* record = recordOf(number);
				if (const auto* arena = record != nullptr ? std::get_if<gamesave::ArenaRecord>(record) : nullptr;
				    arena != nullptr)
				{
					plan.arenas.push_back({.number = number, .record = *arena});
				}
			}
			// each player's towns, in the order its list holds them
			for (const auto player : game->players)
			{
				const auto* record = recordOf(player);
				if (const auto* owner = record != nullptr ? std::get_if<gamesave::PlayerRecord>(record) : nullptr;
				    owner != nullptr)
				{
					plan.ownersTowns.insert(plan.ownersTowns.end(), owner->towns.begin(), owner->towns.end());
				}
			}
		}
	}
	// the scripts' timers, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* timer = record != nullptr ? std::get_if<gamesave::ScriptTimerRecord>(record) : nullptr;
		    timer != nullptr)
		{
			plan.timers.push_back({.number = number, .record = *timer});
		}
	}
	const std::unordered_set<gamesave::ObjectNumber> inList(listed.begin(), listed.end());
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* forest = record != nullptr ? std::get_if<gamesave::ForestRecord>(record) : nullptr;
		if (forest == nullptr)
		{
			continue;
		}
		// every forest writes the two values of the whole game, and each one read sets them
		plan.lastTreeCreatedTurn = forest->firstGameWord;
		plan.nextForestId = forest->secondGameWord;
		if (!inList.contains(number))
		{
			plan.forests.push_back({.number = number, .record = *forest});
		}
	}
	for (const auto number : listed | std::views::reverse)
	{
		const auto* record = recordOf(number);
		if (const auto* forest = record != nullptr ? std::get_if<gamesave::ForestRecord>(record) : nullptr; forest != nullptr)
		{
			plan.forests.push_back({.number = number, .record = *forest});
		}
	}

	// The towns, the same way as the forests: the list holds them newest first
	const std::unordered_set<gamesave::ObjectNumber> townsInList(listedTowns.begin(), listedTowns.end());
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* town = record != nullptr ? std::get_if<gamesave::TownRecord>(record) : nullptr;
		if (town != nullptr && !townsInList.contains(number))
		{
			plan.towns.push_back({.number = number, .record = *town});
		}
	}
	for (const auto number : listedTowns | std::views::reverse)
	{
		const auto* record = recordOf(number);
		if (const auto* town = record != nullptr ? std::get_if<gamesave::TownRecord>(record) : nullptr; town != nullptr)
		{
			plan.towns.push_back({.number = number, .record = *town});
		}
	}

	// The footpaths, the same way: the list holds them newest first; each with its nodes in its list's order. Then the
	// footpath links
	const auto footpathOf = [&recordOf](gamesave::ObjectNumber number) -> std::optional<PlannedFootpath> {
		const auto* record = recordOf(number);
		const auto* footpath = record != nullptr ? std::get_if<gamesave::FootpathRecord>(record) : nullptr;
		if (footpath == nullptr)
		{
			return std::nullopt;
		}
		PlannedFootpath planned {.number = number, .record = *footpath};
		for (const auto node : footpath->nodes.numbers)
		{
			const auto* nodeRecord = recordOf(node);
			if (const auto* saved = nodeRecord != nullptr ? std::get_if<gamesave::FootpathNodeRecord>(nodeRecord) : nullptr;
			    saved != nullptr)
			{
				planned.nodes.push_back(*saved);
			}
		}
		return planned;
	};
	const std::unordered_set<gamesave::ObjectNumber> footpathsInList(listedFootpaths.begin(), listedFootpaths.end());
	for (const auto number : world.loadOrder)
	{
		if (footpathsInList.contains(number))
		{
			continue;
		}
		if (auto footpath = footpathOf(number); footpath.has_value())
		{
			plan.footpaths.push_back(std::move(*footpath));
		}
	}
	for (const auto number : listedFootpaths | std::views::reverse)
	{
		if (auto footpath = footpathOf(number); footpath.has_value())
		{
			plan.footpaths.push_back(std::move(*footpath));
		}
	}
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* link = record != nullptr ? std::get_if<gamesave::FootpathLinkRecord>(record) : nullptr; link != nullptr)
		{
			plan.footpathLinks.push_back({.number = number, .record = *link});
		}
	}

	// The rivers and the banks of mist, in the order their links are put back
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* stream = std::get_if<gamesave::StreamRecord>(record); stream != nullptr)
		{
			plan.streams.push_back({.number = number, .record = *stream});
		}
		else if (const auto* farm = std::get_if<gamesave::FishFarmRecord>(record); farm != nullptr && madeHere(number, *farm))
		{
			plan.fishFarms.push_back({.number = number, .record = *farm});
		}
		else if (const auto* planned = std::get_if<gamesave::PlannedAbodeRecord>(record); planned != nullptr)
		{
			plan.townPlans.try_emplace(number, gamesave::PlannedAbodeRecord::k_Type, planned->planned);
		}
		else if (const auto* centre = std::get_if<gamesave::PlannedTownCentreRecord>(record); centre != nullptr)
		{
			plan.townPlans.try_emplace(number, gamesave::PlannedTownCentreRecord::k_Type, centre->planned);
		}
		else if (const auto* heart = std::get_if<gamesave::PlannedCitadelHeartRecord>(record); heart != nullptr)
		{
			plan.townPlans.try_emplace(number, gamesave::PlannedCitadelHeartRecord::k_Type, heart->planned);
		}
		else if (const auto* mist = std::get_if<gamesave::MistRecord>(record); mist != nullptr)
		{
			plan.mists.push_back({.number = number, .record = *mist});
		}
	}

	// The trees and big forests, in the order their links are put back: the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* tree = std::get_if<gamesave::TreeRecord>(record); tree != nullptr && madeHere(number, *tree))
		{
			if (const auto type = TreeTypeOfInfo(tree->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedTree {.number = number, .type = *type, .record = *tree});
			}
		}
		else if (const auto* big = std::get_if<gamesave::BigForestRecord>(record); big != nullptr && madeHere(number, *big))
		{
			if (const auto type = BigForestTypeOfInfo(big->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedBigForest {.number = number, .type = *type, .record = *big});
			}
		}
		else if (const auto* wood = std::get_if<gamesave::WoodPileRecord>(record); wood != nullptr && madeHere(number, *wood))
		{
			if (const auto type = PotTypeOfInfo(wood->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedPile {.number = number,
				                                       .type = *type,
				                                       .base = wood->base,
				                                       .pile = wood->pile,
				                                       .record = *wood,
				                                       .pilePlayer = rockPlayer(wood->pile.player.number)});
			}
		}
		else if (const auto* magic = std::get_if<gamesave::MagicWoodRecord>(record);
		         magic != nullptr && madeHere(number, *magic))
		{
			if (const auto type = PotTypeOfInfo(magic->base.object.info); type.has_value())
			{
				PlannedPile planned {
				    .number = number, .type = *type, .base = magic->base, .pile = magic->pile, .record = *magic};
				const auto* ownerRecord = recordOf(magic->player.number);
				if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
				    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
				{
					planned.player = static_cast<PlayerNames>(owner->playerNumber);
				}
				plan.objects.emplace_back(std::move(planned));
			}
		}
		else if (const auto* food = std::get_if<gamesave::FoodPileRecord>(record); food != nullptr && madeHere(number, *food))
		{
			if (const auto type = PotTypeOfInfo(food->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedPile {.number = number,
				                                       .type = *type,
				                                       .base = food->base,
				                                       .pile = food->pile,
				                                       .record = *food,
				                                       .pilePlayer = rockPlayer(food->pile.player.number)});
			}
		}
		else if (const auto* pot = std::get_if<gamesave::PotRecord>(record); pot != nullptr && madeHere(number, *pot))
		{
			if (const auto type = PotTypeOfInfo(pot->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedPile {.number = number,
				                                       .type = *type,
				                                       .base = pot->base,
				                                       .pile = {.resourceType = pot->resourceType,
				                                                .parentTown = pot->parentTown,
				                                                .resourceAmount = pot->resourceAmount,
				                                                .flags = pot->flags},
				                                       .record = *pot});
			}
		}
		else if (const auto* dead = std::get_if<gamesave::DeadTreeRecord>(record); dead != nullptr && madeHere(number, *dead))
		{
			if (const auto type = TreeTypeOfInfo(dead->info); type.has_value())
			{
				plan.deadTrees.push_back({.number = number,
				                          .type = *type,
				                          .player = playerOf(dead->object.number),
				                          .record = *dead,
				                          .inHand = plan.inHand.contains(number)});
			}
		}
		else if (const auto* felled = std::get_if<gamesave::FelledTreeRecord>(record);
		         felled != nullptr && madeHere(number, *felled))
		{
			if (const auto type = TreeTypeOfInfo(felled->info); type.has_value())
			{
				plan.deadTrees.push_back({.number = number,
				                          .type = *type,
				                          .player = playerOf(felled->object.number),
				                          .record = *felled,
				                          .inHand = plan.inHand.contains(number)});
			}
		}
		else if (const auto* feature = std::get_if<gamesave::FeatureRecord>(record);
		         feature != nullptr && madeHere(number, *feature))
		{
			if (const auto type = FeatureTypeOfInfo(feature->base.object.info); type.has_value())
			{
				plan.features.push_back({.number = number, .type = *type, .record = *feature});
			}
		}
		else if (const auto* site = std::get_if<gamesave::StandardBuildingSiteRecord>(record); site != nullptr)
		{
			plan.buildingSites.push_back({.number = number, .record = *site});
		}
		else if (const auto* workshopSite = std::get_if<gamesave::WorkshopBuildingSiteRecord>(record); workshopSite != nullptr)
		{
			plan.buildingSites.push_back({.number = number, .record = *workshopSite});
		}
		else if (const auto* citadelSite = std::get_if<gamesave::CitadelBuildingSiteRecord>(record); citadelSite != nullptr)
		{
			plan.buildingSites.push_back({.number = number, .record = *citadelSite});
		}
		else if (const auto* icon = std::get_if<gamesave::TownCentreSpellIconRecord>(record);
		         icon != nullptr && (icon->thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
		         (icon->icon.base.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) == 0)
		{
			const auto seed = static_cast<int64_t>(k_FirstSpellSeedRow) - static_cast<int64_t>(icon->icon.info);
			if (seed >= 0 && seed < static_cast<int64_t>(magic::k_SpellSeedCount))
			{
				plan.townCentreIcons.push_back({.number = number, .seed = static_cast<SpellSeedType>(seed), .record = *icon});
			}
		}
		else if (const auto* totem = std::get_if<gamesave::TotemStatueRecord>(record);
		         totem != nullptr && madeHere(number, *totem))
		{
			const auto tribe = static_cast<int64_t>(k_FirstTotemStatueRow) - static_cast<int64_t>(totem->base.object.info);
			if (tribe >= 0 && tribe < static_cast<int64_t>(Tribe::_COUNT))
			{
				plan.totemStatues.push_back({.number = number, .tribe = static_cast<Tribe>(tribe), .record = *totem});
			}
		}
		else if (const auto* lantern = std::get_if<gamesave::StreetLanternRecord>(record);
		         lantern != nullptr && (lantern->thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) == 0 &&
		         (lantern->position.flags & (k_PositionUnavailable | k_PositionInPhysics)) == 0)
		{
			if (const auto type = MobileStaticTypeOfInfo(lantern->object.info); type.has_value())
			{
				plan.streetLanterns.push_back({.number = number, .type = *type, .record = *lantern});
			}
		}
		else if (const auto* base = std::get_if<gamesave::BaseOnlyRecord>(record);
		         base != nullptr && MobileStaticTypeOfInfo(base->object.info) == MobileStaticInfo::SingingStoneBase)
		{
			plan.stoneBases.push_back({.number = number, .record = *base});
		}
		else if (const auto* mobile = std::get_if<gamesave::MobileObjectRecord>(record);
		         mobile != nullptr && madeHere(number, *mobile))
		{
			if (const auto type = MobileObjectTypeOfInfo(mobile->base.object.info); type.has_value())
			{
				plan.mobileObjects.push_back(
				    {.number = number, .type = *type, .record = *mobile, .inHand = plan.inHand.contains(number)});
			}
		}
		else if (const auto* poo = std::get_if<gamesave::PooRecord>(record); poo != nullptr && madeHere(number, *poo))
		{
			// a poo: a moving object of the poo's row, its class's own only by its record
			if (const auto type = MobileObjectTypeOfInfo(poo->base.object.info); type.has_value())
			{
				plan.mobileObjects.push_back(
				    {.number = number, .type = *type, .record = *poo, .inHand = plan.inHand.contains(number)});
			}
		}
		else if (const auto* in = std::get_if<gamesave::LandscapeVortexInRecord>(record); in != nullptr)
		{
			PlannedVortex planned {.number = number, .record = *in};
			// its object mover, as the save kept it
			if (in->objects[1].number != 0)
			{
				planned.mover = saved_particles::PlanEffect(world, in->objects[1].number);
			}
			plan.vortices.push_back(std::move(planned));
		}
		else if (const auto* out = std::get_if<gamesave::LandscapeVortexOutRecord>(record); out != nullptr)
		{
			PlannedVortex planned {.number = number, .record = *out};
			const auto* saver = recordOf(out->saver.number);
			if (const auto* save = saver != nullptr ? std::get_if<gamesave::VortexSaveRecord>(saver) : nullptr)
			{
				planned.linesRead = save->word;
			}
			plan.vortices.push_back(std::move(planned));
		}
		else if (const auto* volcano = std::get_if<gamesave::LandscapeVortexVolcanoRecord>(record); volcano != nullptr)
		{
			plan.vortices.push_back({.number = number, .record = *volcano});
		}
		else if (const auto* seed = std::get_if<gamesave::SpellSeedRecord>(record); seed != nullptr)
		{
			const auto row = static_cast<int64_t>(k_FirstSpellSeedRow) - static_cast<int64_t>(seed->info);
			if (row >= 0 && row < static_cast<int64_t>(k_SpellSeedCount))
			{
				plan.spellSeeds.push_back({.number = number,
				                           .type = static_cast<SpellSeedType>(row),
				                           .record = *seed,
				                           .creatorPlayer = playerOf(seed->objects[k_SeedCreator].number),
				                           .statusPlayer = playerOf(seed->objects[k_SeedStatus].number)});
			}
		}
		else if (const auto* orb = std::get_if<gamesave::OneOffSpellSeedRecord>(record);
		         orb != nullptr && madeHere(number, *orb))
		{
			if (const auto type = MobileObjectTypeOfInfo(orb->base.object.info); type.has_value())
			{
				plan.mobileObjects.push_back(
				    {.number = number, .type = *type, .record = *orb, .inHand = plan.inHand.contains(number)});
			}
		}
		else if (const auto* scaffold = std::get_if<gamesave::ScaffoldRecord>(record);
		         scaffold != nullptr && madeHere(number, *scaffold))
		{
			// its plan and candidate, its own planned abodes, read inside it; and its holder
			PlannedScaffold made {.holder = playerOf(scaffold->other.number)};
			for (size_t i = 0; i < made.plans.size(); ++i)
			{
				const auto* saved = recordOf(scaffold->objects.at(i + 1).number);
				if (const auto* planned = saved != nullptr ? std::get_if<gamesave::PlannedAbodeRecord>(saved) : nullptr)
				{
					made.plans.at(i) = *planned;
				}
			}
			plan.mobileObjects.push_back({.number = number,
			                              .type = MobileObjectInfo::OldScaffold,
			                              .record = *scaffold,
			                              .inHand = plan.inHand.contains(number),
			                              .scaffold = made});
		}
		else if (const auto* heart = std::get_if<gamesave::CitadelHeartRecord>(record); heart != nullptr)
		{
			// its citadel's player: the citadel's owner's number
			PlannedCitadelHeart planned {.number = number, .record = *heart};
			if (const auto* citadelRecord = recordOf(heart->citadel.number); citadelRecord != nullptr)
			{
				if (const auto* citadel = std::get_if<gamesave::CitadelRecord>(citadelRecord); citadel != nullptr)
				{
					planned.citadel = *citadel;
					const auto* ownerRecord = recordOf(citadel->container.owner.number);
					if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
					    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
					{
						planned.owner = static_cast<PlayerNames>(owner->playerNumber);
					}
				}
			}
			plan.objects.emplace_back(std::move(planned));
		}
		else if (const auto* reminder = std::get_if<gamesave::ScriptReminderRecord>(record); reminder != nullptr)
		{
			plan.scriptReminders.emplace(number, *reminder);
		}
		else if (const auto* flag = std::get_if<gamesave::TownDesireFlagsRecord>(record); flag != nullptr)
		{
			plan.objects.emplace_back(PlannedDesireFlag {.number = number, .record = *flag});
		}
		else if (const auto* abode = std::get_if<gamesave::AbodeRecord>(record); abode != nullptr && madeHere(number, *abode))
		{
			if (const auto type = AbodeTypeOfInfo(abode->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = abode->base, .abode = abode->abode, .record = *abode});
			}
		}
		else if (const auto* creche = std::get_if<gamesave::CrecheRecord>(record);
		         creche != nullptr && madeHere(number, *creche))
		{
			if (const auto type = AbodeTypeOfInfo(creche->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = creche->base, .abode = creche->abode, .record = *creche});
			}
		}
		else if (const auto* graveyard = std::get_if<gamesave::GraveyardRecord>(record);
		         graveyard != nullptr && madeHere(number, *graveyard))
		{
			if (const auto type = AbodeTypeOfInfo(graveyard->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = graveyard->base, .abode = graveyard->abode, .record = *graveyard});
			}
		}
		else if (const auto* field = std::get_if<gamesave::FieldRecord>(record); field != nullptr && madeHere(number, *field))
		{
			if (const auto type = AbodeTypeOfInfo(field->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = field->base, .abode = field->abode, .record = *field});
			}
		}
		else if (const auto* centre = std::get_if<gamesave::TownCentreRecord>(record);
		         centre != nullptr && madeHere(number, *centre))
		{
			if (const auto type = AbodeTypeOfInfo(centre->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = centre->base, .abode = centre->abode, .record = *centre});
			}
		}
		else if (const auto* dispenser = std::get_if<gamesave::SpellDispenserRecord>(record);
		         dispenser != nullptr && madeHere(number, *dispenser))
		{
			if (const auto type = AbodeTypeOfInfo(dispenser->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedAbode {
				    .number = number, .type = *type, .base = dispenser->base, .abode = dispenser->abode, .record = *dispenser});
			}
		}
		else if (const auto* workshop = std::get_if<gamesave::WorkshopRecord>(record);
		         workshop != nullptr && madeHere(number, *workshop))
		{
			if (const auto type = AbodeTypeOfInfo(workshop->base.object.info); type.has_value())
			{
				// its sign of the wood it wants, an object of its own the workshop's record names
				PlannedAbode planned {
				    .number = number, .type = *type, .base = workshop->base, .abode = workshop->abode, .record = *workshop};
				const auto* sign = recordOf(workshop->second.number);
				if (const auto* saved = sign != nullptr ? std::get_if<gamesave::ShowNeedsVisualsRecord>(sign) : nullptr)
				{
					planned.sign = std::pair(workshop->second.number, *saved);
				}
				plan.objects.emplace_back(std::move(planned));
			}
		}
		else if (const auto* pit = std::get_if<gamesave::StoragePitRecord>(record); pit != nullptr && madeHere(number, *pit))
		{
			if (const auto type = AbodeTypeOfInfo(pit->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedStoragePit {.number = number, .type = *type, .record = *pit});
			}
		}
		else if (const auto* rock = std::get_if<gamesave::RockRecord>(record); rock != nullptr && madeHere(number, *rock))
		{
			if (const auto type = MobileStaticTypeOfInfo(rock->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedMobileStatic {.number = number,
				                                               .type = *type,
				                                               .base = rock->base,
				                                               .mobileStatic = rock->mobileStatic,
				                                               .player = rockPlayer(rock->object.number),
				                                               .record = *rock});
			}
		}
		else if (const auto* bonfire = std::get_if<gamesave::BonfireRecord>(record);
		         bonfire != nullptr && madeHere(number, *bonfire))
		{
			// a rock's parts, then a word of its own, as the bonfire's load reads them; its fire is a container of the
			// game's list, which hangs on it and is loaded with the containers: its load starts no spot visual
			if (const auto type = MobileStaticTypeOfInfo(bonfire->base.object.info); type == MobileStaticInfo::Bonfire)
			{
				plan.objects.emplace_back(PlannedMobileStatic {.number = number,
				                                               .type = *type,
				                                               .base = bonfire->base,
				                                               .mobileStatic = bonfire->mobileStatic,
				                                               .player = rockPlayer(bonfire->object.number),
				                                               .record = *bonfire});
			}
		}
		else if (const auto* animated = std::get_if<gamesave::AnimatedStaticRecord>(record);
		         animated != nullptr && madeHere(number, *animated))
		{
			constexpr uint32_t k_FirstAnimatedStaticRow = 1986;
			if (const auto type = RowOf<AnimatedStaticInfo>(animated->base.object.info, k_FirstAnimatedStaticRow);
			    type.has_value())
			{
				plan.objects.emplace_back(PlannedAnimatedStatic {.number = number, .type = *type, .record = *animated});
			}
		}
		else if (const auto* statics = std::get_if<gamesave::MobileStaticRecord>(record);
		         statics != nullptr && madeHere(number, *statics))
		{
			if (const auto type = MobileStaticTypeOfInfo(statics->base.object.info); type.has_value())
			{
				plan.objects.emplace_back(PlannedMobileStatic {.number = number,
				                                               .type = *type,
				                                               .base = statics->base,
				                                               .mobileStatic = statics->mobileStatic,
				                                               .record = *statics});
			}
		}
	}
	// the worship sites, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* site = record != nullptr ? std::get_if<gamesave::WorshipSiteRecord>(record) : nullptr)
		{
			PlannedWorshipSite planned {.number = number, .record = *site, .totemNumber = site->totem.number};
			const auto* totem = recordOf(site->totem.number);
			if (const auto* saved = totem != nullptr ? std::get_if<gamesave::WorshipTotemRecord>(totem) : nullptr)
			{
				planned.totem = *saved;
			}
			// its need signs, the visuals its needs' record lists
			const auto* needs = recordOf(site->showNeeds.number);
			if (const auto* shown = needs != nullptr ? std::get_if<gamesave::ShowNeedsRecord>(needs) : nullptr)
			{
				planned.needsNumber = site->showNeeds.number;
				for (size_t i = 0; i < planned.signs.size() && i < shown->visuals.numbers.size(); ++i)
				{
					const auto visual = shown->visuals.numbers[i];
					planned.signs[i].first = visual;
					const auto* sign = recordOf(visual);
					if (const auto* saved = sign != nullptr ? std::get_if<gamesave::ShowNeedsVisualsRecord>(sign) : nullptr)
					{
						planned.signs[i].second = *saved;
					}
				}
			}
			// its spell icons, in its list's order
			for (const auto icon : site->icons.numbers)
			{
				const auto* iconRecord = recordOf(icon);
				if (const auto* saved =
				        iconRecord != nullptr ? std::get_if<gamesave::WorshipSpellIconRecord>(iconRecord) : nullptr)
				{
					planned.icons.emplace_back(icon, *saved);
				}
			}
			// its mana path's effect, as the save kept its particles
			if (site->manaPath.number != 0)
			{
				planned.manaPath = saved_particles::PlanEffect(world, site->manaPath.number);
			}
			plan.worshipSites.push_back(std::move(planned));
		}
	}
	// the villagers, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* villager = std::get_if<gamesave::VillagerRecord>(record); villager != nullptr)
		{
			plan.villagers.push_back({.number = number,
			                          .type = VillagerTypeOfInfo(villager->object.info).value_or(VillagerInfo::None),
			                          .record = *villager,
			                          .inHand = plan.inHand.contains(number)});
		}
		else if (const auto* special = std::get_if<gamesave::SpecialVillagerRecord>(record); special != nullptr)
		{
			plan.villagers.push_back({.number = number,
			                          .type = VillagerTypeOfInfo(special->object.info).value_or(VillagerInfo::None),
			                          .record = *special,
			                          .inHand = plan.inHand.contains(number)});
		}
		else
		{
			continue;
		}
		// the walk along a track it names
		const auto path =
		    std::visit([](const auto& made) { return made.values.living.dataPath.number; }, plan.villagers.back().record);
		if (const auto* walk = path != 0 ? recordOf(path) : nullptr; walk != nullptr)
		{
			if (const auto* data = std::get_if<gamesave::DataPathRecord>(walk); data != nullptr)
			{
				plan.villagers.back().dataPath = *data;
			}
		}
	}
	// the flocks, the animals and birds, and the creatures, in the order their reads ended
	std::unordered_map<gamesave::ObjectNumber, size_t> readEnd;
	for (size_t i = 0; i < world.loadOrder.size(); ++i)
	{
		readEnd.emplace(world.loadOrder[i], i);
	}
	plan.readEnds = readEnd;
	std::vector<gamesave::ObjectNumber> listedFlocks;
	uint32_t gameTurn = 0;
	if (const auto* first = recordOf(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			listedFlocks = game->lists[k_FlockList].numbers;
			gameTurn = game->data[k_GameTurnWord];
			// each villager's age at the loaded turn, for its 3D object made again
			for (auto& villager : plan.villagers)
			{
				villager.ageAtLoad = std::visit(
				    [gameTurn](const auto& record) { return villager_age::AgeOf(gameTurn, record.values.living.birthTurn); },
				    villager.record);
			}
			// the music attached to things, as the audio's list holds it, head first
			for (const auto number : game->tail.sounds.numbers)
			{
				const auto* music = recordOf(number);
				if (const auto* saved = music != nullptr ? std::get_if<gamesave::ThingMusicInfoRecord>(music) : nullptr)
				{
					plan.thingMusic.emplace_back(number, *saved);
				}
			}
			// the script music and the town the music last played for, as the audio's part of the tail keeps them
			plan.scriptMusic = PlannedScriptMusic {
			    .type = game->tail.audioWords[0], .playing = game->tail.audioWords[1], .town = game->tail.audioObject.number};
		}
	}
	// a member's place in the line, read as its living part ends: its record's, but 0 for a member whose own read was
	// still going on while its flock was read (it began before the flock's and ends after it)
	const auto lineOrderOf = [&recordOf, &readEnd](gamesave::ObjectNumber member, gamesave::ObjectNumber flock) -> uint8_t {
		const auto memberEnd = readEnd.find(member);
		const auto flockEnd = readEnd.find(flock);
		if (member < flock && memberEnd != readEnd.end() && flockEnd != readEnd.end() && memberEnd->second > flockEnd->second)
		{
			return 0;
		}
		const auto* record = recordOf(member);
		if (record == nullptr)
		{
			return 0;
		}
		return std::visit(
		    [](const auto& saved) -> uint8_t {
			    if constexpr (requires { saved.living.secondByte; })
			    {
				    return saved.living.secondByte;
			    }
			    else if constexpr (requires { saved.values.living.secondByte; })
			    {
				    return saved.values.living.secondByte;
			    }
			    else
			    {
				    return 0;
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(*record));
	};
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		if (const auto* flock = std::get_if<gamesave::FlockRecord>(record); flock != nullptr)
		{
			PlannedFlock planned {.number = number, .record = *flock};
			const auto* ownerRecord = recordOf(flock->container.owner.number);
			if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
			    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
			{
				planned.player = static_cast<PlayerNames>(owner->playerNumber);
			}
			if (const auto inList = std::ranges::find(listedFlocks, number); inList != listedFlocks.end())
			{
				planned.stamp = static_cast<uint32_t>(listedFlocks.end() - inList);
			}
			for (const auto member : flock->members.numbers)
			{
				planned.lineOrders.push_back(lineOrderOf(member, number));
			}
			plan.flocks.push_back(std::move(planned));
		}
		else if (const auto* creature = std::get_if<gamesave::CreatureRecord>(record); creature != nullptr)
		{
			plan.creatures.push_back({.number = number,
			                          .species = CreatureSpeciesOfInfo(creature->object.info),
			                          .readEnd = static_cast<uint32_t>(readEnd.at(number)),
			                          .record = *creature});
		}
		else
		{
			std::visit(
			    [&plan, number, record, gameTurn, &readEnd](const auto& saved) {
				    if constexpr (requires { saved.animal; })
				    {
					    if constexpr (std::is_same_v<std::decay_t<decltype(saved.animal)>, gamesave::AnimalFields>)
					    {
						    plan.animals.push_back({.number = number,
						                            .recordType = std::decay_t<decltype(saved)>::k_Type,
						                            .type = AnimalTypeOfInfo(saved.object.info).value_or(AnimalInfo::None),
						                            .age = AgeAt(gameTurn, saved.living.birthTurn),
						                            .readEnd = static_cast<uint32_t>(readEnd.at(number)),
						                            .thing = saved.thing,
						                            .position = saved.position,
						                            .object = saved.object,
						                            .mobile = saved.mobile,
						                            .move = saved.move,
						                            .living = saved.living,
						                            .animal = saved.animal,
						                            .record = *record,
						                            .inHand = plan.inHand.contains(number)});
						    // one the physics moved goes back into it with its body once made, as the game's load does
						    if (!plan.inHand.contains(number) &&
						        LivingReturnsToThePhysics(saved.thing, saved.position, saved.object))
						    {
							    plan.inPhysics.insert_or_assign(number, *saved.object.physics);
						    }
					    }
				    }
			    },
			    static_cast<const gamesave::SavedRecordVariant&>(*record));
		}
	}
	// the fires' two tags, as the last fire read leaves them (each fire's read ends by reading them); and the fireflies'
	// reward table and top-up flag, as the last firefly read leaves them
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (const auto* fire = record != nullptr ? std::get_if<gamesave::FireEffectRecord>(record) : nullptr; fire != nullptr)
		{
			plan.fireTags = {fire->lastFirstMark, fire->lastSecondMark};
		}
		if (const auto* fly = record != nullptr ? std::get_if<gamesave::FireFlyRecord>(record) : nullptr; fly != nullptr)
		{
			plan.lastFireFly = *fly;
		}
	}
	// the Hanoi puzzle's blocks that stand on the map or are in a hand, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* block = record != nullptr ? std::get_if<gamesave::HanoiBlockRecord>(record) : nullptr;
		if (block == nullptr || !madeHere(number, *block))
		{
			continue;
		}
		if (const auto type = MobileObjectTypeOfInfo(block->base.object.info); type.has_value())
		{
			plan.hanoiBlocks.push_back(
			    {.number = number, .type = *type, .record = *block, .inHand = plan.inHand.contains(number)});
		}
	}
	// the puzzle games, as the game's list holds them (newest first), from its last to its first
	if (const auto* first = recordOf(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			constexpr auto k_PuzzleGameList = static_cast<size_t>(openblack::ecs::saved_world_write::GlobalList::PuzzleGame);
			for (const auto number : game->lists[k_PuzzleGameList].numbers | std::views::reverse)
			{
				const auto* record = recordOf(number);
				if (const auto* puzzle = record != nullptr ? std::get_if<gamesave::PuzzleGameRecord>(record) : nullptr;
				    puzzle != nullptr)
				{
					plan.puzzleGames.push_back({.number = number, .record = *puzzle});
				}
			}
		}
	}
	// the wonders that stand on the map, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* wonder = record != nullptr ? std::get_if<gamesave::WonderRecord>(record) : nullptr;
		if (wonder == nullptr || !madeHere(number, *wonder))
		{
			continue;
		}
		if (const auto type = AbodeTypeOfInfo(wonder->base.object.info); type.has_value())
		{
			plan.wonders.push_back({.number = number, .type = *type, .record = *wonder});
		}
	}
	// the piles of magic food that stand on the map, are in a hand or in the physics, in the order their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		const auto* food = record != nullptr ? std::get_if<gamesave::MagicFoodRecord>(record) : nullptr;
		if (food == nullptr || !madeHere(number, *food))
		{
			continue;
		}
		if (const auto type = PotTypeOfInfo(food->base.object.info); type.has_value())
		{
			PlannedMagicFood planned {.number = number, .type = *type, .record = *food};
			const auto* ownerRecord = recordOf(food->player.number);
			if (const auto* owner = ownerRecord != nullptr ? std::get_if<gamesave::PlayerRecord>(ownerRecord) : nullptr;
			    owner != nullptr && owner->playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT))
			{
				planned.player = static_cast<PlayerNames>(owner->playerNumber);
			}
			plan.magicFoods.push_back(std::move(planned));
		}
	}
	// the spot visuals' containers, as the game's list holds them, the newest first, with their effects
	if (const auto* game = world.objects.empty() || !world.objects.front().has_value()
	                           ? nullptr
	                           : std::get_if<gamesave::GameHead>(&*world.objects.front());
	    game != nullptr)
	{
		plan.containers = saved_particles::PlanContainers(
		    world, game->lists.at(static_cast<size_t>(saved_world_write::GlobalList::ParticleContainers)).numbers,
		    [&playerOf](gamesave::ObjectNumber object) {
			    const auto player = playerOf(object);
			    return player.has_value() ? static_cast<int>(*player) : -1;
		    });
		// the particles of the spell in the hand, which the hand's load reads (ReadSafe), with the seed whose effect
		// they are and the flag of their draw at the hand's bone
		if (const auto number = game->tail.handObjects.at(0).number; number != 0)
		{
			if (auto effect = saved_particles::PlanEffect(world, number); effect.has_value())
			{
				const auto player = playerOf(effect->manager.object.number);
				effect->player = player.has_value() ? static_cast<int>(*player) : -1;
				plan.handEffect = WorldPlan::PlannedHandEffect {.effect = std::move(*effect),
				                                                .seed = game->tail.handObjects.at(1).number,
				                                                .atBone = game->tail.handByte != 0};
			}
		}
		// the physical shields, as the game's list of map shields holds them, the newest first, with their effects
		plan.physicalShields = saved_spells::PlanPhysicalShields(
		    world, game->lists.at(static_cast<size_t>(saved_world_write::GlobalList::MapShields)).numbers);
		plan.spiritualShields = saved_spells::PlanSpiritualShields(
		    world, game->lists.at(static_cast<size_t>(saved_world_write::GlobalList::MapShields)).numbers);
		// the teleports' stones, as the game's list of objects of several cells holds them, each with its player
		plan.teleportStones = saved_spells::PlanTeleportStones(
		    world, game->lists.at(static_cast<size_t>(saved_world_write::GlobalList::MultiMapFixed)).numbers, playerOf);
		// the water and shield spells, as the game's list holds them, the newest first, with their effects
		plan.spells = saved_spells::PlanSpells(
		    world, game->lists.at(static_cast<size_t>(saved_world_write::GlobalList::Spells)).numbers, playerOf);
	}
	// the ways in's object movers: their player, and the animals they carry, which are made despite their carried marks
	std::unordered_set<gamesave::ObjectNumber> carried;
	for (auto& vortex : plan.vortices)
	{
		if (!vortex.mover.has_value())
		{
			continue;
		}
		const auto player = playerOf(vortex.mover->manager.object.number);
		vortex.mover->player = player.has_value() ? static_cast<int>(*player) : -1;
		vortex.mover->players.insert_or_assign(vortex.mover->manager.object.number, vortex.mover->player);
		for (const auto& [part, record] : vortex.mover->parts)
		{
			const auto* object = std::get_if<gamesave::ParticleGameObjectRecord>(&record);
			if (object == nullptr)
			{
				continue;
			}
			if (object->objects[0].number != 0)
			{
				carried.insert(object->objects[0].number);
			}
			// the player the thing was taken for, as its particle names it
			const auto taker = playerOf(object->objects[1].number);
			vortex.mover->players.insert_or_assign(object->objects[1].number,
			                                       taker.has_value() ? static_cast<int>(*taker) : -1);
		}
	}
	for (auto& animal : plan.animals)
	{
		animal.carriedByParticles = carried.contains(animal.number);
	}
	// the piles in play made to start their reaction whose reaction was not started when saved, as their reads ended
	for (const auto number : world.loadOrder)
	{
		const auto* record = recordOf(number);
		if (record == nullptr)
		{
			continue;
		}
		std::visit(
		    [&plan, number](const auto& saved) {
			    using Record = std::decay_t<decltype(saved)>;
			    if constexpr (std::is_same_v<Record, gamesave::FoodPileRecord> ||
			                  std::is_same_v<Record, gamesave::WoodPileRecord> ||
			                  std::is_same_v<Record, gamesave::MagicFoodRecord> ||
			                  std::is_same_v<Record, gamesave::MagicWoodRecord>)
			    {
				    if (saved.pile.buildingByte != 0 && (saved.pile.flags & gamesave::pot_flags::k_ReactionStarted) == 0 &&
				        (saved.thing.flags & k_ThingUnavailable) == 0)
				    {
					    plan.pileReactions.push_back(number);
				    }
			    }
		    },
		    *record);
	}
	return plan;
}

std::optional<std::filesystem::path> saved_world_load::CreatureMindFileOf(const gamesave::CreatureFields& fields,
                                                                          const std::filesystem::path& slotFolder,
                                                                          const std::optional<std::filesystem::path>& profile)
{
	if (fields.owner == static_cast<uint8_t>(k_LocalPlayer))
	{
		return profile;
	}
	if (fields.index == k_NoCreatureIndex)
	{
		return std::nullopt;
	}
	return slotFolder / gamesave::SavedCreatureFileName(fields.index);
}

void saved_world_load::Resolve(std::span<const entt::entity> resolves, const ResolveContext& context)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : resolves)
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		if (registry.AllOf<components::Animal>(entity))
		{
			SetAnimalStateAnim(entity);
			continue;
		}
		const auto* kept = registry.TryGet<const components::LoadedRecord<gamesave::CreatureRecord>>(entity);
		if (kept == nullptr || !registry.AllOf<components::Creature>(entity))
		{
			continue;
		}
		// its mind: the profile's file for the local player's creature, its slot's own file for any other; only a file
		// that opens and is not empty is read, and only then is it put in the map
		const auto& fields = kept->record.creature;
		const auto path = MindFileOf(fields, context.slotFolder);
		const auto mind = path.has_value() ? ReadMindFile(*path) : std::nullopt;
		if (!mind.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: creature {}'s mind file is not there: it is left out of the map",
			                   entt::to_integral(entity));
			map_cells::RemoveMapObject(entity);
			continue;
		}
		player_creature::DrawSavedSourcesOf(*mind);
		if (const auto body = creature_mind_body::FromMindFile(*mind); body.has_value())
		{
			const auto& creature = registry.Get<const components::Creature>(entity);
			const player_creature::LoadPlan plan {.species = creature.species,
			                                      .size = body->size,
			                                      .alignment = body->alignment,
			                                      .strength = body->strength,
			                                      .position = glm::vec3(0.0f),
			                                      .fatness = body->fatness,
			                                      .previousFatness = body->previousFatness,
			                                      .needs = body->needs,
			                                      .leashes = body->leashes,
			                                      .tattoos = body->tattoos};
			auto* physiology =
			    Locator::creaturePhysiologySystem::has_value() ? &Locator::creaturePhysiologySystem::value() : nullptr;
			player_creature::RestoreBody(registry, physiology, entity, plan);
			if (Locator::leashSystem::has_value())
			{
				player_creature::RestoreLeashes(Locator::leashSystem::value(), entity, plan);
			}
		}
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().LoadMind(entity, std::make_shared<const creaturemind::MindFileData>(*mind));
		}
	}
	// the local player's creature's three leashes, from the game's leash byte, once every object is resolved
	if (Locator::leashSystem::has_value() && context.leashes.has_value())
	{
		auto& leash = Locator::leashSystem::value();
		if (const auto creature = leash.PlayersCreature(k_LocalPlayer); creature.has_value())
		{
			leash.SetKnown(*creature, LeashType::Evil, (*context.leashes & k_EvilLeashBit) != 0);
			leash.SetKnown(*creature, LeashType::Rope, (*context.leashes & k_RopeLeashBit) != 0);
			leash.SetKnown(*creature, LeashType::Good, (*context.leashes & k_GoodLeashBit) != 0);
		}
	}
}

void saved_world_load::CountKept(Census& census, const saved_world_write::LoadedObjects& loaded)
{
	std::unordered_map<uint32_t, uint32_t> kept;
	for (const auto& [number, object] : loaded)
	{
		++kept[object.type];
	}
	for (auto& line : census)
	{
		if (const auto found = kept.find(line.type); found != kept.end())
		{
			line.made = std::min(line.saved, std::max(line.made, found->second));
		}
	}
}

void saved_world_load::GiveFiresTheirReactions(std::span<const PlannedFire> fires,
                                               const std::unordered_map<gamesave::ObjectNumber, uint32_t>& fireIds,
                                               const std::unordered_map<gamesave::ObjectNumber, uint32_t>& reactionIds)
{
	if (!Locator::fireSystem::has_value())
	{
		return;
	}
	for (const auto& fire : fires)
	{
		const auto id = fireIds.find(fire.number);
		const auto reaction = reactionIds.find(fire.record.objects[3].number);
		if (id == fireIds.end() || reaction == reactionIds.end())
		{
			continue;
		}
		if (auto* made = Locator::fireSystem::value().FindById(id->second); made != nullptr)
		{
			made->reaction = reaction->second;
		}
	}
}

saved_world_load::Census saved_world_load::Apply(const WorldPlan& plan, const Makers& makers,
                                                 saved_world_write::LoadedObjects* loaded, std::vector<entt::entity>* resolves)
{
	// each object made, by its number in the file, for the next save's writer and for the reactions that name it
	std::unordered_map<gamesave::ObjectNumber, entt::entity> entities;
	// (census) the records each class's entities stand for, for the classes the census counts no other way
	std::unordered_map<uint32_t, uint32_t> keptByType;
	// the number each entity was first kept under, which places its creation draws among the others' at the end
	std::unordered_map<entt::entity, gamesave::ObjectNumber> numberOfEntity;
	const auto keep = [loaded, &entities, &keptByType, &numberOfEntity](gamesave::ObjectNumber number, entt::entity entity,
	                                                                    uint32_t type) {
		if (entity != entt::null && entities.try_emplace(number, entity).second)
		{
			++keptByType[type];
		}
		if (entity != entt::null)
		{
			numberOfEntity.try_emplace(entity, number);
		}
		if (loaded != nullptr && entity != entt::null)
		{
			loaded->try_emplace(number, saved_world_write::LoadedObject {.entity = entity, .type = type});
		}
	};
	// an object no entity stands for, by its class and its id there: a part of a thing made again, which the census counts
	// as made with it
	std::unordered_set<gamesave::ObjectNumber> sourcesKept;
	// the effects made again, by their managers' numbers, which the fireballs' balls name
	std::unordered_map<gamesave::ObjectNumber, uint64_t> managersMade;
	const auto keepSource = [loaded, &keptByType, &sourcesKept, &managersMade](gamesave::ObjectNumber number, uint32_t type,
	                                                                           uint64_t id) {
		if (sourcesKept.insert(number).second)
		{
			++keptByType[type];
		}
		if (type == gamesave::ParticleManagerRecord::k_Type)
		{
			managersMade.try_emplace(number, id);
		}
		if (loaded != nullptr)
		{
			loaded->try_emplace(number, saved_world_write::LoadedObject {.type = type, .id = id});
		}
	};
	const auto typeOf = [](const auto& record) {
		return std::visit([](const auto& saved) { return std::decay_t<decltype(saved)>::k_Type; }, record);
	};
	// the draws the objects' creations take from the C runtime's random numbers wait until every object is made, as the
	// game takes them once all are read, object by object in the order their reads ended (ecs::creation_draws)
	if (Locator::entitiesRegistry::has_value())
	{
		creation_draws::BeginLoad(Locator::entitiesRegistry::value().Context().creationDraws);
	}
	// (openblack) a load that stops on an error leaves no draw waiting, and the draws after it are taken at once
	struct DrawsLeftOver
	{
		DrawsLeftOver() = default;
		DrawsLeftOver(const DrawsLeftOver&) = delete;
		DrawsLeftOver(DrawsLeftOver&&) = delete;
		DrawsLeftOver& operator=(const DrawsLeftOver&) = delete;
		DrawsLeftOver& operator=(DrawsLeftOver&&) = delete;
		~DrawsLeftOver()
		{
			if (Locator::entitiesRegistry::has_value())
			{
				creation_draws::Drop(Locator::entitiesRegistry::value().Context().creationDraws);
			}
		}
	} drawsLeftOver;
	auto& forests = Locator::forestSystem::value();
	std::unordered_map<gamesave::ObjectNumber, uint32_t> forestKeys; // each saved forest's key
	for (const auto& [number, record] : plan.forests)
	{
		// the forest keeps its saved id, which another forest may share; the counter is put back below
		const auto key = forests.Create(record.id, map_coords::ToWorld(CoordsOf(record.container.position.position)));
		auto& forest = forests.All().at(key);
		forest.id = record.id;
		forest.scenic = record.isScenic != 0;
		forest.saved = record;
		forestKeys.emplace(number, key);
		keepSource(number, gamesave::ForestRecord::k_Type, key);
	}
	if (plan.lastTreeCreatedTurn.has_value())
	{
		forests.SetLastTreeCreatedTurn(*plan.lastTreeCreatedTurn);
	}
	if (plan.nextForestId.has_value())
	{
		forests.SetNextId(*plan.nextForestId);
	}

	const auto keyOf = [&forestKeys](gamesave::ObjectNumber number) {
		const auto found = forestKeys.find(number);
		return found != forestKeys.end() ? found->second : 0U;
	};

	// the footpaths, then the links that list them
	std::unordered_map<gamesave::ObjectNumber, entt::entity> footpathsMade;
	for (const auto& footpath : plan.footpaths)
	{
		if (const auto made = makers.footpath ? makers.footpath(footpath) : entt::entity {entt::null}; made != entt::null)
		{
			footpathsMade.emplace(footpath.number, made);
			keep(footpath.number, made, gamesave::FootpathRecord::k_Type);
			// its nodes, each by its number, as the footpath made them (their ids from 0, in its list's order), so
			// that what names a node, a walker on the footpath, names the same object
			const auto* component = Locator::entitiesRegistry::has_value()
			                            ? Locator::entitiesRegistry::value().TryGet<const components::Footpath>(made)
			                            : nullptr;
			for (size_t node = 0;
			     component != nullptr && node < component->nodes.size() && node < footpath.record.nodes.numbers.size(); ++node)
			{
				keepSource(footpath.record.nodes.numbers[node], gamesave::FootpathNodeRecord::k_Type,
				           saved_world_write::PartId(made, static_cast<uint32_t>(component->nodes[node].id)));
			}
		}
	}
	std::unordered_map<gamesave::ObjectNumber, entt::entity> linksMade;
	for (const auto& link : plan.footpathLinks)
	{
		std::vector<entt::entity> listed;
		for (const auto number : link.record.footpaths.numbers)
		{
			if (const auto found = footpathsMade.find(number); found != footpathsMade.end())
			{
				listed.push_back(found->second);
			}
		}
		if (const auto made = makers.footpathLink ? makers.footpathLink(link, listed) : entt::entity {entt::null};
		    made != entt::null)
		{
			linksMade.emplace(link.number, made);
			keep(link.number, made, gamesave::FootpathLinkRecord::k_Type);
		}
	}
	// a thing made gets the footpath link its record names
	const auto giveLink = [&linksMade](entt::entity thing, gamesave::ObjectNumber link) {
		if (const auto found = linksMade.find(link); found != linksMade.end() && Locator::entitiesRegistry::has_value())
		{
			footpaths::SetFootpathLink(thing, found->second);
		}
	};

	// the climates, then the rivers and the banks of mist
	const auto climatesMade = makers.climates ? static_cast<uint32_t>(makers.climates(plan.climates)) : 0U;
	if (climatesMade != 0)
	{
		// each climate by its id
		for (const auto& climate : plan.climates)
		{
			keepSource(climate.number, gamesave::ClimateRecord::k_Type, climate.record.words[2]);
		}
	}
	// the things of the weather, from the list's last to its first, each at the head of the things' list
	uint32_t weatherThingsMade = 0;
	for (const auto& thing : plan.weatherThings | std::views::reverse)
	{
		const auto made = makers.weatherThing ? makers.weatherThing(thing) : entt::entity {entt::null};
		weatherThingsMade += made != entt::null ? 1 : 0;
		keep(thing.number, made, gamesave::WeatherThingRecord::k_Type);
	}
	// the arenas, from the list's last to its first, so that the newest is made last: each a land's arena, among the
	// entities made and, for the writer, the source's object of its entity
	uint32_t arenasMade = 0;
	for (const auto& arena : plan.arenas | std::views::reverse)
	{
		const auto made = makers.arena ? makers.arena(arena) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++arenasMade;
			entities.try_emplace(arena.number, made);
			keepSource(arena.number, gamesave::ArenaRecord::k_Type, saved_world_write::LandArenaId(made));
		}
	}
	uint32_t highlightsMade = 0;
	for (const auto& highlight : plan.highlights | std::views::reverse)
	{
		const auto made = makers.highlight ? makers.highlight(highlight) : entt::entity {entt::null};
		highlightsMade += made != entt::null ? 1 : 0;
		keep(highlight.number, made, gamesave::ScriptHighlightRecord::k_Type);
	}
	uint32_t markersMade = 0;
	for (const auto& marker : plan.markers | std::views::reverse)
	{
		const auto made = makers.marker ? makers.marker(marker) : entt::entity {entt::null};
		markersMade += made != entt::null ? 1 : 0;
		keep(marker.number, made, gamesave::ScriptMarkerRecord::k_Type);
	}
	uint32_t waypointsMade = 0;
	for (const auto& waypoint : plan.waypoints | std::views::reverse)
	{
		const auto made = makers.waypoint ? makers.waypoint(waypoint) : entt::entity {entt::null};
		waypointsMade += made != entt::null ? 1 : 0;
		keep(waypoint.number, made, gamesave::WaypointRecord::k_Type);
	}
	uint32_t timersMade = 0;
	for (const auto& timer : plan.timers)
	{
		const auto made = makers.timer ? makers.timer(timer) : entt::entity {entt::null};
		timersMade += made != entt::null ? 1 : 0;
		keep(timer.number, made, gamesave::ScriptTimerRecord::k_Type);
	}
	uint32_t streamsMade = 0;
	for (const auto& stream : plan.streams)
	{
		const auto made = makers.stream ? makers.stream(stream) : entt::entity {entt::null};
		streamsMade += made != entt::null ? 1 : 0;
		keep(stream.number, made, gamesave::StreamRecord::k_Type);
	}
	uint32_t mistsMade = 0;
	for (const auto& mist : plan.mists)
	{
		const auto made = makers.mist ? makers.mist(mist) : entt::entity {entt::null};
		mistsMade += made != entt::null ? 1 : 0;
		keep(mist.number, made, gamesave::MistRecord::k_Type);
	}

	// the towns, each with its forests in its list's order (the head first)
	std::unordered_map<gamesave::ObjectNumber, entt::entity> towns;
	for (const auto& town : plan.towns)
	{
		const auto made = makers.town ? makers.town(town) : entt::entity {entt::null};
		if (made == entt::null)
		{
			continue;
		}
		towns.emplace(town.number, made);
		keep(town.number, made, gamesave::TownRecord::k_Type);
		if (const auto* component = Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(made)
		                                ? Locator::entitiesRegistry::value().TryGet<components::Town>(made)
		                                : nullptr;
		    component != nullptr)
		{
			auto& list = forests.TownLists()[component->id];
			list.clear();
			for (const auto forest : town.record.forests.numbers)
			{
				if (const auto key = keyOf(forest); key != 0)
				{
					list.push_back(key);
				}
			}
		}
	}
	// each player's list of towns in its saved order: the owner's list stamps follow it
	if (Locator::entitiesRegistry::has_value())
	{
		for (const auto number : plan.ownersTowns)
		{
			if (const auto found = towns.find(number); found != towns.end())
			{
				auto& registry = Locator::entitiesRegistry::value();
				if (auto* component =
				        registry.Valid(found->second) ? registry.TryGet<components::Town>(found->second) : nullptr;
				    component != nullptr)
				{
					component->ownerListStamp = town_queries::NextOwnerListStamp();
				}
			}
		}
	}
	// the towns' keys, which their buildings name them by
	std::unordered_map<gamesave::ObjectNumber, uint32_t> townKeys;
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& [number, made] : towns)
		{
			if (const auto* component = registry.Valid(made) ? registry.TryGet<components::Town>(made) : nullptr;
			    component != nullptr)
			{
				townKeys.emplace(number, component->id);
			}
		}
	}
	const auto townKeyOf = [&townKeys](gamesave::ObjectNumber number) {
		const auto found = townKeys.find(number);
		return found != townKeys.end() ? found->second : components::Abode::k_NoTown;
	};
	uint32_t treesMade = 0;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> bigForests;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> piles;
	// the food piles sped up, by the number of the container of their sparkle
	std::unordered_map<gamesave::ObjectNumber, entt::entity> pileSparkles;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> storagePits;
	std::unordered_map<gamesave::ObjectNumber, entt::entity> hearts;
	uint32_t entrancesMade = 0;
	/// the abodes, creches and graveyards made, by class
	std::unordered_map<uint32_t, uint32_t> abodesMade;
	uint32_t desireFlagsMade = 0;
	/// the rocks and other mobile statics made, by class
	std::unordered_map<uint32_t, uint32_t> mobileStaticsMade;
	uint32_t animatedStaticsMade = 0;
	/// each citadel's first heart, which keeps the citadel
	std::unordered_map<gamesave::ObjectNumber, entt::entity> citadels;
	const auto pileOf = [&piles](gamesave::ObjectNumber number) {
		const auto found = piles.find(number);
		return found != piles.end() ? found->second : entt::entity {entt::null};
	};
	for (const auto& object : plan.objects)
	{
		if (const auto* tree = std::get_if<PlannedTree>(&object); tree != nullptr)
		{
			if (const auto made = makers.tree(*tree, keyOf(tree->record.forest.number)); made != entt::null)
			{
				++treesMade;
				keep(tree->number, made, gamesave::TreeRecord::k_Type);
			}
		}
		else if (const auto* big = std::get_if<PlannedBigForest>(&object); big != nullptr)
		{
			if (const auto made = makers.bigForest(*big, keyOf(big->record.forest.number)); made != entt::null)
			{
				bigForests.emplace(big->number, made);
				keep(big->number, made, gamesave::BigForestRecord::k_Type);
				giveLink(made, big->record.base.multi.footpathLink);
			}
		}
		else if (const auto* pile = std::get_if<PlannedPile>(&object); pile != nullptr && makers.pile)
		{
			if (const auto made = makers.pile(*pile); made != entt::null)
			{
				// the town it was made for, which only a saved game reads
				auto* registry = Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
				auto* pot = registry != nullptr && registry->Valid(made) ? registry->TryGet<components::Pot>(made) : nullptr;
				if (const auto town = towns.find(pile->pile.parentTown.number); pot != nullptr && town != towns.end())
				{
					pot->town = town->second;
				}
				// its sparkle is the container the save kept, made with the containers
				if (const auto* food = std::get_if<gamesave::FoodPileRecord>(&pile->record);
				    pot != nullptr && food != nullptr && food->store.number != 0)
				{
					pileSparkles.emplace(food->store.number, made);
				}
				piles.emplace(pile->number, made);
				keep(pile->number, made, typeOf(pile->record));
			}
		}
		else if (const auto* pit = std::get_if<PlannedStoragePit>(&object); pit != nullptr && makers.storagePit)
		{
			// its piles were read inside it, so they are made already: its food pile, then its five other piles
			archetypes::AbodeArchetype::StoragePitPiles pitPiles;
			if (!pit->record.foodPile.numbers.empty())
			{
				pitPiles.food = pileOf(pit->record.foodPile.numbers.front());
			}
			for (size_t i = 0; i < k_PitWoodPiles && i < pit->record.otherPiles.numbers.size(); ++i)
			{
				pitPiles.wood.at(i) = pileOf(pit->record.otherPiles.numbers.at(i));
			}
			if (const auto made = makers.storagePit(*pit, townKeyOf(pit->record.abode.town.number), pitPiles);
			    made != entt::null)
			{
				storagePits.emplace(pit->number, made);
				keep(pit->number, made, gamesave::StoragePitRecord::k_Type);
				giveLink(made, pit->record.base.multi.footpathLink);
			}
		}
		else if (const auto* abode = std::get_if<PlannedAbode>(&object); abode != nullptr && makers.abode)
		{
			if (const auto made = makers.abode(*abode, townKeyOf(abode->abode.town.number)); made != entt::null)
			{
				++abodesMade[typeOf(abode->record)];
				keep(abode->number, made, typeOf(abode->record));
				giveLink(made, abode->base.multi.footpathLink);
			}
		}
		else if (const auto* flag = std::get_if<PlannedDesireFlag>(&object); flag != nullptr && makers.desireFlag)
		{
			const auto town = towns.find(flag->record.town.number);
			if (town == towns.end())
			{
				continue;
			}
			if (const auto made = makers.desireFlag(*flag, town->second); made != entt::null)
			{
				++desireFlagsMade;
				keep(flag->number, made, gamesave::TownDesireFlagsRecord::k_Type);
			}
		}
		else if (const auto* statics = std::get_if<PlannedMobileStatic>(&object); statics != nullptr && makers.mobileStatic)
		{
			if (const auto made = makers.mobileStatic(*statics); made != entt::null)
			{
				++mobileStaticsMade[typeOf(statics->record)];
				keep(statics->number, made, typeOf(statics->record));
				giveLink(made, statics->base.multi.footpathLink);
			}
		}
		else if (const auto* animated = std::get_if<PlannedAnimatedStatic>(&object);
		         animated != nullptr && makers.animatedStatic)
		{
			if (const auto made = makers.animatedStatic(*animated); made != entt::null)
			{
				++animatedStaticsMade;
				keep(animated->number, made, gamesave::AnimatedStaticRecord::k_Type);
				giveLink(made, animated->record.base.multi.footpathLink);
			}
		}
		else if (const auto* heart = std::get_if<PlannedCitadelHeart>(&object); heart != nullptr && makers.citadelHeart)
		{
			const auto found = citadels.find(heart->record.citadel.number);
			const auto citadel = found != citadels.end() ? found->second : entt::entity {entt::null};
			if (const auto made = makers.citadelHeart(*heart, citadel); made != entt::null)
			{
				hearts.emplace(heart->number, made);
				keep(heart->number, made, gamesave::CitadelHeartRecord::k_Type);
				// its entrance, which its making makes with it, under the number the heart's record names
				if (const auto number = heart->record.entrance.number; number != 0 && Locator::entitiesRegistry::has_value())
				{
					Locator::entitiesRegistry::value().Each<const components::CitadelEntrance>(
					    [&](entt::entity entrance, const components::CitadelEntrance& of) {
						    if (of.heart == made)
						    {
							    ++entrancesMade;
							    keep(number, entrance, gamesave::CitadelEntranceRecord::k_Type);
						    }
					    });
				}
				// the citadel's first heart keeps the citadel's record too
				if (citadels.try_emplace(heart->record.citadel.number, made).second)
				{
					keep(heart->record.citadel.number, made, gamesave::CitadelRecord::k_Type);
				}
				giveLink(made, heart->record.base.multi.footpathLink);
			}
		}
	}
	// (the citadels' hearts are made in the loop above, each of the citadel its first heart keeps)
	// the dead and felled trees and the singing stones' bases, in the order their reads ended
	for (const auto& dead : plan.deadTrees)
	{
		if (const auto made = makers.deadTree ? makers.deadTree(dead) : entt::entity {entt::null}; made != entt::null)
		{
			++mobileStaticsMade[typeOf(dead.record)];
			keep(dead.number, made, typeOf(dead.record));
		}
	}
	uint32_t featuresMade = 0;
	for (const auto& feature : plan.features)
	{
		if (const auto made = makers.feature ? makers.feature(feature) : entt::entity {entt::null}; made != entt::null)
		{
			++featuresMade;
			keep(feature.number, made, gamesave::FeatureRecord::k_Type);
			giveLink(made, feature.record.base.multi.footpathLink);
		}
	}
	uint32_t lanternsMade = 0;
	for (const auto& lantern : plan.streetLanterns)
	{
		if (const auto made = makers.streetLantern ? makers.streetLantern(lantern) : entt::entity {entt::null};
		    made != entt::null)
		{
			++lanternsMade;
			keep(lantern.number, made, gamesave::StreetLanternRecord::k_Type);
		}
	}
	for (const auto& base : plan.stoneBases)
	{
		if (const auto made = makers.stoneBase ? makers.stoneBase(base) : entt::entity {entt::null}; made != entt::null)
		{
			++mobileStaticsMade[gamesave::BaseOnlyRecord::k_Type];
			keep(base.number, made, gamesave::BaseOnlyRecord::k_Type);
		}
	}
	// the moving objects, the one-off spells' orbs and the scaffolds, in the order their reads ended
	std::unordered_map<uint32_t, uint32_t> mobileObjectsMade;
	std::vector<std::pair<const PlannedMobileObject*, entt::entity>> madeScaffolds;
	for (const auto& mobile : plan.mobileObjects)
	{
		if (const auto made = makers.mobileObject ? makers.mobileObject(mobile) : entt::entity {entt::null}; made != entt::null)
		{
			++mobileObjectsMade[typeOf(mobile.record)];
			keep(mobile.number, made, typeOf(mobile.record));
			if (std::holds_alternative<gamesave::ScaffoldRecord>(mobile.record))
			{
				madeScaffolds.emplace_back(&mobile, made);
			}
		}
	}
	// the wonders, buildings of their towns, in the order their reads ended
	for (const auto& wonder : plan.wonders)
	{
		if (const auto made =
		        makers.wonder ? makers.wonder(wonder, townKeyOf(wonder.record.abode.town.number)) : entt::entity {entt::null};
		    made != entt::null)
		{
			++abodesMade[gamesave::WonderRecord::k_Type];
			keep(wonder.number, made, gamesave::WonderRecord::k_Type);
			giveLink(made, wonder.record.base.multi.footpathLink);
		}
	}
	// the piles of magic food, in the order their reads ended, each with the town it was made for, which only a saved
	// game reads, and its sparkle made again standing for the container the save kept
	uint32_t magicFoodsMade = 0;
	for (const auto& food : plan.magicFoods)
	{
		const auto made = makers.magicFood ? makers.magicFood(food) : entt::entity {entt::null};
		if (made == entt::null)
		{
			continue;
		}
		++magicFoodsMade;
		auto* registry = Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
		auto* pot = registry != nullptr && registry->Valid(made) ? registry->TryGet<components::Pot>(made) : nullptr;
		if (const auto town = towns.find(food.record.pile.parentTown.number); pot != nullptr && town != towns.end())
		{
			pot->town = town->second;
		}
		if (const auto effect = pot != nullptr && food.record.store.number != 0
		                            ? saved_world_write::SpotVisualEffect(pot->speedUpVisual)
		                            : std::nullopt;
		    effect.has_value())
		{
			keepSource(food.record.store.number, gamesave::ParticleContainerRecord::k_Type, *effect);
		}
		keep(food.number, made, gamesave::MagicFoodRecord::k_Type);
	}
	// the Hanoi puzzle's blocks, moving objects too, in the order their reads ended; their links come with their puzzle
	uint32_t hanoiBlocksMade = 0;
	for (const auto& block : plan.hanoiBlocks)
	{
		const auto made = makers.hanoiBlock ? makers.hanoiBlock(block) : entt::entity {entt::null};
		hanoiBlocksMade += made != entt::null ? 1 : 0;
		keep(block.number, made, gamesave::HanoiBlockRecord::k_Type);
	}
	// each workshop's sign, made from its own record, then its wood pile, its sign and its scaffolds as its record names
	// them, once the piles and the scaffolds are made
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		for (const auto& object : plan.objects)
		{
			const auto* abode = std::get_if<PlannedAbode>(&object);
			const auto* record = abode != nullptr ? std::get_if<gamesave::WorkshopRecord>(&abode->record) : nullptr;
			const auto workshop = record != nullptr ? objectOf(abode->number) : entt::entity {entt::null};
			if (workshop == entt::null)
			{
				continue;
			}
			if (abode->sign.has_value())
			{
				const auto& [number, saved] = *abode->sign;
				keep(number, load_workshops::MakeSign(saved, workshop), gamesave::ShowNeedsVisualsRecord::k_Type);
			}
			load_workshops::SetPileAndSign(workshop, *record, objectOf);
			load_workshops::SetScaffolds(workshop, *record, objectOf);
		}
		// each town's workshops, in its record's order, the head of its list first
		for (const auto& town : plan.towns)
		{
			const auto made = towns.find(town.number);
			auto* component = made != towns.end() && Locator::entitiesRegistry::has_value()
			                      ? Locator::entitiesRegistry::value().TryGet<components::Town>(made->second)
			                      : nullptr;
			if (component == nullptr)
			{
				continue;
			}
			component->workshops.clear();
			for (const auto number : town.record.workshops.numbers)
			{
				if (const auto workshop = objectOf(number); workshop != entt::null)
				{
					component->workshops.push_back(workshop);
				}
			}
		}
	}
	// each town's storage pit, as its record names it
	for (const auto& town : plan.towns)
	{
		const auto made = towns.find(town.number);
		const auto pit = storagePits.find(town.record.mainStoragePit.number);
		if (made != towns.end() && pit != storagePits.end())
		{
			town_stores::SetStoragePit(made->second, pit->second);
		}
	}
	// each town's centre, as its record names it
	for (const auto& town : plan.towns)
	{
		const auto made = towns.find(town.number);
		const auto centre = entities.find(town.record.townCentre.number);
		auto* component = made != towns.end() && centre != entities.end()
		                      ? Locator::entitiesRegistry::value().TryGet<components::Town>(made->second)
		                      : nullptr;
		if (component != nullptr && Locator::entitiesRegistry::value().AllOf<components::Abode>(centre->second))
		{
			component->centre = centre->second;
		}
	}
	// each town's two temporary pots, the food's then the wood's, as its record names them, once the pots are made
	for (const auto& town : plan.towns)
	{
		const auto made = towns.find(town.number);
		auto* component =
		    made != towns.end() ? Locator::entitiesRegistry::value().TryGet<components::Town>(made->second) : nullptr;
		if (component == nullptr)
		{
			continue;
		}
		const auto& numbers = town.record.temporaryStores.numbers;
		for (size_t slot = 0; slot < numbers.size() && slot < component->temporaryPots.size(); ++slot)
		{
			const auto pot = numbers.at(slot) != 0 ? entities.find(numbers.at(slot)) : entities.end();
			if (pot != entities.end())
			{
				component->temporaryPots.at(slot) = pot->second;
			}
		}
	}
	// the town centres' totem statues, in the order their reads ended, once their town centres are made
	uint32_t totemStatuesMade = 0;
	for (const auto& totem : plan.totemStatues)
	{
		const auto centre = entities.find(totem.record.object.number);
		const auto made = makers.totemStatue
		                      ? makers.totemStatue(totem, centre != entities.end() ? centre->second : entt::entity {entt::null})
		                      : entt::entity {entt::null};
		if (made != entt::null)
		{
			++totemStatuesMade;
			keep(totem.number, made, gamesave::TotemStatueRecord::k_Type);
			// its footpath link, as the building's load puts its link back
			giveLink(made, totem.record.base.multi.footpathLink);
		}
	}
	// the town centres' spell icons, in the order their reads ended, once their towns and town centres are made; then
	// each town's list of them in its record's order, the newest first
	uint32_t townCentreIconsMade = 0;
	const auto centreObjectOf = [&entities](gamesave::ObjectNumber number) {
		const auto found = entities.find(number);
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	for (const auto& icon : plan.townCentreIcons)
	{
		const auto made = makers.townCentreIcon ? makers.townCentreIcon(icon, centreObjectOf(icon.record.town.number),
		                                                                centreObjectOf(icon.record.object.number))
		                                        : entt::entity {entt::null};
		if (made != entt::null)
		{
			++townCentreIconsMade;
			keep(icon.number, made, gamesave::TownCentreSpellIconRecord::k_Type);
		}
	}
	if (townCentreIconsMade != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& town : plan.towns)
		{
			const auto made = towns.find(town.number);
			auto* magic = made != towns.end() ? registry.TryGet<components::TownMagic>(made->second) : nullptr;
			if (magic == nullptr)
			{
				continue;
			}
			for (const auto number : town.record.spellIcons.numbers)
			{
				const auto icon = centreObjectOf(number);
				if (icon != entt::null && registry.AllOf<components::TownCentreSpellIcon>(icon))
				{
					magic->spellIcons.push_back(icon);
				}
			}
		}
	}
	// each town centre's six slots as its record names its icons, as the centre's own load reads them: an icon whose
	// record names no centre (the original's saves hold none) is in the slot its centre's record names it in, still
	// naming no centre
	if (townCentreIconsMade != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<std::pair<entt::entity, std::vector<gamesave::ObjectNumber>>> centres;
		registry.Each<const components::LoadedRecord<gamesave::TownCentreRecord>>(
		    [&centres](entt::entity centre, const components::LoadedRecord<gamesave::TownCentreRecord>& kept) {
			    centres.emplace_back(centre, kept.record.icons.numbers);
		    });
		for (const auto& [centre, numbers] : centres)
		{
			for (size_t slot = 0; slot < numbers.size() && slot < components::TownCentreIcons {}.icons.size(); ++slot)
			{
				const auto icon = centreObjectOf(numbers.at(slot));
				auto* ofTown = icon != entt::null ? registry.TryGet<components::TownCentreSpellIcon>(icon) : nullptr;
				if (ofTown == nullptr || ofTown->townCentre != entt::null)
				{
					continue;
				}
				ofTown->slot = static_cast<uint8_t>(slot);
				if (registry.TryGet<components::TownCentreIcons>(centre) == nullptr)
				{
					registry.Assign<components::TownCentreIcons>(centre);
				}
				registry.Get<components::TownCentreIcons>(centre).icons.at(slot) = icon;
			}
		}
	}
	// the fish farms, each of the town its record names
	uint32_t fishFarmsMade = 0;
	for (const auto& farm : plan.fishFarms)
	{
		const auto town = towns.find(farm.record.town.number);
		const auto made = makers.fishFarm
		                      ? makers.fishFarm(farm, town != towns.end() ? town->second : entt::entity {entt::null})
		                      : entt::entity {entt::null};
		if (made != entt::null)
		{
			++fishFarmsMade;
			keep(farm.number, made, gamesave::FishFarmRecord::k_Type);
			giveLink(made, farm.record.base.multi.footpathLink);
		}
	}
	// each town's plans in its list's order, with the footpath link each names (made already)
	uint32_t plannedAbodesMade = 0;
	uint32_t plannedCentresMade = 0;
	uint32_t plannedHeartsMade = 0;
	for (const auto& town : plan.towns)
	{
		const auto made = towns.find(town.number);
		auto* component = made != towns.end() && Locator::entitiesRegistry::has_value()
		                      ? Locator::entitiesRegistry::value().TryGet<components::Town>(made->second)
		                      : nullptr;
		if (component == nullptr)
		{
			continue;
		}
		for (const auto number : town.record.planned.numbers)
		{
			const auto found = plan.townPlans.find(number);
			if (found == plan.townPlans.end())
			{
				continue;
			}
			const auto& [kind, fields] = found->second;
			const bool townCentre = kind == gamesave::PlannedTownCentreRecord::k_Type;
			const bool heart = kind == gamesave::PlannedCitadelHeartRecord::k_Type;
			// a planned citadel heart is of the citadel heart's row, the one the game's tables hold
			const auto type = heart ? (fields.info == k_CitadelHeartInfoRow ? std::optional(AbodeInfo::None) : std::nullopt)
			                        : AbodeTypeOfInfo(fields.info);
			if (!type.has_value())
			{
				continue;
			}
			const auto link = entities.find(fields.footpathLink.number);
			// (census) a plan its town takes back is its record made again
			++(heart ? plannedHeartsMade : townCentre ? plannedCentresMade : plannedAbodesMade);
			component->plannedAbodes.push_back(
			    components::PlannedAbode {.info = *type,
			                              .position = map_coords::ToWorld(CoordsOf(fields.position.position)),
			                              .yAngleRadians = fields.yAngle,
			                              .scale = fields.scale,
			                              .townCentre = townCentre,
			                              .wasBuilt = fields.wasConstructed != 0,
			                              .creationTurn = static_cast<uint32_t>(fields.creationTurn),
			                              .footpathLink = link != entities.end() ? link->second : entt::entity {entt::null},
			                              .citadelHeart = heart});
			// kept by its town and its place in its list, for the next save's writer
			keepSource(number, kind,
			           saved_world_write::PartId(made->second, static_cast<uint32_t>(component->plannedAbodes.size() - 1)));
		}
	}
	// each forest points at the big forest its own record names, once every big forest is made
	for (const auto& [number, record] : plan.forests)
	{
		if (const auto found = bigForests.find(record.bigForest.number); found != bigForests.end())
		{
			forests.All().at(keyOf(number)).bigForest = found->second;
		}
	}

	// the worship sites, once their citadels' hearts, towns and food pots are made
	uint32_t sitesMade = 0;
	std::vector<std::pair<const PlannedWorshipSite*, entt::entity>> madeSites;
	if (makers.worshipSite)
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		for (const auto& planned : plan.worshipSites)
		{
			const auto made = makers.worshipSite(planned, objectOf);
			if (made != entt::null)
			{
				++sitesMade;
				keep(planned.number, made, gamesave::WorshipSiteRecord::k_Type);
				madeSites.emplace_back(&planned, made);
				// its totem and its need signs, as the site made them
				const auto& component = Locator::entitiesRegistry::value().Get<const components::WorshipSite>(made);
				if (planned.totem.has_value())
				{
					keep(planned.totemNumber, component.totem, gamesave::WorshipTotemRecord::k_Type);
					// its needs, which its totem stands for in a save as its writer names it
					if (planned.needsNumber != 0)
					{
						keep(planned.needsNumber, component.totem, gamesave::ShowNeedsRecord::k_Type);
					}
				}
				for (size_t i = 0; i < planned.signs.size(); ++i)
				{
					if (planned.signs[i].second.has_value())
					{
						keep(planned.signs[i].first, component.needsSigns.at(i), gamesave::ShowNeedsVisualsRecord::k_Type);
					}
				}
				// its spell icons, in its list's order
				for (size_t i = 0; i < planned.icons.size() && i < component.icons.size(); ++i)
				{
					keep(planned.icons[i].first, component.icons[i], gamesave::WorshipSpellIconRecord::k_Type);
				}
			}
		}
	}
	// the building sites, in the order their reads ended, once their buildings and wood piles are made
	std::unordered_map<uint32_t, uint32_t> buildingSitesMade;
	std::vector<std::pair<const PlannedBuildingSite*, entt::entity>> madeBuildingSites;
	const std::function<entt::entity(gamesave::ObjectNumber)> siteObjectOf = [&entities](gamesave::ObjectNumber number) {
		const auto found = entities.find(number);
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	for (const auto& planned : plan.buildingSites)
	{
		const auto root = std::visit([](const auto& record) { return record.rootBuilding.number; }, planned.record);
		const auto made =
		    makers.buildingSite ? makers.buildingSite(planned, siteObjectOf(root), siteObjectOf) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++buildingSitesMade[typeOf(planned.record)];
			keep(planned.number, made, typeOf(planned.record));
			madeBuildingSites.emplace_back(&planned, made);
		}
	}
	// each scaffold's building site, workshop, town and holder as its record names them, once the sites are made;
	// then its phantom as the original's load resolves it: a site's building's, else its plan's building's (none for an
	// object being deleted)
	for (const auto& [planned, entity] : madeScaffolds)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* scaffold = registry.TryGet<components::Scaffold>(entity);
		if (scaffold == nullptr)
		{
			continue;
		}
		// (census) its own plans that its making took back are their records made again
		plannedAbodesMade += (scaffold->plan.has_value() ? 1U : 0U) + (scaffold->candidate.has_value() ? 1U : 0U);
		const auto& record = std::get<gamesave::ScaffoldRecord>(planned->record);
		scaffold->site = siteObjectOf(record.objects[0].number);
		scaffold->owner = siteObjectOf(record.base.mobileObject.object);
		scaffold->town = siteObjectOf(record.last.number);
		scaffold->holder = planned->scaffold.holder;
		if ((record.thing.flags & k_ThingUnavailable) == 0)
		{
			scaffolds::ResolvePhantomAfterLoad(entity);
		}
	}
	// the villagers, in the order their reads ended: each whose states load nothing draws its counter as its read ends,
	// made or not
	std::unordered_map<uint32_t, uint32_t> villagersMade;
	std::vector<std::pair<const PlannedVillager*, entt::entity>> madeVillagers;
	for (const auto& planned : plan.villagers)
	{
		const auto& values =
		    std::visit([](const auto& record) -> const gamesave::VillagerValues& { return record.values; }, planned.record);
		std::optional<uint16_t> counter;
		if (VillagerCounterDrawnOnLoad(values.living.action[0], values.living.action[1]))
		{
			counter = static_cast<uint16_t>(game_random::GameRand(k_LoadedCounterRange) + 1);
		}
		const auto made = makers.villager ? makers.villager(planned, counter) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++villagersMade[typeOf(planned.record)];
			keep(planned.number, made, typeOf(planned.record));
			madeVillagers.emplace_back(&planned, made);
			// its walk along a script's track, made with it, by the id the data paths' writer names it by
			if (values.living.dataPath.number != 0 && Locator::entitiesRegistry::has_value() &&
			    Locator::entitiesRegistry::value().AllOf<components::LivingWalkPath>(made))
			{
				keepSource(values.living.dataPath.number, gamesave::DataPathRecord::k_Type,
				           saved_world_write::DataPathIdOf(made));
			}
		}
	}
	// each building site's builders in its list's order, and each town's sites in its own list's order, once the
	// villagers are made
	if (!madeBuildingSites.empty())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& [planned, site] : madeBuildingSites)
		{
			auto& component = registry.Get<components::BuildingSite>(site);
			const auto& workers =
			    std::visit([](const auto& record) -> const gamesave::RefList& { return record.workers; }, planned->record);
			for (const auto number : workers.numbers)
			{
				if (const auto builder = siteObjectOf(number); builder != entt::null)
				{
					component.builders.push_back(builder);
				}
			}
		}
		for (const auto& town : plan.towns)
		{
			const auto made = siteObjectOf(town.number);
			auto* component = made != entt::null ? registry.TryGet<components::Town>(made) : nullptr;
			if (component == nullptr)
			{
				continue;
			}
			for (const auto number : town.record.buildingSites.numbers)
			{
				const auto site = siteObjectOf(number);
				if (site != entt::null && registry.AllOf<components::BuildingSite>(site))
				{
					component->buildingSites.push_back(site);
				}
			}
		}
	}
	if (!madeVillagers.empty())
	{
		const auto entityOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		auto& registry = Locator::entitiesRegistry::value();
		// each footpath's nodes by their numbers, in its list's order
		std::unordered_map<gamesave::ObjectNumber, std::vector<gamesave::ObjectNumber>> footpathNodes;
		for (const auto& footpath : plan.footpaths)
		{
			footpathNodes.try_emplace(footpath.number, footpath.record.nodes.numbers);
		}
		// each villager's home, town and mother, as its record names them
		for (const auto& [planned, entity] : madeVillagers)
		{
			const auto& fields = std::visit(
			    [](const auto& record) -> const gamesave::VillagerFields& { return record.values.villager; }, planned->record);
			auto& villager = registry.Get<components::Villager>(entity);
			villager.abode = entityOf(fields.home.number);
			villager.town = entityOf(fields.town.number);
			villager.mother = entityOf(fields.mother.number);
			// the footpath it walks on, its node there and its way along it, and the footpath it takes next
			const auto& walk = std::visit([](const auto& record) -> const gamesave::VillagerValues& { return record.values; },
			                              planned->record);
			const auto footpath = entityOf(walk.living.footpath.number);
			const auto next = entityOf(walk.move.footpath.number);
			if (footpath != entt::null || next != entt::null)
			{
				auto& onFootpath = registry.AssignOrReplace<components::LivingFootpath>(entity);
				onFootpath.footpath = footpath;
				onFootpath.next = next;
				onFootpath.reverse = (walk.living.status & k_LivingFootpathReverse) != 0;
				if (const auto found = footpathNodes.find(walk.living.footpath.number); found != footpathNodes.end())
				{
					const auto& nodes = found->second;
					const auto at = std::ranges::find(nodes, walk.living.footpathNode.number);
					if (walk.living.footpathNode.number != 0 && at != nodes.end())
					{
						onFootpath.node = static_cast<openblack::ecs::footpaths::NodeId>(std::distance(nodes.begin(), at));
					}
				}
			}
			// the circle of a thing it heads for or goes round, as the walk keeps it, whatever its turns to it (none in
			// reach going round): the thing's circle at the saved place among its circles (its only circle for -1)
			if (const auto& circle = walk.move.circle; circle.has_value() && circle->object.number != 0)
			{
				const auto thing = entityOf(circle->object.number);
				const auto circles = thing != entt::null ? openblack::ecs::thing_circles::CirclesOf(registry, thing)
				                                         : std::vector<openblack::ecs::wall_hug::BlockingCircle> {};
				const auto index = static_cast<size_t>(std::max(circle->index, 0));
				if (index < circles.size())
				{
					const auto& found = circles[index];
					registry.AssignOrReplace<components::WallHugObjectReference>(
					    entity, components::WallHugObjectReference {.entity = thing,
					                                                .circle = static_cast<uint16_t>(index),
					                                                .centre = found.centre,
					                                                .radius = found.radius,
					                                                .landscape = false,
					                                                .landscapeOrFence = found.landscapeOrFence});
				}
			}
			// what it works on, when one of its states saves it
			const auto& values = std::visit([](const auto& record) -> const gamesave::VillagerValues& { return record.values; },
			                                planned->record);
			for (const auto& [state, saved] : {std::pair {values.living.action[0], &values.topState},
			                                   std::pair {values.living.action[1], &values.finalState}})
			{
				if (TargetsAThing(gamesave::VillagerStateSaveOf(state)) && !saved->empty())
				{
					villager.targetThing = entityOf(saved->front().number);
				}
				// a farmer's field, after its work point
				constexpr size_t k_FarmingField = 1;
				if (gamesave::VillagerStateSaveOf(state) == gamesave::VillagerStateSave::Farming &&
				    saved->size() > k_FarmingField)
				{
					villager.targetThing = entityOf(saved->at(k_FarmingField).number);
				}
				// a builder's building site, before its place there
				if (gamesave::VillagerStateSaveOf(state) == gamesave::VillagerStateSave::Building && !saved->empty())
				{
					villager.buildingSite = entityOf(saved->front().number);
				}
			}
		}
		// each abode's villagers and counts, as its own record lists them
		const auto giveVillagers = [&](gamesave::ObjectNumber number, const gamesave::AbodeFields& fields) {
			const auto abode = entityOf(number);
			auto* component = abode != entt::null ? registry.TryGet<components::Abode>(abode) : nullptr;
			if (component == nullptr)
			{
				return;
			}
			component->inhabitants.clear();
			for (const auto villager : fields.villagers.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					component->inhabitants.push_back(entity);
				}
			}
			component->adultCount = fields.adults;
			component->presentAtHome = fields.presentAtHome;
			component->childCount = fields.children;
		};
		for (const auto& object : plan.objects)
		{
			if (const auto* abode = std::get_if<PlannedAbode>(&object); abode != nullptr)
			{
				giveVillagers(abode->number, abode->abode);
			}
			else if (const auto* pit = std::get_if<PlannedStoragePit>(&object); pit != nullptr)
			{
				giveVillagers(pit->number, pit->record.abode);
			}
		}
		for (const auto& wonder : plan.wonders)
		{
			giveVillagers(wonder.number, wonder.record.abode);
		}
		// each worship site's villagers, and those requesting to go home, sorted as its load sorts them
		for (const auto& [planned, site] : madeSites)
		{
			auto& component = registry.Get<components::WorshipSite>(site);
			component.villagers.clear();
			for (const auto villager : planned->record.worshipping.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					component.villagers.push_back(entity);
				}
			}
			component.goHomeRequests.clear();
			for (const auto villager : planned->record.villagersGoingHome.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					openblack::worship::site::InsertGoHomeRequest(component, entity);
				}
			}
		}
		// each town's homeless villagers, as its record lists them
		for (const auto& town : plan.towns)
		{
			const auto made = towns.find(town.number);
			if (made == towns.end())
			{
				continue;
			}
			auto* component = registry.TryGet<components::Town>(made->second);
			if (component == nullptr)
			{
				continue;
			}
			auto& homeless = component->homelessVillagers;
			homeless.clear();
			for (const auto villager : town.record.homeless.numbers)
			{
				if (const auto entity = entityOf(villager); entity != entt::null)
				{
					homeless.push_back(entity);
				}
			}
		}
	}
	else
	{
		// with no villager made, each abode still gets back the counts its record keeps (of adults, of those at home and
		// of children), as the load reads them with the rest of the abode; it holds no villager
		auto& registry = Locator::entitiesRegistry::value();
		const auto giveCounts = [&](gamesave::ObjectNumber number, const gamesave::AbodeFields& fields) {
			const auto found = entities.find(number);
			auto* component = found != entities.end() ? registry.TryGet<components::Abode>(found->second) : nullptr;
			if (component == nullptr)
			{
				return;
			}
			component->adultCount = fields.adults;
			component->presentAtHome = fields.presentAtHome;
			component->childCount = fields.children;
		};
		for (const auto& object : plan.objects)
		{
			if (const auto* abode = std::get_if<PlannedAbode>(&object); abode != nullptr)
			{
				giveCounts(abode->number, abode->abode);
			}
			else if (const auto* pit = std::get_if<PlannedStoragePit>(&object); pit != nullptr)
			{
				giveCounts(pit->number, pit->record.abode);
			}
		}
		for (const auto& wonder : plan.wonders)
		{
			giveCounts(wonder.number, wonder.record.abode);
		}
	}

	const auto objectOfMade = [&entities](gamesave::ObjectNumber number) {
		const auto found = entities.find(number);
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	// the animals and birds, then the flocks, each in the order their reads ended; a flock is made only once everything
	// it names is, and an animal of a flock not made is not made either, so that a flock and its members come back
	// together; then the creatures
	std::unordered_map<gamesave::ObjectNumber, entt::entity> animalEntities;
	std::vector<std::pair<const PlannedAnimal*, entt::entity>> madeAnimals;
	for (const auto& planned : plan.animals)
	{
		const auto made = makers.animal ? makers.animal(planned, objectOfMade) : entt::entity {entt::null};
		if (made != entt::null)
		{
			animalEntities.emplace(planned.number, made);
			madeAnimals.emplace_back(&planned, made);
		}
	}
	const auto objectOrAnimalOf = [&objectOfMade, &animalEntities](gamesave::ObjectNumber number) {
		const auto found = animalEntities.find(number);
		return found != animalEntities.end() ? found->second : objectOfMade(number);
	};
	uint32_t flocksMade = 0;
	std::unordered_set<gamesave::ObjectNumber> flocksKept;
	for (const auto& planned : plan.flocks)
	{
		const auto made = makers.flock ? makers.flock(planned, objectOrAnimalOf) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++flocksMade;
			flocksKept.insert(planned.number);
			keep(planned.number, made, gamesave::FlockRecord::k_Type);
		}
	}
	// each villager in a flock (a shepherd's) is in it again once the flock is made
	for (const auto& [planned, villagerEntity] : madeVillagers)
	{
		const auto flock = std::visit([](const auto& record) { return record.values.living.flock.number; }, planned->record);
		if (const auto found = flock != 0 ? entities.find(flock) : entities.end(); found != entities.end())
		{
			if (auto* villager = Locator::entitiesRegistry::value().TryGet<components::Villager>(villagerEntity))
			{
				villager->flock = found->second;
			}
		}
	}
	std::unordered_map<uint32_t, uint32_t> animalsMade;
	std::erase_if(madeAnimals, [&flocksKept, &animalEntities](const auto& made) {
		const auto flock = made.first->living.flock.number;
		if (flock == 0 || flocksKept.contains(flock))
		{
			return false;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: animal {} not made: its flock {} is not", made.first->number, flock);
		Locator::entitiesRegistry::value().Destroy(made.second);
		animalEntities.erase(made.first->number);
		return true;
	});
	for (const auto& [planned, entity] : madeAnimals)
	{
		++animalsMade[planned->recordType];
		keep(planned->number, entity, planned->recordType);
	}
	if (!madeAnimals.empty())
	{
		auto& registry = Locator::entitiesRegistry::value();
		// each town's flocks, as its record lists them
		for (const auto& town : plan.towns)
		{
			const auto made = entities.find(town.number);
			auto* component = made != entities.end() ? registry.TryGet<components::Town>(made->second) : nullptr;
			if (component == nullptr)
			{
				continue;
			}
			component->flocks.clear();
			for (const auto flock : town.record.flocks.numbers)
			{
				if (const auto entity = objectOfMade(flock); entity != entt::null)
				{
					component->flocks.push_back(entity);
				}
			}
		}
		// each animal's town and the things it names, then into the map; the clip of the state it was saved in comes
		// with the resolves that draw, after the seeds
		for (const auto& [planned, entity] : madeAnimals)
		{
			// its flock as its own record names it, its place in the flock's line as the flock's record lists it
			if (auto* animal = registry.TryGet<components::Animal>(entity))
			{
				animal->flock = objectOfMade(planned->living.flock.number);
				animal->town = objectOfMade(planned->animal.object.number);
			}
			if (auto* brain = registry.TryGet<components::AnimalBrain>(entity))
			{
				brain->predator = objectOfMade(planned->living.otherObject.number);
				brain->target = objectOfMade(planned->move.target.number);
			}
			archetypes::AnimalArchetype::PlaceRestored(entity);
		}
	}
	uint32_t creaturesMade = 0;
	// the resolves that draw, the animals' and the creatures', in the order their reads ended
	std::vector<std::pair<uint32_t, entt::entity>> toResolve;
	for (const auto& [planned, entity] : madeAnimals)
	{
		toResolve.emplace_back(planned->readEnd, entity);
	}
	struct CreatureBeam
	{
		gamesave::ObjectNumber number;
		entt::entity creature;
		size_t hand;
	};
	std::vector<CreatureBeam> creatureBeams;
	for (const auto& planned : plan.creatures)
	{
		const auto made = makers.creature ? makers.creature(planned, objectOfMade) : entt::entity {entt::null};
		if (made != entt::null)
		{
			++creaturesMade;
			keep(planned.number, made, gamesave::CreatureRecord::k_Type);
			toResolve.emplace_back(planned.readEnd, made);
			// the beams at its hands from a cast from above, given back once the containers are made
			for (size_t i = 0; i < planned.record.creature.handParticles.size(); ++i)
			{
				if (const auto number = planned.record.creature.handParticles.at(i).number; number != 0)
				{
					creatureBeams.push_back({.number = number, .creature = made, .hand = i});
				}
			}
		}
	}
	// the creatures in their duel, the creature that started it and its opponent: as the game's load leaves them, with no
	// fight of their bodies, their fight is over at once with neither winning, and each responds to it as a fight that
	// ended so does
	for (const auto& planned : plan.creatures)
	{
		const auto& fields = planned.record.creature;
		const auto duel = fields.keepsPlan != 0 ? DuelOf(fields.plan) : std::nullopt;
		const auto self = objectOfMade(planned.number);
		const auto other = duel.has_value() ? objectOfMade(duel->other) : entt::entity {entt::null};
		if (!duel.has_value() || self == entt::null)
		{
			continue;
		}
		const auto found = std::ranges::find(plan.arenas, fields.arena.number, &PlannedArena::number);
		const bool withArena = duel->startedIt && fields.arena.number != 0 && found != plan.arenas.end();
		if (other == entt::null)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Load: creature {} is in a duel with {}, which is not made: it fights no more", planned.number,
			                   duel->other);
			continue;
		}
		components::CreatureFighting fighting {
		    .stage = components::CreatureFighting::Stage::Respond,
		    .opponent = other,
		    .madeArena = duel->startedIt,
		    .ended = true,
		};
		if (withArena)
		{
			const auto& at = found->record.position.position;
			fighting.arena = {.centre = {map_coords::ToMetres(at.x), map_coords::ToMetres(at.z)},
			                  .radius = std::bit_cast<float>(found->record.words[1])};
		}
		fighting.fighter.health = fields.fightHealth;
		fighting.fighter.state = creature_fight::State::Idle;
		// the starter's plan of the fight, which its mind takes up once it is set up
		if (duel->startedIt)
		{
			if (auto* mind = Locator::entitiesRegistry::value().TryGet<components::CreatureMindState>(self))
			{
				mind->loadedFightDesire = static_cast<creature_desires::Desire>(fields.plan.desire);
			}
		}
		fighting.fighter.control = AutoFightControlOf(fields.autoFight);
		auto& registry = Locator::entitiesRegistry::value();
		if (const auto* transform = registry.TryGet<const components::Transform>(self))
		{
			fighting.startPosition = transform->position;
		}
		registry.AssignOrReplaceState<components::CreatureFighting>(self, std::move(fighting));
	}
	std::ranges::stable_sort(toResolve, {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<entt::entity> resolving;
	std::ranges::transform(toResolve, std::back_inserter(resolving), [](const auto& each) { return each.second; });
	if (resolves != nullptr)
	{
		resolves->insert(resolves->end(), resolving.begin(), resolving.end());
	}
	else
	{
		Resolve(resolving, {});
	}

	// the dances, from the list's last to its first, once their dancers and what they are danced about are made; each
	// group, key frame and action by its dance and its number there
	uint32_t dancesMade = 0;
	if (makers.dance)
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		for (const auto& planned : plan.dances | std::views::reverse)
		{
			const auto made = makers.dance(planned, objectOf);
			if (made == entt::null)
			{
				continue;
			}
			++dancesMade;
			keep(planned.number, made, gamesave::DanceRecord::k_Type);
			for (size_t group = 0; group < planned.groups.size(); ++group)
			{
				keepSource(planned.groups[group].first, gamesave::DanceGroupRecord::k_Type,
				           saved_world_write::PartId(made, static_cast<uint32_t>(group)));
			}
			const auto& frames = planned.record.group.keyFrames.numbers;
			for (size_t frame = 0; frame < frames.size(); ++frame)
			{
				keepSource(frames[frame], gamesave::DanceKeyFrameRecord::k_Type,
				           saved_world_write::PartId(made, static_cast<uint32_t>(frame)));
				const auto* record = plan.keyFrames.contains(frames[frame]) ? &plan.keyFrames.at(frames[frame]) : nullptr;
				for (size_t action = 0; record != nullptr && action < record->actions.numbers.size(); ++action)
				{
					keepSource(record->actions.numbers[action], gamesave::DanceKeyActionRecord::k_Type,
					           saved_world_write::PartId(made, static_cast<uint32_t>((frame << k_KeyFrameShift) | action)));
				}
			}
		}
	}

	// the fireballs' balls, from the list's last to its first, so that the newest is at the list's head again, before
	// the fires that burn them; the effect whose atom carries each is given to it once the effects are made
	std::vector<std::pair<const PlannedFireBall*, entt::entity>> madeFireBalls;
	if (makers.fireBall)
	{
		for (const auto& ball : plan.fireBalls | std::views::reverse)
		{
			const auto made = makers.fireBall(ball);
			keep(ball.number, made, gamesave::MagicFireBallRecord::k_Type);
			if (made != entt::null)
			{
				madeFireBalls.emplace_back(&ball, made);
			}
		}
	}

	// the fires, once every object they can burn is made, each fire's id by its number
	uint32_t firesMade = 0;
	std::unordered_map<gamesave::ObjectNumber, uint32_t> fireOf;
	if (makers.fires && !plan.fires.empty())
	{
		const auto objectOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		const auto ids = makers.fires(plan.fires, plan.fireTags, objectOf);
		fireOf.clear();
		for (size_t i = 0; i < ids.size() && i < plan.fires.size(); ++i)
		{
			if (ids[i] != 0)
			{
				++firesMade;
				keepSource(plan.fires[i].number, gamesave::FireEffectRecord::k_Type, ids[i]);
				fireOf.emplace(plan.fires[i].number, ids[i]);
			}
		}
		// each villager by a fire flees or fights its fire again, as its state's values name it
		for (const auto& [planned, entity] : madeVillagers)
		{
			const auto& values = std::visit([](const auto& record) -> const gamesave::VillagerValues& { return record.values; },
			                                planned->record);
			for (const auto& [state, saved] : {std::pair {values.living.action[0], &values.topState},
			                                   std::pair {values.living.action[1], &values.finalState}})
			{
				constexpr size_t k_FireValue = 1;
				if (gamesave::VillagerStateSaveOf(state) != gamesave::VillagerStateSave::Fire || saved->size() <= k_FireValue)
				{
					continue;
				}
				if (const auto found = fireOf.find(saved->at(k_FireValue).number); found != fireOf.end())
				{
					auto& registry = Locator::entitiesRegistry::value();
					auto& onFire = registry.AllOf<components::VillagerFireState>(entity)
					                   ? registry.Get<components::VillagerFireState>(entity)
					                   : registry.Assign<components::VillagerFireState>(entity);
					onFire.fire = found->second;
				}
			}
		}
	}

	// the fireflies, the list's head first, each at the list's tail as the load reads it, kept by its creation number
	// for the writer; then the reward table and the top-up flag the last one read left
	uint32_t fireFliesMade = 0;
	if (makers.fireFly)
	{
		for (const auto& [number, record] : plan.fireFlies)
		{
			const auto saved = SavedFireFlyOf(record);
			makers.fireFly(saved);
			keepSource(number, gamesave::FireFlyRecord::k_Type, saved.creationIndex);
			++fireFliesMade;
		}
		if (plan.lastFireFly.has_value() && makers.fireFlyRewards)
		{
			makers.fireFlyRewards(*plan.lastFireFly);
		}
	}

	// the vortices, each with its crossing file's save as the source the writer names it by
	std::unordered_map<uint32_t, uint32_t> vorticesMade;
	const auto vortexObjectOf = [&entities, &animalEntities](gamesave::ObjectNumber number) {
		if (const auto found = number != 0 ? entities.find(number) : entities.end(); found != entities.end())
		{
			return found->second;
		}
		const auto animal = animalEntities.find(number);
		return animal != animalEntities.end() ? animal->second : entt::entity {entt::null};
	};
	for (const auto& vortex : plan.vortices)
	{
		const auto made = makers.vortex ? makers.vortex(vortex, vortexObjectOf) : entt::entity {entt::null};
		if (made == entt::null)
		{
			continue;
		}
		++vorticesMade[typeOf(vortex.record)];
		keep(vortex.number, made, typeOf(vortex.record));
		// a way in's object mover made again: its interface and manager records kept, so a write gives back what
		// openblack does not hold (the manager's point +0x78, its byte +0xAD, its targets' points and words) as read
		if (const auto* landscape = Locator::entitiesRegistry::value().TryGet<const components::LandscapeVortex>(made);
		    vortex.mover.has_value() && landscape != nullptr && landscape->moverLoaded)
		{
			keepSource(vortex.mover->interfaceNumber, gamesave::ParticleInterfaceRecord::k_Type, landscape->objectMover);
			keepSource(vortex.mover->managerNumber, gamesave::ParticleManagerRecord::k_Type, landscape->objectMover);
		}
		const auto saver = std::visit(
		    [](const auto& record) -> gamesave::ObjectNumber {
			    if constexpr (requires { record.saver; })
			    {
				    return record.saver.number;
			    }
			    else if constexpr (requires { record.objects; })
			    {
				    return record.objects[0].number;
			    }
			    else
			    {
				    return 0;
			    }
		    },
		    vortex.record);
		if (saver != 0)
		{
			keepSource(saver, gamesave::VortexSaveRecord::k_Type, saved_world_write::PartId(made, 0));
		}
	}
	// the spells' seeds, once their icons and creators are made
	uint32_t spellSeedsMade = 0;
	const auto madeEntityOf = [&entities](gamesave::ObjectNumber number) {
		const auto found = number != 0 ? entities.find(number) : entities.end();
		return found != entities.end() ? found->second : entt::entity {entt::null};
	};
	for (const auto& seed : plan.spellSeeds)
	{
		const auto made = makers.spellSeed ? makers.spellSeed(seed, madeEntityOf(seed.record.objects[k_SeedIcon].number),
		                                                      madeEntityOf(seed.record.objects[k_SeedCreator].number))
		                                   : entt::entity {entt::null};
		spellSeedsMade += made != entt::null ? 1 : 0;
		keep(seed.number, made, gamesave::SpellSeedRecord::k_Type);
	}
	// the reward chests, once their towns are made, the oldest first
	for (const auto& reward : plan.rewards)
	{
		const auto made = makers.reward ? makers.reward(reward, madeEntityOf(reward.record.objects[k_RewardTown].number))
		                                : entt::entity {entt::null};
		keep(reward.number, made, gamesave::RewardRecord::k_Type);
	}
	// the footballs, once their towns are made, the oldest first
	for (const auto& ball : plan.balls)
	{
		const auto made = makers.ball ? makers.ball(ball, madeEntityOf(ball.record.town.number)) : entt::entity {entt::null};
		keep(ball.number, made, gamesave::BallRecord::k_Type);
	}
	// each town's things to play with, once the footballs are made, in its record's order; one the load did not make
	// is left out
	for (const auto& town : plan.towns)
	{
		const auto made = madeEntityOf(town.number);
		auto* component = made != entt::null ? Locator::entitiesRegistry::value().TryGet<components::Town>(made) : nullptr;
		if (component == nullptr)
		{
			continue;
		}
		component->playthings.clear();
		for (const auto number : town.record.playthings.numbers)
		{
			if (const auto thing = madeEntityOf(number); thing != entt::null)
			{
				component->playthings.push_back(thing);
			}
		}
	}
	// the thing each villager walks to, once everything it can walk to is made
	for (const auto& [planned, entity] : madeVillagers)
	{
		const auto target = std::visit([](const auto& record) { return record.values.move.target.number; }, planned->record);
		if (auto* villager = Locator::entitiesRegistry::value().TryGet<components::Villager>(entity); villager != nullptr)
		{
			villager->walkTarget = objectOfMade(target);
		}
	}
	// the puzzle games, once every object they name is made, the oldest first
	uint32_t puzzleGamesMade = 0;
	for (const auto& puzzle : plan.puzzleGames)
	{
		const auto made = makers.puzzleGame ? makers.puzzleGame(puzzle, madeEntityOf) : entt::entity {entt::null};
		puzzleGamesMade += made != entt::null ? 1 : 0;
		keep(puzzle.number, made, gamesave::PuzzleGameRecord::k_Type);
	}
	// the influence rings, once every object they can follow is made, from the list's last to its first
	uint32_t ringsMade = 0;
	for (const auto& ring : plan.rings | std::views::reverse)
	{
		const auto found = entities.find(ring.record.attached.number);
		const auto attached =
		    ring.record.attached.number != 0 && found != entities.end() ? found->second : entt::entity {entt::null};
		const auto made = makers.influenceRing ? makers.influenceRing(ring, attached) : entt::entity {entt::null};
		ringsMade += made != entt::null ? 1 : 0;
		keep(ring.number, made, gamesave::InfluenceRingRecord::k_Type);
	}

	// each spell dispenser's orb, once the orbs are made
	for (const auto& object : plan.objects)
	{
		const auto* abode = std::get_if<PlannedAbode>(&object);
		const auto* record = abode != nullptr ? std::get_if<gamesave::SpellDispenserRecord>(&abode->record) : nullptr;
		const auto dispenser = record != nullptr ? entities.find(abode->number) : entities.end();
		const auto orb =
		    record != nullptr && record->object.number != 0 ? entities.find(record->object.number) : entities.end();
		if (dispenser != entities.end() && orb != entities.end())
		{
			if (auto* component = Locator::entitiesRegistry::value().TryGet<components::SpellDispenser>(dispenser->second))
			{
				component->orb = orb->second;
			}
		}
	}

	// each villager's script memory, from the reminder its living part names, once every thing its walk can follow is
	// made
	uint32_t scriptRemindersMade = 0;
	for (const auto& [planned, villager] : madeVillagers)
	{
		const auto& living = std::visit(
		    [](const auto& record) -> const gamesave::LivingFields& { return record.values.living; }, planned->record);
		const auto reminder = plan.scriptReminders.find(living.scriptReminder.number);
		if (living.scriptReminder.number == 0 || reminder == plan.scriptReminders.end())
		{
			continue;
		}
		const auto target = entities.find(reminder->second.object.number);
		script_memory::Make(villager) = ScriptMemoryOf(
		    reminder->second, reminder->second.object.number != 0 && target != entities.end() ? target->second : entt::null);
		// for the next save's writer: the villager's script memory stands for this reminder
		keepSource(reminder->first, gamesave::ScriptReminderRecord::k_Type, saved_world_write::PartId(villager, 0));
		++scriptRemindersMade;
	}
	// each animal's, the same way: nothing but a save reads an animal's, so it is kept to be written again
	for (const auto& [planned, animal] : madeAnimals)
	{
		const auto reminder = plan.scriptReminders.find(planned->living.scriptReminder.number);
		if (planned->living.scriptReminder.number == 0 || reminder == plan.scriptReminders.end())
		{
			continue;
		}
		const auto target = entities.find(reminder->second.object.number);
		script_memory::Make(animal) = ScriptMemoryOf(
		    reminder->second, reminder->second.object.number != 0 && target != entities.end() ? target->second : entt::null);
		keepSource(reminder->first, gamesave::ScriptReminderRecord::k_Type, saved_world_write::PartId(animal, 0));
		++scriptRemindersMade;
	}

	// the teleports' stones and the physical shields, then the spells, once their creators, seeds, rings and the things
	// their effects follow are made, before the reactions they started, which name them as their initiators
	// (census) an effect made again (its manager kept) took its every part with it, its collections, atoms and their
	// data: each record of them counts as made, once
	const auto creditParts = [&sourcesKept, &keptByType](const saved_particles::PlannedEffect& effect) {
		if (!sourcesKept.contains(effect.managerNumber))
		{
			return;
		}
		for (const auto& [number, record] : effect.parts)
		{
			if (sourcesKept.insert(number).second)
			{
				++keptByType[gamesave::TypeOf(record)];
			}
		}
	};
	// a way in's object mover made again took its parts with it: its collection, its carriers, their data and the things
	// they carry
	for (const auto& vortex : plan.vortices)
	{
		if (vortex.mover.has_value())
		{
			creditParts(*vortex.mover);
		}
	}
	saved_spells::MadeSpells shieldsMade;
	saved_spells::MadeSpells spellsMade;
	saved_spells::MadeSpells stonesMade;
	if (Locator::particleSystem::has_value())
	{
		stonesMade = saved_spells::MakeTeleportStones(plan.teleportStones, keep);
		shieldsMade = saved_spells::MakePhysicalShields(plan.physicalShields, {.entityOf = madeEntityOf}, keepSource, keep,
		                                                plan.spiritualShields);
		spellsMade = saved_spells::MakeSpells(plan.spells, {.entityOf = madeEntityOf, .forestOf = keyOf}, keepSource, keep);
		for (const auto& planned : plan.physicalShields)
		{
			if (planned.effect.has_value())
			{
				creditParts(*planned.effect);
			}
		}
		for (const auto& planned : plan.spells)
		{
			if (planned.effect.has_value())
			{
				creditParts(*planned.effect);
			}
		}
	}
	// each creature's spell slots, the miracles they name
	if (Locator::entitiesRegistry::has_value())
	{
		LinkSpellSlots(Locator::entitiesRegistry::value(), spellsMade);
	}

	// the reactions, from the list's last to its first, so that the newest is the newest again, each with what its
	// initiator and its dance are now
	uint32_t reactionsMade = 0;
	std::unordered_map<gamesave::ObjectNumber, uint32_t> reactionIds;
	for (const auto& reaction : plan.reactions | std::views::reverse)
	{
		const auto entityOf = [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		};
		// one an interface status started is the hand's: the local player's hand
		auto initiator = entityOf(reaction.record.object.number);
		if (reaction.statusPlayer == PlayerNames::PLAYER_ONE && Locator::handSystem::has_value())
		{
			initiator =
			    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(systems::HandSystemInterface::Side::Left)];
		}
		const auto id = makers.reaction ? makers.reaction(reaction, initiator, entityOf(reaction.record.second.number)) : 0U;
		if (id != 0)
		{
			++reactionsMade;
			keepSource(reaction.number, gamesave::ReactionRecord::k_Type, id);
			reactionIds.emplace(reaction.number, id);
		}
	}
	// each spell made, its reaction by the id it was made again with
	saved_spells::LinkReactions(plan.spells, spellsMade, reactionIds);
	// each stone, its spell and the reaction it spreads
	saved_spells::LinkTeleportStones(plan.teleportStones, stonesMade, spellsMade, reactionIds);
	// each villager's reaction, by the id its reaction was made again with, and what it reacts to; the followers of a
	// reaction joined it in the order its list holds them from the last, the newest first
	{
		std::unordered_map<gamesave::ObjectNumber, size_t> followerPlace;
		for (const auto& reaction : plan.reactions)
		{
			const auto& livings = reaction.record.livings.numbers;
			for (size_t at = 0; at < livings.size(); ++at)
			{
				followerPlace[livings[at]] = livings.size() - at;
			}
		}
		std::vector<std::tuple<size_t, gamesave::ObjectNumber, entt::entity, const PlannedVillager*>> following;
		for (const auto& [planned, entity] : madeVillagers)
		{
			const auto& living = std::visit(
			    [](const auto& record) -> const gamesave::LivingFields& { return record.values.living; }, planned->record);
			if (living.reaction.number != 0)
			{
				const auto place = followerPlace.find(planned->number);
				following.emplace_back(place != followerPlace.end() ? place->second : 0, living.reaction.number, entity,
				                       planned);
			}
		}
		std::ranges::sort(following, {}, [](const auto& entry) { return std::get<0>(entry); });
		for (const auto& [place, reactionNumber, entity, planned] : following)
		{
			const auto id = reactionIds.find(reactionNumber);
			auto* made = Locator::reactionSystem::has_value() && id != reactionIds.end()
			                 ? Locator::reactionSystem::value().Find(id->second)
			                 : nullptr;
			if (made == nullptr)
			{
				continue;
			}
			const auto& living = std::visit(
			    [](const auto& record) -> const gamesave::LivingFields& { return record.values.living; }, planned->record);
			const auto found = entities.find(living.otherObject.number);
			Locator::entitiesRegistry::value().AssignOrReplace<components::LivingReaction>(
			    entity, components::LivingReaction {.reaction = id->second,
			                                        .type = made->type,
			                                        .object = found != entities.end() ? found->second : entt::null,
			                                        .joinOrder = Locator::reactionSystem::value().NextJoinOrder()});
		}
	}
	// each fire's reaction, by the id the reaction its record names was made again with, as the fire's load reads it
	GiveFiresTheirReactions(plan.fires, fireOf, reactionIds);
	// each animal's reaction, by the id the reaction its record names was made again with
	for (const auto& [planned, entity] : madeAnimals)
	{
		auto* brain = Locator::entitiesRegistry::value().TryGet<components::AnimalBrain>(entity);
		const auto id = reactionIds.find(planned->living.reaction.number);
		if (brain != nullptr && id != reactionIds.end())
		{
			brain->reaction = id->second;
		}
	}

	// a pile in play saved made to start its reaction with none started starts it again, as the game's load does as it
	// resolves the pile, after every reaction the save kept
	if (makers.pileReaction)
	{
		for (const auto number : plan.pileReactions)
		{
			if (const auto found = entities.find(number); found != entities.end())
			{
				makers.pileReaction(found->second);
			}
		}
	}

	// a thing in a hand is made as any other, then taken off the map, as the game's load does with a thing that is
	// unavailable; the hand's own load puts it in the hand
	const auto madeOf = [&entities, &animalEntities](gamesave::ObjectNumber number) {
		if (const auto found = entities.find(number); found != entities.end())
		{
			return found->second;
		}
		const auto animal = animalEntities.find(number);
		return animal != animalEntities.end() ? animal->second : entt::entity {entt::null};
	};
	for (const auto number : plan.inHand)
	{
		if (const auto made = madeOf(number);
		    made != entt::null && Locator::mapCellsSystem::has_value() && map_cells::IsObjectInMap(made))
		{
			map_cells::RemoveMapObject(made);
		}
	}
	// what the physics moved goes back into it standing still, then takes its saved body: its rows and centre, its
	// velocity and its angular momentum
	if (Locator::dynamicsSystem::has_value())
	{
		auto& dynamics = Locator::dynamicsSystem::value();
		for (const auto& [number, saved] : plan.inPhysics)
		{
			const auto made = madeOf(number);
			auto* entry = made != entt::null ? dynamics.AddObject(made, glm::vec3(0.0f), glm::vec3(0.0f)) : nullptr;
			if (entry == nullptr)
			{
				continue;
			}
			const auto& m = saved.matrix;
			entry->body.RestorePose(glm::mat3(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]),
			                        glm::vec3(m[9], m[10], m[11]));
			entry->body.velocity = glm::vec3(saved.velocity[0], saved.velocity[1], saved.velocity[2]);
			entry->body.angularMomentum = glm::vec3(saved.spin[0], saved.spin[1], saved.spin[2]);
			dynamics.SyncTurnStart(*entry);
		}
	}

	// the music attached to things, once every object is made
	uint32_t thingMusicMade = 0;
	if (makers.thingMusic)
	{
		thingMusicMade = makers.thingMusic(plan.thingMusic, [&entities](gamesave::ObjectNumber number) {
			const auto found = entities.find(number);
			return found != entities.end() ? found->second : entt::entity {entt::null};
		});
		// each one put back keeps its number as its key, for the next save's writer
		for (const auto& [number, record] : plan.thingMusic)
		{
			if (record.object.number != 0 && entities.contains(record.object.number))
			{
				keepSource(number, gamesave::ThingMusicInfoRecord::k_Type, number);
			}
		}
	}
	if (makers.scriptMusic && plan.scriptMusic.has_value())
	{
		const auto town = towns.find(plan.scriptMusic->town);
		makers.scriptMusic(*plan.scriptMusic, town != towns.end() ? town->second : entt::entity {entt::null});
	}
	// the towns' artifacts, once their objects, towns and dances are made, each on its object, as its load reads it: its
	// town, its owner, its worth, its dance, whether that has started and the turn it did; its site is not saved (none)
	std::vector<const PlannedArtifact*> madeArtifacts;
	if (Locator::artefactSystem::has_value())
	{
		for (const auto& planned : plan.artifacts)
		{
			const auto object = madeOf(planned.record.objects[0].number);
			if (object == entt::null)
			{
				continue;
			}
			const auto& record = planned.record;
			Locator::artefactSystem::value().Restore(object,
			                                         components::TownArtefact {.town = madeOf(record.objects[1].number),
			                                                                   .inTown = planned.inTown,
			                                                                   .joined = planned.joined,
			                                                                   .made = planned.made,
			                                                                   .player = planned.player,
			                                                                   .value = std::bit_cast<float>(record.word),
			                                                                   .dance = madeOf(record.others[0].number),
			                                                                   .dancing = record.words[0] != 0,
			                                                                   .danceStarted = record.words[1]});
			keep(planned.number, object, gamesave::TownArtifactRecord::k_Type);
			madeArtifacts.push_back(&planned);
		}
	}
	// the spot visuals' containers with their effects, once the things they hang on are made; a pile's sparkle is its
	// container
	if (Locator::particleSystem::has_value())
	{
		const auto containers = saved_particles::MakeContainers(plan.containers, {.entityOf = madeOf}, keepSource, keep);
		// each artifact's glow, its container made again, or started as its load's resolve starts one when it is worth it
		for (const auto* planned : madeArtifacts)
		{
			const auto object = madeOf(planned->record.objects[0].number);
			auto* artefact = Locator::entitiesRegistry::value().TryGet<components::TownArtefact>(object);
			if (artefact == nullptr)
			{
				continue;
			}
			if (const auto found = containers.find(planned->record.others[1].number); found != containers.end())
			{
				artefact->effect = found->second;
			}
			Locator::artefactSystem::value().ResolveLoad(object);
		}
		// each spell's blast takes its beam's spot visual again, once the containers are made
		for (const auto& [number, spell] : spellsMade)
		{
			const auto* values = Locator::entitiesRegistry::value().TryGet<const components::Spell>(spell);
			if (auto* effect = values != nullptr ? Locator::particleSystem::value().Find(values->effect) : nullptr;
			    effect != nullptr)
			{
				particles::explosion::LinkLoadedFx(*effect, madeOf);
			}
		}
		for (const auto& planned : plan.containers)
		{
			creditParts(planned.effect);
		}
		// each worship site's mana path, its particles made again from what the save kept and given to the site, as its
		// effect goes on; one that cannot be made is left for the site to start again, with one log line
		for (const auto& [planned, site] : madeSites)
		{
			if (!planned->manaPath.has_value())
			{
				continue;
			}
			const auto& effect = *planned->manaPath;
			const auto type = static_cast<ParticleType>(effect.manager.persistent.first);
			const auto fileName = particles::ParticleTypeFile(type);
			std::string why = fileName.empty() ? std::string("its particle type has no file") : std::string();
			auto made = fileName.empty() ? nullptr
			                             : saved_particles::RestoreEffect(effect, particles::File::Load(std::string(fileName)),
			                                                              {.entityOf = madeOf}, why);
			auto* component = Locator::entitiesRegistry::value().TryGet<components::WorshipSite>(site);
			if (made == nullptr || component == nullptr)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: worship site {}'s mana path is not made: {}", planned->number,
				                   why);
				continue;
			}
			component->manaPathEffect = particles::manager::AdoptForSpell(std::move(made), nullptr);
			keepSource(effect.interfaceNumber, gamesave::ParticleInterfaceRecord::k_Type, component->manaPathEffect);
			keepSource(effect.managerNumber, gamesave::ParticleManagerRecord::k_Type, component->manaPathEffect);
			creditParts(effect);
		}
		// the particles of the spell in the hand, once its seed is made, given to the hand's effects
		if (plan.handEffect.has_value() && makers.handEffect)
		{
			const auto& planned = *plan.handEffect;
			const auto type = static_cast<ParticleType>(planned.effect.manager.persistent.first);
			const auto fileName = particles::ParticleTypeFile(type);
			std::string why;
			auto effect = fileName.empty()
			                  ? nullptr
			                  : saved_particles::RestoreEffect(planned.effect, particles::File::Load(std::string(fileName)),
			                                                   {.entityOf = madeOf}, why);
			const auto id =
			    effect != nullptr ? makers.handEffect(std::move(effect), madeOf(planned.seed), type, planned.atBone) : 0;
			if (id != 0)
			{
				keepSource(planned.effect.interfaceNumber, gamesave::ParticleInterfaceRecord::k_Type, id);
				keepSource(planned.effect.managerNumber, gamesave::ParticleManagerRecord::k_Type, id);
				creditParts(planned.effect);
			}
			else if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Load: the particles of the spell in the hand are kept as read: {}",
				                   fileName.empty() ? std::string("their particle type has no file") : why);
			}
		}
		for (const auto& [number, pile] : pileSparkles)
		{
			auto* pot = Locator::entitiesRegistry::value().TryGet<components::Pot>(pile);
			if (const auto found = containers.find(number); pot != nullptr && found != containers.end())
			{
				pot->speedUpVisual = found->second;
			}
		}
		// a creature's beams still close down and follow its hands each turn, as in play once a cast has let go
		for (const auto& beam : creatureBeams)
		{
			if (const auto found = containers.find(beam.number); found != containers.end())
			{
				auto& registry = Locator::entitiesRegistry::value();
				auto* existing = registry.TryGet<components::CreatureCasting>(beam.creature);
				auto& casting =
				    existing != nullptr ? *existing : registry.AssignState<components::CreatureCasting>(beam.creature);
				casting.beams.at(beam.hand) = found->second;
			}
		}
	}

	// each lightning made again linked with the bolts its saved links name, once every effect is made
	particles::lightning::LinkLoadedBolts();
	// each fireball's ball, the effect whose atom carries it, once the effects are made again: the effect its manager
	// names
	for (const auto& [ball, entity] : madeFireBalls)
	{
		if (const auto found = managersMade.find(ball->record.manager.number); found != managersMade.end())
		{
			magic::fireball::RestoreEffect(entity, static_cast<uint32_t>(found->second));
		}
	}

	// the player each food and wood pile answers with, as the load's resolve sets it once every object is made: one
	// part of a structure its structure's player now; any other the player it was saved with, which it answers with
	// from then on
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& entry : piles)
		{
			const auto pile = entry.second;
			auto* pot = registry.Valid(pile) ? registry.TryGet<components::Pot>(pile) : nullptr;
			if (pot == nullptr || !registry.AnyOf<components::LoadedRecord<gamesave::FoodPileRecord>,
			                                      components::LoadedRecord<gamesave::WoodPileRecord>>(pile))
			{
				continue;
			}
			if (resource_stores::StructureOfPile(pile) != entt::null)
			{
				pot->playerWhenMade = object_resources::PlayerOfPile(pile);
			}
			else
			{
				pot->owner = pot->playerWhenMade;
			}
		}
	}

	// the creation draws, by the order the reads of the objects they belong to ended
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto taken = creation_draws::EndLoad(
		    registry.Context().creationDraws,
		    [&plan, &numberOfEntity](entt::entity owner) -> std::optional<size_t> {
			    const auto number = numberOfEntity.find(owner);
			    if (number == numberOfEntity.end())
			    {
				    return std::nullopt;
			    }
			    const auto rank = plan.readEnds.find(number->second);
			    return rank != plan.readEnds.end() ? std::optional(rank->second) : std::nullopt;
		    },
		    [&registry](entt::entity owner) { return registry.Valid(owner); });
		// (debug readout) the draws the load held back, how many of them belong to a saved record, and how many were drawn
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: {} creation draws held, {} by a saved record's read, {} drawn",
		                   taken.held, taken.ranked, taken.drawn);
	}

	Census census;
	for (const auto& [type, saved] : plan.census)
	{
		uint32_t made = 0;
		if (type == gamesave::TreeRecord::k_Type)
		{
			made = treesMade;
		}
		else if (type == gamesave::ForestRecord::k_Type)
		{
			made = static_cast<uint32_t>(forestKeys.size());
		}
		else if (type == gamesave::BigForestRecord::k_Type)
		{
			made = static_cast<uint32_t>(bigForests.size());
		}
		else if (type == gamesave::TownRecord::k_Type)
		{
			made = static_cast<uint32_t>(towns.size());
		}
		else if (type == gamesave::StoragePitRecord::k_Type)
		{
			made = static_cast<uint32_t>(storagePits.size());
		}
		else if (type == gamesave::ClimateRecord::k_Type)
		{
			made = climatesMade;
		}
		else if (type == gamesave::StreamRecord::k_Type)
		{
			made = streamsMade;
		}
		else if (type == gamesave::WeatherThingRecord::k_Type)
		{
			made = weatherThingsMade;
		}
		else if (type == gamesave::ArenaRecord::k_Type)
		{
			made = arenasMade;
		}
		else if (type == gamesave::ScriptHighlightRecord::k_Type)
		{
			made = highlightsMade;
		}
		else if (type == gamesave::ScriptMarkerRecord::k_Type)
		{
			made = markersMade;
		}
		else if (type == gamesave::ScriptTimerRecord::k_Type)
		{
			made = timersMade;
		}
		else if (type == gamesave::WaypointRecord::k_Type)
		{
			made = waypointsMade;
		}
		else if (type == gamesave::InfluenceRingRecord::k_Type)
		{
			made = ringsMade;
		}
		else if (type == gamesave::FishFarmRecord::k_Type)
		{
			made = fishFarmsMade;
		}
		else if (type == gamesave::ReactionRecord::k_Type)
		{
			made = reactionsMade;
		}
		else if (type == gamesave::MistRecord::k_Type)
		{
			made = mistsMade;
		}
		else if (type == gamesave::TownDesireFlagsRecord::k_Type)
		{
			made = desireFlagsMade;
		}
		else if (abodesMade.contains(type))
		{
			made = abodesMade.at(type);
		}
		else if (mobileStaticsMade.contains(type))
		{
			made = mobileStaticsMade.at(type);
		}
		else if (type == gamesave::AnimatedStaticRecord::k_Type)
		{
			made = animatedStaticsMade;
		}
		else if (mobileObjectsMade.contains(type))
		{
			made = mobileObjectsMade.at(type);
		}
		else if (type == gamesave::ScriptReminderRecord::k_Type)
		{
			made = scriptRemindersMade;
		}
		else if (type == gamesave::SpellSeedRecord::k_Type)
		{
			made = spellSeedsMade;
		}
		else if (type == gamesave::SpellWaterRecord::k_Type || type == gamesave::SpellShieldRecord::k_Type)
		{
			made = static_cast<uint32_t>(std::ranges::count_if(plan.spells, [&](const auto& planned) {
				return spellsMade.contains(planned.number) &&
				       std::visit([](const auto& record) { return std::decay_t<decltype(record)>::k_Type; }, planned.record) ==
				           type;
			}));
		}
		else if (type == gamesave::PhysicalShieldRecord::k_Type)
		{
			made = static_cast<uint32_t>(std::ranges::count_if(
			    plan.physicalShields, [&shieldsMade](const auto& planned) { return shieldsMade.contains(planned.number); }));
		}
		else if (type == gamesave::SpiritualShieldRecord::k_Type)
		{
			made = static_cast<uint32_t>(std::ranges::count_if(
			    plan.spiritualShields, [&shieldsMade](const auto& planned) { return shieldsMade.contains(planned.number); }));
		}
		else if (type == gamesave::MagicTeleportRecord::k_Type)
		{
			made = static_cast<uint32_t>(stonesMade.size());
		}
		else if (type == gamesave::HanoiBlockRecord::k_Type)
		{
			made = hanoiBlocksMade;
		}
		else if (type == gamesave::PuzzleGameRecord::k_Type)
		{
			made = puzzleGamesMade;
		}
		else if (type == gamesave::MagicFoodRecord::k_Type)
		{
			made = magicFoodsMade;
		}
		else if (type == gamesave::FeatureRecord::k_Type)
		{
			made = featuresMade;
		}
		else if (type == gamesave::StreetLanternRecord::k_Type)
		{
			made = lanternsMade;
		}
		else if (buildingSitesMade.contains(type))
		{
			made = buildingSitesMade.at(type);
		}
		else if (type == gamesave::TotemStatueRecord::k_Type)
		{
			made = totemStatuesMade;
		}
		else if (type == gamesave::TownCentreSpellIconRecord::k_Type)
		{
			made = townCentreIconsMade;
		}
		else if (vorticesMade.contains(type))
		{
			made = vorticesMade.at(type);
		}
		else if (type == gamesave::FireEffectRecord::k_Type)
		{
			made = firesMade;
		}
		else if (type == gamesave::FireFlyRecord::k_Type)
		{
			made = fireFliesMade;
		}
		else if (villagersMade.contains(type))
		{
			made = villagersMade.at(type);
		}
		else if (type == gamesave::FlockRecord::k_Type)
		{
			made = flocksMade;
		}
		else if (animalsMade.contains(type))
		{
			made = animalsMade.at(type);
		}
		else if (type == gamesave::CreatureRecord::k_Type)
		{
			made = creaturesMade;
		}
		else if (type == gamesave::DanceRecord::k_Type)
		{
			made = dancesMade;
		}
		else if (type == gamesave::ThingMusicInfoRecord::k_Type)
		{
			made = thingMusicMade;
		}
		else if (type == gamesave::PlannedAbodeRecord::k_Type)
		{
			made = plannedAbodesMade;
		}
		else if (type == gamesave::PlannedTownCentreRecord::k_Type)
		{
			made = plannedCentresMade;
		}
		else if (type == gamesave::PlannedCitadelHeartRecord::k_Type)
		{
			made = plannedHeartsMade;
		}
		else if (type == gamesave::WorshipSiteRecord::k_Type)
		{
			made = sitesMade;
		}
		else if (type == gamesave::CitadelHeartRecord::k_Type)
		{
			made = static_cast<uint32_t>(hearts.size());
		}
		else if (type == gamesave::CitadelEntranceRecord::k_Type)
		{
			made = entrancesMade;
		}
		else if (type == gamesave::CitadelRecord::k_Type)
		{
			made = static_cast<uint32_t>(citadels.size());
		}
		else if (type == gamesave::FootpathRecord::k_Type)
		{
			made = static_cast<uint32_t>(footpathsMade.size());
		}
		else if (type == gamesave::FootpathLinkRecord::k_Type)
		{
			made = static_cast<uint32_t>(linksMade.size());
		}
		else if (type == gamesave::WoodPileRecord::k_Type || type == gamesave::FoodPileRecord::k_Type ||
		         type == gamesave::MagicWoodRecord::k_Type || type == gamesave::PotRecord::k_Type)
		{
			made = static_cast<uint32_t>(std::ranges::count_if(plan.objects, [&piles, type](const PlannedObject& object) {
				const auto* pile = std::get_if<PlannedPile>(&object);
				const auto pileType =
				    pile == nullptr
				        ? 0U
				        : std::visit([](const auto& saved) { return std::decay_t<decltype(saved)>::k_Type; }, pile->record);
				return pile != nullptr && pileType == type && piles.contains(pile->number);
			}));
		}
		else if (const auto found = keptByType.find(type); found != keptByType.end())
		{
			// a class with no count of its own: the records its entities stand for
			made = found->second;
		}
		census.push_back({.type = type, .saved = saved, .made = made});
	}
	return census;
}

entt::entity saved_world_load::MakeTreeEntity(const PlannedTree& tree, uint32_t forestId)
{
	const auto& record = tree.record;
	const auto& object = record.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::TreeArchetype::Create(
	    forestId, position, tree.type, false, object.heading, record.secondValue, object.scale,
	    {.growing = false, .counter = static_cast<uint16_t>(record.firstValue)});
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeBigForestEntity(const PlannedBigForest& bigForest, uint32_t forestId)
{
	const auto& record = bigForest.record;
	const auto& object = record.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	// the word it keeps after its forest is its wood, a float
	const auto entity = archetypes::BigForestArchetype::Create(position, bigForest.type, object.heading, object.scale, forestId,
	                                                           std::bit_cast<float>(record.word));
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeTownEntity(const PlannedTown& town)
{
	const auto& record = town.record;
	auto& registry = Locator::entitiesRegistry::value();
	// openblack keeps one town per key: a saved id another town has takes the next free key, and keeps the id as the
	// number the scripts find it by
	bool taken = false;
	uint32_t next = 0;
	registry.Each<const components::Town>([&](entt::entity, const components::Town& other) {
		taken = taken || other.id == record.id;
		next = std::max(next, other.id + 1);
	});
	const auto key = taken ? next : record.id;
	const auto owner = record.playerNumber < static_cast<uint8_t>(PlayerNames::_COUNT)
	                       ? static_cast<PlayerNames>(record.playerNumber)
	                       : PlayerNames::NEUTRAL;
	const auto tribe = record.tribe < static_cast<uint32_t>(Tribe::_COUNT) ? static_cast<Tribe>(record.tribe) : Tribe::NONE;
	const auto position = map_coords::ToWorld(CoordsOf(record.container.position.position));
	const auto entity = archetypes::TownArchetype::CreateWithoutDesireFlags(static_cast<int>(key), position, owner, tribe);
	auto& component = registry.Get<components::Town>(entity);
	if (taken)
	{
		component.scriptId = record.id;
	}
	component.belief.beliefInNeutralPlayer = record.beliefInNeutralPlayer;
	// its belief in each player and the rest of its belief as saved, and the scale of its pending belief: a town left
	// with none would go to the strongest player, the neutral one, at its first fold
	ReadBeliefBlock(record.belief, component.belief);
	component.belief.beliefScale = record.balanceBeliefScale;
	// its counts, its deaths, the food and wood it used and its injured, as its figures block holds them
	ReadFiguresBlock(record.stats, component, registry.AssignOrReplace<components::TownDeaths>(entity));
	// the countdown to its being empty, as saved (running once its last villager left)
	component.emptyCountdown = record.emptyTownTimer;
	// the magic types it holds, as saved; its player's own record holds which of them the player may cast
	if (auto* magic = registry.TryGet<components::TownMagic>(entity); magic != nullptr)
	{
		std::ranges::transform(record.magicTypesHeld, magic->held.begin(), [](int32_t held) { return held != 0; });
	}
	component.uninhabitable = record.uninhabitable != 0;
	component.areaMin = {record.areaMin.x, record.areaMin.z};
	component.areaMax = {record.areaMax.x, record.areaMax.z};
	registry.Assign<components::LoadedRecord<gamesave::TownRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakePileEntity(const PlannedPile& pile)
{
	const auto& object = pile.base.object;
	const auto position = map_coords::ToWorld(CoordsOf(pile.base.position.position));
	const auto entity = archetypes::PotArchetype::Create(position, object.heading, pile.type,
	                                                     static_cast<int32_t>(pile.pile.resourceAmount), true);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// its size as saved, as the object's load reads it (a new magic wood is made at 0.7, magic food at 0.3), placed again
	// at its spot and angle with that size; a pot's size then follows its amount again as its making sets it
	registry.Get<components::Transform>(entity).scale = glm::vec3(object.scale);
	object_placement::PlaceYXZ(registry, entity, position, glm::vec3(0.0f, object.heading, 0.0f), object.scale);
	// a pot's size by its amount; a pile at once at the height of its amount, then buried again (below)
	archetypes::PotArchetype::SetSize(entity, false);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, object.life);
	}
	// its poison, its speed-up and whether its reaction was started, as saved; then what only a saved game reads: whether
	// it was made for a building site, and to start its reaction
	auto& pot = registry.Get<components::Pot>(entity);
	const auto flags = pile.pile.flags;
	pot.poisoned = (flags & gamesave::pot_flags::k_Poisoned) != 0;
	pot.speedUp = (flags & gamesave::pot_flags::k_SpeedUp) != 0;
	pot.reactionStarted = (flags & gamesave::pot_flags::k_ReactionStarted) != 0;
	pot.fromBuildingSite = (flags & gamesave::pot_flags::k_FromBuildingSite) != 0;
	pot.reactsWhenMade = pile.pile.buildingByte != 0;
	// a magic wood's player
	if (pile.player.has_value())
	{
		pot.owner = *pile.player;
	}
	// the player it answers with, as the load's resolve sets it: a magic pile's its own player; a food or wood pile's the
	// one its record names, the neutral player when it names none (one part of a structure answers with the
	// structure's player instead, once the structures are made: Apply)
	const bool magic = pile.type == PotInfo::MagicWood || pile.type == PotInfo::MagicFood;
	if (magic)
	{
		pot.playerWhenMade = pot.owner;
	}
	else if (!std::holds_alternative<gamesave::PotRecord>(pile.record))
	{
		pot.playerWhenMade = pile.pilePlayer.value_or(PlayerNames::NEUTRAL);
	}
	// a pile starts buried again at the depth of its height and rises to the height of its amount over 1 s, as a new
	// pile does, whatever sink it was saved with: the saved sink is not used
	Zoomer buried;
	buried.SetPosition(-openblack::ecs::object::GetHeight(entity));
	archetypes::PotArchetype::SetSink(entity, buried);
	archetypes::PotArchetype::SetSize(entity, true);
	// a food pile saved sped up keeps its flag; its sparkle is the container the save kept, which the load makes with the
	// containers and gives it (Apply)
	std::visit(
	    [&registry, entity](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    pile.record);
	return entity;
}

entt::entity saved_world_load::MakeStoragePitEntity(const PlannedStoragePit& pit, uint32_t townKey,
                                                    const archetypes::AbodeArchetype::StoragePitPiles& piles)
{
	const auto& record = pit.record;
	const auto& object = record.base.object;
	const auto& amounts = record.abode.resources.values;
	const auto food = amounts.empty() ? 0U : amounts.at(0);
	const auto wood = amounts.size() < 2 ? 0U : amounts.at(1);
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::AbodeArchetype::Create(townKey, position, pit.type, object.heading, object.scale, food,
	                                                       wood, false, false, &piles);
	if (entity != entt::null)
	{
		KeepSaved(entity, record);
	}
	return entity;
}

entt::entity saved_world_load::MakeFootpathEntity(const PlannedFootpath& footpath)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = footpaths::Create();
	auto& made = registry.Get<components::Footpath>(entity);
	made.nodes.reserve(footpath.nodes.size());
	for (const auto& node : footpath.nodes)
	{
		// its place on the land at its map position, raised by its height above it, as the footpath file's nodes
		const auto coords = CoordsOf(node.coords);
		made.nodes.push_back({map_coords::ToWorld(coords), coords, node.flags, made.nextNodeId++});
	}
	made.active = footpath.record.active != 0;
	registry.Assign<components::LoadedRecord<gamesave::FootpathRecord>>(entity, footpath.record);
	return entity;
}

entt::entity saved_world_load::MakeFootpathLinkEntity(const PlannedFootpathLink& link,
                                                      const std::vector<entt::entity>& footpaths)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	std::vector<components::Footpath::Id> ids;
	ids.reserve(footpaths.size());
	for (const auto footpath : footpaths)
	{
		ids.push_back(static_cast<components::Footpath::Id>(footpath));
	}
	registry.Assign<components::FootpathLink>(entity, glm::vec3(0.0f), std::move(ids));
	registry.Assign<components::LoadedRecord<gamesave::FootpathLinkRecord>>(entity, link.record);
	return entity;
}

entt::entity saved_world_load::MakeCitadelHeartEntity(const PlannedCitadelHeart& heart, entt::entity citadel)
{
	const auto& record = heart.record;
	const auto& object = record.base.object;
	const auto& multi = record.base.multi;
	const bool underConstruction = (multi.fixedFlags & components::CitadelPartBuild::k_UnderConstruction) != 0;
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::CitadelArchetype::CreateHeart(position, heart.owner, citadel, object.heading, object.scale,
	                                                              object.life, underConstruction, false);
	auto& registry = Locator::entitiesRegistry::value();
	auto& build = registry.Get<components::CitadelPartBuild>(entity);
	build.buildFlags = multi.fixedFlags;
	build.percentBuilt = multi.percentBuilt;
	worship::citadel::SetHeartDrawPercent(entity, build.percentBuilt);
	life::SetLife(entity, object.life);
	// the angle it keeps as saved, which its turn need not give back, and its place's marks
	placed_turn::KeepAngles(registry, entity, glm::vec3(0.0f, object.heading, 0.0f));
	RestorePlaceMarks(entity, record.base.position.flags);
	// the heart's own values openblack holds: the last blow that harmed it, its destruction's start and clocks, its fade
	// and its beam's turn (the beams' and the glow's objects are not made again)
	registry.Get<components::CitadelHeart>(entity).breakingBlowTurn = record.lastImpactTurn;
	auto& temple = registry.Get<components::Temple>(entity);
	temple.destroying = record.destructionStarted != 0;
	temple.destructionClock = record.destructionTime;
	temple.destructionBeamClock = record.nextPlasmaTime;
	temple.beamTurn = record.lastTransferTurn;
	temple.fadeAlpha = static_cast<uint8_t>(static_cast<uint32_t>(record.drawColour) >> 24U);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	registry.Assign<components::LoadedRecord<gamesave::CitadelHeartRecord>>(entity, record);
	if (citadel == entt::null)
	{
		registry.Assign<components::LoadedRecord<gamesave::CitadelRecord>>(entity, heart.citadel);
	}
	return entity;
}

namespace
{
/// The number of the first field kind's row in the game's tables; the other kinds follow it downwards
constexpr uint32_t k_FirstFieldTypeRow = 1765;

/// A field the load made as an abode of its row: its kind, its town, its food, its growth, the crops sown and its turn
/// offset as its record says, and in the map's cells as a field. Its farmers stay as the record names them, as the
/// load makes no farmer's place in it
void MakeField(entt::entity entity, const openblack::gamesave::FieldRecord& record)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto townId = registry.Get<const components::Abode>(entity).townId;
	auto& field = registry.Assign<components::Field>(entity, static_cast<int>(townId));
	const auto kind = static_cast<int64_t>(k_FirstFieldTypeRow) - static_cast<int64_t>(record.typeInfo);
	field.type = kind >= 0 && kind < static_cast<int64_t>(openblack::FieldTypeInfo::_COUNT)
	                 ? static_cast<openblack::FieldTypeInfo>(kind)
	                 : openblack::FieldTypeInfo::Wheat;
	field.food = std::bit_cast<float>(record.secondWord);
	field.growth = std::bit_cast<float>(record.fourthWord);
	field.crops = record.firstByte;
	field.turnOffset = static_cast<uint8_t>(record.thirdWord);
	// in again with the field's type, at the head of the same cells, as a field's making does
	map_cells::RemoveMapObject(entity);
	map_cells::InsertMapObject(entity);
}
} // namespace

entt::entity saved_world_load::MakeAbodeEntity(const PlannedAbode& abode, uint32_t townKey)
{
	const auto& object = abode.base.object;
	const auto& multi = abode.base.multi;
	const auto& amounts = abode.abode.resources.values;
	const auto food = amounts.empty() ? 0U : amounts.at(0);
	const auto wood = amounts.size() < 2 ? 0U : amounts.at(1);
	const bool underConstruction = (multi.fixedFlags & components::Abode::k_UnderConstruction) != 0;
	const auto position = map_coords::ToWorld(CoordsOf(abode.base.position.position));
	// a town centre is not made functional: its totem and its icons have records of their own
	const auto entity = archetypes::AbodeArchetype::Create(townKey, position, abode.type, object.heading, object.scale, food,
	                                                       wood, underConstruction, false, nullptr, false);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& made = registry.Get<components::Abode>(entity);
	made.buildFlags = multi.fixedFlags;
	made.percentBuilt = multi.percentBuilt;
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, object.life);
	}
	std::visit(
	    [&registry, entity](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    if constexpr (std::is_same_v<Record, gamesave::GraveyardRecord>)
		    {
			    if (auto* graveyard = registry.TryGet<components::Graveyard>(entity); graveyard != nullptr)
			    {
				    graveyard->dead = record.graves;
				    graveyard->gravesStage = 0;
			    }
		    }
		    if constexpr (std::is_same_v<Record, gamesave::FieldRecord>)
		    {
			    MakeField(entity, record);
		    }
		    // a spell dispenser: its count, period, activity and magic as saved, and its effect, as its creation makes it,
		    // when it was saved available
		    if constexpr (std::is_same_v<Record, gamesave::SpellDispenserRecord>)
		    {
			    openblack::worship::dispenser::Restore(
			        entity,
			        components::SpellDispenser {
			            .magicType = static_cast<MagicType>(record.words[3]),
			            .timer = {.tick = record.words[0], .period = record.words[1], .active = record.words[2] != 0}},
			        (record.thing.flags & k_ThingUnavailable) == 0);
		    }
		    // a workshop's own values, as its load reads them; its pile, its sign and its scaffolds once they are made
		    if constexpr (std::is_same_v<Record, gamesave::WorkshopRecord>)
		    {
			    load_workshops::SetValues(entity, record);
		    }
		    // a town centre's resolve makes its belief symbols' effect when it is functional
		    if constexpr (std::is_same_v<Record, gamesave::TownCentreRecord>)
		    {
			    if (abode_queries::IsFunctional(entity))
			    {
				    openblack::particles::town_belief::MakeEffect(entity);
			    }
		    }
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    abode.record);
	return entity;
}

entt::entity saved_world_load::MakeWorshipSiteEntity(const PlannedWorshipSite& planned,
                                                     const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto citadel = objectOf(record.part.citadel.number);
	auto* worship = citadel != entt::null ? registry.TryGet<components::CitadelWorship>(citadel) : nullptr;
	const auto tribeIndex = static_cast<int64_t>(k_FirstTribeRow) - static_cast<int64_t>(record.tribeInfo);
	if (worship == nullptr || record.slot >= components::CitadelWorship::k_Sites ||
	    worship->sites.at(record.slot) != entt::null || tribeIndex < 0 || tribeIndex >= static_cast<int64_t>(Tribe::_COUNT))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: worship site {} not made: no citadel, a taken slot or no tribe",
		                   planned.number);
		return entt::null;
	}
	// the citadel's own record names its six sites, by slot, as the original's load sets them: it should name this one
	if (const auto* kept = registry.TryGet<const components::LoadedRecord<gamesave::CitadelRecord>>(citadel);
	    kept != nullptr &&
	    (record.slot >= kept->record.parts.numbers.size() || kept->record.parts.numbers.at(record.slot) != planned.number))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: worship site {} is not its citadel's site in slot {}", planned.number,
		                   record.slot);
	}
	const auto& base = record.part.base;
	const auto entity =
	    openblack::worship::site::CreateInSlot(citadel, static_cast<Tribe>(tribeIndex), record.slot, base.object.heading);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, base.object.creationIndex);
	RestorePlaceMarks(entity, base.position.flags);
	if (base.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, base.object.life);
	}
	registry.AssignOrReplace<components::CitadelPartBuild>(
	    entity, components::CitadelPartBuild {.buildFlags = base.multi.fixedFlags, .percentBuilt = base.multi.percentBuilt});
	auto& site = registry.Get<components::WorshipSite>(entity);
	for (const auto number : record.towns.numbers)
	{
		const auto town = objectOf(number);
		if (town == entt::null)
		{
			continue;
		}
		site.towns.push_back(town);
		if (auto* magic = registry.TryGet<components::TownMagic>(town))
		{
			magic->worshipSite = entity;
		}
	}
	site.foodPot = objectOf(record.foodPot.number);
	site.villagersAtSite = static_cast<int32_t>(record.worshippers);
	site.foodOnTheWay =
	    record.resourcesOnTheWay.values.empty() ? 0 : static_cast<int32_t>(record.resourcesOnTheWay.values.front());
	site.battery = record.chants[0];
	site.sacrificed = record.chants[1];
	site.available = record.chants[2];
	site.used = record.chants[3];
	site.requested = record.chants[4];
	site.chantDamage = record.chants[5];
	site.infiniteChants = record.infiniteChants != 0;
	site.freeMaintenance = record.freeMaintenance != 0;
	site.iconTookChants = record.isSupplyingChants != 0;
	site.strain = record.overloaded;
	site.strainPhase = record.overloadPulsePhase;
	site.strainPulse = record.overloadPulse;
	// its totem and need signs, made as the game makes them, then given their saved creation numbers, a sign its saved
	// desire, and their records
	site.totem = openblack::worship::site::CreateTotemOf(entity);
	if (site.totem != entt::null && planned.totem.has_value())
	{
		registry.AssignOrReplace<components::ObjectCreationIndex>(site.totem, planned.totem->part.base.object.creationIndex);
		RestorePlaceMarks(site.totem, planned.totem->part.base.position.flags);
		registry.AssignOrReplace<components::LoadedRecord<gamesave::WorshipTotemRecord>>(site.totem, *planned.totem);
		// the altar's own build state, as its site's above
		const auto& multi = planned.totem->part.base.multi;
		registry.AssignOrReplace<components::CitadelPartBuild>(
		    site.totem, components::CitadelPartBuild {.buildFlags = multi.fixedFlags, .percentBuilt = multi.percentBuilt});
		abodes::RedrawConstruction(site.totem);
	}
	for (size_t i = 0; i < planned.signs.size(); ++i)
	{
		const auto sign = site.needsSigns.at(i);
		const auto& saved = planned.signs[i].second;
		if (sign == entt::null || !saved.has_value())
		{
			continue;
		}
		registry.AssignOrReplace<components::ObjectCreationIndex>(sign, saved->object.creationIndex);
		if (auto* visuals = registry.TryGet<components::ShowNeedsVisuals>(sign))
		{
			visuals->desire = std::bit_cast<float>(saved->lastWord);
		}
		registry.AssignOrReplace<components::LoadedRecord<gamesave::ShowNeedsVisualsRecord>>(sign, *saved);
	}
	// its spell icons, each made at its saved place as the game makes one, at the head of the site's list, so that the
	// last of the saved list made first leaves them in its order; then their saved values
	for (const auto& [number, saved] : planned.icons | std::views::reverse)
	{
		const auto seedRow = static_cast<int64_t>(k_FirstSpellSeedRow) - static_cast<int64_t>(saved.icon.info);
		const auto icon = openblack::worship::icon::Create(map_coords::ToWorld(CoordsOf(saved.icon.base.position.position)),
		                                                   static_cast<SpellSeedType>(seedRow), entity,
		                                                   static_cast<int16_t>(saved.secondHalf));
		if (icon == entt::null)
		{
			continue;
		}
		registry.AssignOrReplace<components::ObjectCreationIndex>(icon, saved.icon.base.object.creationIndex);
		RestorePlaceMarks(icon, saved.icon.base.position.flags);
		auto& values = registry.Get<components::WorshipSpellIcon>(icon);
		values.removeTimer = static_cast<int16_t>(saved.half);
		values.savedScale = std::bit_cast<float>(saved.words[0]);
		values.chargeStartTurn = saved.words[1];
		values.charging = saved.byte != 0;
		values.powerUp = static_cast<int>(saved.thirdWord);
		values.chantStore = std::bit_cast<float>(saved.fourthWord);
		openblack::worship::icon::UpdatePowerUpGraphics(icon);
		if (saved.other.number != 0 || !saved.objects.numbers.empty())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: spell icon {}: its charging hand and its seeds are not put back yet",
			                   number);
		}
		registry.Assign<components::LoadedRecord<gamesave::WorshipSpellIconRecord>>(icon, saved);
	}
	registry.Assign<components::LoadedRecord<gamesave::WorshipSiteRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeDanceEntity(const PlannedDance& planned,
                                               const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	// its dancers first: one not made leaves the dance unmade
	std::vector<std::vector<entt::entity>> dancers;
	for (const auto& [number, group] : planned.groups)
	{
		auto& members = dancers.emplace_back();
		for (const auto member : group.members.numbers)
		{
			const auto entity = objectOf(member);
			if (entity == entt::null)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: dance {} not made: its dancer {} is not", planned.number,
				                   member);
				return entt::null;
			}
			members.push_back(entity);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	// it comes after every dance there is
	uint32_t made = 0;
	registry.Each<const components::Dance>(
	    [&made](entt::entity, const components::Dance& other) { made = std::max(made, other.made + 1); });
	const auto entity = registry.Create();
	auto& dance = registry.Assign<components::Dance>(entity);
	dance.made = made;
	dance.type = static_cast<DanceInfo>(record.type);
	const auto& group = record.group;
	dance.place = {
	    .x = group.position.position.x, .z = group.position.position.z, .altitude = group.position.position.altitude};
	dance.clock = group.time;
	dance.loopLength = group.length;
	dance.dancers = group.dancers;
	dance.groups.round = group.proportionCounter;
	dance.groups.roundLength = group.denominator;
	dance.rate = group.speed;
	dance.dancingRate = group.lastSpeed;
	// its camera: whether the hand stayed near it, whether a key frame called the camera in, the time the camera takes
	// to come and the turns the hand has been near it
	dance.useCamera = group.flags[0] != 0;
	dance.cameraCalled = group.flags[2] != 0;
	dance.cameraTravelTime = std::bit_cast<uint32_t>(group.cameraTravelTime);
	dance.cameraNearTurns = group.cameraNearTurns;
	dance.madeByScript = group.flags[1] != 0;
	dance.speed = record.worshipSpeed;
	dance.owner = objectOf(record.target.number);
	dance.state = record.words[0] == 0 ? components::Dance::State::Dancing : components::Dance::State::Stopped;
	dance.duration = record.words[1];
	dance.startTurn = record.words[2];
	if (record.words[3] != 0)
	{
		dance.firstDancerTurn = record.words[4];
	}
	dance.onTheirWay = record.words[5];
	if (Locator::infoConstants::has_value() && record.type < Locator::infoConstants::value().dance.size())
	{
		dance.autostart = Locator::infoConstants::value().dance.at(record.type).startsAutomatically != 0;
	}
	if (Locator::danceSystem::has_value())
	{
		dance.file = Locator::danceSystem::value().FileOf(dance.type);
	}
	// its groups, as their records say, in its order
	const auto indexOf = [&planned](gamesave::ObjectNumber number) {
		const auto found = std::ranges::find(planned.groups, number, [](const auto& entry) { return entry.first; });
		return static_cast<size_t>(found - planned.groups.begin());
	};
	for (size_t index = 0; index < planned.groups.size(); ++index)
	{
		const auto& saved = planned.groups[index].second;
		auto& part = dance.groups.all.emplace_back();
		// made as the game makes a group, which takes the next of the numbers it names groups by, then named as saved
		if (Locator::danceSystem::has_value())
		{
			std::ignore = Locator::danceSystem::value().NextUntitledNumber();
		}
		part.name = std::string(saved.text.data(), strnlen(saved.text.data(), saved.text.size()));
		part.dancers = dancers[index];
		part.flag = saved.words[1] != 0;
		part.formation = saved.words[2];
		part.shape = saved.words[3];
		std::memcpy(&part.offset, saved.eightBytes.data(), saved.eightBytes.size());
		part.radius = std::bit_cast<float>(saved.secondWords[0]);
		part.rotation = std::bit_cast<float>(saved.secondWords[2]);
		std::array<uint32_t, 4> move {};
		std::memcpy(move.data(), saved.sixteenBytes.data(), saved.sixteenBytes.size());
		part.move = {.action = move[0], .first = move[1], .second = move[2], .end = move[3]};
		part.spin = saved.value;
		if (saved.other.number != 0)
		{
			part.parent = indexOf(saved.other.number);
		}
		part.indexInParent = saved.byte;
		part.limitedDancers = saved.thirdWord;
		part.moved = saved.firstInt != 0;
		part.speed = saved.secondInt;
		part.hasLastCentre = saved.thirdInt != 0;
		part.centre = {.x = saved.firstPosition.x, .z = saved.firstPosition.z, .altitude = saved.firstPosition.altitude};
		part.lastCentre = {.x = saved.secondPosition.x, .z = saved.secondPosition.z, .altitude = saved.secondPosition.altitude};
		part.danceType = saved.danceType;
		part.inFormation = saved.fourthWord;
		std::array<uint32_t, 3> proportion {};
		std::memcpy(proportion.data(), saved.proportion.data(), saved.proportion.size());
		part.limited = proportion[0] != 0;
		part.quota = proportion[1];
		part.weight = proportion[2];
		part.children.assign(saved.stack.values.begin(), saved.stack.values.end());
		part.sexes = saved.villagerType;
		// each dancer in its place in the dance
		for (const auto dancer : part.dancers)
		{
			registry.AssignOrReplace<components::Dancer>(dancer, components::Dancer {.dance = entity, .group = index});
		}
	}
	for (const auto number : group.fixedGroups.numbers)
	{
		dance.groups.limited.push_back(indexOf(number));
	}
	for (const auto number : group.proportionGroups.numbers)
	{
		dance.groups.shared.push_back(indexOf(number));
	}
	registry.Assign<components::LoadedRecord<gamesave::DanceRecord>>(entity, record);
	// the creature that made it names it again as the dance it made last
	registry.Each<const components::LoadedRecord<gamesave::CreatureRecord>, components::CreatureDance>(
	    [&planned, entity](entt::entity, const components::LoadedRecord<gamesave::CreatureRecord>& creature,
	                       components::CreatureDance& danceOf) {
		    if (creature.record.creature.dance.number == planned.number)
		    {
			    danceOf.dance = entity;
		    }
	    });
	// a worship site's dance is the site's own again, facing the way the site does
	if (auto* site = dance.owner != entt::null ? registry.TryGet<components::WorshipSite>(dance.owner) : nullptr;
	    site != nullptr)
	{
		site->dance = entity;
		registry.Get<components::Dance>(entity).angle = site->yAngle;
	}
	return entity;
}

std::vector<uint32_t> saved_world_load::MakeFires(const std::vector<PlannedFire>& fires, const std::array<uint8_t, 2>& tags,
                                                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	std::vector<uint32_t> ids(fires.size(), 0);
	std::unordered_map<gamesave::ObjectNumber, fire::FireEffect*> made;
	// from the list's last to its first, each at the head of the fires' list
	for (size_t i = fires.size(); i-- > 0;)
	{
		const auto& record = fires[i].record;
		auto* restored = fire::Restore({.object = objectOf(record.objects[0].number),
		                                .source = objectOf(record.objects[1].number),
		                                .player = fires[i].player,
		                                .temperature = std::bit_cast<float>(record.words[0]),
		                                .previous = std::bit_cast<float>(record.words[1]),
		                                .charring = std::bit_cast<float>(record.word),
		                                .tag = record.byte,
		                                .flags = record.secondByte});
		if (restored != nullptr)
		{
			ids[i] = restored->id;
			made.emplace(fires[i].number, restored);
		}
	}
	// each in its group: the group's made fires in the order its saved chain runs from its root, the first of them its
	// root, so that every fire's root and the chain from that root agree even when a fire of the group was not made
	std::unordered_map<gamesave::ObjectNumber, const PlannedFire*> byNumber;
	for (const auto& planned : fires)
	{
		byNumber.emplace(planned.number, &planned);
	}
	std::unordered_set<gamesave::ObjectNumber> linked;
	const auto linkChainFrom = [&](gamesave::ObjectNumber first) {
		fire::FireEffect* groupRoot = nullptr;
		fire::FireEffect* last = nullptr;
		std::unordered_set<gamesave::ObjectNumber> walked;
		for (auto number = first; number != 0 && walked.insert(number).second;)
		{
			const auto planned = byNumber.find(number);
			if (planned == byNumber.end())
			{
				break;
			}
			if (const auto found = made.find(number); found != made.end() && linked.insert(number).second)
			{
				auto* fire = found->second;
				fire->root = groupRoot != nullptr ? groupRoot : fire;
				fire->next = nullptr;
				if (last != nullptr)
				{
					last->next = fire;
				}
				groupRoot = groupRoot != nullptr ? groupRoot : fire;
				last = fire;
			}
			number = planned->second->record.others[0].number;
		}
	};
	// the chains of the saved roots first (a root that was not made still leads to the rest of its chain), then any made
	// fire no saved root's chain reached, from itself
	for (const auto& planned : fires)
	{
		if (planned.record.others[1].number == planned.number)
		{
			linkChainFrom(planned.number);
		}
	}
	for (const auto& planned : fires)
	{
		if (made.contains(planned.number) && !linked.contains(planned.number))
		{
			linkChainFrom(planned.number);
		}
	}
	// with the firemen each record lists
	for (const auto& planned : fires)
	{
		const auto found = made.find(planned.number);
		if (found == made.end())
		{
			continue;
		}
		const auto& record = planned.record;
		for (const auto fireman : record.burning.numbers)
		{
			if (const auto entity = objectOf(fireman); entity != entt::null)
			{
				found->second->firemen.push_back(entity);
			}
		}
	}
	auto& system = Locator::fireSystem::value();
	system.SetProcessTag(tags[0]);
	system.SetCreateTag(tags[1]);
	return ids;
}

namespace
{
/// The index a walk's circle of water or the land's edge is saved with, and a walk holding none
constexpr int32_t k_LandscapeCircleIndex = 1;
constexpr int32_t k_NoCircleIndex = 0;

/// The walk states a loaded walker is made in: none yet, arrived, its last step, the wander, stepping through to its
/// goal and the straight walks, the circling states with no thing held (a circle of water or the land's edge, or no
/// obstacle ahead)
bool LoadsWalkState(uint8_t state)
{
	using openblack::ecs::wall_hug::WalkState;
	switch (state)
	{
	case 0:
	case static_cast<uint8_t>(WalkState::Arrived):
	case static_cast<uint8_t>(WalkState::FinalStep):
	case static_cast<uint8_t>(WalkState::Wander):
	case static_cast<uint8_t>(WalkState::StepThrough):
	case static_cast<uint8_t>(WalkState::Linear):
	case static_cast<uint8_t>(WalkState::LinearClockwise):
	case static_cast<uint8_t>(WalkState::LinearAnticlockwise):
	case static_cast<uint8_t>(WalkState::OrbitClockwise):
	case static_cast<uint8_t>(WalkState::OrbitAnticlockwise):
	case static_cast<uint8_t>(WalkState::ExitAnticlockwise):
	case static_cast<uint8_t>(WalkState::ExitClockwise):
		return true;
	default:
		return false;
	}
}

/// A loaded walker's walk as its record holds it: the tag of the walk state it is in, its way round and the place it
/// set out from (its goal for its last step), and the turns to its next step's aim when it steps through to its goal;
/// an arrived walker and one wandering keep the state after their walk, as a walk's dropped tags leave it; and the
/// circle of water or the land's edge it heads for or goes round
void RestoreWalk(entt::entity entity, const openblack::gamesave::WallHugPart& move)
{
	namespace components = openblack::ecs::components;
	using components::MoveStateClockwise;
	using openblack::ecs::wall_hug::WalkState;
	auto& registry = openblack::Locator::entitiesRegistry::value();
	auto& wallHug = registry.Get<components::WallHug>(entity);
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	const glm::vec2 here = transform != nullptr ? glm::vec2(transform->position.x, transform->position.z) : glm::vec2(0.0f);
	switch (static_cast<WalkState>(move.moveState))
	{
	case WalkState::Arrived:
	case WalkState::Wander:
		registry.AssignOrReplace<components::WalkStateAfterWalk>(entity, move.moveState);
		break;
	case WalkState::FinalStep:
		registry.Assign<components::MoveStateFinalStepTag>(entity, MoveStateClockwise::Undefined, wallHug.goal);
		break;
	case WalkState::StepThrough:
		registry.Assign<components::MoveStateStepThroughTag>(entity, MoveStateClockwise::Undefined, here);
		wallHug.turnsUntilStepRebuild = move.turnsToStepRebuild.value_or(0);
		break;
	case WalkState::Linear:
		registry.Assign<components::MoveStateLinearTag>(entity, MoveStateClockwise::Undefined, here);
		break;
	case WalkState::LinearClockwise:
		registry.Assign<components::MoveStateLinearTag>(entity, MoveStateClockwise::Clockwise, here);
		break;
	case WalkState::LinearAnticlockwise:
		registry.Assign<components::MoveStateLinearTag>(entity, MoveStateClockwise::CounterClockwise, here);
		break;
	case WalkState::OrbitClockwise:
		registry.Assign<components::MoveStateOrbitTag>(entity, MoveStateClockwise::Clockwise, here);
		break;
	case WalkState::OrbitAnticlockwise:
		registry.Assign<components::MoveStateOrbitTag>(entity, MoveStateClockwise::CounterClockwise, here);
		break;
	case WalkState::ExitClockwise:
		registry.Assign<components::MoveStateExitCircleTag>(entity, MoveStateClockwise::Clockwise, here);
		break;
	case WalkState::ExitAnticlockwise:
		registry.Assign<components::MoveStateExitCircleTag>(entity, MoveStateClockwise::CounterClockwise, here);
		break;
	default:
		break;
	}
	// the circle of water or the land's edge it heads for or goes round, as the walk keeps it, whatever its turns to it
	if (move.circle.has_value() && move.circle->object.number == 0 && move.circle->index == k_LandscapeCircleIndex)
	{
		constexpr uint32_t k_CellShift = 16;
		const glm::ivec2 cell {static_cast<int32_t>(static_cast<uint32_t>(move.circle->coords.x) >> k_CellShift),
		                       static_cast<int32_t>(static_cast<uint32_t>(move.circle->coords.z) >> k_CellShift)};
		registry.AssignOrReplace<components::WallHugObjectReference>(
		    entity, components::WallHugObjectReference {.entity = entt::null,
		                                                .centre = openblack::ecs::wall_hug::LandscapeCircleCentre(cell),
		                                                .radius = openblack::ecs::wall_hug::k_LandscapeBlockerRadius,
		                                                .landscape = true,
		                                                .cell = cell,
		                                                .landscapeOrFence = true});
	}
}
} // namespace

entt::entity saved_world_load::MakeVillagerEntity(const PlannedVillager& planned, std::optional<uint16_t> counter)
{
	const auto& base =
	    std::visit([](const auto& record) { return std::tie(record.thing, record.position, record.object, record.values); },
	               planned.record);
	const auto& [thing, position, object, values] = base;
	const auto& move = values.move;
	const auto& living = values.living;
	const auto& fields = values.villager;
	// what the load does not make yet
	const char* why = nullptr;
	if (planned.type == VillagerInfo::None)
	{
		why = "no villager row";
	}
	else if (!planned.inHand && ((thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) != 0 ||
	                             (position.flags & (k_PositionUnavailable | k_PositionInPhysics)) != 0))
	{
		why = "held by a script or in flight";
	}
	else if (living.dataPath.number != 0 &&
	         (!planned.dataPath.has_value() || camera_edits::FindTrack(planned.dataPath->trackNumber) == nullptr))
	{
		why = "on a data path whose track is not there";
	}
	else if (living.reaction.number == 0 && living.otherObject.number != 0)
	{
		// something it reacts to with no reaction
		why = "reacting with no reaction";
	}
	else if (living.reactionsDone.size() > components::ReactionRecords {}.records.size())
	{
		why = "more reactions done than a living thing keeps";
	}
	else if (!LoadsWalkState(move.moveState))
	{
		why = "in a walk's state the load does not make";
	}
	else if (move.circle.has_value() && move.circle->object.number == 0 && move.circle->index != k_NoCircleIndex &&
	         move.circle->index != k_LandscapeCircleIndex)
	{
		// a circle neither of a thing nor of water or the land's edge (with none held the index is 0)
		why = "walking round a circle of nothing";
	}
	else if (!StateValuesLoaded(living.action[0], values.topState, fields.discipleType) ||
	         !StateValuesLoaded(living.action[1], values.finalState, fields.discipleType))
	{
		why = "in a state that loads values";
	}
	if (why != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: villager {} not made: {} (states {} and {})", planned.number, why,
		                   living.action[0], living.action[1]);
		return entt::null;
	}
	const bool child = (fields.flags & components::Villager::k_FlagChild) != 0;
	const auto entity = archetypes::VillagerArchetype::Restore(map_coords::ToWorld(CoordsOf(position.position)), planned.type,
	                                                           object.heading, object.scale, child);
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	RestorePlaceMarks(entity, position.flags);
	// the thing's mark of a hidden name: a villager a script made shows no name after the load either
	if ((thing.flags & k_ThingNameHidden) != 0)
	{
		registry.AssignOrReplace<components::NameHidden>(entity);
	}
	auto& villager = registry.Get<components::Villager>(entity);
	villager.life = object.life;
	villager.flags = static_cast<uint16_t>(fields.flags & 0xC6FAU);
	villager.woken = (fields.flags & 0x1U) != 0;
	villager.food = fields.food;
	villager.lastCheckTurn = static_cast<uint32_t>(fields.lastCheckTurn);
	villager.foodSpeedUp = fields.foodSpeedUp;
	villager.discipleType = fields.discipleType;
	villager.resourceHeld = {fields.foodHeld, fields.woodHeld};
	villager.pregnancy = fields.pregnant;
	villager.birthTurn = living.birthTurn;
	// its 3D object made again, with its age at the loaded turn
	registry.AssignOrReplace<components::DrawImportance>(
	    entity, components::DrawImportance {.importance = villager_age::DrawImportanceFor(planned.ageAtLoad)});
	villager.status = static_cast<uint16_t>(living.status & 0x0471U);
	villager.flockOrder = living.secondByte;
	villager.crecheWalk = std::bit_cast<int32_t>(living.secondWord);
	// the reactions it did and their turns, oldest first, as its record lists them
	if (!living.reactionsDone.empty())
	{
		auto& memory = registry.AssignOrReplace<components::ReactionRecords>(entity);
		memory.count = 0;
		for (const auto& done : living.reactionsDone)
		{
			memory.records.at(memory.count++) = {.type = static_cast<uint8_t>(done.reaction), .turn = done.turn};
		}
	}
	// a dead villager's cause, its state's one byte
	for (const auto& [state, saved] :
	     {std::pair {living.action[0], &values.topState}, std::pair {living.action[1], &values.finalState}})
	{
		const auto save = gamesave::VillagerStateSaveOf(state);
		if (save == gamesave::VillagerStateSave::Dead && !saved->empty())
		{
			villager.deathReason = static_cast<DeathReason>(saved->front().bytes[0]);
		}
		// in the hand: the disciple it was when it was picked up, which it becomes again as it lands
		if (save == gamesave::VillagerStateSave::InHand && !saved->empty())
		{
			villager.discipleOnPickUp = saved->front().bytes[0];
		}
		// a farmer's work point in its field, as the game holds it (its field once every object is made)
		if (save == gamesave::VillagerStateSave::Farming && !saved->empty())
		{
			std::memcpy(&villager.workPos, saved->front().bytes.data(), sizeof(villager.workPos));
		}
		// a builder's place at its building site (its site once every object is made)
		constexpr size_t k_BuildingPlace = 1;
		if (save == gamesave::VillagerStateSave::Building && saved->size() > k_BuildingPlace)
		{
			std::memcpy(&villager.buildPosIndex, saved->at(k_BuildingPlace).bytes.data(), sizeof(villager.buildPosIndex));
		}
		// a breeder's place to wander about, in whole map x and z (its partner once every object is made)
		constexpr size_t k_WanderArea = 1;
		if (save == gamesave::VillagerStateSave::Breeder && saved->size() > k_WanderArea)
		{
			int32_t x = 0;
			int32_t z = 0;
			std::memcpy(&x, saved->at(k_WanderArea).bytes.data(), sizeof(x));
			std::memcpy(&z, saved->at(k_WanderArea).bytes.data() + sizeof(x), sizeof(z));
			villager.wanderArea = {.x = x, .z = z, .altitude = 0.0f};
		}
		// by a fire: the place it was going to, which it goes back to once it stops being on fire, in whole map x and z
		// (its fire once the fires are made)
		if (save == gamesave::VillagerStateSave::Fire && !saved->empty())
		{
			int32_t x = 0;
			int32_t z = 0;
			std::memcpy(&x, saved->front().bytes.data(), sizeof(x));
			std::memcpy(&z, saved->front().bytes.data() + sizeof(x), sizeof(z));
			registry.AssignOrReplace<components::VillagerFireState>(entity).savedDestination = {map_coords::ToMetres(x),
			                                                                                    map_coords::ToMetres(z)};
		}
		// a shepherd's turns to wait and the state it goes on to, none before its first wait (0 and 0)
		constexpr size_t k_ShepherdValues = 2;
		if (save == gamesave::VillagerStateSave::Shepherd && saved->size() >= k_ShepherdValues)
		{
			int16_t wait = 0;
			uint16_t pending = 0;
			std::memcpy(&wait, saved->at(0).bytes.data(), sizeof(wait));
			std::memcpy(&pending, saved->at(1).bytes.data(), sizeof(pending));
			if (wait != 0 || pending != 0)
			{
				registry.AssignOrReplace<components::ShepherdWork>(
				    entity, components::ShepherdWork {.waitTurns = wait, .pending = static_cast<VillagerStates>(pending)});
			}
		}
		// a script's state: the place it gave in whole map x and z, the number it gave, then the pair of whole numbers
		// it gave; what no script gave yet (all 0) is not made
		constexpr size_t k_ScriptValues = 4;
		if ((save == gamesave::VillagerStateSave::InScript || save == gamesave::VillagerStateSave::ScriptPos) &&
		    saved->size() >= k_ScriptValues)
		{
			const auto word = [saved](size_t value, size_t at) {
				uint32_t read = 0;
				std::memcpy(&read, saved->at(value).bytes.data() + at, sizeof(read));
				return read;
			};
			const auto x = std::bit_cast<int32_t>(word(0, 0));
			const auto z = std::bit_cast<int32_t>(word(0, sizeof(uint32_t)));
			const auto number = std::bit_cast<float>(word(1, 0));
			if (x != 0 || z != 0 || number != 0.0f)
			{
				registry.AssignOrReplace<components::ScriptStateArguments>(
				    entity, components::ScriptStateArguments {.place = {map_coords::ToMetres(x), map_coords::ToMetres(z)},
				                                              .number = number});
			}
			if (const auto clip = word(2, 0), plays = word(3, 0); clip != 0 || plays != 0)
			{
				registry.AssignOrReplace<components::ScriptAnimation>(
				    entity, components::ScriptAnimation {.clip = static_cast<AnimId>(clip), .playsLeft = plays});
			}
		}
	}
	if ((living.status & 0x0002U) != 0)
	{
		registry.Assign<components::Poisoned>(entity);
	}
	if ((living.status & 0x0080U) != 0)
	{
		registry.Assign<components::DownedVillager>(entity);
	}
	if ((fields.flags & 0x4U) != 0)
	{
		registry.Assign<components::AtHome>(entity).beenToBed = (fields.flags & 0x2000U) != 0;
	}
	if (living.specular != std::array<uint8_t, 4> {})
	{
		registry.Assign<components::SpecularColour>(
		    entity, components::SpecularColour {.colour = {living.specular[2], living.specular[1], living.specular[0]}});
	}
	auto& action = registry.Get<components::LivingAction>(entity);
	action.states = {living.action[0], living.action[1], living.action[2]};
	action.turnsSinceStateChange = static_cast<uint16_t>(living.action[4] | (living.action[5] << 8U));
	action.turnsUntilStateChange = counter.value_or(std::bit_cast<uint16_t>(move.turnsToStateChange));
	auto& wallHug = registry.Get<components::WallHug>(entity);
	wallHug.speed = map_coords::ToMetres(static_cast<int32_t>(move.speed));
	wallHug.gameAngle = move.angle;
	walk_goal::Set(entity, wallHug.goal, {map_coords::ToMetres(move.goal.x), map_coords::ToMetres(move.goal.z)});
	walk_goal::SetAltitude(entity, move.goal.altitude);
	wallHug.step = {move.step.x, move.step.z};
	wallHug.stepMetres = {map_coords::ToMetres(move.step.x), map_coords::ToMetres(move.step.z)};
	// the turns to the circle and the entry distance, kept whether a circle is held or not
	if (move.circle.has_value())
	{
		wallHug.turnsToObj = move.circle->turnsToObject;
		wallHug.entryDistance = move.circle->entryDistance;
	}
	RestoreWalk(entity, move);
	std::visit(
	    [&registry, entity](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    if constexpr (std::is_same_v<Record, gamesave::SpecialVillagerRecord>)
		    {
			    registry.Assign<components::NamedVillager>(entity).row = static_cast<uint8_t>(record.infoIndex);
		    }
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    planned.record);
	// its walk along a script's track: the track, the share of the way it stops at, which way, how far it has got and
	// gets a turn, and the speed its step was worked out for
	if (planned.dataPath.has_value())
	{
		const auto& data = *planned.dataPath;
		if (auto track = camera_edits::FindTrack(data.trackNumber); track != nullptr)
		{
			const bool forward = data.reverse != 0;
			auto walk = camera_track::StartWalk(*track, forward, 0.0f, data.endPercentage);
			walk.current = data.frame;
			walk.step = data.framesPerTurn;
			registry.AssignOrReplace<components::LivingWalkPath>(
			    entity, components::LivingWalkPath {.path = {.number = data.trackNumber,
			                                                 .track = std::move(track),
			                                                 .walk = std::move(walk),
			                                                 .living = true,
			                                                 .speed = static_cast<int32_t>(data.last)},
			                                        .trackNumber = data.trackNumber});
		}
	}
	archetypes::VillagerArchetype::PlaceRestored(entity);
	// the clips it plays as it was saved, and what it carries
	if (auto* animation = registry.TryGet<components::SkeletalAnimation>(entity))
	{
		animation->transitionFlags = static_cast<uint16_t>(fields.flags & 0x1800U);
		animation->carriedObject = fields.carriedObjectType;
	}
	return entity;
}

std::vector<size_t> saved_world_load::LoadedFlockLine(std::span<const uint8_t> orders)
{
	std::vector<size_t> line;
	for (size_t i = 0; i < orders.size(); ++i)
	{
		const auto before = std::ranges::find_if(line, [&orders, i](size_t member) { return orders[i] <= orders[member]; });
		line.insert(before, i);
	}
	return line;
}

entt::entity saved_world_load::MakeFlockEntity(const PlannedFlock& planned,
                                               const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	// everything it names must be made: its shepherd, its town, its temple's heart and each member
	const auto made = [&objectOf](gamesave::ObjectNumber number) { return number == 0 || objectOf(number) != entt::null; };
	const char* why = nullptr;
	if (!made(record.shepherd.number) || !made(record.town.number) || !made(record.citadelHeart.number))
	{
		why = "its shepherd, its town or its temple is not made";
	}
	else if (!std::ranges::all_of(record.members.numbers, made))
	{
		why = "one of its members is not made";
	}
	if (why != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: flock {} not made: {}", planned.number, why);
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& flock = registry.Assign<components::Flock>(entity);
	flock.id = record.lastWord == 0 ? -1 : static_cast<int32_t>(record.lastWord);
	flock.info = record.container.info;
	flock.player = planned.player;
	flock.domainCentre = map_coords::ToWorld(CoordsOf(record.container.position.position));
	flock.savedDomainCentre = map_coords::ToWorld(CoordsOf(record.savedDomainCentre));
	flock.domainRadius = record.domainRadius;
	flock.flockDistance = record.flockDistance;
	flock.town = objectOf(record.town.number);
	flock.shepherd = objectOf(record.shepherd.number);
	flock.followShepherd = record.followShepherd != 0;
	flock.maxMembers = record.maxMembers;
	flock.altitude = record.flyingHeight;
	flock.followState = static_cast<uint8_t>(record.words[0]);
	flock.afterMove = static_cast<uint8_t>(record.words[1]);
	flock.followMode = static_cast<uint8_t>(record.words[2]);
	flock.leaderTurns = record.word;
	flock.calm = static_cast<int32_t>(record.secondWord);
	flock.creationStamp = planned.stamp;
	auto& counter = registry.Context().flockCreationCounter;
	counter = std::max(counter, planned.stamp);
	// its members in the line its load puts them in; the list here runs from the tail
	const auto line = LoadedFlockLine(planned.lineOrders);
	for (const auto index : line | std::views::reverse)
	{
		flock.members.push_back(objectOf(record.members.numbers[index]));
	}
	for (const auto member : flock.members)
	{
		if (auto* animal = registry.TryGet<components::Animal>(member))
		{
			animal->flock = entity;
		}
	}
	if (const auto heart = objectOf(record.citadelHeart.number); heart != entt::null)
	{
		if (auto* component = registry.TryGet<components::CitadelHeart>(heart))
		{
			component->flock = entity;
		}
	}
	registry.Assign<components::LoadedRecord<gamesave::FlockRecord>>(entity, record);
	return entity;
}

namespace
{
/// What a spell's animal keeps of its spell, as its record holds it: a dove's or a bat's fade (its zoomer, as saved:
/// its fade goes on from there, and its dying state, put back with the animal's state, reads it); a wolf's fade, its
/// final destination, its corridor (the plane's normal across the run and its offset), the corridor's half width and
/// its player. Its spell names it as the flock spells are made; its stored point is where its turn starts
void RestoreSpellAnimal(entt::entity entity, const openblack::gamesave::SavedRecord& record,
                        const std::function<entt::entity(openblack::gamesave::ObjectNumber)>& objectOf)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto* dove = std::get_if<openblack::gamesave::SpellDoveRecord>(&record);
	const auto* bat = std::get_if<openblack::gamesave::SpellBatRecord>(&record);
	const auto* wolf = std::get_if<openblack::gamesave::SpellWolfRecord>(&record);
	if (dove == nullptr && bat == nullptr && wolf == nullptr)
	{
		return;
	}
	auto& data = registry.AssignOrReplace<openblack::magic::SpellFlockAnimal>(entity);
	data.stored = registry.Get<const components::Transform>(entity).position;
	if (wolf == nullptr)
	{
		data.fade = saved_zoomer::ZoomerOf(dove != nullptr ? dove->spell : bat->spell);
		return;
	}
	data.wolf = true;
	data.fade = saved_zoomer::ZoomerOf(wolf->fade);
	data.finalDestination = {
	    .x = wolf->finalDestination.x, .z = wolf->finalDestination.z, .altitude = wolf->finalDestination.altitude};
	data.destination = {openblack::map_coords::ToMetres(wolf->finalDestination.x),
	                    openblack::map_coords::ToMetres(wolf->finalDestination.z)};
	const auto plane = std::bit_cast<std::array<float, 4>>(wolf->corridor);
	data.normal = {plane[0], plane[2]};
	data.offset = plane[3];
	data.halfWidth = wolf->halfWidth;
	if (auto* brain = registry.TryGet<components::AnimalBrain>(entity); brain != nullptr)
	{
		brain->finalDestination = data.destination;
	}
	const auto player = wolf->player.number != 0 && objectOf ? objectOf(wolf->player.number) : entt::entity {entt::null};
	if (const auto* owner =
	        player != entt::null && registry.Valid(player) ? registry.TryGet<const components::Player>(player) : nullptr;
	    owner != nullptr)
	{
		registry.Get<components::Animal>(entity).player = static_cast<int32_t>(owner->name);
	}
}
} // namespace

entt::entity saved_world_load::MakeAnimalEntity(const PlannedAnimal& planned,
                                                const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& living = planned.living;
	const auto& move = planned.move;
	const auto thingCircle = AnimalThingCircleOf(move, objectOf);
	const char* why = nullptr;
	if (planned.type == AnimalInfo::None)
	{
		why = "no animal row";
	}
	else if (planned.type >= k_FirstPuzzleAnimal)
	{
		why = "a puzzle game's piece";
	}
	else if (!planned.inHand &&
	         ((planned.thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) != 0 ||
	          (planned.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) != 0) &&
	         !LivingReturnsToThePhysics(planned.thing, planned.position, planned.object) && !CarriedByAMover(planned))
	{
		why = "held by a script or flown by the particles";
	}
	else if (living.footpath.number != 0 || move.footpath.number != 0)
	{
		why = "on a footpath";
	}
	else if (living.dataPath.number != 0)
	{
		why = "on a data path";
	}
	else if (living.danceGroup.number != 0)
	{
		why = "in a dance";
	}
	else if (move.circle.has_value() && move.circle->object.number == 0 && move.circle->index != k_NoCircleIndex &&
	         move.circle->index != k_LandscapeCircleIndex)
	{
		// a circle neither of a thing nor of water or the land's edge (with none held the index is 0)
		why = "walking round a circle of nothing";
	}
	else if (move.circle.has_value() && move.circle->object.number != 0 && !thingCircle.has_value())
	{
		why = "walking round a thing the load does not make";
	}
	else if (living.reactionsDone.size() > components::ReactionRecords {}.records.size())
	{
		why = "remembering more reactions than openblack keeps";
	}
	if (why != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: animal {} not made: {}", planned.number, why);
		return entt::null;
	}
	const auto& object = planned.object;
	const auto entity = archetypes::AnimalArchetype::Restore(map_coords::ToWorld(CoordsOf(planned.position.position)),
	                                                         planned.type, object.heading, object.scale, planned.age);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	RestorePlaceMarks(entity, planned.position.flags);
	registry.Assign<components::Life>(entity, object.life);
	auto& brain = registry.Assign<components::AnimalBrain>(entity);
	brain.counter = move.turnsToStateChange;
	brain.speed = move.speed;
	brain.angle = move.angle;
	brain.moveState = move.moveState;
	brain.turnsUntilStepRebuild = move.turnsToStepRebuild.value_or(int8_t {0});
	brain.goal = {map_coords::ToMetres(move.goal.x), map_coords::ToMetres(move.goal.z)};
	brain.goalAltitude = move.goal.altitude;
	brain.step = {move.step.x, move.step.z};
	brain.altitude = planned.position.position.altitude;
	brain.topState = living.action[0];
	brain.finalState = living.action[1];
	brain.previousState = living.action[2];
	brain.turnsSinceStateChange = static_cast<uint16_t>(living.action[4] | (living.action[5] << 8U));
	brain.birthTurn = std::bit_cast<int32_t>(living.birthTurn);
	brain.status = static_cast<uint16_t>(living.status & k_AnimalStatus);
	brain.hunger = planned.animal.hunger;
	brain.sleep = planned.animal.tiredness;
	brain.breed = planned.animal.breedingUrge;
	// its sleeping cell, two half words
	brain.sleepCell = {static_cast<uint16_t>(planned.animal.sleepPosition & 0xFFFFU),
	                   static_cast<uint16_t>(planned.animal.sleepPosition >> 16U)};
	brain.givenYAngle = object.heading;
	RestoreAnimalCircle(brain, move, thingCircle);
	auto& animal = registry.Get<components::Animal>(entity);
	animal.flockOrder = living.secondByte;
	animal.deathReason = static_cast<DeathReason>(planned.animal.deathReason);
	if (living.specular != std::array<uint8_t, 4> {})
	{
		registry.Assign<components::SpecularColour>(
		    entity, components::SpecularColour {.colour = {living.specular[2], living.specular[1], living.specular[0]}});
	}
	// the reactions it did and their turns, oldest first, as its record lists them
	if (!living.reactionsDone.empty())
	{
		auto& memory = registry.Assign<components::ReactionRecords>(entity);
		for (const auto& done : living.reactionsDone)
		{
			memory.records.at(memory.count++) = {.type = static_cast<uint8_t>(done.reaction), .turn = done.turn};
		}
	}
	std::visit(
	    [&registry, entity](const auto& record) {
		    if constexpr (requires { record.animal; })
		    {
			    using Record = std::decay_t<decltype(record)>;
			    registry.Assign<components::LoadedRecord<Record>>(entity, record);
		    }
	    },
	    static_cast<const gamesave::SavedRecordVariant&>(planned.record));
	RestoreSpellAnimal(entity, planned.record, objectOf);
	return entity;
}

std::optional<openblack::creature_script_play::Request> saved_world_load::ScriptPlayOf(const gamesave::KeptCreaturePlan& plan)
{
	constexpr uint32_t k_ObeyPlayer = 24;
	constexpr uint32_t k_FollowPlayer = 22;
	constexpr uint32_t k_PlayIndividual = 10;
	constexpr uint32_t k_PlayStatic = 4;
	constexpr uint32_t k_WaitUntilFree = 0x85;
	if (plan.desire != k_ObeyPlayer || plan.action != k_FollowPlayer || plan.steps.empty() ||
	    plan.steps.back().action != k_WaitUntilFree)
	{
		return std::nullopt;
	}
	openblack::creature_script_play::Request request;
	const auto plays = std::span(plan.steps).first(plan.steps.size() - 1);
	for (const auto& step : plays)
	{
		// one animation, played as the game's play of it: an individual one by its number, a static one held its seconds
		const bool individual = step.integer >= openblack::creature_script_play::k_FirstIndividualAction;
		if (step.action != (individual ? k_PlayIndividual : k_PlayStatic) ||
		    (request.plays != 0 && step.integer != request.animation))
		{
			return std::nullopt;
		}
		request.animation = step.integer;
		++request.plays;
	}
	return request;
}

void saved_world_load::LinkSpellSlots(ecs::Registry& registry, const std::unordered_map<uint32_t, entt::entity>& made)
{
	std::vector<entt::entity> named;
	std::as_const(registry).Each<const components::LoadedSpellSlots>(
	    [&named](entt::entity entity, const components::LoadedSpellSlots&) { named.push_back(entity); });
	for (const auto entity : named)
	{
		const auto numbers = registry.Get<const components::LoadedSpellSlots>(entity).miracles;
		if (auto* spells = registry.TryGet<components::CreatureSpells>(entity))
		{
			for (size_t i = 0; i < numbers.size(); ++i)
			{
				const auto found = numbers.at(i) != 0 ? made.find(numbers.at(i)) : made.end();
				spells->spells.slots.at(i).miracle = found != made.end() ? found->second : entt::entity {entt::null};
			}
		}
		registry.Remove<components::LoadedSpellSlots>(entity);
	}
}

std::optional<saved_world_load::KeptForcedPlan> saved_world_load::ForcedPlanOf(const gamesave::KeptCreaturePlan& plan)
{
	constexpr uint32_t k_ObeyPlayer = 24;
	// the forced actions taken up, by their rows, and their agendas' sub-actions
	constexpr uint32_t k_LookAtCamera = 82;
	constexpr uint32_t k_DeadForever = 261;
	constexpr uint32_t k_LookAtCameraStep = 0x42;
	constexpr uint32_t k_FaintStep = 0x0E;
	constexpr uint32_t k_WaitStep = 0x18;
	const auto stepsAre = [&plan](std::initializer_list<uint32_t> steps) {
		return plan.count == steps.size() && std::ranges::equal(plan.steps, steps, {}, &gamesave::AgendaStep::action);
	};
	// sleeping at home: the steps left of a yawn or not, the walk home, the face down the slope, the sleep and the wake
	constexpr uint32_t k_SleepAtHome = 17;
	constexpr uint32_t k_PlayIndividualStep = 0x0A;
	constexpr uint32_t k_MoveToPosStep = 8;
	constexpr uint32_t k_FaceDownSlopeStep = 0x44;
	constexpr uint32_t k_SleepStep = 0x84;
	const auto sleepsAtHome = [&plan]() {
		constexpr std::array<uint32_t, 5> k_Yawning {k_PlayIndividualStep, k_MoveToPosStep, k_FaceDownSlopeStep, k_SleepStep,
		                                             k_PlayIndividualStep};
		const auto full = plan.count == k_Yawning.size()       ? std::span<const uint32_t>(k_Yawning)
		                  : plan.count == k_Yawning.size() - 1 ? std::span<const uint32_t>(k_Yawning).subspan(1)
		                                                       : std::span<const uint32_t> {};
		return !full.empty() && plan.current < full.size() &&
		       std::ranges::equal(plan.steps, full.subspan(plan.current), {}, &gamesave::AgendaStep::action);
	};
	// sitting down at a thing: its four steps, the last the sit, a pose held
	constexpr uint32_t k_SitDown = 81;
	constexpr uint32_t k_SitSteps = 4;
	constexpr uint32_t k_PlayStaticStep = 4;
	const auto sitsDown = [&plan]() {
		return plan.count == k_SitSteps && plan.steps.size() == plan.count - plan.current &&
		       plan.steps.back().action == k_PlayStaticStep;
	};
	if (plan.desire != k_ObeyPlayer || plan.current >= plan.count ||
	    !((plan.action == k_LookAtCamera && stepsAre({k_LookAtCameraStep})) ||
	      (plan.action == k_DeadForever && stepsAre({k_FaintStep, k_WaitStep})) ||
	      (plan.action == k_SleepAtHome && sleepsAtHome()) || (plan.action == k_SitDown && sitsDown())))
	{
		return std::nullopt;
	}
	KeptForcedPlan forced {.action = plan.action,
	                       .objects = {plan.objects[0].number, plan.objects[1].number, plan.objects[2].number},
	                       .step = plan.current,
	                       .count = plan.count};
	if (plan.action == k_LookAtCamera)
	{
		forced.turns = plan.steps.front().integer;
	}
	if (plan.steps.front().action == k_PlayStaticStep)
	{
		forced.seconds = plan.steps.front().real;
	}
	return forced;
}

std::optional<std::vector<std::pair<glm::vec3, float>>> saved_world_load::ScriptMoveOf(const gamesave::KeptCreaturePlan& plan)
{
	constexpr uint32_t k_ObeyPlayer = 24;
	constexpr uint32_t k_MoveToPosStep = 8;
	if (plan.desire != k_ObeyPlayer || plan.action != creature_planner::k_FollowPlayerAction || plan.steps.empty() ||
	    !std::ranges::all_of(plan.steps, [](const auto& step) { return step.action == k_MoveToPosStep; }))
	{
		return std::nullopt;
	}
	std::vector<std::pair<glm::vec3, float>> legs;
	for (const auto& step : plan.steps)
	{
		legs.emplace_back(glm::vec3(step.point[0], step.point[1], step.point[2]), step.real);
	}
	return legs;
}

std::optional<saved_world_load::KeptDuel> saved_world_load::DuelOf(const gamesave::KeptCreaturePlan& plan)
{
	// the starter's plan of a fight, at the fight among the eighteen steps of its agenda, and the opponent's of obeying
	// it, at the fight and then the wait for its next order
	constexpr uint32_t k_FightAction = 21;
	constexpr uint32_t k_FightSteps = 18;
	constexpr uint32_t k_DuelStep = 14;
	constexpr uint32_t k_ObeyCreature = 26;
	constexpr uint32_t k_ObeyCreatureAction = 124;
	constexpr uint32_t k_FightStep = 0x1C;
	constexpr uint32_t k_WaitForeverStep = 0x4E;
	if (plan.steps.empty() || plan.steps.front().action != k_FightStep || plan.steps.front().object.number == 0)
	{
		return std::nullopt;
	}
	const auto other = plan.steps.front().object.number;
	if (plan.action == k_FightAction && plan.count == k_FightSteps && plan.current == k_DuelStep)
	{
		return KeptDuel {.startedIt = true, .other = other};
	}
	if (plan.desire == k_ObeyCreature && plan.action == k_ObeyCreatureAction && plan.current == 0 && plan.steps.size() == 2 &&
	    plan.steps[1].action == k_WaitForeverStep)
	{
		return KeptDuel {.startedIt = false, .other = other};
	}
	return std::nullopt;
}

openblack::creature_fight::Control saved_world_load::AutoFightControlOf(uint32_t autoFight)
{
	constexpr uint32_t k_Player = 1;
	constexpr uint32_t k_Computer = 2;
	using Control = openblack::creature_fight::Control;
	return autoFight == k_Computer ? Control::Computer : autoFight == k_Player ? Control::Player : Control::None;
}

std::optional<saved_world_load::KeptDance> saved_world_load::DanceOf(const gamesave::KeptCreaturePlan& plan)
{
	// the dance's sub-actions in its agenda's order
	constexpr std::array<uint32_t, 14> k_Codes {0x1D, 0x3D, 0x06, 0x0A, 0x7E, 0x0C, 0x30,
	                                            0x08, 0x50, 0x83, 0x19, 0x2C, 0x2E, 0x17};
	if (plan.count != k_Codes.size() || plan.current >= k_Codes.size() ||
	    !std::ranges::equal(plan.steps, std::span(k_Codes).subspan(plan.current), {}, &gamesave::AgendaStep::action))
	{
		return std::nullopt;
	}
	KeptDance dance {.desire = plan.desire,
	                 .action = plan.action,
	                 .objects = {plan.objects[0].number, plan.objects[1].number, plan.objects[2].number},
	                 .step = plan.current};
	const auto stepAt = [&plan](size_t index) -> const gamesave::AgendaStep* {
		return index >= plan.current ? &plan.steps[index - plan.current] : nullptr;
	};
	constexpr size_t k_GoNear = 1;
	constexpr size_t k_SetDance = 4;
	constexpr size_t k_CallDancers = 5;
	constexpr size_t k_WalkThere = 7;
	constexpr size_t k_PointThere = 8;
	constexpr size_t k_Dance = 13;
	if (const auto* step = stepAt(k_GoNear))
	{
		dance.nearDistance = step->real;
	}
	if (const auto* step = stepAt(k_SetDance))
	{
		dance.type = step->integer;
	}
	if (const auto* step = stepAt(k_CallDancers))
	{
		dance.fewestDancers = step->integer;
	}
	for (const auto index : {k_SetDance, k_WalkThere, k_PointThere})
	{
		if (const auto* step = stepAt(index))
		{
			dance.place = {step->point[0], step->point[2]};
			break;
		}
	}
	dance.danceTurns = stepAt(k_Dance)->integer;
	return dance;
}

std::optional<saved_world_load::KeptScriptTurn> saved_world_load::ScriptTurnOf(const gamesave::KeptCreaturePlan& plan)
{
	constexpr uint32_t k_ObeyPlayer = 24;
	constexpr uint32_t k_TurnToFaceObjectStep = 6;
	constexpr uint32_t k_TurnToFacePosStep = 7;
	if (plan.desire != k_ObeyPlayer || plan.action != creature_planner::k_FollowPlayerAction || plan.steps.size() != 1)
	{
		return std::nullopt;
	}
	const auto& step = plan.steps.front();
	if (step.action == k_TurnToFacePosStep)
	{
		return KeptScriptTurn {.point = glm::vec3(step.point[0], step.point[1], step.point[2])};
	}
	if (step.action == k_TurnToFaceObjectStep && step.object.number != 0)
	{
		return KeptScriptTurn {.thing = step.object.number, .seconds = step.real};
	}
	return std::nullopt;
}

bool saved_world_load::IsClearedPlan(const gamesave::KeptCreaturePlan& plan)
{
	constexpr uint32_t k_NoDesire = 40;
	return plan.desire == k_NoDesire && plan.action == 0 && plan.count == 0 && plan.steps.empty();
}

const char* saved_world_load::WhyCreatureNotMade(const PlannedCreature& planned)
{
	const auto& record = planned.record;
	const auto& fields = record.creature;
	const char* why = nullptr;
	if (!planned.species.has_value())
	{
		why = "no creature row";
	}
	else if ((record.thing.flags & (k_ThingUnavailable | k_ThingFlownByParticles)) != 0 ||
	         (record.position.flags & (k_PositionUnavailable | k_PositionInPhysics)) != 0)
	{
		why = "held or in flight";
	}
	else if (fields.arena.number != 0 && (fields.keepsPlan == 0 || !DuelOf(fields.plan).has_value()))
	{
		why = "fighting";
	}
	else if (fields.keepsPlan != 0 && !IsClearedPlan(fields.plan) && !ScriptPlayOf(fields.plan).has_value() &&
	         !ForcedPlanOf(fields.plan).has_value() && !ScriptMoveOf(fields.plan).has_value() &&
	         !DuelOf(fields.plan).has_value() && !DanceOf(fields.plan).has_value() && !ScriptTurnOf(fields.plan).has_value())
	{
		why = "carrying out a kept plan other than a script's play";
	}
	else if (!fields.queued.empty() ||
	         std::ranges::any_of(fields.spellVisuals, [](const auto& ref) { return ref.number != 0; }))
	{
		why = "holding a miracle, or with a spell waiting";
	}
	else if (record.living.reactionsDone.size() > components::ReactionRecords {}.records.size())
	{
		why = "remembering more reactions than openblack keeps";
	}
	return why;
}

entt::entity saved_world_load::MakeCreatureEntity(const PlannedCreature& planned,
                                                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	const auto& fields = record.creature;
	const char* why = WhyCreatureNotMade(planned);
	if (why != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: creature {} not made: {}", planned.number, why);
		return entt::null;
	}
	using archetypes::CreatureArchetype;
	auto species = *planned.species;
	// the local player's creature is of the species of the profile's mind file, as the game's load reads it first
	if (fields.owner == static_cast<uint8_t>(k_LocalPlayer))
	{
		const auto path = MindFileOf(fields, {});
		const auto mind = path.has_value() ? ReadMindFile(*path) : std::nullopt;
		const auto body = mind.has_value() ? creature_mind_body::FromMindFile(*mind) : std::nullopt;
		if (body.has_value() && body->species != CreatureType::Unknown)
		{
			species = body->species;
		}
	}
	const auto owner =
	    fields.owner < static_cast<uint8_t>(PlayerNames::_COUNT) ? static_cast<PlayerNames>(fields.owner) : PlayerNames::_COUNT;
	auto body = CreatureArchetype::StartBody(species);
	body.alignment = fields.alignment;
	body.strength = fields.strength;
	// made as the game's load makes it: its constructors' draws come before the seeds are put back
	const auto entity = CreatureArchetype::Create(map_coords::ToWorld(CoordsOf(record.position.position)), owner, species,
	                                              entt::id_type {0}, record.object.heading, fields.size, body);
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, record.object.creationIndex);
	RestorePlaceMarks(entity, record.position.flags);
	auto& creature = registry.Get<components::Creature>(entity);
	creature.birthTurn = record.living.birthTurn;
	creature.canDie = fields.canDie != 0;
	creature.mayGoThroughVortex = fields.mayGoThroughVortex != 0;
	// the miracles on it, as their slots were saved; each slot names its miracle once the miracles are made again
	// (LinkSpellSlots)
	auto& spells = registry.AllOf<components::CreatureSpells>(entity) ? registry.Get<components::CreatureSpells>(entity)
	                                                                  : registry.Assign<components::CreatureSpells>(entity);
	components::LoadedSpellSlots named;
	for (size_t i = 0; i < creature_spells::k_SpellCount && i < fields.spells.size(); ++i)
	{
		const auto& saved = fields.spells.at(i);
		spells.spells.slots.at(i) = {.phase = static_cast<creature_spells::Phase>(saved.phase),
		                             .turnsLeft = saved.turnsLeft,
		                             .holdTurns = saved.holdTurns,
		                             .miracle = entt::null,
		                             .before = saved.found};
		named.miracles.at(i) = saved.miracle.number;
	}
	if (std::ranges::any_of(named.miracles, [](uint32_t number) { return number != 0; }))
	{
		registry.AssignOrReplace<components::LoadedSpellSlots>(entity, named);
	}
	spells.freeze = fields.freeze;
	spells.fizz = fields.fizz;
	spells.invisible = spells.spells.IsActive(creature_spells::Spell::Invisible);
	if (fields.fizz != 0.0f || fields.fizzTarget != 0.0f || fields.fizzPerSecond != 0.0f || fields.fizzedForGood != 0)
	{
		registry.AssignOrReplace<components::CreatureFizz>(
		    entity, components::CreatureFizz {.fizz = {.now = fields.fizz,
		                                               .target = fields.fizzTarget,
		                                               .perSecond = fields.fizzPerSecond,
		                                               .goesForGood = fields.fizzedForGood != 0}});
	}
	if (auto* mind = registry.TryGet<components::CreatureMindState>(entity))
	{
		mind->paused = fields.mindProcessing == 0;
		// a script's play it was carrying out, which it takes up again once its mind is set up
		if (fields.keepsPlan != 0)
		{
			mind->loadedScriptPlay = ScriptPlayOf(fields.plan);
			// or a script's move
			mind->loadedScriptMove = ScriptMoveOf(fields.plan).value_or(std::vector<std::pair<glm::vec3, float>> {});
			// or a script's focus's turn, to its point or to the thing made again
			if (const auto turn = ScriptTurnOf(fields.plan); turn.has_value())
			{
				if (turn->point.has_value())
				{
					mind->loadedScriptTurn = openblack::creature_mind::TurnToFacePoint({turn->point->x, turn->point->z});
				}
				else if (const auto thing = objectOf(turn->thing); thing != entt::null)
				{
					// held the script's two seconds, as the save keeps them
					mind->loadedScriptTurn = openblack::creature_mind::FaceThingForScript(entt::to_integral(thing));
				}
			}
			// or a dance with villagers, its agenda made again from the saved one's steps
			if (const auto dance = DanceOf(fields.plan); dance.has_value())
			{
				const auto numberOf = [&objectOf](gamesave::ObjectNumber number) {
					const auto thing = number != 0 ? objectOf(number) : entt::entity {entt::null};
					return thing != entt::null ? std::optional(entt::to_integral(thing)) : std::nullopt;
				};
				const auto villager = numberOf(dance->objects[1]);
				auto agenda = openblack::creature_dance::Agenda(
				    {.villager = villager.value_or(0),
				     .place = dance->place,
				     .type = dance->type,
				     .fewestDancers = dance->fewestDancers,
				     .height = dance->nearDistance / openblack::creature_dance::k_GoNearHeights});
				agenda.back().dance.count = dance->danceTurns;
				agenda[1].movement.maxDistance = dance->nearDistance;
				mind->loadedForcedPlan =
				    components::LoadedForcedPlan {.action = dance->action,
				                                  .desire = static_cast<creature_desires::Desire>(dance->desire),
				                                  .activityObject = numberOf(dance->objects[0]),
				                                  .object = villager,
				                                  .instrument = numberOf(dance->objects[2]),
				                                  .step = dance->step,
				                                  .count = agenda.size(),
				                                  .agenda = std::move(agenda)};
			}
			// or a plan forced on it, what it is about by the things made again
			if (const auto forced = ForcedPlanOf(fields.plan); forced.has_value())
			{
				const auto numberOf = [&objectOf](gamesave::ObjectNumber number) {
					const auto thing = number != 0 ? objectOf(number) : entt::entity {entt::null};
					return thing != entt::null ? std::optional(entt::to_integral(thing)) : std::nullopt;
				};
				mind->loadedForcedPlan = components::LoadedForcedPlan {.action = forced->action,
				                                                       .activityObject = numberOf(forced->objects[0]),
				                                                       .object = numberOf(forced->objects[1]),
				                                                       .instrument = numberOf(forced->objects[2]),
				                                                       .step = forced->step,
				                                                       .count = forced->count,
				                                                       .turns = forced->turns,
				                                                       .seconds = forced->seconds};
			}
		}
	}
	registry.AssignOrReplace<components::CreatureSizeLimits>(
	    entity, components::CreatureSizeLimits {.limits = {.smallest = fields.smallestSize, .largest = fields.largestSize}});
	// where the leash last sent it, and its home
	if (auto* leash = registry.TryGet<components::CreatureLeash>(entity))
	{
		leash->targetPoint = {fields.targetPoint[0], fields.targetPoint[1], fields.targetPoint[2]};
		leash->targetThing = objectOf(fields.targetObject.number);
		leash->hasTarget = fields.hasTarget != 0;
	}
	if (fields.home != gamesave::MapCoords {} && Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetHome(entity, map_coords::ToWorld(CoordsOf(fields.home)));
	}
	registry.AssignOrReplace<components::CreatureFightHealth>(entity,
	                                                          components::CreatureFightHealth {.health = fields.fightHealth});
	if (fields.autoScale != 0)
	{
		registry.AssignOrReplace<components::CreatureAutoscale>(
		    entity, components::CreatureAutoscale {.enabled = true, .factor = fields.autoScaleShare});
	}
	// the glows of its creed powers, each place in turn given its saved power (none where it is 0); the creature hums
	// when one of them has power
	std::array<creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount> creed {};
	for (size_t i = 0; i < fields.creedPowers.size() && i < creed.size(); ++i)
	{
		creed.at(i) = {.scale = fields.creedPowers.at(i).scale, .power = fields.creedPowers.at(i).power};
	}
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().LoadCreedPowers(entity, creed);
	}
	else
	{
		auto& glows = registry.AllOf<components::CreatureHandGlows>(entity)
		                  ? registry.Get<components::CreatureHandGlows>(entity)
		                  : registry.Assign<components::CreatureHandGlows>(entity);
		static_cast<void>(creature_hand_glow::LoadCreedPowers(glows.glows, creed));
	}
	// the reactions it did and their turns, oldest first, as its record lists them
	if (!record.living.reactionsDone.empty())
	{
		auto& memory = registry.AllOf<components::ReactionRecords>(entity)
		                   ? registry.Get<components::ReactionRecords>(entity)
		                   : registry.Assign<components::ReactionRecords>(entity);
		memory = {};
		for (const auto& done : record.living.reactionsDone)
		{
			memory.records.at(memory.count++) = {.type = static_cast<uint8_t>(done.reaction), .turn = done.turn};
		}
	}
	// the villagers it called to dance with it, the last called first, that the load made (a dancer the load did not
	// make is left out); the dance it made last is named once the dances are made
	auto& danceOf = registry.AllOf<components::CreatureDance>(entity) ? registry.Get<components::CreatureDance>(entity)
	                                                                  : registry.Assign<components::CreatureDance>(entity);
	danceOf.dancers.clear();
	for (const auto& partner : fields.dancePartners.numbers)
	{
		if (const auto dancer = objectOf(partner); dancer != entt::null)
		{
			danceOf.dancers.push_back(dancer);
		}
	}
	registry.Assign<components::LoadedRecord<gamesave::CreatureRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeMobileStaticEntity(const PlannedMobileStatic& object)
{
	const auto& base = object.base;
	const auto& coords = base.position.position;
	// on the land under its place, raised by its altitude, at its X, Y and Z angles
	const auto ground = map_coords::ToWorld(map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = 0.0f});
	const auto entity = archetypes::MobileStaticArchetype::Create(
	    ground, object.type, coords.altitude, std::bit_cast<float>(object.mobileStatic.words[0]), base.object.heading,
	    std::bit_cast<float>(object.mobileStatic.words[1]), base.object.scale);
	auto& registry = Locator::entitiesRegistry::value();
	if (object.player.has_value())
	{
		registry.Get<components::MobileStatic>(entity).owner = *object.player;
	}
	if (base.multi.percentBuilt != 0.0f)
	{
		registry.AssignOrReplace<components::StaticBuiltPercentage>(entity, base.multi.percentBuilt);
	}
	std::visit([entity](const auto& record) { KeepSaved(entity, record); }, object.record);
	return entity;
}

entt::entity saved_world_load::MakeAnimatedStaticEntity(const PlannedAnimatedStatic& object)
{
	const auto& record = object.record;
	const auto& base = record.base;
	const auto& coords = base.position.position;
	const auto ground = map_coords::ToWorld(map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = 0.0f});
	const auto entity =
	    archetypes::AnimatedStaticArchetype::Create(ground, object.type, base.object.heading, base.object.scale);
	auto& registry = Locator::entitiesRegistry::value();
	auto& still = registry.Get<components::AnimatedStatic>(entity);
	still.openState = std::bit_cast<int32_t>(record.word);
	// each slot keeps the model of the stone laid in it, 0 when empty; the made plinth lays them again slot by slot,
	// each in its first empty slot, as the game does once it is loaded
	for (const auto slot : record.frames.values | std::views::take(animated_static::k_GateStoneSlots))
	{
		if (slot != 0)
		{
			[[maybe_unused]] const bool laid = animated_static::AddGateStone(still.gateStones, static_cast<MeshId>(slot));
		}
	}
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeDeadTreeEntity(const PlannedDeadTree& planned)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& base =
	    std::visit([](const auto& record) -> const gamesave::MultiMapFixedFields& { return record.base; }, planned.record);
	const auto& part =
	    std::visit([](const auto& record) -> const gamesave::MobileStaticPart& { return record.mobileStatic; }, planned.record);
	const auto& coords = base.position.position;
	// on the land under its place, raised by its altitude
	auto at = map_coords::ToWorld(map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = 0.0f});
	at.y += coords.altitude;
	const auto entity = archetypes::DeadTreeArchetype::Create(at, planned.type, planned.player.value_or(PlayerNames::NEUTRAL),
	                                                          base.object.life, std::bit_cast<float>(part.words[0]),
	                                                          base.object.heading, std::bit_cast<float>(part.words[1]));
	if (entity == entt::null)
	{
		return entity;
	}
	std::visit(
	    [&registry, entity](const auto& record) {
		    registry.Get<components::DeadTree>(entity).woodValueMultiplier = std::bit_cast<float>(record.word);
	    },
	    planned.record);
	if (std::holds_alternative<gamesave::FelledTreeRecord>(planned.record))
	{
		registry.AssignOrReplace<components::FelledTree>(entity);
	}
	if (planned.inHand && map_cells::IsObjectInMap(entity))
	{
		map_cells::RemoveMapObject(entity);
	}
	std::visit(
	    [&registry, entity](const auto& record) {
		    RestorePlaceMarks(entity, record.base.position.flags);
		    registry.AssignOrReplace<components::ObjectCreationIndex>(entity, record.base.object.creationIndex);
		    using Record = std::decay_t<decltype(record)>;
		    registry.Assign<components::LoadedRecord<Record>>(entity, record);
	    },
	    planned.record);
	return entity;
}

std::optional<openblack::FeatureInfo> saved_world_load::FeatureTypeOfInfo(uint32_t info)
{
	constexpr uint32_t k_FirstFeatureRow = 2064;
	return RowOf<FeatureInfo>(info, k_FirstFeatureRow);
}

entt::entity saved_world_load::MakeBuildingSiteEntity(const PlannedBuildingSite& planned, entt::entity building,
                                                      const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (building == entt::null || !registry.Valid(building))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: building site {} not made: its building is not made", planned.number);
		return entt::null;
	}
	const auto site = registry.Create();
	auto& made = registry.Assign<components::BuildingSite>(site);
	// the game's list of building sites holds the loaded ones as saved; a site made since goes at its head
	made.creationStamp = ++registry.Context().siteCreationCounter;
	made.root = building;
	std::visit(
	    [&](const auto& record) {
		    using Record = std::decay_t<decltype(record)>;
		    // its scaffolds (made already, as moving objects), each added at the end in the file's order
		    for (const auto number : record.scaffolds.numbers)
		    {
			    if (const auto scaffold = objectOf(number);
			        scaffold != entt::null && registry.AllOf<components::Scaffold>(scaffold))
			    {
				    made.scaffolds.push_back(scaffold);
			    }
		    }
		    // the second list and the word only a site's making sets (empty and 0); openblack keeps neither
		    if (!record.third.numbers.empty() || record.word != 0)
		    {
			    SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                       "Load: building site {}: its second list of {} and its word {} are not read", planned.number,
			                       record.third.numbers.size(), record.word);
		    }
		    static_assert(sizeof(made.ring) == sizeof(record.positions));
		    std::memcpy(made.ring.data(), record.positions.data(), sizeof(made.ring));
		    made.builderCount = static_cast<int32_t>(record.secondWord);
		    made.isRepairSite = record.thirdWord != 0;
		    made.desireBoost = record.forcedDesire;
		    made.repairBase = record.life;
		    // a standard site's own wood pile; a workshop's site has none of its own (its pile is the workshop's); a
		    // citadel's its six by their slots
		    if constexpr (std::is_same_v<Record, gamesave::StandardBuildingSiteRecord>)
		    {
			    made.kind = components::BuildingSite::Kind::Standard;
			    made.woodPile = objectOf(record.woodPile.number);
		    }
		    else if constexpr (std::is_same_v<Record, gamesave::WorkshopBuildingSiteRecord>)
		    {
			    made.kind = components::BuildingSite::Kind::Workshop;
		    }
		    else
		    {
			    made.kind = components::BuildingSite::Kind::Standard;
			    auto& citadel = registry.Assign<components::CitadelBuildingSite>(site);
			    for (size_t i = 0; i < citadel.piles.size() && i < record.piles.numbers.size(); ++i)
			    {
				    citadel.piles.at(i) = objectOf(record.piles.numbers.at(i));
			    }
		    }
		    registry.Assign<components::LoadedRecord<Record>>(site, record);
	    },
	    planned.record);
	// the building links back to its site
	if (auto* abode = registry.TryGet<components::Abode>(building); abode != nullptr)
	{
		abode->buildingSite = site;
	}
	if (auto* part = registry.TryGet<components::CitadelPartBuild>(building); part != nullptr)
	{
		part->buildingSite = site;
	}
	// the partly built model, as a site's building draws
	abodes::RedrawConstruction(building);
	return site;
}

entt::entity saved_world_load::MakeTotemStatueEntity(const PlannedTotemStatue& totem, entt::entity townCentre)
{
	const auto& record = totem.record;
	const auto& object = record.base.object;
	auto& registry = Locator::entitiesRegistry::value();
	if (townCentre != entt::null && !registry.Valid(townCentre))
	{
		townCentre = entt::null;
	}
	const auto plinth = archetypes::VillageTotemArchetype::CreateAt(
	    townCentre, static_cast<size_t>(totem.tribe), map_coords::ToWorld(CoordsOf(record.base.position.position)),
	    object.heading, object.scale);
	if (plinth == entt::null)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: totem statue {} not made: its tribe has no totem statue row",
		                   totem.number);
		return entt::null;
	}
	registry.AssignOrReplace<components::ObjectCreationIndex>(plinth, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<components::Life>(plinth, object.life);
	}
	RestorePlaceMarks(plinth, record.base.position.flags);
	auto& statue = registry.Get<components::VillageTotem>(plinth);
	statue.percentBuilt = record.base.multi.percentBuilt;
	// its town centre has it as its totem when the centre's own record names it
	const auto* centre = townCentre != entt::null
	                         ? registry.TryGet<const components::LoadedRecord<gamesave::TownCentreRecord>>(townCentre)
	                         : nullptr;
	statue.onCentre = townCentre != entt::null && (centre == nullptr || centre->record.totemStatue.number == totem.number);
	// the worship share it was saved rising to, where it stands at once: its load puts the share's ease there, still;
	// its resolve sets the hand's share to where it stands
	const auto share = std::bit_cast<float>(record.words.at(0));
	auto& worship = registry.AssignOrReplace<components::TotemWorship>(plinth);
	worship.percentage = share;
	worship.rise.SetPosition(share);
	worship.rising = false;
	worship.lastMs = game_clock::EngineMs();
	statue.held = share;
	registry.Assign<components::LoadedRecord<gamesave::TotemStatueRecord>>(plinth, record);
	return plinth;
}

entt::entity saved_world_load::MakeTownCentreSpellIconEntity(const PlannedTownCentreSpellIcon& planned, entt::entity town,
                                                             entt::entity townCentre)
{
	const auto& record = planned.record;
	const auto& base = record.icon.base;
	auto& registry = Locator::entitiesRegistry::value();
	if (townCentre != entt::null && !registry.Valid(townCentre))
	{
		townCentre = entt::null;
	}
	// its slot: where its town centre's record names it; one the record does not name is in no slot
	std::optional<uint8_t> slot;
	if (const auto* centre = townCentre != entt::null
	                             ? registry.TryGet<const components::LoadedRecord<gamesave::TownCentreRecord>>(townCentre)
	                             : nullptr;
	    centre != nullptr)
	{
		const auto& icons = centre->record.icons.numbers;
		for (size_t i = 0; i < icons.size() && i < components::TownCentreIcons {}.icons.size(); ++i)
		{
			if (icons.at(i) == planned.number)
			{
				slot = static_cast<uint8_t>(i);
			}
		}
	}
	if (!slot.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: town centre spell icon {} is in no slot of its town centre",
		                   planned.number);
	}
	constexpr uint8_t k_TownIconRow = 1;
	const auto& iconInfo = Locator::infoConstants::value().spellIcon.at(k_TownIconRow);
	const auto position = map_coords::ToWorld(CoordsOf(base.position.position));
	const auto entity = registry.Create();
	registry.Assign<components::ObjectCreationIndex>(entity, base.object.creationIndex);
	registry.Assign<components::Transform>(entity, position, affine::AngleY(base.object.heading), glm::vec3(base.object.scale));
	registry.Assign<components::Mesh>(entity, resources::HashIdentifier(iconInfo.meshId), static_cast<int8_t>(0),
	                                  static_cast<int8_t>(0));
	auto& spellIcon = registry.Assign<components::SpellIcon>(entity);
	spellIcon.infoIndex = k_TownIconRow;
	spellIcon.seedType = planned.seed;
	spellIcon.player = town != entt::null ? openblack::worship::town::OwnerOf(town) : PlayerNames::NEUTRAL;
	auto& ofTown = registry.Assign<components::TownCentreSpellIcon>(entity, town, townCentre, slot.value_or(0));
	int level = -1;
	for (size_t i = 0; i < ofTown.powerUps.size() && i < record.words.values.size(); ++i)
	{
		ofTown.powerUps.at(i) = record.words.values.at(i) != 0;
		level = ofTown.powerUps.at(i) ? static_cast<int>(i) : level;
	}
	openblack::worship::town_centre::ConnectDeletionListener();
	if (slot.has_value())
	{
		if (registry.TryGet<components::TownCentreIcons>(townCentre) == nullptr)
		{
			registry.Assign<components::TownCentreIcons>(townCentre);
		}
		registry.Get<components::TownCentreIcons>(townCentre).icons.at(*slot) = entity;
	}
	// its seed's graphic 1 m above the icon's top point, showing the highest power-up level the icon holds
	const auto top = openblack::worship::GetSpecialPoint(entity, 0);
	const auto graphic = openblack::worship::seed_graphic::Create(
	    (top.has_value() ? top->position : position) + glm::vec3(0.0f, 1.0f, 0.0f), planned.seed, spellIcon.player, 1.0f, -1);
	openblack::worship::seed_graphic::SetAutoUpdate(graphic, false);
	registry.Get<components::SpellIcon>(entity).graphic = graphic;
	openblack::worship::seed_graphic::SetPowerUpType(graphic, level);
	RestorePlaceMarks(entity, base.position.flags);
	registry.Assign<components::LoadedRecord<gamesave::TownCentreSpellIconRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeStreetLanternEntity(const PlannedStreetLantern& lantern)
{
	const auto& record = lantern.record;
	const auto& coords = record.position.position;
	// on the land under its place, as its making puts it, with no look for another object near it
	const auto ground = map_coords::ToWorld(map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = 0.0f});
	const auto entity = archetypes::StreetLanternArchetype::CreateLoaded(ground, lantern.type, record.word != 0);
	auto& registry = Locator::entitiesRegistry::value();
	RestorePlaceMarks(entity, record.position.flags);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, record.object.creationIndex);
	if (record.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, record.object.life);
	}
	registry.Assign<components::LoadedRecord<gamesave::StreetLanternRecord>>(entity, record);
	// each lantern's load reads the lanterns' sounds' flag, so the last read holds
	audio::lantern_sounds::SetOn(record.mark != 0);
	return entity;
}

entt::entity saved_world_load::MakeRewardEntity(const PlannedReward& reward, entt::entity town)
{
	if (!Locator::rewardSystem::has_value())
	{
		return entt::null;
	}
	const auto& record = reward.record;
	const auto& base = record.base;
	const auto& words = record.words;
	const auto state = words[k_RewardState] == 2   ? components::Reward::State::Opened
	                   : words[k_RewardState] == 1 ? components::Reward::State::Landed
	                                               : components::Reward::State::Falling;
	const auto entity =
	    Locator::rewardSystem::value().Restore({.position = map_coords::ToWorld(CoordsOf(base.position.position)),
	                                            .heading = base.object.heading,
	                                            .scale = base.object.scale,
	                                            .type = reward.type,
	                                            .player = reward.player,
	                                            .town = town,
	                                            .state = state,
	                                            .opened = words[k_RewardOpened] != 0,
	                                            .seconds = std::bit_cast<float>(words[k_RewardClock]),
	                                            .goesIntoMap = words[k_RewardGoesIntoMap] != 0,
	                                            .dustMilliseconds = words[k_RewardDust],
	                                            .landed = words[k_RewardLanded] != 0});
	auto& registry = Locator::entitiesRegistry::value();
	RestorePlaceMarks(entity, base.position.flags);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, base.object.creationIndex);
	if (base.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, base.object.life);
	}
	registry.Assign<components::LoadedRecord<gamesave::RewardRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeBallEntity(const PlannedBall& ball, entt::entity town)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& record = ball.record;
	const auto& base = record.base;
	const auto position = map_coords::ToWorld(CoordsOf(base.position.position));
	const auto entity = archetypes::BallArchetype::CreateLoaded(position, base.object.heading, base.object.scale);
	auto& registry = Locator::entitiesRegistry::value();
	// at its x, y and z angles: made upright, then leaning as saved
	const glm::vec3 angles {base.mobileObject.xAngle, base.object.heading, base.mobileObject.zAngle};
	if (angles.x != 0.0f || angles.z != 0.0f)
	{
		registry.Get<components::Transform>(entity).rotation =
		    archetypes::MobileStaticArchetype::XYZRotation(angles.x, angles.y, angles.z);
		object_placement::PlaceYXZ(registry, entity, position, angles, base.object.scale);
		placed_turn::KeepAngles(registry, entity, angles);
	}
	auto& made = registry.Get<components::Ball>(entity);
	made.point = {record.point[0], record.point[1], record.point[2]};
	made.previousPoint = {record.previousPoint[0], record.previousPoint[1], record.previousPoint[2]};
	// its town as saved; whether that town has taken it is not saved, so it is free to be picked up again
	made.town = town;
	made.owned = false;
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeFeatureEntity(const PlannedFeature& planned)
{
	const auto& record = planned.record;
	const auto& object = record.base.object;
	const auto entity = archetypes::FeatureArchetype::Create(map_coords::ToWorld(CoordsOf(record.base.position.position)),
	                                                         planned.type, object.heading, object.scale);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<components::Feature>(entity).percentBuilt = record.base.multi.percentBuilt;
	KeepSaved(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeStoneBaseEntity(const PlannedStoneBase& planned)
{
	const auto& record = planned.record;
	const auto& coords = record.position.position;
	const auto ground = map_coords::ToWorld(map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = 0.0f});
	const auto entity = archetypes::MobileStaticArchetype::Create(ground, MobileStaticInfo::SingingStoneBase, coords.altitude,
	                                                              0.0f, record.object.heading, 0.0f, record.object.scale);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the thing's marks of an object out of play and of a hidden name
	constexpr uint8_t k_Unavailable = 0x01;
	constexpr uint8_t k_NameHidden = 0x40;
	if ((record.thing.flags & k_Unavailable) != 0)
	{
		registry.AssignOrReplace<components::Unavailable>(entity);
	}
	if ((record.thing.flags & k_NameHidden) != 0)
	{
		registry.AssignOrReplace<components::NameHidden>(entity);
	}
	RestorePlaceMarks(entity, record.position.flags);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, record.object.creationIndex);
	if (record.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, record.object.life);
	}
	registry.Assign<components::LoadedRecord<gamesave::BaseOnlyRecord>>(entity, record);
	return entity;
}

namespace
{
/// The scaffold's plan as its record holds it: where it is to stand, its angle and scale, whether it is available, the
/// turn it was made, and the building it offers (none for no row); a row that is no abode's is logged and offers none
openblack::ecs::components::ScaffoldPlan ScaffoldPlanOf(const openblack::gamesave::PlannedAbodeRecord& record)
{
	using namespace openblack;
	constexpr uint32_t k_NoRow = 0xFFFFFFFF;
	const auto& fields = record.planned;
	ecs::components::ScaffoldPlan plan {.position = CoordsOf(fields.position.position),
	                                    .yAngle = fields.yAngle,
	                                    .scale = fields.scale,
	                                    .available = (record.thing.flags & k_ThingUnavailable) == 0,
	                                    .creationTurn = static_cast<uint32_t>(fields.creationTurn)};
	if (fields.info == k_NoRow)
	{
		return plan;
	}
	const auto type = ecs::saved_world_load::AbodeTypeOfInfo(fields.info);
	if (!type.has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: a scaffold's plan of the row {} offers no building: no abode's row",
		                   fields.info);
		return plan;
	}
	plan.info = &Locator::infoConstants::value().abode.at(static_cast<size_t>(*type));
	return plan;
}

/// A scaffold's own values as its record holds them, as the original's load reads them: its plan and candidate, the
/// cross-fade's timer, the phantom's angle and scale, its flags and the building it may offer; it counts as placed
/// (the game turn 1). Its building site, workshop, town and holder are put back once the building sites are made
void RestoreScaffold(openblack::ecs::components::Scaffold& scaffold, const openblack::gamesave::ScaffoldRecord& record,
                     const openblack::ecs::saved_world_load::PlannedScaffold& planned)
{
	const auto planOf = [](const std::optional<openblack::gamesave::PlannedAbodeRecord>& saved) {
		return saved.has_value() ? std::optional(ScaffoldPlanOf(*saved)) : std::nullopt;
	};
	scaffold.plan = planOf(planned.plans.at(0));
	scaffold.candidate = planOf(planned.plans.at(1));
	scaffold.phantomMesh = 0;
	scaffold.fade = static_cast<int32_t>(record.firstWord);
	scaffold.phantomAngle = std::bit_cast<float>(record.secondWord);
	scaffold.phantomScale = std::bit_cast<float>(record.thirdWord);
	scaffold.flags = record.half;
	scaffold.limit = static_cast<int32_t>(record.lastWord);
	scaffold.placedTurn = 1;
}
} // namespace

entt::entity saved_world_load::MakeMobileObjectEntity(const PlannedMobileObject& object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& base =
	    std::visit([](const auto& record) -> const gamesave::MobileObjectFields& { return record.base; }, object.record);
	const auto position = map_coords::ToWorld(CoordsOf(base.position.position));
	entt::entity entity = entt::null;
	if (const auto* orb = std::get_if<gamesave::OneOffSpellSeedRecord>(&object.record); orb != nullptr)
	{
		// its power-up, its seed and its multiplier, as the orb writes them
		entity =
		    archetypes::OneOffSpellSeedArchetype::Create(position, static_cast<SpellSeedType>(orb->words[1]),
		                                                 static_cast<int>(orb->words[0]), std::bit_cast<float>(orb->words[2]));
	}
	else
	{
		// at its x, y and z angles: the archetype places it upright, then it leans as saved; a scaffold is made as a
		// new one is, with its saved values put back below
		const auto* scaffold = std::get_if<gamesave::ScaffoldRecord>(&object.record);
		entity = scaffold != nullptr
		             ? scaffolds::Create(CoordsOf(base.position.position), entt::null, entt::null, base.object.heading,
		                                 base.object.scale)
		             : archetypes::MobileObjectArchetype::Create(position, object.type, base.object.heading, base.object.scale);
		if (scaffold != nullptr && entity != entt::null)
		{
			RestoreScaffold(registry.Get<components::Scaffold>(entity), *scaffold, object.scaffold);
		}
		const glm::vec3 angles {base.mobileObject.xAngle, base.object.heading, base.mobileObject.zAngle};
		if (entity != entt::null && (angles.x != 0.0f || angles.z != 0.0f))
		{
			registry.Get<components::Transform>(entity).rotation =
			    archetypes::MobileStaticArchetype::XYZRotation(angles.x, angles.y, angles.z);
			object_placement::PlaceYXZ(registry, entity, position, angles, base.object.scale);
			placed_turn::KeepAngles(registry, entity, angles);
		}
	}
	if (entity == entt::null)
	{
		return entity;
	}
	// a moving object enters the map as it is made, unless it is held; an orb's maker leaves that to its caller
	const bool inMap = map_cells::IsObjectInMap(entity);
	if (object.inHand && inMap)
	{
		map_cells::RemoveMapObject(entity);
	}
	else if (!object.inHand && !inMap)
	{
		map_cells::InsertMapObject(entity);
	}
	std::visit([entity](const auto& record) { KeepSaved(entity, record); }, object.record);
	return entity;
}

entt::entity saved_world_load::MakeSpellSeedEntity(const PlannedSpellSeed& planned, entt::entity icon, entt::entity creator)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& record = planned.record;
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), planned.type);
	const auto entity = archetypes::SpellSeedArchetype::Create(map_coords::ToWorld(CoordsOf(record.position.position)),
	                                                           planned.type, info.scale);
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = registry.Assign<components::SpellSeed>(entity);
	seed.seedType = planned.type;
	seed.powerUp = static_cast<int>(record.word);
	seed.flags = record.byte;
	seed.icon = icon;
	// as every seed the game makes
	seed.inInterface = true;
	// its creator: a player, the icon that charged it, or none; the player of its interface's status in every case
	using Kind = components::SpellCreator::Kind;
	if (planned.creatorPlayer.has_value())
	{
		seed.creator = {.kind = Kind::Player, .player = *planned.creatorPlayer};
	}
	else if (creator != entt::null)
	{
		seed.creator = {.kind = Kind::WorshipSpellIcon, .entity = creator};
	}
	if (planned.statusPlayer.has_value())
	{
		seed.creator.player = *planned.statusPlayer;
	}
	seed.hasCast = record.bytes[k_SeedHasCast] != 0;
	seed.learnedFrom = record.bytes[k_SeedLearnedFrom] != 0;
	seed.chantStore = std::bit_cast<float>(record.words[k_SeedChantStore]);
	seed.chantStoreCopy = std::bit_cast<float>(record.words[k_SeedChantStoreCopy]);
	seed.storedChants = std::bit_cast<float>(record.words[k_SeedStoredChants]);
	seed.storedAge = std::bit_cast<float>(record.words[k_SeedStoredAge]);
	seed.storedMaxObjects = static_cast<int>(record.words[k_SeedStoredMaxObjects]);
	seed.power = std::bit_cast<float>(record.words[k_SeedPower]);
	seed.castMultiplier = std::bit_cast<float>(record.words[k_SeedCastMultiplier]);
	seed.ready = record.flag != 0;
	seed.turnsInHand = static_cast<int>(record.lastWords[k_SeedTurnsInHand]);
	seed.lastMagic = static_cast<MagicType>(record.lastWords[k_SeedLastMagic]);
	seed.fromOneShot = record.lastFlag != 0;
	seed.madeAt = CoordsOf(record.object.coords);
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, record.object.creationIndex);
	if (record.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, record.object.life);
	}
	registry.Assign<components::LoadedRecord<gamesave::SpellSeedRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeFireBallEntity(const PlannedFireBall& planned)
{
	const auto& record = planned.record;
	const auto entity = magic::fireball::Restore({.position = map_coords::ToWorld(CoordsOf(record.position.position)),
	                                              .madeAt = CoordsOf(record.object.coords),
	                                              .infoRow = planned.infoRow,
	                                              .scale = record.object.scale,
	                                              .affectedByRain = record.affectedByRain != 0,
	                                              .creationIndex = record.object.creationIndex});
	auto& registry = Locator::entitiesRegistry::value();
	if (record.object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, record.object.life);
	}
	registry.Assign<components::LoadedRecord<gamesave::MagicFireBallRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeVortexEntity(const PlannedVortex& planned,
                                                const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto type = std::holds_alternative<gamesave::LandscapeVortexInRecord>(planned.record)    ? VortexType::In
	                  : std::holds_alternative<gamesave::LandscapeVortexOutRecord>(planned.record) ? VortexType::Out
	                                                                                               : VortexType::Volcano;
	const auto& point = std::visit([](const auto& record) -> const gamesave::Point& { return record.point; }, planned.record);
	// a way in's object mover made again from its records, its carriers carrying the things already made; when it
	// cannot be, the vortex starts a mover of its own, as before, with one log line
	std::unique_ptr<particles::Effect> mover;
	if (planned.mover.has_value() && Locator::particleSystem::has_value())
	{
		const auto file = particles::ParticleTypeFile(static_cast<ParticleType>(planned.mover->manager.persistent.first));
		std::string why = file.empty() ? std::string("its particle type has no file") : std::string();
		if (!file.empty())
		{
			// the players its manager and its carried things name, as the plan resolved them
			const auto& players = planned.mover->players;
			const auto playerOf = [&players](gamesave::ObjectNumber number) {
				const auto found = players.find(number);
				return found != players.end() ? found->second : -1;
			};
			mover = saved_particles::RestoreEffect(*planned.mover, particles::File::Load(std::string(file)),
			                                       {.playerOf = playerOf, .entityOf = objectOf}, why);
		}
		if (mover == nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: vortex {}'s object mover is not made: {}", planned.number, why);
		}
	}
	const auto entity =
	    vortex::Create(glm::vec3 {point[0], point[1], point[2]}, type, vortex::Making::Loaded, std::move(mover));
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::visit(
	    [&registry, entity](const auto& record) {
		    auto& made = registry.Get<components::LandscapeVortex>(entity);
		    made.state = static_cast<VortexStateType>(record.words[1]);
		    made.radius = std::bit_cast<float>(record.words[2]);
		    made.meanAltitude = std::bit_cast<float>(record.words[3]);
		    made.savedAltitudes = record.bytes.values;
		    // its name hidden, as a script's vortex is made
		    constexpr uint8_t k_NameHidden = 0x40;
		    if ((record.thing.flags & k_NameHidden) != 0)
		    {
			    registry.AssignOrReplace<components::NameHidden>(entity);
		    }
		    KeepSaved(entity, record);
	    },
	    planned.record);
	if (const auto* out = std::get_if<gamesave::LandscapeVortexOutRecord>(&planned.record); out != nullptr)
	{
		constexpr size_t k_StatObjects = 0;
		constexpr size_t k_StatResources = 2;
		constexpr size_t k_StatVillagers = 3;
		auto& made = registry.Get<components::LandscapeVortex>(entity);
		made.animalFlock = objectOf(out->animalFlock.number);
		made.flock = objectOf(out->flockToJoin.number);
		made.flockPosition = map_coords::ToWorld(CoordsOf(out->flockCoords));
		made.flockA = out->flockDistance;
		made.flockB = out->domainRadius;
		made.town = objectOf(out->town.number);
		const auto statOf = [out](size_t stat) { return stat < out->created.values.size() ? out->created.values[stat] : 0U; };
		made.statObjects = statOf(k_StatObjects);
		made.statResources = statOf(k_StatResources);
		made.statVillagers = statOf(k_StatVillagers);
		made.thrownVillagers.clear();
		for (const auto number : out->thrown.numbers)
		{
			if (const auto thrown = objectOf(number); thrown != entt::null)
			{
				made.thrownVillagers.push_back(thrown);
			}
		}
		// its reader: none once it had read the whole file, else on past the lines it had read
		if (out->saver.number == 0)
		{
			made.reader.reset();
		}
		else if (made.reader != nullptr && planned.linesRead.has_value())
		{
			vortex_save::SkipLines(*made.reader, *planned.linesRead);
		}
	}
	return entity;
}

entt::entity saved_world_load::MakeDesireFlagEntity(const PlannedDesireFlag& flag, entt::entity town)
{
	const auto& record = flag.record;
	const auto& object = record.object;
	if (record.desireType >= components::k_TownDesireCount)
	{
		return entt::null;
	}
	const auto point = town_desire_flags::PointOf(record.slot);
	const auto position = map_coords::ToWorld(CoordsOf(record.position.position));
	const auto entity =
	    archetypes::TownDesireFlagArchetype::Create(town, static_cast<TownDesireInfo>(record.desireType), point.value_or(0),
	                                                static_cast<MeshId>(record.mesh), object.scale, position);
	auto& registry = Locator::entitiesRegistry::value();
	auto& made = registry.Get<components::TownDesireFlag>(entity);
	// where it was made: the object's own coordinates, which the game sets once
	made.madeAt = map_coords::ToWorld(CoordsOf(object.coords));
	made.overGround = record.position.position.altitude;
	made.storedDesire = record.desire;
	// the angle it keeps as saved, which its turn need not give back
	placed_turn::KeepAngles(registry, entity, glm::vec3(0.0f, object.heading, 0.0f));
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	registry.Assign<components::LoadedRecord<gamesave::TownDesireFlagsRecord>>(entity, record);
	// its town keeps it by its desire
	if (registry.Valid(town))
	{
		if (!registry.AllOf<components::TownDesireFlags>(town))
		{
			components::TownDesireFlags none;
			none.flags.fill(entt::null);
			registry.AssignState<components::TownDesireFlags>(town, none);
		}
		registry.Get<components::TownDesireFlags>(town).flags.at(record.desireType) = entity;
	}
	return entity;
}

entt::entity saved_world_load::MakeStreamEntity(const PlannedStream& stream)
{
	const auto& record = stream.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// its id, then its points: each node keeps its place as a point (x, the ground's height, z) and a word of the game's
	// memory
	const auto id = static_cast<components::Stream::Id>(record.words[0]);
	auto& made = registry.Assign<components::Stream>(
	    entity, components::Stream {.id = id, .creationStamp = ++registry.Context().streamCreationCounter});
	made.points.reserve(record.nodes.values.size());
	for (const auto& node : record.nodes.values)
	{
		std::array<float, 3> point {};
		std::memcpy(point.data(), node.data(), sizeof(point));
		made.points.emplace_back(point[0], point[1], point[2]);
	}
	registry.Context().streams.insert_or_assign(id, entity);
	registry.Assign<components::LoadedRecord<gamesave::StreamRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeWeatherThingEntity(const PlannedWeatherThing& thing)
{
	const auto& record = thing.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// at its place on the map, as a thing made by a script stands
	const auto& coords = record.position.position;
	registry.Assign<components::Transform>(entity,
	                                       glm::vec3(map_coords::ToMetres(coords.x), 0.0f, map_coords::ToMetres(coords.z)),
	                                       glm::mat3(1.0f), glm::vec3(1.0f));
	auto& made = registry.Assign<components::WeatherThing>(entity);
	made.descriptor = StormDescriptorOf(record.descriptor);
	made.affectedByWind = record.affectedByWind != 0;
	// its storm: made from its saved descriptor (at the head of the storms' list), then its age, speed and target
	if (record.storm.storm.has_value())
	{
		const auto& saved = *record.storm.storm;
		made.storm = weather::storms::Create(StormDescriptorOf(saved.descriptor));
		if (auto* storm = weather::storms::Find(made.storm))
		{
			storm->age = std::bit_cast<float>(saved.age);
			storm->speed = std::bit_cast<float>(saved.speed);
			storm->target = glm::vec3(saved.target[0], saved.target[1], saved.target[2]);
		}
	}
	weather::weather_thing::AddLoaded(entity);
	registry.Assign<components::LoadedRecord<gamesave::WeatherThingRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeScriptTimerEntity(const PlannedScriptTimer& timer)
{
	constexpr uint8_t k_NameHidden = 0x40;
	const auto& record = timer.record;
	// a thing with its timer, as a script makes one, set as saved
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<components::ScriptTimer>(
	    entity,
	    components::ScriptTimer {.timer = {.setTurn = record.words[0], .turns = static_cast<int32_t>(record.words[1])}});
	if ((record.thing.flags & k_NameHidden) != 0)
	{
		registry.AssignOrReplace<components::NameHidden>(entity);
	}
	RestorePlaceMarks(entity, record.position.flags);
	registry.Assign<components::LoadedRecord<gamesave::ScriptTimerRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeScriptMarkerEntity(const PlannedScriptMarker& marker)
{
	constexpr uint8_t k_NameHidden = 0x40;
	const auto& record = marker.record;
	const auto entity = archetypes::ScriptMarkerArchetype::Create(map_coords::ToWorld(CoordsOf(record.position.position)));
	auto& registry = Locator::entitiesRegistry::value();
	if ((record.thing.flags & k_NameHidden) != 0)
	{
		registry.AssignOrReplace<components::NameHidden>(entity);
	}
	registry.Assign<components::LoadedRecord<gamesave::ScriptMarkerRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeWaypointEntity(const PlannedWaypoint& waypoint)
{
	// as the land's script makes one: numbered in the order they are made, at its place, with no height
	auto& registry = Locator::entitiesRegistry::value();
	const auto number = static_cast<uint32_t>(registry.Size<components::DrinkWaypoint>());
	const auto entity = registry.Create();
	registry.Assign<components::DrinkWaypoint>(entity, map_coords::ToWorld(CoordsOf(waypoint.record.position.position)),
	                                           number);
	registry.Assign<components::LoadedRecord<gamesave::WaypointRecord>>(entity, waypoint.record);
	return entity;
}

entt::entity saved_world_load::MakeInfluenceRingEntity(const PlannedInfluenceRing& ring, entt::entity attached)
{
	if (!ring.player.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: influence ring {} not made: no player", ring.number);
		return entt::null;
	}
	const auto& record = ring.record;
	const auto entity = openblack::influence::CreateRing(map_coords::ToWorld(CoordsOf(record.position.position)), *ring.player,
	                                                     record.radius, std::bit_cast<int32_t>(record.anti));
	auto& registry = Locator::entitiesRegistry::value();
	if (attached != entt::null && registry.Valid(attached))
	{
		registry.Get<components::InfluenceRing>(entity).attached = attached;
	}
	registry.Assign<components::LoadedRecord<gamesave::InfluenceRingRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeHighlightEntity(const PlannedHighlight& highlight)
{
	constexpr uint32_t k_FirstHighlightRow = 2088;
	constexpr uint8_t k_NameHidden = 0x40;
	const auto& record = highlight.record;
	const auto& object = record.base.object;
	const auto row = static_cast<int64_t>(k_FirstHighlightRow) - static_cast<int64_t>(object.info);
	if (row < 0 || row >= static_cast<int64_t>(script_highlight::k_InfoCount))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: script highlight {} not made: no row", highlight.number);
		return entt::null;
	}
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity =
	    script_highlight::Create(position, static_cast<uint32_t>(row), record.words[4], object.heading, object.scale);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& made = registry.Get<components::ScriptHighlight>(entity);
	made.hidden = std::bit_cast<int32_t>(record.words[0]);
	made.category = static_cast<DykCategory>(record.words[5]);
	if (record.words[2] != 0)
	{
		script_highlight::SetYPos(entity, std::bit_cast<float>(record.words[3]));
	}
	if ((record.words[1] != 0) != made.active)
	{
		script_highlight::SetActivated(entity, record.words[1] != 0);
	}
	if ((record.thing.flags & k_NameHidden) != 0)
	{
		registry.AssignOrReplace<components::NameHidden>(entity);
	}
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	registry.Assign<components::LoadedRecord<gamesave::ScriptHighlightRecord>>(entity, record);
	return entity;
}

components::ScriptMemory saved_world_load::ScriptMemoryOf(const gamesave::ScriptReminderRecord& reminder, entt::entity target)
{
	const auto word = [](const auto& bytes, size_t at) {
		uint32_t read = 0;
		std::memcpy(&read, bytes.data() + at, sizeof(read));
		return read;
	};
	components::ScriptMemory memory {.target = target};
	memory.destination = {.x = std::bit_cast<int32_t>(word(reminder.firstValue, 0)),
	                      .z = std::bit_cast<int32_t>(word(reminder.firstValue, sizeof(uint32_t))),
	                      .altitude = std::bit_cast<float>(word(reminder.firstValue, 2 * sizeof(uint32_t)))};
	const auto x = std::bit_cast<int32_t>(word(reminder.thirdValue, 0));
	const auto z = std::bit_cast<int32_t>(word(reminder.thirdValue, sizeof(uint32_t)));
	const auto number = std::bit_cast<float>(reminder.words[0]);
	if (x != 0 || z != 0 || number != 0.0f)
	{
		memory.arguments =
		    components::ScriptStateArguments {.place = {map_coords::ToMetres(x), map_coords::ToMetres(z)}, .number = number};
	}
	if (reminder.words[1] != 0 || reminder.words[2] != 0)
	{
		memory.clipAndPlays =
		    components::ScriptAnimation {.clip = static_cast<AnimId>(reminder.words[1]), .playsLeft = reminder.words[2]};
	}
	memory.stateClip = std::bit_cast<int32_t>(reminder.words[3]);
	memory.state = static_cast<VillagerStates>(reminder.words[4]);
	return memory;
}

openblack::ecs::SavedFireFly saved_world_load::SavedFireFlyOf(const gamesave::FireFlyRecord& record)
{
	const auto coordsOf = [](const gamesave::Point& point) {
		return map_coords::MapCoords {
		    .x = std::bit_cast<int32_t>(point[0]), .z = std::bit_cast<int32_t>(point[1]), .altitude = point[2]};
	};
	const auto floatOf = [&record](size_t index) { return std::bit_cast<float>(record.words.at(index)); };
	return {.state = static_cast<fire_flies::State>(record.word),
	        .coords = CoordsOf(record.position.position),
	        .madeAt = CoordsOf(record.object.coords),
	        .home = coordsOf(record.points[0]),
	        .light = coordsOf(record.points[1]),
	        .last = coordsOf(record.points[2]),
	        .drawn = glm::vec3(record.points[3][0], record.points[3][1], record.points[3][2]),
	        .clock = floatOf(0),
	        .drift = floatOf(1),
	        .progress = floatOf(2),
	        .duration = floatOf(3),
	        .speedA = floatOf(4),
	        .speedB = floatOf(5),
	        .phases = record.floats,
	        .flags = record.byte,
	        .creationIndex = record.object.creationIndex};
}

void saved_world_load::RestoreFireFlyRewards(const gamesave::FireFlyRecord& record)
{
	std::vector<float> sums;
	for (const auto bits : record.first.values)
	{
		sums.push_back(std::bit_cast<float>(bits));
	}
	std::vector<float> probabilities;
	for (const auto bits : record.second.values)
	{
		probabilities.push_back(std::bit_cast<float>(bits));
	}
	worship::fire_fly::RestoreRewards(sums, probabilities);
	SetFireFliesMaySpawn(record.flag != 0);
}

entt::entity saved_world_load::MakeFishFarmEntity(const PlannedFishFarm& farm, entt::entity town)
{
	constexpr uint32_t k_FirstFishFarmRow = 1759;
	const auto& record = farm.record;
	const auto& object = record.base.object;
	if (object.info > k_FirstFishFarmRow)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: fish farm {} not made: no row", farm.number);
		return entt::null;
	}
	const auto position = map_coords::ToWorld(CoordsOf(record.base.position.position));
	const auto entity = archetypes::FishFarmArchetype::Create(position, k_FirstFishFarmRow - object.info);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& made = registry.Get<components::FishFarm>(entity);
	made.town = town;
	made.food = record.second;
	registry.AssignOrReplace<components::ObjectCreationIndex>(entity, object.creationIndex);
	if (object.life != 1.0f)
	{
		registry.Assign<components::Life>(entity, object.life);
	}
	registry.Assign<components::LoadedRecord<gamesave::FishFarmRecord>>(entity, record);
	return entity;
}

entt::entity saved_world_load::MakeArenaEntity(const PlannedArena& arena)
{
	const auto& record = arena.record;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// its record's second word is its radius; it is the next in the arenas' order
	const auto& coords = record.position.position;
	const auto x = map_coords::ToMetres(coords.x);
	const auto z = map_coords::ToMetres(coords.z);
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	const auto number = static_cast<uint32_t>(registry.Size<components::Arena>()) + 1;
	registry.Assign<components::Arena>(entity, glm::vec3(x, ground, z), std::bit_cast<float>(record.words[1]),
	                                   map_coords::MapCoords {.x = coords.x, .z = coords.z, .altitude = coords.altitude},
	                                   number);
	return entity;
}

entt::entity saved_world_load::MakeMistEntity(const PlannedMist& mist)
{
	const auto& record = mist.record;
	const auto& coords = record.position.position;
	// its three words: its size, its colour and its edge-on shrink
	const auto entity = archetypes::MistArchetype::Create(
	    glm::vec3(map_coords::ToMetres(coords.x), 0.0f, map_coords::ToMetres(coords.z)), coords.altitude, record.words[1],
	    std::bit_cast<float>(record.words[0]), std::bit_cast<float>(record.words[2]));
	Locator::entitiesRegistry::value().Assign<components::LoadedRecord<gamesave::MistRecord>>(entity, record);
	return entity;
}

std::optional<uint32_t> saved_world_load::ClimateTypeOfInfo(uint32_t info)
{
	if (info > k_FirstClimateRow || info + k_ClimateRows <= k_FirstClimateRow)
	{
		return std::nullopt;
	}
	return k_FirstClimateRow - info;
}

namespace
{
/// Each value of a storm's descriptor with its place in the 80 bytes the game writes
template <typename Descriptor, typename Function>
void EachDescriptorValue(Descriptor& descriptor, Function value)
{
	// the weather it adds: eight bytes
	static_assert(sizeof(openblack::weather::WeatherInfo) == 8);
	value(0x00, descriptor.position.x);
	value(0x04, descriptor.position.y);
	value(0x08, descriptor.position.z);
	value(0x0C, descriptor.innerRadius);
	value(0x10, descriptor.outerRadius);
	value(0x14, descriptor.fadeInTime);
	value(0x18, descriptor.lifeTime);
	value(0x1C, descriptor.strength);
	value(0x20, descriptor.numClouds);
	value(0x24, descriptor.blackness);
	value(0x28, descriptor.elevation);
	value(0x2C, descriptor.fallSpeed);
	value(0x30, descriptor.sheetMin);
	value(0x34, descriptor.sheetMax);
	value(0x38, descriptor.forkMin);
	value(0x3C, descriptor.forkMax);
	value(0x40, descriptor.snowCoverRate);
	value(0x44, descriptor.unknown0x44);
	value(0x48, descriptor.weather);
}
} // namespace

uint32_t saved_world_load::ClimateInfoRowOf(uint32_t type)
{
	return k_FirstClimateRow - type;
}

openblack::weather::storms::StormDescriptor saved_world_load::StormDescriptorOf(const std::array<uint8_t, 80>& bytes)
{
	weather::storms::StormDescriptor descriptor;
	EachDescriptorValue(descriptor,
	                    [&bytes]<typename T>(size_t at, T& into) { std::memcpy(&into, bytes.data() + at, sizeof(T)); });
	return descriptor;
}

std::array<uint8_t, 80> saved_world_load::StormDescriptorBytes(const weather::storms::StormDescriptor& descriptor)
{
	std::array<uint8_t, 80> bytes {};
	EachDescriptorValue(descriptor,
	                    [&bytes]<typename T>(size_t at, const T& from) { std::memcpy(bytes.data() + at, &from, sizeof(T)); });
	return bytes;
}

size_t saved_world_load::MakeClimates(const std::vector<PlannedClimate>& climates)
{
	if (climates.empty())
	{
		return 0;
	}
	// the land's climates go first, with their storms, before the saved ones' storms are made
	weather::climate::Reset();
	std::list<weather::climate::Climate> made;
	weather::climate::Counters counters;
	for (const auto& planned : climates)
	{
		const auto& record = planned.record;
		weather::climate::Climate climate;
		// its place: the map coordinates' two words and the altitude
		climate.x = std::bit_cast<int32_t>(record.point[0]);
		climate.z = std::bit_cast<int32_t>(record.point[1]);
		climate.y = record.point[2];
		climate.innerRadius = std::bit_cast<float>(record.words[0]);
		climate.outerRadius = std::bit_cast<float>(record.words[1]);
		climate.id = static_cast<int32_t>(record.words[2]);
		climate.info = static_cast<int32_t>(ClimateTypeOfInfo(record.words[4]).value_or(0));
		// the rain: its desire, its two day counts and its flags
		std::memcpy(&climate.rain.desire, record.sixteen.data(), sizeof(float));
		std::memcpy(&climate.rain.dryDays, record.sixteen.data() + 4, sizeof(int32_t));
		std::memcpy(&climate.rain.rainingDays, record.sixteen.data() + 8, sizeof(int32_t));
		climate.rain.flags = record.sixteen[12];
		std::memcpy(&climate.temperature, record.eight.data(), sizeof(float));
		std::memcpy(&climate.targetTemperature, record.eight.data() + 4, sizeof(float));
		climate.windX = record.secondPoint[0];
		climate.windZ = record.secondPoint[1];
		climate.windAngle = record.secondPoint[2];
		climate.maxStorms = static_cast<int32_t>(record.word);
		climate.world = record.byte != 0;
		climate.stormElevation = static_cast<int32_t>(record.lastWords[0]);
		climate.fallSpeed = std::bit_cast<float>(record.lastWords[1]);
		climate.unknown0x70 = std::bit_cast<float>(record.lastWords[2]);
		climate.sheetMin = std::bit_cast<float>(record.lastWords[3]);
		climate.sheetMax = std::bit_cast<float>(record.lastWords[4]);
		climate.forkMin = std::bit_cast<float>(record.lastWords[5]);
		climate.forkMax = std::bit_cast<float>(record.lastWords[6]);
		climate.lightning = record.lastByte;
		climate.saved = record;
		// its natural storms, in their saved order, each at the head of its list
		for (const auto& area : record.areas.elements)
		{
			const auto id = weather::storms::Create(StormDescriptorOf(area.values));
			if (auto* storm = weather::storms::Find(id))
			{
				storm->age = std::bit_cast<float>(area.words[0]);
				storm->speed = std::bit_cast<float>(area.words[1]);
				storm->target = glm::vec3(area.second[0], area.second[1], area.second[2]);
			}
			climate.storms.push_front(id);
		}
		made.push_back(std::move(climate));
		// every climate writes the counters of the whole system: the next id, the last day, the season and the last
		// season
		counters = {.nextId = static_cast<int32_t>(record.words[3]),
		            .lastDay = static_cast<int32_t>(record.words[5]),
		            .season = record.words[6],
		            .lastSeason = record.words[7]};
	}
	const auto count = made.size();
	weather::climate::Restore(std::move(made), counters);
	return count;
}

namespace
{
/// The music attached to things put back into the game's music, as many as were of things the load made
uint32_t
MakeThingMusic(std::span<const std::pair<openblack::gamesave::ObjectNumber, openblack::gamesave::ThingMusicInfoRecord>> saved,
               const std::function<entt::entity(openblack::gamesave::ObjectNumber)>& objectOf)
{
	auto infos = load_thing_music::InfosOf(saved, objectOf);
	const auto made = static_cast<uint32_t>(infos.size());
	load_thing_music::Restore(std::move(infos));
	return made;
}
} // namespace

saved_world_load::Makers saved_world_load::GameMakers()
{
	return {.footpath = MakeFootpathEntity,
	        .footpathLink = MakeFootpathLinkEntity,
	        .town = MakeTownEntity,
	        .tree = MakeTreeEntity,
	        .bigForest = MakeBigForestEntity,
	        .pile = MakePileEntity,
	        .storagePit = MakeStoragePitEntity,
	        .citadelHeart = MakeCitadelHeartEntity,
	        .abode = MakeAbodeEntity,
	        .desireFlag = MakeDesireFlagEntity,
	        .mobileStatic = MakeMobileStaticEntity,
	        .animatedStatic = MakeAnimatedStaticEntity,
	        .mobileObject = MakeMobileObjectEntity,
	        .deadTree = MakeDeadTreeEntity,
	        .stoneBase = MakeStoneBaseEntity,
	        .feature = MakeFeatureEntity,
	        .streetLantern = MakeStreetLanternEntity,
	        .reward = MakeRewardEntity,
	        .ball = MakeBallEntity,
	        .buildingSite = MakeBuildingSiteEntity,
	        .totemStatue = MakeTotemStatueEntity,
	        .townCentreIcon = MakeTownCentreSpellIconEntity,
	        .spellSeed = MakeSpellSeedEntity,
	        .vortex = MakeVortexEntity,
	        .stream = MakeStreamEntity,
	        .mist = MakeMistEntity,
	        .climates = MakeClimates,
	        .weatherThing = MakeWeatherThingEntity,
	        .worshipSite = MakeWorshipSiteEntity,
	        .villager = MakeVillagerEntity,
	        .flock = MakeFlockEntity,
	        .animal = MakeAnimalEntity,
	        .creature = MakeCreatureEntity,
	        .dance = MakeDanceEntity,
	        .fireBall = MakeFireBallEntity,
	        .fires = MakeFires,
	        .fireFly = RestoreFireFly,
	        .fireFlyRewards = RestoreFireFlyRewards,
	        .arena = MakeArenaEntity,
	        .highlight = MakeHighlightEntity,
	        .marker = MakeScriptMarkerEntity,
	        .waypoint = MakeWaypointEntity,
	        .timer = MakeScriptTimerEntity,
	        .influenceRing = MakeInfluenceRingEntity,
	        .fishFarm = MakeFishFarmEntity,
	        .reaction = MakeReaction,
	        .pileReaction = animal_ai::SetupPotReaction,
	        .hanoiBlock = MakeHanoiBlockEntity,
	        .puzzleGame = MakePuzzleGameEntity,
	        .wonder = MakeWonderEntity,
	        .magicFood = MakeMagicFoodEntity,
	        .thingMusic = MakeThingMusic,
	        .scriptMusic = load_thing_music::RestoreScriptMusic,
	        .handEffect = magic::hand_fx::RestoreInHandEffect};
}

uint32_t saved_world_load::MakeReaction(const PlannedReaction& reaction, entt::entity initiator, entt::entity dance)
{
	if (!Locator::reactionSystem::has_value())
	{
		return 0;
	}
	const auto& record = reaction.record;
	return Locator::reactionSystem::value().Add({.initiator = initiator,
	                                             .type = static_cast<openblack::Reaction>(record.secondWord),
	                                             .player = reaction.player,
	                                             .turnCreated = record.thirdWord,
	                                             .radius = std::bit_cast<float>(record.lastWord),
	                                             .available = record.fourthWord != 0,
	                                             .stealth = record.firstWord != 0,
	                                             .dance = dance});
}

namespace
{
/// The places in a puzzle game's record of its type, of whether its play has begun, of its size, of its two grid steps
/// and of the Hanoi puzzle's first ring
constexpr size_t k_PuzzleTypeWord = 0;
constexpr size_t k_PuzzleBegunWord = 2;
constexpr size_t k_PuzzleSizeWord = 1;
constexpr size_t k_PuzzleCosineStepWord = 0;
constexpr size_t k_PuzzleSineStepWord = 1;
constexpr size_t k_PuzzleFirstRing = openblack::ecs::hanoi::k_Bases;
/// The places in a tree puzzle's cell of its tree, its mist and its bat
constexpr size_t k_CellTree = 0;
constexpr size_t k_CellMist = 1;
constexpr size_t k_CellBat = 2;
/// The places in a Hanoi block's record of the blocks above and below it and of its puzzle
constexpr size_t k_BlockAbove = 0;
constexpr size_t k_BlockBelow = 1;
constexpr size_t k_BlockPuzzle = 2;

/// A Hanoi block's links as its record names them, to the puzzle made of it (its number in the file given)
void LinkHanoiBlock(entt::entity piece, entt::entity puzzle, openblack::gamesave::ObjectNumber puzzleNumber,
                    const std::function<entt::entity(openblack::gamesave::ObjectNumber)>& objectOf)
{
	namespace components = openblack::ecs::components;
	auto& registry = openblack::Locator::entitiesRegistry::value();
	auto* block = piece != entt::null ? registry.TryGet<components::HanoiBlock>(piece) : nullptr;
	const auto* kept = block != nullptr
	                       ? registry.TryGet<const components::LoadedRecord<openblack::gamesave::HanoiBlockRecord>>(piece)
	                       : nullptr;
	if (kept == nullptr)
	{
		return;
	}
	const auto& objects = kept->record.objects;
	block->above = objectOf(objects.at(k_BlockAbove).number);
	block->below = objectOf(objects.at(k_BlockBelow).number);
	const auto named = objects.at(k_BlockPuzzle).number;
	block->puzzle = named == puzzleNumber ? puzzle : objectOf(named);
}

/// A tree puzzle's grid as its record holds it: its place, steps, spots' counts, trees, mist and bats, and lanterns. A
/// spot's place is where its tree stands when the tree was saved on it, as the tree planted there stands, else where a
/// tree planted there would stand
openblack::ecs::components::TreePuzzle
TreePuzzleOf(const openblack::gamesave::PuzzleGameRecord& record,
             const std::function<entt::entity(openblack::gamesave::ObjectNumber)>& objectOf)
{
	using namespace openblack;
	namespace components = ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	components::TreePuzzle grid;
	grid.place = CoordsOf(record.position.position);
	grid.cosineStep = std::bit_cast<int32_t>(record.thirdWords.at(k_PuzzleCosineStepWord));
	grid.sineStep = std::bit_cast<int32_t>(record.thirdWords.at(k_PuzzleSineStepWord));
	const ecs::hanoi::Steps steps {.cosine = grid.cosineStep, .sine = grid.sineStep};
	for (int32_t across = 0; across < ecs::tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < ecs::tree_puzzle::k_Side; ++along)
		{
			const auto spot = ecs::tree_puzzle::SpotIndex(across, along);
			const auto& cell = record.cells.at(spot);
			grid.counts.at(spot) = static_cast<int8_t>(cell.byte);
			grid.trees.at(spot) = objectOf(cell.objects.at(k_CellTree).number);
			grid.mists.at(spot) = objectOf(cell.objects.at(k_CellMist).number);
			grid.bats.at(spot) = objectOf(cell.objects.at(k_CellBat).number);
			const auto place = ecs::hanoi::GridPlace(grid.place, steps, across, along);
			const auto tree = grid.trees.at(spot);
			const auto* saved =
			    tree != entt::null ? registry.TryGet<const components::LoadedRecord<gamesave::TreeRecord>>(tree) : nullptr;
			grid.placedAt.at(spot) = saved != nullptr && CoordsOf(saved->record.base.position.position) == place
			                             ? ecs::object::MapCoordsOf(tree)
			                             : map_coords::FromWorld(map_coords::ToWorld(place));
		}
	}
	for (size_t corner = 0; corner < grid.lanterns.size() && corner < record.third.numbers.size(); ++corner)
	{
		grid.lanterns.at(corner) = objectOf(record.third.numbers.at(corner));
	}
	grid.started = record.words.at(k_PuzzleBegunWord) != 0;
	return grid;
}
} // namespace

entt::entity saved_world_load::MakeHanoiBlockEntity(const PlannedHanoiBlock& block)
{
	const auto entity =
	    MakeMobileObjectEntity({.number = block.number,
	                            .type = block.type,
	                            .record = gamesave::MobileObjectRecord {.thing = block.record.thing, .base = block.record.base},
	                            .inHand = block.inHand});
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<components::LoadedRecord<gamesave::MobileObjectRecord>>(entity);
	registry.Assign<components::LoadedRecord<gamesave::HanoiBlockRecord>>(entity, block.record);
	// its rest place, kept as the map coordinates the game keeps it in
	const map_coords::MapCoords home {.x = std::bit_cast<int32_t>(block.record.point.at(0)),
	                                  .z = std::bit_cast<int32_t>(block.record.point.at(1)),
	                                  .altitude = block.record.point.at(2)};
	const auto standsAtHome = CoordsOf(block.record.base.position.position) == home;
	registry.AssignState<components::HanoiBlock>(
	    entity, components::HanoiBlock {.home = home,
	                                    .placedAt = standsAtHome ? registry.Get<const components::Transform>(entity).position
	                                                             : map_coords::ToWorld(home)});
	return entity;
}

bool saved_world_load::PuzzleGameTypeLoaded(uint32_t type)
{
	if (type >= static_cast<uint32_t>(script::PuzzleGameType::Last))
	{
		return true;
	}
	const auto kind = static_cast<script::PuzzleGameType>(type);
	return kind == script::PuzzleGameType::Hanoi || IsTreePuzzle(kind) || CompletesAtOnce(kind);
}

entt::entity saved_world_load::MakePuzzleGameEntity(const PlannedPuzzleGame& planned,
                                                    const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	const auto& record = planned.record;
	const auto type = record.words.at(k_PuzzleTypeWord);
	if (!PuzzleGameTypeLoaded(type))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Load: puzzle game {} not made: its type {} keeps a state the load does not make", planned.number,
		                   type);
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& puzzle = registry.Assign<components::PuzzleGame>(entity);
	puzzle.type = static_cast<script::PuzzleGameType>(type);
	puzzle.place = CoordsOf(record.position.position);
	puzzle.position = map_coords::ToWorld(puzzle.place);
	puzzle.angle = record.half;
	puzzle.scale = std::bit_cast<float>(record.secondWords.at(k_PuzzleSizeWord));
	puzzle.cosineStep = std::bit_cast<int32_t>(record.thirdWords.at(k_PuzzleCosineStepWord));
	puzzle.sineStep = std::bit_cast<int32_t>(record.thirdWords.at(k_PuzzleSineStepWord));
	puzzle.played = record.lastWord != 0;
	puzzle.calmTurns = hanoi::k_CalmTop;
	const auto position = puzzle.position;
	const auto scale = puzzle.scale;
	registry.Assign<components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(scale));
	// its place's marks (held by its script and under its control) and its hidden name, as saved
	constexpr uint8_t k_NameHidden = 0x40;
	RestorePlaceMarks(entity, record.position.flags);
	if ((record.thing.flags & k_NameHidden) != 0)
	{
		registry.AssignOrReplace<components::NameHidden>(entity);
	}
	auto& made = registry.Get<components::PuzzleGame>(entity);
	if (made.type == script::PuzzleGameType::Hanoi)
	{
		made.begun = record.words.at(k_PuzzleBegunWord) != 0;
		for (size_t base = 0; base < hanoi::k_Bases; ++base)
		{
			made.bases.at(base) = objectOf(record.objects.at(base).number);
		}
		for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
		{
			made.rings.at(ring) = objectOf(record.objects.at(k_PuzzleFirstRing + ring).number);
		}
		// each block's links, then the last state of each piece, from which the puzzle makes again a piece that goes
		for (size_t slot = 0; slot < made.kept.size(); ++slot)
		{
			const auto piece = slot < hanoi::k_Bases ? made.bases.at(slot) : made.rings.at(slot - hanoi::k_Bases);
			LinkHanoiBlock(piece, entity, planned.number, objectOf);
			if (const auto* block = piece != entt::null ? registry.TryGet<const components::HanoiBlock>(piece) : nullptr)
			{
				made.kept.at(slot) = *block;
			}
		}
	}
	else if (IsTreePuzzle(made.type))
	{
		auto grid = TreePuzzleOf(record, objectOf);
		registry.Assign<components::TreePuzzle>(entity, std::move(grid));
	}
	registry.Assign<components::LoadedRecord<gamesave::PuzzleGameRecord>>(entity, record);
	TrackPuzzleGame(entity);
	return entity;
}

entt::entity saved_world_load::MakeWonderEntity(const PlannedWonder& wonder, uint32_t townKey)
{
	const auto& record = wonder.record;
	if (!Locator::infoConstants::has_value() ||
	    Locator::infoConstants::value().abode.at(static_cast<size_t>(wonder.type)).abodeType != AbodeType::Wonder)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: wonder {} not made: its row is not a wonder's", wonder.number);
		return entt::null;
	}
	// a wonder's making adds a built one's power to its player's most tribal power; a load's leaves it as it was, the
	// player's record holding it
	using TribalPowers = std::array<float, components::PlayerMagic::k_Tribes>;
	std::array<TribalPowers, static_cast<size_t>(PlayerNames::_COUNT)> powers {};
	const bool players = Locator::playerSystem::has_value();
	for (size_t player = 0; players && player < powers.size(); ++player)
	{
		powers.at(player) = magic::players::MagicOf(static_cast<PlayerNames>(player)).maxTribalPower;
	}
	const auto entity =
	    MakeAbodeEntity({.number = wonder.number,
	                     .type = wonder.type,
	                     .base = record.base,
	                     .abode = record.abode,
	                     .record = gamesave::AbodeRecord {.thing = record.thing, .base = record.base, .abode = record.abode}},
	                    townKey);
	for (size_t player = 0; players && player < powers.size(); ++player)
	{
		magic::players::MagicOf(static_cast<PlayerNames>(player)).maxTribalPower = powers.at(player);
	}
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<components::LoadedRecord<gamesave::AbodeRecord>>(entity);
	registry.Assign<components::LoadedRecord<gamesave::WonderRecord>>(entity, record);
	// its power, as saved
	wonders::SetPower(entity, std::bit_cast<float>(record.word));
	return entity;
}

entt::entity saved_world_load::MakeMagicFoodEntity(const PlannedMagicFood& food)
{
	const auto& record = food.record;
	// a food pile's load, then its player
	const auto entity = MakePileEntity(
	    {.number = food.number,
	     .type = food.type,
	     .base = record.base,
	     .pile = record.pile,
	     .record =
	         gamesave::FoodPileRecord {.thing = record.thing, .base = record.base, .pile = record.pile, .store = record.store},
	     .player = food.player});
	if (entity == entt::null)
	{
		return entity;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<components::LoadedRecord<gamesave::FoodPileRecord>>(entity);
	registry.Assign<components::LoadedRecord<gamesave::MagicFoodRecord>>(entity, record);
	// its size as saved comes with the food pile's load
	return entity;
}

std::optional<components::AnimalBrain::HugCircle>
saved_world_load::AnimalThingCircleOf(const gamesave::WallHugPart& move,
                                      const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	if (!move.circle.has_value() || move.circle->object.number == 0 || !objectOf)
	{
		return std::nullopt;
	}
	const auto thing = objectOf(move.circle->object.number);
	if (thing == entt::null)
	{
		return std::nullopt;
	}
	const auto circles = openblack::ecs::thing_circles::CirclesOf(Locator::entitiesRegistry::value(), thing);
	const auto index = static_cast<size_t>(std::max(move.circle->index, 0));
	if (index >= circles.size())
	{
		return std::nullopt;
	}
	return components::AnimalBrain::HugCircle {
	    .centre = circles[index].centre, .radius = circles[index].radius, .owner = thing, .set = true};
}

void saved_world_load::RestoreAnimalCircle(components::AnimalBrain& brain, const gamesave::WallHugPart& move,
                                           const std::optional<components::AnimalBrain::HugCircle>& thingCircle)
{
	if (!move.circle.has_value())
	{
		return;
	}
	const auto& circle = *move.circle;
	brain.turnsToObj = circle.turnsToObject;
	brain.hugGoalDistance = circle.entryDistance;
	if (thingCircle.has_value())
	{
		brain.hugCircle = *thingCircle;
	}
	else if (circle.object.number == 0 && circle.index == k_LandscapeCircleIndex)
	{
		// water or the land's edge: the circle in the middle of the cell its saved centre is in
		constexpr uint32_t k_CellShift = 16;
		const glm::ivec2 cell {static_cast<int32_t>(static_cast<uint32_t>(circle.coords.x) >> k_CellShift),
		                       static_cast<int32_t>(static_cast<uint32_t>(circle.coords.z) >> k_CellShift)};
		brain.hugCircle = components::AnimalBrain::HugCircle {.centre = openblack::ecs::wall_hug::LandscapeCircleCentre(cell),
		                                                      .radius = openblack::ecs::wall_hug::k_LandscapeBlockerRadius,
		                                                      .owner = entt::null,
		                                                      .set = true};
	}
	else
	{
		brain.hugCircle = {};
	}
}
