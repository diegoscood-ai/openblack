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

#include <entt/entity/fwd.hpp>

#include "ScriptHeaders/ScriptEnums.h"

/// What a thing is to the scripts (runblack.exe W120): its SCRIPT_OBJECT_TYPE (GameThing::GetScriptObjectType, vtable
/// +0x4E8) and its sub-type (fn_006F6D00, the index of its info record). The finders CALL 026, CALL_NEAR 051 and the
/// ones still to port (CALL_IN 055, CALL_IN_NEAR 067, CALL_NEAR_IN_STATE 316, the script containers' SET_SCRIPT_STATE)
/// filter with them. Only the reverse direction (CHL CREATE, CHLApi CreateScriptObject) existed before. Research:
/// dev\documentacion\intro\spec_misc_ops.md §026; the per-class values were read from every vtable's +0x4E8 slot.
///
/// openblack has no classes: the components stand in for them. A class openblack does not have (Reward, Dance, Ball,
/// LandscapeVortex, ScriptTimer, ScriptHighlight until edit_highlight.py adds ECS/ScriptHighlight.h, Scaffold,
/// GComputerPlayer, the ScriptMarker, which keeps only a Transform) gives ObjectType::None, as GameThingWithPos 0x570200
/// does for the classes without an override.
namespace openblack::ecs::script_type
{

/// The CHL "any sub-type" (0x1388, compared at 0x6F6FBA)
constexpr uint32_t k_AnySubtype = 5000;
/// fn_006F6D00's "no sub-type" (0x270F): the filter 0x6F6FA0 never accepts it, not even for an equal sub-type
constexpr uint32_t k_NoSubtype = 9999;

/// GetScriptObjectType (vt +0x4E8). Per class (W120): Abode 0x406810 = 2 (every abode class: Field, StoragePit,
/// TownCentre, Totem, Creche, Graveyard, Windmill, Workshop, Wonder, Football, PuzzleTotem), Feature 0x5276C0 = 3 (also
/// AnimatedStatic, the chess pieces, Flowers, WorshipSiteUpgrade), Villager 0x753020 = IsAChild ? 5 : 4, Animal 0x41B200
/// = 6, Dove 0x41EAA0 = 21 (the flying species and the Vulture), Reward 0x6E5CA0 = 7, MobileStatic 0x609330 = 8
/// (Bonfire, Fragment, MagicTeleport and GStreetLantern too), Rock 0x6E79E0 = 33, Town 0x73E200 = 9, Dance 0x50C3C0 =
/// 10, Flock 0x530490 = 11, Creature 0x47C8B0 = 12, DeadTree 0x5115B0 = 13 (FelledTree too), InfluenceRing 0x5CDC50 = 14, WeatherThing
/// 0x774360 = 15, Pot 0x66F530 = 16 (every pile), ScriptTimer 0x711600 = 17, CitadelHeart 0x4680B0 = 18, WorshipSite
/// 0x77D2E0 = 19, MobileObject 0x607B60 = 20 (Whale, OneOffSpellSeed, crops, creeds...), Tree 0x74C130 = 22 (MagicTree
/// too), LandscapeVortex 0x5FFFF0 = 23, SpellSeed 0x729C90 = +0x72 ? 30 : 24, Poo 0x6083C0 = 25, Ball 0x436100 = 28, Mist
/// 0x606910 = 29, SpellDispenser 0x722FB0 = 36, ScriptHighlight 0x70AE30 = 37, GComputerPlayer 0x6587B0 = 38, Scaffold
/// 0x6EAB60 = 39, TotemStatue 0x738EB0 = 40, ScriptMarker 0x70D960 = 1; every other class 0 (GameThingWithPos 0x570200:
/// the Citadel, PuzzleGame, BigForest, FishFarm, MapShield, the spell icons, GBaseOnly...)
[[nodiscard]] script::ObjectType TypeOf(entt::entity thing);

/// fn_006F6D00: by TypeOf (byte table 0x6F6F78, jump table 0x6F6F28, types 2..40), the index of the thing's info record
/// in its class's info array ((Object +0x28 - array) / record size); k_NoSubtype, with "Unknown type for search"
/// (0xC0D368), for a type without one (TOWN, DANCE, FLOCK, INFLUENCE_RING, WEATHER_THING, TIMER, CITADEL, BALL, MIST,
/// ONE_SHOT_SPELL_IN_HAND, TOTEM, COMPUTER_PLAYER, SCAFFOLD, below 2 or above 40), and with "Not implemented" (0xC0D380)
/// for DEAD_TREE
[[nodiscard]] uint32_t SubtypeOf(entt::entity thing);

/// The CALL / CALL_NEAR filter 0x6F6FA0 (thing, type, subtype): TypeOf == type, and subtype k_AnySubtype or
/// SubtypeOf == subtype (never when SubtypeOf is k_NoSubtype; SubtypeOf is not asked for k_AnySubtype)
[[nodiscard]] bool Matches(entt::entity thing, script::ObjectType type, uint32_t subtype);

} // namespace openblack::ecs::script_type
