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
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/polar_coordinates.hpp>
#include <glm/gtx/string_cast.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "LHScriptX/Script.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/Trees.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Influence/Influence.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Magic/Script/MapScriptMagic.h"
#include "Magic/Script/MapScriptWeather.h"
#include "Worship/WorshipPercentage.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptingBindingUtils.h"

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
const auto k_VillagerNumberLookup = makeLookup<VillagerNumber>(k_VillagerNumberStrs);

std::tuple<Tribe, VillagerNumber> GetVillagerTribeAndNumber(const std::string& villagerTribeWithType)
{
	const auto pos = villagerTribeWithType.find_first_of('_');
	const auto tribeStr = villagerTribeWithType.substr(0, pos);
	const auto roleStr = villagerTribeWithType.substr(pos + 1);

	try
	{
		const auto tribe = k_TribeLookup.at(tribeStr);
		const auto role = k_VillagerNumberLookup.at(roleStr);
		return std::make_tuple(tribe, role);
	}
	catch (...)
	{
		std::throw_with_nested(std::runtime_error("Could not recognize either villager tribe or role"));
	}
}

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

/// GPlayer::GetPlayerFromText 0x64B5E0 (SET_TOWN_BELIEF, SET_TOWN_BELIEF_CAP): the player whose name matches without
/// case (_stricmp 0x64B609), else the neutral player (0x64B627..0x64B643). Not GetPlayerName, which throws
PlayerNames PlayerFromText(const std::string& name)
{
	for (size_t i = 0; i < k_PlayerNamesStrs.size(); ++i)
	{
		const auto& candidate = k_PlayerNamesStrs.at(i);
		if (candidate.size() == name.size() &&
		    std::equal(candidate.begin(), candidate.end(), name.begin(), [](char a, char b) {
			    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		    }))
		{
			return static_cast<PlayerNames>(i);
		}
	}
	return PlayerNames::NEUTRAL;
}

/// GGame::FindTownWithID 0x552FA0
entt::entity FindTown(int32_t townId)
{
	const auto& towns = Locator::entitiesRegistry::value().Context().towns;
	const auto town = towns.find(static_cast<uint32_t>(townId));
	return town != towns.end() ? town->second : entt::null;
}

/// fn_00552FF0 (`ret 4`, one MapCoords, no id branch; called at 0x7156A6, 0x7157FF, 0x715AD6, 0x715B79, 0x7168FE and
/// 0x717E15): the global town list g_game+0x205C84 (town_queries::TownsNewestFirst: the newest town first,
/// 0x73964D), the first always taken, then fn_00605CD0 = GUtils::GetDistanceInMetres 0x74CD70 strictly smaller
/// (fcomp; test ah, 1 at 0x55301B), so an exact tie goes to the newer town; none without towns. MapCoords x, z only
/// (FromMetres: the altitude is not read)
entt::entity FindNearestTown(const glm::vec3& position)
{
	return ecs::map_cells::FindNearestTownInList(ecs::map_coords::FromMetres(glm::vec2(position.x, position.z)));
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
	return glm::mat4(lh_matrix::AngleY(static_cast<float>(rotation) * 0.001f));
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
	// command "AFNFF" (0x7155C9): Mist::Create 0x6063D0(pos with relY = F1, size F3, ARGB colour N2, k F4)
	MistArchetype::Create(position, param2, static_cast<uint32_t>(param3), param4, param5);
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

	TownArchetype::Create(townId, position, GetPlayerName(playerOwner), tribe);
	// the town is not an Object, but its 7 TownDesireFlags are
	ecs::object_index::Skip(7);
}

void FeatureScriptCommands::SetTownBelief(int32_t townId, const std::string& playerOwner, float belief)
{
	// SET_TOWN_BELIEF (fn_00715150, 0x71542B): GGame::FindTownWithID 0x715449 (none: je 0x717E8A, nothing), then
	// Town::SetBeliefInPlayer 0x73BA70(P, f) 0x715460: the neutral player -> +0x5D8 = f; SetBelief 0x4387D0, capped.
	// It overwrites the slot (the old string map inserted, so it kept the first value)
	const auto player = PlayerFromText(playerOwner); // GetPlayerFromText 0x64B5E0 (0x715432)
	if (const auto town = FindTown(townId); town != entt::null)
	{
		ecs::town_belief::SetBeliefInPlayer(Locator::entitiesRegistry::value().Get<Town>(town), player, belief);
	}
}

void FeatureScriptCommands::SetTownBeliefCap(int32_t townId, const std::string& playerOwner, float belief)
{
	// SET_TOWN_BELIEF_CAP (0x715472): GetPlayerFromText 0x715479, FindTownWithID 0x715492 (none: je 0x717E8A),
	// GBelief::SetBeliefInPlayerCap 0x438A00(P, f) 0x7154B5. (pending) 0x7154BA..0x715528: the land name ==
	// .\mpm_3p_1.txt (strcmp with 0xD99648) && IsMultiplayerGame 0x552F80 (0x7154FB) && town +0x5B4 == 3 && P not
	// neutral -> cap 2.0; openblack plays no multiplayer land
	const auto player = PlayerFromText(playerOwner); // GetPlayerFromText 0x64B5E0 (0x715479)
	if (const auto town = FindTown(townId); town != entt::null)
	{
		ecs::town_belief::SetCap(Locator::entitiesRegistry::value().Get<Town>(town).belief, player, belief);
	}
}

void FeatureScriptCommands::SetTownUninhabitable(int32_t townId)
{
	// case 5 (0x715542): town +0x5F4 = 1; nothing without the town
	if (const auto town = FindTown(townId); town != entt::null)
	{
		Locator::entitiesRegistry::value().Get<Town>(town).uninhabitable = true;
	}
}

void FeatureScriptCommands::SetTownCongregationPos(int32_t townId, glm::vec3 position)
{
	// case 6 (0x715580): GGame::FindTownWithID (none: nothing, 0x717E8A); town +0xF10 = GetScriptPos(N1) (0x7155A3..
	// 0x7155B9, all three dwords), the cache of Town::GetCongregationPos 0x7408B0 (ecs::town_queries).
	// GetScriptPos 0x718250 = MapCoords(const char*) 0x6031D0 -> MapCoords::Set 0x603280: x = ftol(atof x 65536 / 10)
	// (0x6032AF), +8 = 0 (0x6032B1), z the second field (0x6032D5), and with a third ',' +8 = atof(third) (0x6032E4..
	// 0x6032EF, not scaled). Then += the offset at 0xD99724 if set (0x718261..0x71826F); GSetup::LoadMapFeatures
	// clears it (0x7180FE, ebp = 0 from 0x7180BA) and only fn_0076FA50 0x76FA69 (LandscapeVortexOut::
	// ProcessContentsOfVortex 0x5FE249) sets it, so a land's features script never has one.
	// openblack's script parser (Script.cpp GetParameter) only makes a vector of a two-field string, and its y is the
	// terrain height, not a third field: y = 0 is the literal value for that form. Every SET_TOWN_CONGREGATION_POS of
	// the shipped scripts has two fields. (aproximado) a three-field string does not reach here in openblack
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
	AbodeArchetype::Create(townId, position, type, rotation * 0.001f, size * 0.001f, static_cast<uint32_t>(foodAmount),
	                       static_cast<uint32_t>(woodAmount));
}

void FeatureScriptCommands::CreatePlannedAbode(int32_t townId, glm::vec3 position, const std::string& abodeInfo,
                                               int32_t rotation, int32_t size, [[maybe_unused]] int32_t foodAmount,
                                               [[maybe_unused]] int32_t woodAmount)
{
	// case 8 (0x715629, shared with CREATE_ABODE): the town, else the nearest one (fn_00552FF0), else nothing; the food
	// and wood are not used. Abode type 0x404 (TownCentre) -> PlannedTownCentre::Create 0x7444D0, otherwise
	// PlannedAbode::Create 0x405600; both go on the town's planned list and are never drawn.
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
}

void FeatureScriptCommands::CreateTownCentre(int32_t townId, glm::vec3 position, const std::string& abodeInfo, int32_t rotation,
                                             int32_t size, int32_t worshipPercentage)
{
	// case 9 (0x71577C)
	const auto type = GAbodeInfo::Find(abodeInfo);
	if (type == AbodeInfo::None)
	{
		return; // openblack: the original has no check (see GAbodeInfo::Find)
	}
	const auto centre = AbodeArchetype::Create(townId, position, type, rotation * 0.001f, size * 0.001f,
	                                           static_cast<uint32_t>(0), static_cast<uint32_t>(0));
	if (centre == entt::null || type == AbodeInfo::None ||
	    Locator::infoConstants::value().abode.at(static_cast<size_t>(type)).abodeType != AbodeType::TownCentre)
	{
		return;
	}
	// the town's centre (+0x9A4) if it had none, and Town::SetWorshipPercentage(N5 * 0.001) on the centre's town (the
	// branch for a centre without a town, TotemStatue::SetWorshipPercentage, can't happen here: openblack always gives
	// the abode a town)
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = FindTown(static_cast<int32_t>(registry.Get<Abode>(centre).townId));
	if (town == entt::null)
	{
		return;
	}
	auto& townData = registry.Get<Town>(town);
	if (townData.centre == entt::null)
	{
		townData.centre = centre;
	}
	townData.worshipPercentage = static_cast<float>(worshipPercentage) * 0.001f;
	// the real Town::SetWorshipPercentage 0x73C060 (Worship/WorshipPercentage.cpp: kept only with a worship site, the
	// totem statue, the villagers sent)
	worship::percentage::SetWorshipPercentage(town, townData.worshipPercentage);
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
	// command 12, the same handler as CREATE_TOWN_SPELL (0x715B4A)
	magic::script::CreateTownSpell(townId, spellName); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateSpellIcon(glm::vec3 position, const std::string& param2, int32_t param3, int32_t param4,
                                            int32_t param5)
{
	// command 13 does nothing in the original either (0x715B7C)
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

void FeatureScriptCommands::CreateVillager(glm::vec3 param1, glm::vec3 param2, const std::string& param3)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {}({}, {}, {}) not implemented.", __FILE__,
	                    __LINE__, __func__, glm::to_string(param1), glm::to_string(param2), param3);
}

void FeatureScriptCommands::CreateTownVillager(int32_t townId, glm::vec3 position, const std::string& villagerType, int32_t age)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {}({}, {}, {}, {}) not implemented.", __FILE__,
	                    __LINE__, __func__, townId, glm::to_string(position), villagerType, age);
}

void FeatureScriptCommands::CreateSpecialTownVillager(int32_t param1, glm::vec3 position, int32_t param3, int32_t param4)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {}({}, {}, {}, {}) not implemented.", __FILE__,
	                    __LINE__, __func__, param1, glm::to_string(position), param3, param4);
}

void FeatureScriptCommands::CreateVillagerPos(glm::vec3 abodePosition, glm::vec3 position, const std::string& tribeAndNumber,
                                              int32_t age)
{
	auto [tribe, number] = GetVillagerTribeAndNumber(tribeAndNumber);
	// GSetup::MapCommands case CREATE_VILLAGER_POS 0x715A4C..0x715AF7 ("AALN", 0xC21020): Villager::Create(GetScriptPos(
	// arg 1) (edi = Pram +0x800, the villager's position), GetInfoFromText(arg 2), arg 3 the age); the first argument is
	// not read as a position
	const auto villager = VillagerArchetype::Create(abodePosition, position, GVillagerInfo::Find(tribe, number), age, false);
	if (villager == entt::null)
	{
		return; // 0x715AA2: no villager -> 0x717E8A
	}
	// 0x715AA8..0x715AB5: GGame::FindTownWithID([ebp + 0x6000]): the integer slot 0, which ScanLine 0x7E7540 does not
	// write for this command (its first argument is an 'A'): the town id of the last command whose first argument was
	// an 'N' (the CREATE_ABODE / CREATE_TOWN before it in the shipped lands)
	auto town = FindTown(Script::IntSlot(0));
	// 0x715ABE..0x715ADD: none -> fn_00552FF0(GetScriptPos(arg 1)), the town nearest to the villager's position; none ->
	// nothing (0x717E8A)
	if (town == entt::null)
	{
		town = FindNearestTown(position);
	}
	if (town == entt::null)
	{
		return;
	}
	// 0x715AE3..0x715AE6: Town::AddVillagerToTown 0x73A090 (the abode: FindAbodeWithSpaceInTown)
	ecs::town_villagers::AddVillagerToTown(town, villager);
}

void FeatureScriptCommands::CreateCitadel(glm::vec3 position, int32_t, const std::string& playerOwner, int32_t rotation,
                                          int32_t /*size*/)
{
	// Citadel::CreateCitadel 0x463240 passes (angle, scale 1.0, life 1.0, 0) to CitadelHeart::Create: the script's size is
	// ignored (some lands pass 0, 300 or 4121) and the temple is made built
	// (the creation indices of the heart, its CitadelEntrance and its TempleLeash: CitadelArchetype::CreateHeart)
	CitadelArchetype::Create(position, GetPlayerName(playerOwner), GetRotation(rotation), glm::vec3(1.0f));
}

void FeatureScriptCommands::CreatePlannedCitadel(int32_t townId, glm::vec3 position, int32_t heartInfo,
                                                 const std::string& playerOwner, int32_t rotation, int32_t size)
{
	// 0x715E91: needs the town (FindTownWithID) and a player string GetPlayerFromText resolves (0x715EBE je 0x717E8A),
	// else nothing; then the PlannedTownCitadelHeart (ctor 0x467DD0): info 0xC5E270 + N3 x 0x158, angle = N5 x 0.001,
	// scale = N6 x 0.001
	const auto town = FindTown(townId);
	if (town == entt::null || !k_PlayerLookup.contains(playerOwner))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), R"(LHScriptX: CREATE_PLANNED_CITADEL: no town {} or player "{}", skipped)",
		                   townId, playerOwner);
		return;
	}
	CitadelArchetype::CreatePlan(town, position, static_cast<uint32_t>(heartInfo),
	                             static_cast<float>(rotation) * 0.001f, static_cast<float>(size) * 0.001f);
}

void FeatureScriptCommands::CreateCreaturePen([[maybe_unused]] glm::vec3 position, int32_t, int32_t, int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateWorshipSite([[maybe_unused]] glm::vec3 position, int32_t, const std::string& playerOwner,
                                              const std::string& tribeType, int32_t, int32_t)
{
	// command 19 (0x7160C8): only the player and the tribe are used; the site's place comes from its citadel slot
	Tribe tribe;
	try
	{
		tribe = k_TribeLookup.at(tribeType);
	}
	catch (...)
	{
		std::throw_with_nested(std::runtime_error("Could not recognize worship site tribe"));
	}
	magic::script::CreateWorshipSite(GetPlayerName(playerOwner), tribe); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreatePlannedWorshipSite([[maybe_unused]] glm::vec3 position, int32_t, const std::string&,
                                                     const std::string&, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateAnimal(glm::vec3 position, int32_t type, int32_t flock, int32_t townId)
{
	// command 24 "ANNN" (0x71649F): type, flock id, town id; age 0 -> random
	CreateNewAnimal(position, type, flock, townId, 0);
}

void FeatureScriptCommands::CreateNewAnimal(glm::vec3 position, int32_t type, int32_t flock, int32_t townId, int32_t age)
{
	// command 25 "ANNNN" (0x716543): type, flock id, town id, age. The flock is the one made by CREATE_FLOCK with that id
	// (g_game+0x205C44 searched by +0x8C); with none, fn_00419D10 takes the path without a flock
	const auto& flocks = Locator::entitiesRegistry::value().Context().flocks;
	const auto found = flocks.find(flock);
	AnimalArchetype::Create(position, static_cast<AnimalInfo>(type), FindTown(townId),
	                        found != flocks.end() ? found->second : entt::null, static_cast<uint32_t>(std::max(age, 0)));
}

void FeatureScriptCommands::CreateForest(int32_t forestId, glm::vec3 position)
{
	// Forest ctor 0x539BD0 with the script's id (0 takes the next free one): the forest its trees are looked up in
	ecs::CreateForest(static_cast<uint32_t>(forestId), position);
}

void FeatureScriptCommands::CreateTree(int32_t forestId, glm::vec3 position, TreeInfo treeType, int32_t rotation, int32_t scale)
{
	CreateNewTree(forestId, position, treeType, 1, rotation * 0.001f, scale * 0.001f, scale * 0.001f);
}

void FeatureScriptCommands::CreateDeadTree(glm::vec3 position, [[maybe_unused]] const std::string& player, TreeInfo treeType,
                                           float life, float xAngle, float yAngle, float zAngle)
{
	// case 43 (0x716E64): fn_00510BB0(pos, GTreeInfo[type], GetPlayerFromText(player), F3 life, F4, F5, F6, 0): a DeadTree
	// with the type's normal mesh, SetLife(F3) and SetXYZAnglesAndScale(F4, F5, F6, 1). The player is not drawn.
	DeadTreeArchetype::Create(position, treeType, life, xAngle, yAngle, zAngle);
}

void FeatureScriptCommands::CreateNewTree(int32_t forestId, glm::vec3 position, TreeInfo treeType, int32_t isNonScenic,
                                          float rotation, float currentSize, float maxSize)
{
	// cases 27 and 28 (0x716235 / 0x7162EE): nothing on top of another object (no log; the script goes on)
	if (!ecs::map_collide::IsOkToCreateAtPos(position, "CREATE_NEW_TREE"))
	{
		return;
	}
	// the script's forest id is looked up in the forest list (0x7162BE): a tree whose forest does not exist has none
	TreeArchetype::Create(ecs::ResolveForestId(forestId), position, treeType, static_cast<bool>(isNonScenic), rotation,
	                      maxSize, currentSize);
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
	// case 31 (0x7166E1): 0x52C7B0(pos, GFishFarmInfo[N1], no town)
	FishFarmArchetype::Create(position, static_cast<uint32_t>(info));
}

void FeatureScriptCommands::CreateTownFishFarm(int32_t townId, glm::vec3 position, int32_t info)
{
	// case 32 (0x716722): nothing without the town; then the same as CREATE_FISH_FARM with it (the farm's ctor takes the
	// nearest town anyway)
	if (FindTown(townId) == entt::null)
	{
		return;
	}
	FishFarmArchetype::Create(position, static_cast<uint32_t>(info));
}

void FeatureScriptCommands::CreateFeature(glm::vec3 position, FeatureInfo type, int32_t rotation, int32_t scale, int32_t)
{
	FeatureArchetype::Create(position, type, rotation * 0.001f, scale * 0.001f);
}

void FeatureScriptCommands::CreateFlowers([[maybe_unused]] glm::vec3 position, int32_t, float, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
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
	// case 38: nothing on top of another object (0x716B0C), then nothing for an amount <= 0 (0x716B19)
	if (!ecs::map_collide::IsOkToCreateAtPos(position, "CREATE_POT") || amount <= 0)
	{
		return;
	}
	// the handler's Pot::Create with its int 1 (0x716B39): the pot spreads its reaction at creation (food: the hungry
	// grazers come and eat)
	ecs::animal_ai::SetupPotReaction(PotArchetype::Create(position, 0.0f, type, amount));
}

void FeatureScriptCommands::CreateTownTemporaryPots(int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateMobileObject(glm::vec3 position, MobileObjectInfo type, int32_t rotation, int32_t scale)
{
	// case 40 (0x716C71): nothing on top of another object
	if (!ecs::map_collide::IsOkToCreateAtPos(position, "CREATE_MOBILEOBJECT"))
	{
		return;
	}
	MobileObjectArchetype::Create(position, type, rotation * 0.001f, scale * 0.001f);
}

void FeatureScriptCommands::CreateMobileStatic(glm::vec3 position, MobileStaticInfo type, float yRotation, float scale)
{
	// CREATE_MOBILESTATIC "ANFF", case 41 (0x716D46): fn_00608770(pos, info, 0, 0, F2, F3)
	MobileStaticArchetype::CreateFromInfo(position, type, 0.0f, yRotation, scale);
}

void FeatureScriptCommands::CreateMobileUStatic(glm::vec3 position, MobileStaticInfo type, float verticalOffset,
                                                float xRotation, float yRotation, float zRotation, float scale)
{
	// CREATE_MOBILE_STATIC "ANFFFFF", case 42 (0x716DC1): fn_00608840(pos with relY = F2, info, 0, 0, F3, F4, F5, F6)
	MobileStaticArchetype::CreateWithXYZAngles(position, type, verticalOffset, xRotation, yRotation, zRotation, scale);
}

void FeatureScriptCommands::CreateScaffold(int32_t, [[maybe_unused]] glm::vec3 position, int32_t, int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
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

void FeatureScriptCommands::CreateCreature(glm::vec3 position, int32_t param2, int32_t param3)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {}({}, {}, {}) not implemented.", __FILE__,
	                    __LINE__, __func__, glm::to_string(position), param2, param3);
}

void FeatureScriptCommands::CreateCreatureFromFile(const std::string& playerName, CreatureType creatureType,
                                                   const std::string& creatureMind, glm::vec3 position)
{
	auto playerType =
	    std::distance(k_PlayerNamesStrs.begin(), std::find(k_PlayerNamesStrs.begin(), k_PlayerNamesStrs.end(), playerName));
	auto yAngleRadians = glm::radians(180.0f);
	auto scale = .3f;
	auto& resources = Locator::resources::value();
	auto& creatureMindManager = resources.GetCreatureMinds();
	auto creatureMindPath = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / creatureMind;
	auto loadResult = creatureMindManager.Load(creatureMind, resources::CreatureMindLoader::FromDiskTag {}, creatureMindPath);
	auto creatureMindId = loadResult.first->first;
	CreatureArchetype::Create(position, static_cast<PlayerNames>(playerType), creatureType, creatureMindId, yAngleRadians,
	                          scale);
}

void FeatureScriptCommands::CreateFlock(int32_t flockId, glm::vec3 position, glm::vec3 domainCentre, int32_t domainRadius,
                                        int32_t param5, int32_t param6)
{
	// case 49 "NAANNN" (0x71634A): Flock::Flock 0x52F780(A1, the current player, id N0), SetDomainCentrePos(A2), domain
	// radius N3 (0 -> 0x50). From VERSION 2.1 on, the flock distance is N4 and the town N5; before, the town is N4 and
	// the distance stays 0x1E. The town gets it on its flock list.
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& flock = registry.Assign<Flock>(entity);
	flock.id = flockId;
	flock.savedDomainCentre = position;
	flock.domainCentre = domainCentre;
	flock.domainRadius = domainRadius != 0 ? static_cast<uint16_t>(domainRadius) : static_cast<uint16_t>(0x50);
	const bool newVersion = !(Game::Instance()->GetMapScriptGlobals().version < 2.1f);
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
}

void FeatureScriptCommands::LoadLandscape(const std::string& path)
{
	Game::Instance()->LoadLandscape(path);
}

void FeatureScriptCommands::Version(float version)
{
	// case 51 (0x716FF9): 0xD9957C
	Game::Instance()->GetMapScriptGlobals().version = version;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "LHScriptX: Land version set to: {}", version);
}

void FeatureScriptCommands::CreateArea([[maybe_unused]] glm::vec3 position, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
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
	BigForestArchetype::Create(position, type, unknown, rotation, scale);
}

void FeatureScriptCommands::CreateInfluenceRing(glm::vec3 position, int32_t player, float radius, int32_t anti)
{
	// case 59 (0x7171A3): InfluenceRing::Create(pos, GGame::GetPlayer(player), radius, anti). (inferido) the range
	// check is openblack's: GetPlayer does not test the index
	if (player >= 0 && player < static_cast<int32_t>(PlayerNames::_COUNT))
	{
		influence::CreateRing(position, static_cast<PlayerNames>(player), radius, anti != 0);
	}
}

void FeatureScriptCommands::CreateWeatherClimate(int32_t id, int32_t info, glm::vec3 position, float radius1,
                                                 float radius2)
{
	// case 60 (0x7171F5) -> fn_00771300(pos, &GClimateInfo[N1], F3, F4, 0, id N0): Magic/Script/MapScriptWeather.cpp ->
	// ECS/Weather/Climate (id 0: GClimate(0), which ignores the rest; otherwise GClimate 0x771170, radii in order)
	magic::map_script::CreateWeatherClimate(id, info, position, radius1, radius2);
}

void FeatureScriptCommands::CreateWeatherClimateRain(int32_t id, float desire, int32_t dryDays, int32_t rainingDays,
                                                     int32_t flags)
{
	// case 61 (0x717250) -> 0x773200: the climate's +0x34.. = {F1, N2, N3, (uint8_t)N4}; nothing for an unknown id
	magic::map_script::CreateWeatherClimateRain(id, desire, dryDays, rainingDays, flags);
}

void FeatureScriptCommands::CreateWeatherClimateTemp(int32_t id, float temperature, float target)
{
	// case 62 (0x7172A2) -> 0x773290: +0x44 = F1, +0x48 = F2
	magic::map_script::CreateWeatherClimateTemp(id, temperature, target);
}

void FeatureScriptCommands::CreateWeatherClimateWind(int32_t id, float windX, float windZ, float angle)
{
	// case 63 (0x7172E0) -> 0x7732D0: +0x4C.. = {F1, F2, F3}
	magic::map_script::CreateWeatherClimateWind(id, windX, windZ, angle);
}

void FeatureScriptCommands::CreateWeatherStorm(int32_t climate, glm::vec3 position, float age, int32_t numClouds,
                                               const std::string& shape, const std::string& clouds,
                                               const std::string& weather, float speed, glm::vec3 target)
{
	magic::map_script::CreateWeatherStorm(climate, position, age, numClouds, shape, clouds, weather, speed, target);
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

	// 0x717550: the point sits on the ground; appended at the tail (GStream::AddPoint 0x733B90)
	auto point = position;
	if (Locator::terrainSystem::has_value())
	{
		point.y = Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	}
	registry.Get<Stream>(registryContext.streams.at(streamId)).points.push_back(point);
}

void FeatureScriptCommands::CreateWaterfall([[maybe_unused]] glm::vec3 position)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateArena(glm::vec3 position, float radius)
{
	// case 69 (0x7175FA) -> fn_00424820: a GArena, not drawn until a creature fight is on
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<Arena>(registry.Create(), position, radius);
}

void FeatureScriptCommands::CreateFootpath(int32_t footpathId)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Footpath>(entity);
	auto& registryContext = registry.Context();
	registryContext.footpaths.insert({footpathId, entity});
}

void FeatureScriptCommands::CreateFootpathNode(int footpathId, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& registryContext = registry.Context();
	auto& footpath = registry.Get<Footpath>(registryContext.footpaths.at(footpathId));
	footpath.nodes.emplace_back(Footpath::Node {position});
}

void FeatureScriptCommands::LinkFootpath(int32_t footpathId)
{
	// TODO(#482): The last MultiMapFixed created in this script is an implicit param
	//             This Command adds the footpath to a list in a FootpathLink on the MultiMapFixed
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {}({}) not implemented.", __FILE__, __LINE__,
	                    __func__, footpathId);
}

void FeatureScriptCommands::CreateBonfire(glm::vec3 position, [[maybe_unused]] float temperature, float yAngle, float scale)
{
	// case 73 (0x7176AE): fn_00439850(pos, F1 temperature, F2 Y angle, F3 scale); the ctor 0x4395C0 ignores F1
	BonfireArchetype::Create(position, yAngle, scale);
}

void FeatureScriptCommands::CreateBase([[maybe_unused]] glm::vec3 position, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateNewFeature(glm::vec3 position, const std::string& type, int32_t rotation, int32_t scale,
                                             int32_t param5)
{
	// case 75 (0x716973): with N5 != 0 a PlannedFeature 0x527440, which is never drawn (PlannedMultiMapFixed::Draw
	// 0x648930 is a ret) and only matters to the town's building plans (not simulated yet): nothing is made here.
	// No land uses it.
	if (param5 != 0)
	{
		return;
	}
	const auto info = GFeatureInfo::Find(type);
	if (info == FeatureInfo::None)
	{
		return; // openblack: the original has no check (see GFeatureInfo::Find)
	}
	FeatureArchetype::Create(position, info, rotation * 0.001f, scale * 0.001f);
}

void FeatureScriptCommands::SetInteractDesire(float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::ToggleComputerPlayer(const std::string& affiliation, int32_t)
{
	PlayerArchetype::Create(GetPlayerName(affiliation));
}

void FeatureScriptCommands::SetComputerPlayerCreatureLike(const std::string&, const std::string&)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::MultiplayerDebug(int32_t, int32_t)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::CreateStreetLantern(glm::vec3 position, int32_t type)
{
	// case 80 (0x717720): GStreetLantern::Create 0x7346E0(pos, &GMobileStaticInfo[N1]); any info other than 7 (Land1: 59,
	// Country Lantern) is a country lantern with the campfire mesh, not a Bonfire
	StreetLanternArchetype::Create(position, static_cast<MobileStaticInfo>(type));
}

void FeatureScriptCommands::CreateStreetLight(glm::vec3 position)
{
	// TODO: case 81 (0x717763) makes a GStreetLight (0x734E60), not decoded yet; no land uses it. Drawn as a town lantern.
	StreetLanternArchetype::Create(position, MobileStaticInfo::StreetLantern);
}

void FeatureScriptCommands::SetLandNumber(int32_t number)
{
	// case 82 (0x7177A4): g_game+0x205A08 (read by the influence and the worship sites, and kept in the map globals)
	Game::Instance()->GetMapScriptGlobals().landNumber = number; // read by ECS/Influence and the worship sites
}

void FeatureScriptCommands::CreateOneShotSpell(glm::vec3 position, const std::string& seed)
{
	magic::script::CreateOneShotSpell(position, seed); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3 position, const std::string& magicName)
{
	magic::script::CreateOneShotSpellPu(position, magicName); // Magic/Script/MapScriptMagic.cpp
}

void FeatureScriptCommands::CreateFireFly([[maybe_unused]] glm::vec3 position)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::TownDesireBoost(int32_t townId, const std::string& desire, float boost)
{
	// command 86 of GSetup::MapCommands (0x7179EC): GGame::FindTownWithID 0x552FA0 and fn_747270 (the desire's name,
	// _stricmp); both found -> town +0xD4[d] = boost (0x717A26), without re-sorting nor a range check (Land2.txt:
	// "Abodes" / "Civic_Buildings" -0.75). ecs::town_desire::MapTownDesireBoost
	ecs::town_desire::MapTownDesireBoost(FindTown(townId), desire, boost);
}

void FeatureScriptCommands::CreateAnimatedStatic(glm::vec3 position, const std::string& type, int32_t rotation, int32_t scale)
{
	auto animatedStaticType = GAnimatedStaticInfo::Find(type);
	if (animatedStaticType == AnimatedStaticInfo::None)
	{
		return; // openblack: the original has no check (see GAnimatedStaticInfo::Find)
	}
	AnimatedStaticArchetype::Create(position, animatedStaticType, rotation * 0.001f, scale * 0.001f);
}

void FeatureScriptCommands::FireFlySpellRewardProb(const std::string& spell, float probability)
{
	magic::script::FireFlySpellRewardProb(spell, probability); // Magic/Script/MapScriptMagic.cpp
	// the same table kept in the map globals: case 88 (0x717998): GMagicInfo::GetInfoFromText 0x5FB3B0 (the first of
	// the 42 magic effect names equal without case, else 42) -> 0x52B630: out of range does nothing; else the table
	// entry and the running sums
	auto& globals = Game::Instance()->GetMapScriptGlobals();
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
	FieldArchetype::Create(townId, position, townFieldType, rotation);
}

void FeatureScriptCommands::CreateSpellDispenser(int32_t townId, glm::vec3 position, const std::string& abodeInfo,
                                                 const std::string& magicName, float yAngle, float scale, float period)
{
	// the dispenser Abode and its one-shot orb take their own creation indices
	magic::script::CreateSpellDispenser(townId, position, GAbodeInfo::Find(abodeInfo), magicName, yAngle, scale,
	                                    period); // Magic/Script/MapScriptMagic.cpp
	// it is a MultiMapFixed (its Abode's InsertMapObject, AbodeArchetype::Create): it blocks the trees made after it
}

void FeatureScriptCommands::LoadComputerPlayerPersonality(int32_t, glm::vec3)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::SetComputerPlayerPersonality(const std::string&, glm::vec3, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
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
	// case 95 (0x716616) -> 0x770BC0: an invisible waypoint
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<DrinkWaypoint>(registry.Create(), position);
}

void FeatureScriptCommands::SetTownInfluenceMultiplier(float multiplier)
{
	// case 96 (0x717B5C): g_game+0x250078
	Game::Instance()->GetMapScriptGlobals().townInfluenceMultiplier = multiplier; // read by ECS/Influence
}

void FeatureScriptCommands::SetPlayerInfluenceMultiplier(float multiplier)
{
	// case 97 (0x717B7B): g_game+0x25007C
	Game::Instance()->GetMapScriptGlobals().playerInfluenceMultiplier = multiplier; // read by ECS/Influence
}

void FeatureScriptCommands::SetTownBalanceBeliefScale(int32_t townId, float scale)
{
	// case 98: FindTownWithID (none: je 0x717E8A at 0x717BAE), then town +0x5DC = the scale (0x717BBA), the scale of
	// its pending belief (ecs::town_belief::Fold)
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
	// fn_00717171 -> GGameInfo::SetVisualTimeCycleFromMapEditor 0x557BB0
	Game::Instance()->GetDayNightClock().SetCycleFromMapEditor(duration, night, change);
}

void FeatureScriptCommands::MakeLastObjectArtifact(int32_t, const std::string&, float)
{
	// SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LHScriptX: {}:{}: Function {} not implemented.", __FILE__, __LINE__,
	// __func__);
}

void FeatureScriptCommands::SetLostTownScale(float scale)
{
	// case 104: [0xBF33F0] = the scale (0x717E85), 1 again with GLandBalance::Init (land_balance::Reset); read by the
	// town belief's fold (the boredom and the belief left in a player's towns when one is lost)
	land_balance::SetLostTownScale(scale);
}
