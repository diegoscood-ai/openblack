/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FeatureScriptCommands.h"

#include <cctype>

#include <algorithm>
#include <optional>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/polar_coordinates.hpp>
#include <glm/gtx/string_cast.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/FlowersArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Footpaths.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/TownDesire.h"
#include "ECS/Trees.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "LHScriptX/VillagerCommands.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Script/MapScriptMagic.h"
#include "Magic/Script/MapScriptWeather.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "ScriptingBindingUtils.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
template <class C, size_t size>
constexpr std::unordered_map<std::string_view, C> makeLookup(std::array<std::string_view, size> strings)
{
	std::unordered_map<std::string_view, C> table;
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& str : strings)
	{
		table.insert(std::make_pair(str, static_cast<C>(i)));
		++i;
	}
	return table;
}

const auto k_PlayerLookup = makeLookup<PlayerNames>(k_PlayerNamesStrs);
const auto k_TribeLookup = makeLookup<Tribe>(k_TribeStrs);
PlayerNames GetPlayerName(const std::string& name)
{
	PlayerNames player;
	try
	{
		player = k_PlayerLookup.at(name);
	}
	catch (...)
	{
		std::throw_with_nested(std::runtime_error(fmt::format("Could not recognize player name: {}", name)));
	}
	return player;
}


/// The town with the script's id (ecs::town_queries::FindTownWithID)
entt::entity FindTown(int32_t townId)
{
	return ecs::town_queries::FindTownWithID(static_cast<uint32_t>(townId));
}

/// The script's town number as the town's key (Town::id, which AbodeArchetype / FieldArchetype take); a number no town
/// has -> Abode::k_NoTown (their own fallback: the nearest town)
uint32_t TownKeyOf(int32_t townId)
{
	const auto town = FindTown(townId);
	return town != entt::null ? Locator::entitiesRegistry::value().Get<const Town>(town).id : Abode::k_NoTown;
}

/// The nearest town: the town list newest first (town_queries::TownsNewestFirst), the first always taken, then a
/// strictly smaller distance in metres, so an exact tie goes to the newer town; none without towns. MapCoords x, z
/// only (FromMetres: the altitude is not read)
entt::entity FindNearestTown(const glm::vec3& position)
{
	return ecs::map_cells::FindNearestTownInList(map_coords::FromMetres(glm::vec2(position.x, position.z)));
}

/// The info index a command passes to the villager's creation, or nullopt for one past openblack's table
std::optional<VillagerInfo> ValidVillagerInfo(int32_t index, const char* command)
{
	if (index < 0 || static_cast<size_t>(index) >= Locator::infoConstants::value().villager.size())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "LHScriptX: {}: villager info {} out of the table, skipped", command,
		                   index);
		return std::nullopt;
	}
	return static_cast<VillagerInfo>(index);
}

/// CREATE_TOWN_VILLAGER / CREATE_SPECIAL_TOWN_VILLAGER: the villager at A1 with the info and age N3; it becomes the
/// last created object, null too; none -> end. Then the town with id N0, else the nearest town to A1, else end; the
/// villager is added to that town
void CreateVillagerInTown(int32_t townId, const glm::vec3& position, VillagerInfo info, int32_t age)
{
	const auto villager = VillagerArchetype::Create(position, position, info, static_cast<uint32_t>(age), false);
	Script::SetLastCreated(villager);
	if (villager == entt::null)
	{
		return;
	}
	auto town = FindTown(townId);
	if (town == entt::null)
	{
		town = FindNearestTown(position);
	}
	if (town == entt::null)
	{
		return;
	}
	ecs::town_villagers::AddVillagerToTown(town, villager);
}

/// CREATE_VILLAGER / _POS, after villager_commands::FindAbodeAt(abodePosition): the info, or the overriding tribe's
/// villager of the same number when that tribe has one; the villager is made at the position with the info and age
/// (put on the land); it becomes the last created object, null too; none -> end. Then the abode found; else its
/// town: an abode with space in the town, else the town itself; else the game's vagrants
void CreateVillagerAtAbode(const glm::vec3& abodePosition, const glm::vec3& position, VillagerInfo info, int32_t age)
{
	const auto at = villager_commands::FindAbodeAt(abodePosition);
	if (const auto tribe = Script::TribeOverride(); tribe.has_value())
	{
		const auto number = Locator::infoConstants::value().villager.at(static_cast<size_t>(info)).villagerNumber;
		if (const auto other = villager_commands::FindVillagerInfo(*tribe, number); other.has_value())
		{
			info = *other;
		}
	}
	const auto villager = VillagerArchetype::Create(abodePosition, position, info, static_cast<uint32_t>(age), false);
	Script::SetLastCreated(villager);
	if (villager == entt::null)
	{
		return;
	}
	if (at.abode != entt::null)
	{
		ecs::abode_villagers::AddVillagerToAbode(at.abode, villager);
		return;
	}
	if (at.town != entt::null)
	{
		if (const auto abode = ecs::town_villagers::FindAbodeWithSpaceInTown(at.town, villager, 0.0f); abode != entt::null)
		{
			ecs::abode_villagers::AddVillagerToAbode(abode, villager);
			return;
		}
		ecs::town_villagers::AddVillagerToTown(at.town, villager);
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "LHScriptX: no abode at {}: the villager is a vagrant",
	                    glm::to_string(abodePosition));
	ecs::town_villagers::AddToVagrants(villager);
}

} // namespace

const std::array<const ScriptCommandSignature, 106> FeatureScriptCommands::k_Signatures = {{
    CREATE_COMMAND_BINDING("SET_A_TOWNS_INFLUENCE_MULTIPLIER", SetATownInfluenceMultiplier),
    CREATE_COMMAND_BINDING("CREATE_MIST", CreateMist),
    CREATE_COMMAND_BINDING("CREATE_PATH", CreatePath),
    CREATE_COMMAND_BINDING("CREATE_TOWN", CreateTown),
    CREATE_COMMAND_BINDING("SET_TOWN_BELIEF", SetTownBelief),
    CREATE_COMMAND_BINDING("SET_TOWN_BELIEF_CAP", SetTownBeliefCap),
    CREATE_COMMAND_BINDING("SET_TOWN_UNINHABITABLE", SetTownUninhabitable),
    CREATE_COMMAND_BINDING("SET_TOWN_CONGREGATION_POS", SetTownCongregationPos),
    CREATE_COMMAND_BINDING("CREATE_ABODE", CreateAbode),
    CREATE_COMMAND_BINDING("CREATE_PLANNED_ABODE", CreatePlannedAbode),
    CREATE_COMMAND_BINDING("CREATE_TOWN_CENTRE", CreateTownCentre),
    CREATE_COMMAND_BINDING("CREATE_TOWN_SPELL", CreateTownSpell),
    CREATE_COMMAND_BINDING("CREATE_NEW_TOWN_SPELL", CreateNewTownSpell),
    CREATE_COMMAND_BINDING("CREATE_TOWN_CENTRE_SPELL_ICON", CreateTownCentreSpellIcon),
    CREATE_COMMAND_BINDING("CREATE_SPELL_ICON", CreateSpellIcon),
    CREATE_COMMAND_BINDING("CREATE_PLANNED_SPELL_ICON", CreatePlannedSpellIcon),
    CREATE_COMMAND_BINDING("CREATE_VILLAGER", CreateVillager),
    CREATE_COMMAND_BINDING("CREATE_TOWN_VILLAGER", CreateTownVillager),
    CREATE_COMMAND_BINDING("CREATE_SPECIAL_TOWN_VILLAGER", CreateSpecialTownVillager),
    CREATE_COMMAND_BINDING("CREATE_VILLAGER_POS", CreateVillagerPos),
    CREATE_COMMAND_BINDING("CREATE_CITADEL", CreateCitadel),
    CREATE_COMMAND_BINDING("CREATE_PLANNED_CITADEL", CreatePlannedCitadel),
    CREATE_COMMAND_BINDING("CREATE_CREATURE_PEN", CreateCreaturePen),
    CREATE_COMMAND_BINDING("CREATE_WORSHIP_SITE", CreateWorshipSite),
    CREATE_COMMAND_BINDING("CREATE_PLANNED_WORSHIP_SITE", CreatePlannedWorshipSite),
    CREATE_COMMAND_BINDING("CREATE_ANIMAL", CreateAnimal),
    CREATE_COMMAND_BINDING("CREATE_NEW_ANIMAL", CreateNewAnimal),
    CREATE_COMMAND_BINDING("CREATE_FOREST", CreateForest),
    CREATE_COMMAND_BINDING("CREATE_TREE", CreateTree),
    CREATE_COMMAND_BINDING("CREATE_NEW_TREE", CreateNewTree),
    CREATE_COMMAND_BINDING("CREATE_FIELD", CreateField),
    CREATE_COMMAND_BINDING("CREATE_TOWN_FIELD", CreateTownField),
    CREATE_COMMAND_BINDING("CREATE_FISH_FARM", CreateFishFarm),
    CREATE_COMMAND_BINDING("CREATE_TOWN_FISH_FARM", CreateTownFishFarm),
    CREATE_COMMAND_BINDING("CREATE_FEATURE", CreateFeature),
    CREATE_COMMAND_BINDING("CREATE_FLOWERS", CreateFlowers),
    CREATE_COMMAND_BINDING("CREATE_WALL_SECTION", CreateWallSection),
    CREATE_COMMAND_BINDING("CREATE_PLANNED_WALL_SECTION", CreatePlannedWallSection),
    CREATE_COMMAND_BINDING("CREATE_PITCH", CreatePitch),
    CREATE_COMMAND_BINDING("CREATE_POT", CreatePot),
    CREATE_COMMAND_BINDING("CREATE_TOWN_TEMPORARY_POTS", CreateTownTemporaryPots),
    CREATE_COMMAND_BINDING("CREATE_MOBILEOBJECT", CreateMobileObject),
    CREATE_COMMAND_BINDING("CREATE_MOBILESTATIC", CreateMobileStatic),
    CREATE_COMMAND_BINDING("CREATE_MOBILE_STATIC", CreateMobileUStatic),
    CREATE_COMMAND_BINDING("CREATE_DEAD_TREE", CreateDeadTree),
    CREATE_COMMAND_BINDING("CREATE_SCAFFOLD", CreateScaffold),
    CREATE_COMMAND_BINDING("COUNTRY_CHANGE", CountryChange),
    CREATE_COMMAND_BINDING("HEIGHT_CHANGE", HeightChange),
    CREATE_COMMAND_BINDING("CREATE_CREATURE", CreateCreature),
    CREATE_COMMAND_BINDING("CREATE_CREATURE_FROM_FILE", CreateCreatureFromFile),
    CREATE_COMMAND_BINDING("CREATE_FLOCK", CreateFlock),
    CREATE_COMMAND_BINDING("LOAD_LANDSCAPE", LoadLandscape),
    CREATE_COMMAND_BINDING("VERSION", Version),
    CREATE_COMMAND_BINDING("CREATE_AREA", CreateArea),
    CREATE_COMMAND_BINDING("START_CAMERA_POS", StartCameraPos),
    CREATE_COMMAND_BINDING("FLY_BY_FILE", FlyByFile),
    CREATE_COMMAND_BINDING("TOWN_NEEDS_POS", TownNeedsPos),
    CREATE_COMMAND_BINDING("CREATE_FURNITURE", CreateFurniture),
    CREATE_COMMAND_BINDING("CREATE_BIG_FOREST", CreateBigForest),
    CREATE_COMMAND_BINDING("CREATE_NEW_BIG_FOREST", CreateNewBigForest),
    CREATE_COMMAND_BINDING("CREATE_INFLUENCE_RING", CreateInfluenceRing),
    CREATE_COMMAND_BINDING("CREATE_WEATHER_CLIMATE", CreateWeatherClimate),
    CREATE_COMMAND_BINDING("CREATE_WEATHER_CLIMATE_RAIN", CreateWeatherClimateRain),
    CREATE_COMMAND_BINDING("CREATE_WEATHER_CLIMATE_TEMP", CreateWeatherClimateTemp),
    CREATE_COMMAND_BINDING("CREATE_WEATHER_CLIMATE_WIND", CreateWeatherClimateWind),
    CREATE_COMMAND_BINDING("CREATE_WEATHER_STORM", CreateWeatherStorm),
    CREATE_COMMAND_BINDING("BRUSH_SIZE", BrushSize),
    CREATE_COMMAND_BINDING("CREATE_STREAM", CreateStream),
    CREATE_COMMAND_BINDING("CREATE_STREAM_POINT", CreateStreamPoint),
    CREATE_COMMAND_BINDING("CREATE_WATERFALL", CreateWaterfall),
    CREATE_COMMAND_BINDING("CREATE_ARENA", CreateArena),
    CREATE_COMMAND_BINDING("CREATE_FOOTPATH", CreateFootpath),
    CREATE_COMMAND_BINDING("CREATE_FOOTPATH_NODE", CreateFootpathNode),
    CREATE_COMMAND_BINDING("LINK_FOOTPATH", LinkFootpath),
    CREATE_COMMAND_BINDING("CREATE_BONFIRE", CreateBonfire),
    CREATE_COMMAND_BINDING("CREATE_BASE", CreateBase),
    CREATE_COMMAND_BINDING("CREATE_NEW_FEATURE", CreateNewFeature),
    CREATE_COMMAND_BINDING("SET_INTERACT_DESIRE", SetInteractDesire),
    CREATE_COMMAND_BINDING("TOGGLE_COMPUTER_PLAYER", ToggleComputerPlayer),
    CREATE_COMMAND_BINDING("SET_COMPUTER_PLAYER_CREATURE_LIKE", SetComputerPlayerCreatureLike),
    CREATE_COMMAND_BINDING("MULTIPLAYER_DEBUG", MultiplayerDebug),
    CREATE_COMMAND_BINDING("CREATE_STREET_LANTERN", CreateStreetLantern),
    CREATE_COMMAND_BINDING("CREATE_STREET_LIGHT", CreateStreetLight),
    CREATE_COMMAND_BINDING("SET_LAND_NUMBER", SetLandNumber),
    CREATE_COMMAND_BINDING("CREATE_ONE_SHOT_SPELL", CreateOneShotSpell),
    CREATE_COMMAND_BINDING("CREATE_ONE_SHOT_SPELL_PU", CreateOneShotSpellPu),
    CREATE_COMMAND_BINDING("CREATE_FIRE_FLY", CreateFireFly),
    CREATE_COMMAND_BINDING("TOWN_DESIRE_BOOST", TownDesireBoost),
    CREATE_COMMAND_BINDING("CREATE_ANIMATED_STATIC", CreateAnimatedStatic),
    CREATE_COMMAND_BINDING("FIRE_FLY_SPELL_REWARD_PROB", FireFlySpellRewardProb),
    CREATE_COMMAND_BINDING("CREATE_NEW_TOWN_FIELD", CreateNewTownField),
    CREATE_COMMAND_BINDING("CREATE_SPELL_DISPENSER", CreateSpellDispenser),
    CREATE_COMMAND_BINDING("LOAD_COMPUTER_PLAYER_PERSONALLTY", LoadComputerPlayerPersonality),
    CREATE_COMMAND_BINDING("SET_COMPUTER_PLAYER_PERSONALLTY", SetComputerPlayerPersonality),
    CREATE_COMMAND_BINDING("SET_GLOBAL_LAND_BALANCE", SetGlobalLandBalance),
    CREATE_COMMAND_BINDING("SET_LAND_BALANCE", SetLandBalance),
    CREATE_COMMAND_BINDING("CREATE_DRINK_WAYPOINT", CreateDrinkWaypoint),
    CREATE_COMMAND_BINDING("SET_TOWN_INFLUENCE_MULTIPLIER", SetTownInfluenceMultiplier),
    CREATE_COMMAND_BINDING("SET_PLAYER_INFLUENCE_MULTIPLIER", SetPlayerInfluenceMultiplier),
    CREATE_COMMAND_BINDING("SET_TOWN_BALANCE_BELIEF_SCALE", SetTownBalanceBeliefScale),
    CREATE_COMMAND_BINDING("START_GAME_MESSAGE", StartGameMessage),
    CREATE_COMMAND_BINDING("ADD_GAME_MESSAGE_LINE", AddGameMessageLine),
    CREATE_COMMAND_BINDING("EDIT_LEVEL", EditLevel),
    CREATE_COMMAND_BINDING("SET_NIGHTTIME", SetNighttime),
    CREATE_COMMAND_BINDING("MAKE_LAST_OBJECT_ARTIFACT", MakeLastObjectArtifact),
    CREATE_COMMAND_BINDING("SET_LOST_TOWN_SCALE", SetLostTownScale),
}};

inline glm::mat4 GetRotation(int rotation)
{
	return glm::mat4(affine::AngleY(static_cast<float>(rotation) * 0.001f));
}

inline glm::vec3 GetSize(int size)
{
	return glm::vec3(size, size, size) * 0.001f;
}

void FeatureScriptCommands::SetATownInfluenceMultiplier(int32_t townId, float multiplier)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX Function {}(townId={}, multiplier={}) not implemented.", __func__,
	                    townId, multiplier);
}

void FeatureScriptCommands::CreateMist(glm::vec3 position, float param2, int32_t param3, float param4, float param5)
{
	// command "AFNFF": a mist at the position raised by F1, ARGB colour N2, size F3, k F4. It becomes the last created
	// object
	Script::SetLastCreated(MistArchetype::Create(position, param2, static_cast<uint32_t>(param3), param4, param5));
}

void FeatureScriptCommands::CreatePath(int32_t param1, int32_t param2, int32_t param3, int32_t param4)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {}({}, {}, {}, {}) not implemented.", __FILE__,
	                    __LINE__, __func__, param1, param2, param3, param4);
}

void FeatureScriptCommands::CreateTown(int32_t townId, glm::vec3 position, const std::string& playerOwner,
                                       [[maybe_unused]] int32_t, const std::string& tribeType)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), R"(LHScriptX: Creating town {} for "{}" with tribe type "{}".)", townId,
	                    playerOwner, tribeType);

	Tribe tribe;
	try
	{
		tribe = k_TribeLookup.at(tribeType);
	}
	catch (...)
	{
		std::throw_with_nested(std::runtime_error("Could not recognize village tribe"));
	}

	// the town becomes the last created object. It is not an Object, but its desire flags, made with it, are
	Script::SetLastCreated(TownArchetype::Create(townId, position, GetPlayerName(playerOwner), tribe));
}

void FeatureScriptCommands::SetTownBelief(int32_t townId, const std::string& playerOwner, float belief)
{
	// SET_TOWN_BELIEF: nothing without the town; otherwise the town's belief in the player (for the neutral player its
	// own slot), capped. It overwrites the slot (the old string map inserted, so it kept the first value)
	const auto player = PlayerFromText(playerOwner);
	if (const auto town = FindTown(townId); town != entt::null)
	{
		ecs::town_belief::SetBeliefInPlayer(Locator::entitiesRegistry::value().Get<Town>(town), player, belief);
	}
}

void FeatureScriptCommands::SetTownBeliefCap(int32_t townId, const std::string& playerOwner, float belief)
{
	// SET_TOWN_BELIEF_CAP: nothing without the town; otherwise the cap of the town's belief in the player. (pending) on
	// the land .\mpm_3p_1.txt in a multiplayer game, town 3 and a non-neutral player get a cap of 2.0; openblack plays
	// no multiplayer land
	const auto player = PlayerFromText(playerOwner);
	if (const auto town = FindTown(townId); town != entt::null)
	{
		ecs::town_belief::SetCap(Locator::entitiesRegistry::value().Get<Town>(town).belief, player, belief);
	}
}

void FeatureScriptCommands::SetTownUninhabitable(int32_t townId)
{
	// case 5: the town becomes uninhabitable; nothing without the town
	if (const auto town = FindTown(townId); town != entt::null)
	{
		Locator::entitiesRegistry::value().Get<Town>(town).uninhabitable = true;
	}
}

void FeatureScriptCommands::SetTownCongregationPos(int32_t townId, glm::vec3 position)
{
	// case 6: nothing without the town; otherwise the town's congregation position (the cache that
	// ecs::town_queries reads) = the script position N1, all three fields.
	// A script position is x, z in map coordinates with altitude 0, or with a third field the altitude unscaled. A
	// script offset may be added, but it is only set by the vortex between lands, so a land's features script never
	// has one.
	// openblack's script parser (Script.cpp GetParameter) only makes a vector of a two-field string, and its y is the
	// terrain height, not a third field: y = 0 is the literal value for that form. Every SET_TOWN_CONGREGATION_POS of
	// the shipped scripts has two fields. (approximate) a three-field string does not reach here in openblack
	if (const auto town = FindTown(townId); town != entt::null)
	{
		auto& component = Locator::entitiesRegistry::value().Get<Town>(town);
		component.congregationPos = ecs::town_queries::ToMapCoords({position.x, position.z});
		component.congregationPosY = 0.0f;
	}
}

void FeatureScriptCommands::CreateAbode(int32_t townId, glm::vec3 position, const std::string& abodeInfo, int32_t rotation,
                                        int32_t size, int32_t foodAmount, int32_t woodAmount)
{
	// Does not use 3d angle to game angle
	const auto type = GAbodeInfo::Find(abodeInfo);
	if (type == AbodeInfo::None)
	{
		return; // openblack: the original has no check (see GAbodeInfo::Find)
	}
	// the abode becomes the last created object, null too
	Script::SetLastCreated(AbodeArchetype::Create(TownKeyOf(townId), position, type, rotation * 0.001f, size * 0.001f,
	                                              static_cast<uint32_t>(foodAmount), static_cast<uint32_t>(woodAmount)));
}

void FeatureScriptCommands::CreatePlannedAbode(int32_t townId, glm::vec3 position, const std::string& abodeInfo,
                                               int32_t rotation, int32_t size, [[maybe_unused]] int32_t foodAmount,
                                               [[maybe_unused]] int32_t woodAmount)
{
	// case 8 (shared with CREATE_ABODE): the town, else the nearest one, else nothing; the food and wood are not used.
	// A town centre type makes a planned town centre, any other a planned abode; both go on the town's planned list
	// and are never drawn.
	auto town = FindTown(townId);
	if (town == entt::null)
	{
		town = FindNearestTown(position);
	}
	const auto type = GAbodeInfo::Find(abodeInfo);
	if (town == entt::null || type == AbodeInfo::None)
	{
		return;
	}
	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(type));
	Locator::entitiesRegistry::value().Get<Town>(town).plannedAbodes.push_back(
	    PlannedAbode {type, position, rotation * 0.001f, size * 0.001f, info.abodeType == AbodeType::TownCentre});
	// the plan becomes the last created object; openblack's plans are not entities, and a plan holds no footpath link,
	// so the last created object is none
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::CreateTownCentre(int32_t townId, glm::vec3 position, const std::string& abodeInfo, int32_t rotation,
                                             int32_t size, int32_t worshipPercentage)
{
	// case 9
	const auto type = GAbodeInfo::Find(abodeInfo);
	if (type == AbodeInfo::None)
	{
		return; // openblack: the original has no check (see GAbodeInfo::Find)
	}
	const auto centre = AbodeArchetype::Create(TownKeyOf(townId), position, type, rotation * 0.001f, size * 0.001f,
	                                           static_cast<uint32_t>(0), static_cast<uint32_t>(0));
	// it becomes the last created object, null too
	Script::SetLastCreated(centre);
	if (centre == entt::null || type == AbodeInfo::None ||
	    Locator::infoConstants::value().abode.at(static_cast<size_t>(type)).abodeType != AbodeType::TownCentre)
	{
		return;
	}
	// the town's centre if it had none, and the worship percentage N5 * 0.001 on the centre's town (the branch for a
	// centre without a town, which sets it on the totem statue, can't happen here: openblack always gives the abode a
	// town)
	auto& registry = Locator::entitiesRegistry::value();
	// the centre's town key (the nearest town's after the fallback, maybe a founded one)
	const auto town = ecs::town_queries::TownByKey(registry.Get<Abode>(centre).townId);
	if (town == entt::null)
	{
		return;
	}
	auto& townData = registry.Get<Town>(town);
	if (townData.centre == entt::null)
	{
		townData.centre = centre;
	}
	const float share = static_cast<float>(worshipPercentage) * 0.001f;
	// the full worship percentage setter (Worship/WorshipPercentage.cpp: kept only with a worship site, the totem
	// statue, the villagers sent), through the totems' slot; without it (a test that makes no game) directly
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().SetTownShare(town, share);
	}
	else
	{
		worship::percentage::SetWorshipPercentage(town, share);
	}
}

void FeatureScriptCommands::CreateTownSpell(int32_t townId, const std::string& spellName)
{
	magic::script::CreateTownSpell(townId, spellName); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateNewTownSpell(int32_t townId, const std::string& spellName)
{
	// the town centre's spell icon this makes is counted by the object index (TownCentreSpellIcon takes no index of its own)
	ecs::object_index::AddTownSpell(static_cast<uint32_t>(townId), spellName);
	magic::script::CreateNewTownSpell(townId, spellName); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateTownCentreSpellIcon(int32_t townId, const std::string& spellName)
{
	// command 12, the same handler as CREATE_TOWN_SPELL
	magic::script::CreateTownSpell(townId, spellName); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateSpellIcon(glm::vec3 position, const std::string& param2, int32_t param3, int32_t param4,
                                            int32_t param5)
{
	// command 13 does nothing in the original either
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "LHScriptX: CREATE_SPELL_ICON({}, {}, {}, {}, {}) does nothing.",
	                    glm::to_string(position), param2, param3, param4, param5);
}

void FeatureScriptCommands::CreatePlannedSpellIcon(int32_t townId, glm::vec3 position, const std::string& spellName,
                                                   int32_t param4, int32_t param5, int32_t param6)
{
	// command 14: only the town's magic type (the planned icon itself is not made)
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "LHScriptX: CREATE_PLANNED_SPELL_ICON({}, {}, {}, {}, {}, {})", townId,
	                    glm::to_string(position), spellName, param4, param5, param6);
	magic::script::CreatePlannedSpellIcon(townId, spellName); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateVillager(glm::vec3 position, [[maybe_unused]] glm::vec3 param2,
                                           [[maybe_unused]] const std::string& param3)
{
	// command 15 "AAL": the position A0 is also the abode's; the info is read from the text of the SECOND argument, an
	// 'A', so a position: never a villager's name -> no villager info; the age is integer slot 2, which this command's
	// 'L' does not write. No shipped script uses it.
	// (openblack) k_NoVillagerInfo directly: GetParameter has already turned arg 1 into a vec3, its text is gone, and a
	// "x,z" text never names a villager
	const auto info = ValidVillagerInfo(villager_commands::k_NoVillagerInfo, "CREATE_VILLAGER");
	if (!info.has_value())
	{
		return;
	}
	CreateVillagerAtAbode(position, position, *info, Script::IntSlot(2));
}

void FeatureScriptCommands::CreateTownVillager(int32_t townId, glm::vec3 position, const std::string& villagerType, int32_t age)
{
	// command 16 "NAAN": the villager info named by arg 2
	if (const auto info = ValidVillagerInfo(villager_commands::VillagerInfoFromText(villagerType), "CREATE_TOWN_VILLAGER");
	    info.has_value())
	{
		CreateVillagerInTown(townId, position, *info, age);
	}
}

void FeatureScriptCommands::CreateSpecialTownVillager(int32_t townId, glm::vec3 position, int32_t villagerInfo, int32_t age)
{
	// command 17 "NANN": the info index N2 itself (no check in the original)
	if (const auto info = ValidVillagerInfo(villagerInfo, "CREATE_SPECIAL_TOWN_VILLAGER"); info.has_value())
	{
		CreateVillagerInTown(townId, position, *info, age);
	}
}

void FeatureScriptCommands::CreateVillagerPos(glm::vec3 abodePosition, glm::vec3 position, const std::string& tribeAndNumber,
                                              int32_t age)
{
	// command 18 "AALN" (shares CREATE_VILLAGER's handler, not command 16's): the villager info named by arg 2, the age
	// N3, the abode looked up at A0 and the villager made at A1
	if (const auto info = ValidVillagerInfo(villager_commands::VillagerInfoFromText(tribeAndNumber), "CREATE_VILLAGER_POS");
	    info.has_value())
	{
		CreateVillagerAtAbode(abodePosition, position, *info, age);
	}
}

void FeatureScriptCommands::CreateCitadel(glm::vec3 position, int32_t, const std::string& playerOwner, int32_t rotation,
                                          int32_t /*size*/)
{
	// the citadel heart is made with the angle, scale 1.0, life 1.0: the script's size is
	// ignored (some lands pass 0, 300 or 4121) and the temple is made built
	// (the creation indices of the heart, its CitadelEntrance and its three leash posts: CitadelArchetype::CreateHeart)
	// the player's temple heart becomes the last created object
	Script::SetLastCreated(
	    CitadelArchetype::Create(position, GetPlayerName(playerOwner), GetRotation(rotation), glm::vec3(1.0f)));
}

void FeatureScriptCommands::CreatePlannedCitadel(int32_t townId, glm::vec3 position, int32_t heartInfo,
                                                 const std::string& playerOwner, int32_t rotation, int32_t size)
{
	// needs the town and a player name that resolves, else nothing; then the planned citadel heart: heart info N3,
	// angle = N5 x 0.001, scale = N6 x 0.001
	const auto town = FindTown(townId);
	if (town == entt::null || !k_PlayerLookup.contains(playerOwner))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), R"(LHScriptX: CREATE_PLANNED_CITADEL: no town {} or player "{}", skipped)",
		                   townId, playerOwner);
		return;
	}
	CitadelArchetype::CreatePlan(town, position, static_cast<uint32_t>(heartInfo), static_cast<float>(rotation) * 0.001f,
	                             static_cast<float>(size) * 0.001f);
	// the plan is the last created object: none in openblack (see CREATE_PLANNED_ABODE)
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::CreateCreaturePen([[maybe_unused]] glm::vec3 position, int32_t, int32_t, int32_t, int32_t, int32_t)
{
	// (not ported) the pen or its plan, which the original makes the last created object: none here
	Script::SetLastCreated(entt::null);
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateWorshipSite([[maybe_unused]] glm::vec3 position, int32_t, const std::string& playerOwner,
                                              const std::string& tribeType, int32_t, int32_t)
{
	// command 19: only the player and the tribe are used; the site's place comes from its citadel slot
	Tribe tribe;
	try
	{
		tribe = k_TribeLookup.at(tribeType);
	}
	catch (...)
	{
		std::throw_with_nested(std::runtime_error("Could not recognize worship site tribe"));
	}
	// the site becomes the last created object, null too, once the player's temple is found
	if (const auto site = magic::script::CreateWorshipSite(GetPlayerName(playerOwner), tribe); site.has_value())
	{
		Script::SetLastCreated(*site); // Magic/Script/MapScriptMagic.cpp
	}
}

void FeatureScriptCommands::CreatePlannedWorshipSite([[maybe_unused]] glm::vec3 position, int32_t, const std::string&,
                                                     const std::string&, int32_t, int32_t)
{
	// command 23: the original reads the line and does nothing with it. Nothing is made, nothing is drawn from the
	// random generator and the last created object stays the previous command's. A worship site still to be built
	// comes only from a citadel asking for one for a town's tribe
}

void FeatureScriptCommands::CreateAnimal(glm::vec3 position, int32_t type, int32_t flock, int32_t townId)
{
	// command 24 "ANNN": type, flock id, town id; age 0 -> random
	CreateNewAnimal(position, type, flock, townId, 0);
}

void FeatureScriptCommands::CreateNewAnimal(glm::vec3 position, int32_t type, int32_t flock, int32_t townId, int32_t age)
{
	// command 25 "ANNNN": type, flock id, town id, age. The flock is the one made by CREATE_FLOCK with that id; with
	// none, the animal is made without a flock
	const auto& flocks = Locator::entitiesRegistry::value().Context().flocks;
	const auto found = flocks.find(flock);
	// the animal becomes the last created object, null too
	Script::SetLastCreated(AnimalArchetype::Create(position, static_cast<AnimalInfo>(type), FindTown(townId),
	                                               found != flocks.end() ? found->second : entt::null,
	                                               static_cast<uint32_t>(std::max(age, 0))));
}

void FeatureScriptCommands::CreateForest(int32_t forestId, glm::vec3 position)
{
	// a forest with the script's id (0 takes the next free one): the forest its trees are looked up in
	ecs::CreateForest(static_cast<uint32_t>(forestId), position);
	// the forest becomes the last created object. (pending) openblack's forests are not entities: none, so a
	// LINK_FOOTPATH after it links nothing where the original's forest takes the footpath
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::CreateTree(int32_t forestId, glm::vec3 position, TreeInfo treeType, int32_t rotation, int32_t scale)
{
	CreateNewTree(forestId, position, treeType, 1, rotation * 0.001f, scale * 0.001f, scale * 0.001f);
}

void FeatureScriptCommands::CreateDeadTree(glm::vec3 position, const std::string& player, TreeInfo treeType, float life,
                                           float xAngle, float yAngle, float zAngle)
{
	// case 43: a dead tree with the type's normal mesh, life F3, X/Y/Z angles F4, F5, F6 and scale 1, made for the player
	// the text names (the neutral player when it names none). The player is not drawn. It becomes the last created
	// object
	Script::SetLastCreated(DeadTreeArchetype::Create(position, treeType, PlayerFromText(player), life, xAngle, yAngle, zAngle));
}

void FeatureScriptCommands::CreateNewTree(int32_t forestId, glm::vec3 position, TreeInfo treeType, int32_t isNonScenic,
                                          float rotation, float currentSize, float maxSize)
{
	// cases 27 and 28: nothing on top of another object (no log; the script goes on)
	if (!ecs::map_collide::IsOkToCreateAtPos(position, "CREATE_NEW_TREE"))
	{
		return;
	}
	// the script's forest id is looked up in the forest list: a tree whose forest does not exist has none
	// the tree becomes the last created object
	Script::SetLastCreated(TreeArchetype::Create(ecs::ResolveForestId(forestId), position, treeType,
	                                             static_cast<bool>(isNonScenic), rotation, maxSize, currentSize));
}

void FeatureScriptCommands::CreateField(glm::vec3 position, FieldTypeInfo type)
{
	CreateTownField(-1, position, type);
}

void FeatureScriptCommands::CreateTownField(int32_t townId, glm::vec3 position, FieldTypeInfo type)
{
	CreateNewTownField(townId, position, type, 0.0f);
}

void FeatureScriptCommands::CreateFishFarm(glm::vec3 position, int32_t info)
{
	// case 31: a fish farm of info N1, no town. It becomes the last created object
	Script::SetLastCreated(FishFarmArchetype::Create(position, static_cast<uint32_t>(info)));
}

void FeatureScriptCommands::CreateTownFishFarm(int32_t townId, glm::vec3 position, int32_t info)
{
	// case 32: nothing without the town; then the same as CREATE_FISH_FARM with it (the farm's ctor takes the
	// nearest town anyway)
	if (FindTown(townId) == entt::null)
	{
		return;
	}
	CreateFishFarm(position, info);
}

void FeatureScriptCommands::CreateFeature(glm::vec3 position, FeatureInfo type, int32_t rotation, int32_t scale,
                                          int32_t planned)
{
	// case 33: with N4 != 0 a planned feature, as CREATE_NEW_FEATURE's: nothing is made here, and the last created
	// object, the plan, is none
	if (planned != 0)
	{
		Script::SetLastCreated(entt::null);
		return;
	}
	// the feature becomes the last created object
	Script::SetLastCreated(FeatureArchetype::Create(position, type, rotation * 0.001f, scale * 0.001f));
}

void FeatureScriptCommands::CreateFlowers(glm::vec3 position, FlowersInfo type, float rotation, float scale)
{
	// the angle and the scale are the script's floats as they are, not thousandths as CREATE_FEATURE's; the patch
	// becomes the last created object, null too
	Script::SetLastCreated(FlowersArchetype::Create(position, type, rotation, scale));
}

void FeatureScriptCommands::CreateWallSection([[maybe_unused]] glm::vec3 position, int32_t, int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreatePlannedWallSection([[maybe_unused]] glm::vec3 position, int32_t, int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreatePitch([[maybe_unused]] glm::vec3 position, int32_t, int32_t, int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreatePot(glm::vec3 position, PotInfo type, int32_t /*unused*/, int32_t amount)
{
	// case 38: nothing on top of another object, then nothing for an amount <= 0
	if (!ecs::map_collide::IsOkToCreateAtPos(position, "CREATE_POT") || amount <= 0)
	{
		return;
	}
	// the pot is created with its reaction flag set: it spreads its reaction at creation (food: the hungry
	// grazers come and eat). It becomes the last created object
	const auto pot = PotArchetype::Create(position, 0.0f, type, amount);
	ecs::animal_ai::SetupPotReaction(pot);
	Script::SetLastCreated(pot);
}

void FeatureScriptCommands::CreateTownTemporaryPots(int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateMobileObject(glm::vec3 position, MobileObjectInfo type, int32_t rotation, int32_t scale)
{
	// case 40: nothing on top of another object
	if (!ecs::map_collide::IsOkToCreateAtPos(position, "CREATE_MOBILEOBJECT"))
	{
		return;
	}
	// it becomes the last created object
	Script::SetLastCreated(MobileObjectArchetype::Create(position, type, rotation * 0.001f, scale * 0.001f));
}

void FeatureScriptCommands::CreateMobileStatic(glm::vec3 position, MobileStaticInfo type, float yRotation, float scale)
{
	// CREATE_MOBILESTATIC "ANFF", case 41: no vertical offset, Y angle F2, scale F3. It becomes the last created object
	Script::SetLastCreated(MobileStaticArchetype::CreateFromInfo(position, type, 0.0f, yRotation, scale));
}

void FeatureScriptCommands::CreateMobileUStatic(glm::vec3 position, MobileStaticInfo type, float verticalOffset,
                                                float xRotation, float yRotation, float zRotation, float scale)
{
	// CREATE_MOBILE_STATIC "ANFFFFF", case 42: the position raised by F2, X/Y/Z angles F3, F4, F5, scale F6. It becomes
	// the last created object
	Script::SetLastCreated(
	    MobileStaticArchetype::CreateWithXYZAngles(position, type, verticalOffset, xRotation, yRotation, zRotation, scale));
}

void FeatureScriptCommands::CreateScaffold(int32_t townId, glm::vec3 position, int32_t, int32_t rotation, int32_t size)
{
	// CREATE_SCAFFOLD "NANNN", case 44: a scaffold of the town numbered N0 (none for a number no town has) at the
	// position, with no owner, its Y angle N3 and its size N4 in thousandths. N2, its row of the scaffolds' table, is not
	// read: the game has one. It becomes the last created object
	Script::SetLastCreated(ecs::scaffolds::Create(map_coords::FromWorld(position), FindTown(townId), entt::null,
	                                              static_cast<float>(rotation) * 0.001f, static_cast<float>(size) * 0.001f));
}

void FeatureScriptCommands::CountryChange([[maybe_unused]] glm::vec3 position, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::HeightChange([[maybe_unused]] glm::vec3 position, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateCreature(glm::vec3 position, int32_t type, int32_t player)
{
	// the type, a row of the creature tables, and the player by number: a creature for a player who has none, or for
	// nobody when the number names no player. The point through the map's fixed point; what is made, or nothing, is
	// the last created object
	Script::SetLastCreated(ecs::player_creature::MakeForLandScript(
	    type, glm::vec2(map_coords::Quantise(position.x), map_coords::Quantise(position.z)),
	    ecs::player_creature::PlayerOfNumber(player)));
}

PlayerNames FeatureScriptCommands::PlayerFromText(const std::string& name)
{
	for (size_t i = 0; i < k_PlayerNamesStrs.size(); ++i)
	{
		const auto& candidate = k_PlayerNamesStrs.at(i);
		if (candidate.size() == name.size() && std::equal(candidate.begin(), candidate.end(), name.begin(), [](char a, char b) {
			    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		    }))
		{
			return static_cast<PlayerNames>(i);
		}
	}
	return PlayerNames::NEUTRAL;
}

void FeatureScriptCommands::CreateCreatureFromFile(const std::string& playerName, CreatureType creatureType,
                                                   const std::string& creatureMind, glm::vec3 position)
{
	// the owner is found ignoring case among the game's eight players, whether the land uses them or not, any other name
	// being the neutral player's; every one of them can have a creature loaded
	const auto playerType = PlayerFromText(playerName);
	// the script's creature type 0 is the Giant Ape
	if (creatureType == CreatureType::Unknown)
	{
		creatureType = CreatureType::GiantApe;
	}
	// the same step as LOAD_CREATURE's after its checks: the file's body, the leashes known, fully grown, and the
	// creature its owner leads
	ecs::player_creature::MakeFromMindFile(
	    static_cast<int32_t>(openblack::script::property_rules::ScriptCreatureType(creatureType)), creatureMind, playerType,
	    glm::vec2(position.x, position.z));
}

void FeatureScriptCommands::CreateFlock(int32_t flockId, glm::vec3 position, glm::vec3 domainCentre, int32_t domainRadius,
                                        int32_t param5, int32_t param6)
{
	// case 49 "NAANNN": a flock at A1 for the current player with id N0, domain centre A2, domain radius N3 (0 -> 80).
	// From VERSION 2.1 on, the flock distance is N4 and the town N5; before, the town is N4 and the distance stays 30.
	// The town gets it on its flock list.
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& flock = registry.Assign<Flock>(entity);
	flock.id = flockId;
	flock.savedDomainCentre = position;
	flock.domainCentre = domainCentre;
	flock.domainRadius = domainRadius != 0 ? static_cast<uint16_t>(domainRadius) : static_cast<uint16_t>(0x50);
	const bool newVersion = !(Locator::mapScriptSystem::value().Globals().version < 2.1f);
	if (newVersion)
	{
		flock.flockDistance = static_cast<uint16_t>(param5);
	}
	if (const auto town = FindTown(newVersion ? param6 : param5); town != entt::null)
	{
		flock.town = town;
		registry.Get<Town>(town).flocks.push_back(entity);
	}
	registry.Context().flocks.insert_or_assign(flockId, entity);
	// the flock becomes the last created object
	Script::SetLastCreated(entity);
}

void FeatureScriptCommands::LoadLandscape(const std::string& path)
{
	Game::LoadLandscape(path);
}

void FeatureScriptCommands::Version(float version)
{
	// case 51: the land's script version
	Locator::mapScriptSystem::value().Globals().version = version;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "LHScriptX: Land version set to: {}", version);
}

void FeatureScriptCommands::CreateArea([[maybe_unused]] glm::vec3 position, float)
{
	// the area leaves no last created object
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::StartCameraPos(glm::vec3 focus)
{
	static constexpr auto k_DefaultCameraOriginOffset = 120.0f;
	static constexpr auto k_DefaultCameraOriginOffsetAngles = glm::radians(glm::vec2(12.8571f, 157.51f));

	auto& camera = Locator::camera::value();
	const auto offset = k_DefaultCameraOriginOffset * glm::euclidean(k_DefaultCameraOriginOffsetAngles);
	camera.SetFocus(focus).SetOrigin(focus + offset);
}

void FeatureScriptCommands::FlyByFile([[maybe_unused]] const std::string& path)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::TownNeedsPos([[maybe_unused]] int32_t townId, [[maybe_unused]] glm::vec3 position)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateFurniture([[maybe_unused]] glm::vec3 position, int32_t, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateBigForest(glm::vec3 position, BigForestInfo type, float rotation, float scale)
{
	CreateNewBigForest(position, type, 0, rotation, scale);
}

void FeatureScriptCommands::CreateNewBigForest(glm::vec3 position, BigForestInfo type, int32_t unknown, float rotation,
                                               float scale)
{
	// the forest becomes the last created object
	Script::SetLastCreated(BigForestArchetype::Create(position, type, unknown, rotation, scale));
}

void FeatureScriptCommands::CreateInfluenceRing(glm::vec3 position, int32_t player, float radius, int32_t anti)
{
	// case 59: an influence ring for the player, radius and anti flag. (inferred) the range check is openblack's: the
	// original does not test the player index
	if (player >= 0 && player < static_cast<int32_t>(PlayerNames::_COUNT))
	{
		// the ring becomes the last created object
		Script::SetLastCreated(influence::CreateRing(position, static_cast<PlayerNames>(player), radius, anti != 0));
	}
}

void FeatureScriptCommands::CreateWeatherClimate(int32_t id, int32_t info, glm::vec3 position, float radius1, float radius2)
{
	// case 60: a climate of info N1 at the position, radii F3, F4, id N0: Magic/Script/MapScriptWeather.cpp ->
	// ECS/Weather/Climate (id 0 makes a default climate that ignores the rest; otherwise the radii in order)
	magic::map_script::CreateWeatherClimate(id, info, position, radius1, radius2);
	// the climate leaves no last created object
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::CreateWeatherClimateRain(int32_t id, float desire, int32_t dryDays, int32_t rainingDays,
                                                     int32_t flags)
{
	// case 61: the climate's rain = {F1, N2, N3, (uint8_t)N4}; nothing for an unknown id
	magic::map_script::CreateWeatherClimateRain(id, desire, dryDays, rainingDays, flags);
}

void FeatureScriptCommands::CreateWeatherClimateTemp(int32_t id, float temperature, float target)
{
	// case 62: the climate's temperature F1 and target F2
	magic::map_script::CreateWeatherClimateTemp(id, temperature, target);
}

void FeatureScriptCommands::CreateWeatherClimateWind(int32_t id, float windX, float windZ, float angle)
{
	// case 63: the climate's wind = {F1, F2, F3}
	magic::map_script::CreateWeatherClimateWind(id, windX, windZ, angle);
}

void FeatureScriptCommands::CreateWeatherStorm(int32_t climate, glm::vec3 position, float age, int32_t numClouds,
                                               const std::string& shape, const std::string& clouds, const std::string& weather,
                                               float speed, glm::vec3 target)
{
	magic::map_script::CreateWeatherStorm(climate, position, age, numClouds, shape, clouds, weather, speed, target);
	// the storm leaves no last created object
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::BrushSize(float, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateStream(int32_t streamId)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& registryContext = registry.Context();
	const auto entity = registry.Create();

	registry.Assign<Stream>(entity, streamId);
	registryContext.streams.insert({streamId, entity});
}

void FeatureScriptCommands::CreateStreamPoint(int32_t streamId, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& registryContext = registry.Context();

	// the point sits on the ground; appended at the tail
	auto point = position;
	if (Locator::terrainSystem::has_value())
	{
		point.y = Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	}
	registry.Get<Stream>(registryContext.streams.at(streamId)).points.push_back(point);
}

void FeatureScriptCommands::CreateWaterfall(glm::vec3 position)
{
	// an empty object on the ground: it has no mesh, makes no sound, does nothing and is put on no map cell, but it is
	// the last created object (no land of the game makes one; Land 3's waterfall is designed scenery)
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::terrainSystem::has_value())
	{
		position.y = Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	}
	const auto waterfall = registry.Create();
	registry.Assign<Transform>(waterfall, position, glm::mat3(1.0f), glm::vec3(1.0f));
	Script::SetLastCreated(waterfall);
}

void FeatureScriptCommands::CreateArena(glm::vec3 position, float radius)
{
	// case 69: an arena, not drawn until a creature fight is on
	auto& registry = Locator::entitiesRegistry::value();
	const auto arena = registry.Create();
	registry.Assign<Arena>(arena, position, radius);
	// the arena becomes the last created object
	Script::SetLastCreated(arena);
}

void FeatureScriptCommands::CreateFootpath([[maybe_unused]] int32_t footpathId)
{
	// a new empty footpath; the script's number is not kept: the nodes find their footpath by its position in the
	// list (CreateFootpathNode)
	static_cast<void>(ecs::footpaths::Create());
}

void FeatureScriptCommands::CreateFootpathNode(int footpathId, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	// every footpath of the game's list whose position from the head is `footpathId` (at most one) gets the node;
	// none -> nothing
	const auto entity = ecs::footpaths::AtPositionFromHead(footpathId);
	if (entity == entt::null)
	{
		return;
	}
	auto& footpath = registry.Get<Footpath>(entity);
	// a new node at the position's map coordinates, inserted at the list's head
	const auto coords = map_coords::FromWorld(position);
	footpath.nodes.insert(footpath.nodes.begin(), Footpath::Node {position, coords, 1, footpath.nextNodeId++});
}

void FeatureScriptCommands::LinkFootpath(int32_t footpathId)
{
	// case 72: the footpath at position N0 from the head of the game's footpath list goes to the last created object's
	// link; nothing without a last created object or that footpath
	const auto thing = Script::LastCreated();
	const auto footpath = ecs::footpaths::AtPositionFromHead(footpathId);
	if (thing == entt::null || footpath == entt::null)
	{
		return;
	}
	// only a multi-map fixed object holds a footpath link (a forest and a dance too, which are not script things in
	// openblack); any other thing takes nothing
	if (ecs::map_cells::IsMultiCellStaticClass(thing))
	{
		ecs::footpaths::AddFootpath(thing, footpath);
	}
}

void FeatureScriptCommands::CreateBonfire(glm::vec3 position, [[maybe_unused]] float temperature, float yAngle, float scale)
{
	// case 73: F1 temperature, F2 Y angle, F3 scale; the bonfire ignores F1
	// the bonfire becomes the last created object
	Script::SetLastCreated(BonfireArchetype::Create(position, yAngle, scale));
}

void FeatureScriptCommands::CreateBase([[maybe_unused]] glm::vec3 position, int32_t)
{
	// (not ported) the base, which the original makes the last created object: none here
	Script::SetLastCreated(entt::null);
}

void FeatureScriptCommands::CreateNewFeature(glm::vec3 position, const std::string& type, int32_t rotation, int32_t scale,
                                             int32_t param5)
{
	// case 75: with N5 != 0 a planned feature, which is never drawn and only matters to the town's building plans (not
	// simulated yet): nothing is made here, and the last created object, the plan, is none. No land uses it.
	if (param5 != 0)
	{
		Script::SetLastCreated(entt::null);
		return;
	}
	const auto info = GFeatureInfo::Find(type);
	if (info == FeatureInfo::None)
	{
		return; // openblack: the original has no check (see GFeatureInfo::Find)
	}
	// the feature becomes the last created object
	Script::SetLastCreated(FeatureArchetype::Create(position, info, rotation * 0.001f, scale * 0.001f));
}

void FeatureScriptCommands::SetInteractDesire(float desire)
{
	Locator::mapScriptSystem::value().Globals().interactDesire = desire;
}

void FeatureScriptCommands::ToggleComputerPlayer(const std::string& affiliation, int32_t toggle)
{
	// The original's players all exist from the start; openblack makes the player the first time a script names it,
	// listed in the player system like the one LOAD_LANDSCAPE makes
	const auto name = GetPlayerName(affiliation);
	auto player = magic::players::EntityOf(name);
	if (player == entt::null)
	{
		player = PlayerArchetype::Create(name);
		Locator::playerSystem::value().AddPlayer(player);
	}
	auto& playerMagic = Locator::entitiesRegistry::value().Get<PlayerMagic>(player);
	playerMagic.playerType = magic::players::TypeAfterComputerToggle(playerMagic.playerType, toggle != 0);
	// Any value but 0 switches the computer god mind of the player on, and 0 switches it off
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SwitchOn(name, toggle != 0);
	}
}

void FeatureScriptCommands::SetComputerPlayerCreatureLike(const std::string& affiliation, const std::string& like)
{
	// The computer mind keeps the name, which starts the size change of its creature at the start of the mind
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetCreatureLike(GetPlayerName(affiliation), GetPlayerName(like));
	}
}

void FeatureScriptCommands::MultiplayerDebug(int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateStreetLantern(glm::vec3 position, int32_t type)
{
	// case 80: a street lantern of mobile static info N1; any info other than 7 (Land1: 59,
	// Country Lantern) is a country lantern with the campfire mesh, not a Bonfire
	// the lantern becomes the last created object
	Script::SetLastCreated(StreetLanternArchetype::Create(position, static_cast<MobileStaticInfo>(type)));
}

void FeatureScriptCommands::CreateStreetLight(glm::vec3 position)
{
	// TODO: case 81 makes a street light, not decoded yet; no land uses it. Drawn as a town lantern.
	// the light becomes the last created object
	Script::SetLastCreated(StreetLanternArchetype::Create(position, MobileStaticInfo::StreetLantern));
}

void FeatureScriptCommands::SetLandNumber(int32_t number)
{
	// case 82: the land number, kept in the map globals
	Locator::mapScriptSystem::value().Globals().landNumber = number; // read by ECS/Influence and the worship sites
}

void FeatureScriptCommands::CreateOneShotSpell(glm::vec3 position, const std::string& seed)
{
	// the orb becomes the last created object, null too, once the seed is read (Magic/Script/MapScriptMagic.cpp)
	if (const auto orb = magic::script::CreateOneShotSpell(position, seed); orb.has_value())
	{
		Script::SetLastCreated(*orb);
	}
}

void FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3 position, const std::string& magicName)
{
	// the orb becomes the last created object, null too, once the magic is known (Magic/Script/MapScriptMagic.cpp)
	if (const auto orb = magic::script::CreateOneShotSpellPu(position, magicName); orb.has_value())
	{
		Script::SetLastCreated(*orb);
	}
}

void FeatureScriptCommands::CreateFireFly([[maybe_unused]] glm::vec3 position)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::TownDesireBoost(int32_t townId, const std::string& desire, float boost)
{
	// command 86: the town with the id and the desire named (ignoring case); both found -> the town's desire boost =
	// boost, without re-sorting nor a range check (Land2.txt: "Abodes" / "Civic_Buildings" -0.75)
	ecs::town_desire::MapTownDesireBoost(FindTown(townId), desire, boost);
}

void FeatureScriptCommands::CreateAnimatedStatic(glm::vec3 position, const std::string& type, int32_t rotation, int32_t scale)
{
	auto animatedStaticType = GAnimatedStaticInfo::Find(type);
	if (animatedStaticType == AnimatedStaticInfo::None)
	{
		return; // openblack: the original has no check (see GAnimatedStaticInfo::Find)
	}
	// it becomes the last created object
	Script::SetLastCreated(AnimatedStaticArchetype::Create(position, animatedStaticType, rotation * 0.001f, scale * 0.001f));
}

void FeatureScriptCommands::FireFlySpellRewardProb(const std::string& spell, float probability)
{
	magic::script::FireFlySpellRewardProb(spell, probability); // Magic/Script/MapScriptMagic.cpp
	// the same table kept in the map globals: case 88: the first of the 42 magic effect names equal ignoring case,
	// else 42; out of range does nothing; else the table entry and the running sums
	auto& globals = Locator::mapScriptSystem::value().Globals();
	const auto& effects = Locator::infoConstants::value().magicEffect;
	size_t index = MapScriptGlobals::k_MagicCount;
	for (size_t i = 0; i < effects.size() && i < MapScriptGlobals::k_MagicCount; ++i)
	{
		const std::string_view name(effects[i].debugString.data());
		if (name.size() == spell.size() && std::equal(name.begin(), name.end(), spell.begin(), [](char a, char b) {
			    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		    }))
		{
			index = i;
			break;
		}
	}
	if (index >= MapScriptGlobals::k_MagicCount)
	{
		return;
	}
	globals.fireFlySpellRewardProbability.at(index) = probability;
	float sum = 0.0f;
	for (size_t i = 0; i < MapScriptGlobals::k_MagicCount; ++i)
	{
		sum += globals.fireFlySpellRewardProbability.at(i);
		globals.fireFlySpellRewardCumulative.at(i) = sum;
	}
}

void FeatureScriptCommands::CreateNewTownField(int32_t townId, glm::vec3 position, FieldTypeInfo townFieldType, float rotation)
{
	// Rotation is in radians and not scaled
	// the town's ABODE_FIELD abode (mesh 594), angle F3, scale 1
	// the field becomes the last created object (CREATE_FIELD and CREATE_TOWN_FIELD come here too)
	Script::SetLastCreated(FieldArchetype::Create(static_cast<int>(TownKeyOf(townId)), position, townFieldType, rotation));
}

void FeatureScriptCommands::CreateSpellDispenser(int32_t townId, glm::vec3 position, const std::string& abodeInfo,
                                                 const std::string& magicName, float yAngle, float scale, float period)
{
	// the dispenser Abode and its one-shot orb take their own creation indices
	// the dispenser becomes the last created object, null too (Magic/Script/MapScriptMagic.cpp)
	Script::SetLastCreated(
	    magic::script::CreateSpellDispenser(townId, position, GAbodeInfo::Find(abodeInfo), magicName, yAngle, scale, period));
	// it is a fixed map object (its Abode's InsertMapObject, AbodeArchetype::Create): it blocks the trees made after it
}

void FeatureScriptCommands::LoadComputerPlayerPersonality(int32_t player, const std::string& file)
{
	// The player by its number, and the personality file read over its mind as the challenge scripts' load reads it.
	// The second argument is a position's place in the command's table, but its text is taken as it is: the file's name.
	// (inferred) the range check is openblack's: the original has no player for another number and does not test it
	if (player < 0 || player >= static_cast<int32_t>(PlayerNames::_COUNT))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LOAD_COMPUTER_PLAYER_PERSONALLTY: no player {}", player);
		return;
	}
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().LoadPersonality(static_cast<PlayerNames>(player), file);
	}
}

void FeatureScriptCommands::SetComputerPlayerPersonality(const std::string& player, const std::string& name, float value)
{
	// The player by its name, any other name being the neutral player's; every node of its mind that the name names
	// takes the weight, as the challenge scripts' SET_COMPUTER_PLAYER_PERSONALITY gives it. The name stands where the
	// command's table has a position, and its text is taken as it is
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPersonality(PlayerFromText(player), name, value);
	}
}

void FeatureScriptCommands::SetGlobalLandBalance(int32_t index, float value)
{
	land_balance::Set(index, value);
}

void FeatureScriptCommands::SetLandBalance(const std::string&, int32_t, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateDrinkWaypoint(glm::vec3 position)
{
	// case 95: an invisible waypoint, numbered in the order the land makes them
	auto& registry = Locator::entitiesRegistry::value();
	const auto number = static_cast<uint32_t>(registry.Size<DrinkWaypoint>());
	const auto waypoint = registry.Create();
	registry.Assign<DrinkWaypoint>(waypoint, position, number);
	// it becomes the last created object
	Script::SetLastCreated(waypoint);
}

void FeatureScriptCommands::SetTownInfluenceMultiplier(float multiplier)
{
	// case 96
	Locator::mapScriptSystem::value().Globals().townInfluenceMultiplier = multiplier; // read by ECS/Influence
}

void FeatureScriptCommands::SetPlayerInfluenceMultiplier(float multiplier)
{
	// case 97
	Locator::mapScriptSystem::value().Globals().playerInfluenceMultiplier = multiplier; // read by ECS/Influence
}

void FeatureScriptCommands::SetTownBalanceBeliefScale(int32_t townId, float scale)
{
	// case 98: nothing without the town; otherwise the scale of its pending belief (ecs::town_belief::Fold)
	if (const auto town = FindTown(townId); town != entt::null)
	{
		Locator::entitiesRegistry::value().Get<Town>(town).belief.beliefScale = scale;
	}
}

void FeatureScriptCommands::StartGameMessage([[maybe_unused]] const std::string& message, [[maybe_unused]] int32_t landNumber)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::AddGameMessageLine([[maybe_unused]] const std::string& message, [[maybe_unused]] int32_t landNumber)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::EditLevel()
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::SetNighttime(float duration, float night, float change)
{
	// the visual day/night cycle set from the map editor's values
	Locator::dayNightClock::value().Clock().SetCycleFromLand(duration, night, change);
}

void FeatureScriptCommands::MakeLastObjectArtifact(int32_t townId, const std::string& playerName, float value)
{
	// case 103 "NLF": the last created object, when it is still there and can become an artefact (its kind is worth
	// making one of and no script holds it) and it is a fixed thing, becomes an artefact of the player named L1 (the
	// neutral player for a name no player has), then is given the worth F2
	const auto object = Script::LastCreated();
	if (object == entt::null || !ecs::IsAvailable(object) || !Locator::artefactSystem::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* info = ecs::fire::traits::InfoOf(object);
	const auto kind = ecs::map_cells::KindOf(object);
	const bool fixed = kind == ecs::map_cells::InsertKind::SingleMapFixed ||
	                   kind == ecs::map_cells::InsertKind::MultiMapFixed || kind == ecs::map_cells::InsertKind::FishFarm;
	if (info == nullptr || !(info->artifactMultiplier > 0.0f) || registry.AllOf<InScript>(object) || !fixed)
	{
		return;
	}
	const auto player = PlayerFromText(playerName);
	// its town: the one numbered N0; else the nearest town of all to it, when the player owns it; else none
	const auto named = FindTown(townId);
	const auto nearest = named == entt::null ? ecs::map_cells::FindNearestTownInList(ecs::object::MapCoordsOf(object))
	                                         : entt::entity {entt::null};
	std::optional<PlayerNames> nearestOwner;
	if (nearest != entt::null)
	{
		nearestOwner = registry.Get<const Town>(nearest).owner;
	}
	auto& artefacts = Locator::artefactSystem::value();
	switch (ecs::artefact_rules::TownForScriptArtefact(named != entt::null, nearestOwner, player))
	{
	case ecs::artefact_rules::ScriptArtefactTown::Named:
		artefacts.AddToTown(object, named, player);
		break;
	case ecs::artefact_rules::ScriptArtefactTown::Nearest:
		artefacts.AddToTown(object, nearest, player);
		break;
	case ecs::artefact_rules::ScriptArtefactTown::None:
		artefacts.AddToNoTown(object, player);
		break;
	}
	artefacts.SetWorth(object, value);
}

void FeatureScriptCommands::SetLostTownScale(float scale)
{
	// case 104: the lost town scale, 1 again on a land balance reset (land_balance::Reset); read by the
	// town belief's fold (the boredom and the belief left in a player's towns when one is lost)
	land_balance::SetLostTownScale(scale);
}
