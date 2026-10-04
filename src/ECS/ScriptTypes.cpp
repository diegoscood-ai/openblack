/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTypes.h"

#include <cstddef>

#include <spdlog/spdlog.h>

#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/Weather/WeatherThing.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using ScriptType = openblack::script::ObjectType;

namespace
{
/// GScript::ScriptErrorMessage 0x6F62B0 is a bare `ret` in W120: the original prints nothing. openblack keeps the
/// message as a diagnostic of its own (not original)
void ScriptError(const char* message)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}", message);
}

uint32_t Index(const void* record, const void* first, size_t size)
{
	return static_cast<uint32_t>((static_cast<const char*>(record) - static_cast<const char*>(first)) /
	                             static_cast<std::ptrdiff_t>(size));
}
} // namespace

ScriptType script_type::TypeOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return ScriptType::None;
	}
	// The containers and the buildings first: openblack's entities carry no second "class" component that would win
	if (registry.AllOf<Temple>(thing))
	{
		return ScriptType::Citadel; // CitadelHeart 0x4680B0 = 18 (openblack's Temple is the heart)
	}
	if (registry.AllOf<WorshipSite>(thing))
	{
		return ScriptType::WorshipSite; // 0x77D2E0 = 19
	}
	if (registry.AllOf<Town>(thing))
	{
		return ScriptType::Town; // 0x73E200 = 9
	}
	if (registry.AllOf<Creature>(thing))
	{
		return ScriptType::Creature; // 0x47C8B0 = 12
	}
	if (registry.AllOf<Villager>(thing))
	{
		// 0x753020: 4 + IsAChild (vt +0x458, Villager 0x55CB00: IsChild vt +0xAF8 == 1, flags +0xE0 bit 3)
		return villager::IsChild(thing) ? ScriptType::VillagerChild : ScriptType::Villager;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		// Dove 0x41EAA0 = 21 for the Dove classes (Bat, Crow, Dove, Pigeon, Seagull, SpellBat, SpellDove, Swallow,
		// Vulture: the vtable sweep), Animal 0x41B200 = 6 for the others. (inferred) the Dove classes are
		// animal_ai::IsFlyingSpecies plus the Vulture, which it leaves out (as CastRules.cpp names it). The class factory
		// fn_00419E00 has no case for the Vulture, CitadelDove and CitadelBat (0x419FCE), so none is ever made; the
		// class of the last two is not identified (pending: taken as Animal)
		const bool dove = animal_ai::IsFlyingSpecies(animal->type) || animal->type == AnimalInfo::Vulture;
		return dove ? ScriptType::Bird : ScriptType::Animal;
	}
	if (registry.AnyOf<Field, Abode>(thing))
	{
		return ScriptType::Abode; // Abode 0x406810 = 2, not overridden by Field (Field : Abode)
	}
	if (registry.AnyOf<AnimatedStatic, Feature>(thing))
	{
		return ScriptType::Feature; // Feature 0x5276C0 = 3; AnimatedStatic : Feature does not override it
	}
	if (registry.AllOf<TotemStatue>(thing))
	{
		return ScriptType::TotemStatue; // 0x738EB0 = 40
	}
	if (registry.AnyOf<DeadTree, FelledTree>(thing))
	{
		return ScriptType::DeadTree; // DeadTree 0x5115B0 = 13 (FelledTree : DeadTree)
	}
	if (registry.AnyOf<Fragment, MagicTeleport, StreetLantern>(thing))
	{
		// Fragment 0x76F7C0 = jmp MobileStatic's (over Rock's 33), MagicTeleport (no override, : MobileStatic),
		// GStreetLantern 0x734D40 = 8
		return ScriptType::MobileStatic;
	}
	if (const auto* statics = registry.TryGet<const MobileStatic>(thing))
	{
		// CREATE_MOBILE_STATIC 0x716DC1 -> fn_00608840: info 6 is a GBaseOnly (0x609340, no override: 0); fn_00608770:
		// info 8 a Bonfire (0x439A70 = jmp MobileStatic's 8), mobileType 2 a Rock (0x6E79E0 = 33), else a MobileStatic (8)
		if (statics->type == MobileStaticInfo::SingingStoneBase)
		{
			return ScriptType::None;
		}
		return Rocks::IsRock(thing) ? ScriptType::Rock : ScriptType::MobileStatic;
	}
	if (registry.AnyOf<Shark, OneOffSpellSeed>(thing))
	{
		return ScriptType::MobileObject; // Whale and OneOffSpellSeed keep MobileObject's 0x607B60 = 20
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
	{
		// fn_00607000 makes a Poo (0x6083C0 = 25) for GMobileObjectInfo 5 (audit_types.md); (inferred) every openblack
		// mobile object of that info came through it. Crops and creeds stay MobileObject's 20
		return mobile->type == MobileObjectInfo::LumpOfPoo ? ScriptType::Poo : ScriptType::MobileObject;
	}
	if (registry.AnyOf<Tree, MagicTree>(thing))
	{
		return ScriptType::Tree; // Tree 0x74C130 = 22 (MagicTree : Tree)
	}
	if (registry.AllOf<Pot>(thing))
	{
		return ScriptType::Store; // Pot 0x66F530 = 16 (PileFood, PileWood, MagicFood, MagicWood, PotStructure...)
	}
	if (const auto* seed = registry.TryGet<const SpellSeed>(thing))
	{
		// 0x729C90: neg byte +0x72; sbb; and 6; add 0x18
		return seed->fromOneShot ? ScriptType::OneShotSpell : ScriptType::SpellSeed;
	}
	if (registry.AllOf<SpellDispenser>(thing))
	{
		return ScriptType::SpellDispenser; // 0x722FB0 = 36
	}
	if (registry.AllOf<Mist>(thing))
	{
		return ScriptType::Mist; // 0x606910 = 29
	}
	if (registry.AllOf<InfluenceRing>(thing))
	{
		return ScriptType::InfluenceRing; // 0x5CDC50 = 14
	}
	if (registry.AllOf<Flock>(thing))
	{
		return ScriptType::Flock; // 0x530490 = 11
	}
	if (registry.AllOf<WeatherThing>(thing))
	{
		return ScriptType::WeatherThing; // 0x774360 = 15
	}
	if (registry.AllOf<ScriptHighlight>(thing))
	{
		return ScriptType::Highlight; // ScriptHighlight::GetScriptObjectType 0x70AE30 = 37 (ECS/ScriptHighlight.h)
	}
	// GameThingWithPos 0x570200: xor eax, eax. (pending) the ScriptMarker (1) has no component to tell it by
	return ScriptType::None;
}

uint32_t script_type::SubtypeOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	switch (TypeOf(thing)) // 0x6F6D09: (type - 2) unsigned above 0x26 -> "Unknown type for search"
	{
	case ScriptType::Abode: // case 0, 0x6F6DB1: (info - _AbodeInfos 0xC3C690) / 456
		// (approximate) openblack's Abode keeps only its AbodeNumber, not the info row: fire::traits::AbodeInfo finds the
		// row by abode number and mesh (AbodeInfoOf, FireObjectTraits.cpp:72) and falls back to the first row with that
		// number. (pending) a Field gets k_NoSubtype: AbodeInfo reads only Abode, the original gives the field's
		// GAbodeInfo index. No Land 1 intro CALL asks an ABODE sub-type other than 5000
		if (const auto* abode = fire::traits::AbodeInfo(thing); abode != nullptr && info != nullptr)
		{
			return Index(abode, info->abode.data(), sizeof(*abode));
		}
		return k_NoSubtype;
	case ScriptType::Feature: // case 1, 0x6F6DC4: (info - 0xCC99A0, GFeatureInfo) / 292
		if (const auto* feature = registry.TryGet<const Feature>(thing))
		{
			return static_cast<uint32_t>(feature->type);
		}
		// (approximate) an AnimatedStatic: the original divides its GAnimatedStaticInfo address by the GFeatureInfo
		// array's base and size, a number no script asks for; openblack gives no sub-type
		return k_NoSubtype;
	case ScriptType::Villager: // case 2, 0x6F6D2A: (info - 0xDA6BE8, GVillagerInfo) / 932
	case ScriptType::VillagerChild:
		// VillagerInfoOf: null for a villager without an info row (villager::InfoOf would give row 0, not original)
		if (const auto* villagerInfo = VillagerInfoOf(thing); villagerInfo != nullptr && info != nullptr)
		{
			return Index(villagerInfo, info->villager.data(), sizeof(*villagerInfo));
		}
		return k_NoSubtype;
	case ScriptType::Animal: // case 3, 0x6F6D4A: (info - 0xC4D030, GAnimalInfo) / 716
	case ScriptType::Bird:
		return static_cast<uint32_t>(registry.Get<const Animal>(thing).type);
	case ScriptType::MobileStatic: // case 5, 0x6F6E02: (info - 0xD3A6D8, GMobileStaticInfo) / 300
	case ScriptType::Rock:
		if (const auto* statics = registry.TryGet<const MobileStatic>(thing))
		{
			return static_cast<uint32_t>(statics->type);
		}
		if (const auto* lantern = registry.TryGet<const StreetLantern>(thing))
		{
			// GStreetLantern::Create 0x7346E0(pos, &MSInfo[N]): 59 country lantern, 7 street lantern. (approximate)
			// openblack keeps only GStreetLantern +0x58 = (info != &MSInfo[7]), so any lantern that is not a 7 reads as
			// 59: exact for Land 1, where only 7 and 59 are made
			const auto row = lantern->country ? MobileStaticInfo::CountryLantern : MobileStaticInfo::StreetLantern;
			return static_cast<uint32_t>(row);
		}
		if (registry.AllOf<MagicTeleport>(thing))
		{
			return static_cast<uint32_t>(MobileStaticInfo::Teleport); // MagicTeleport 0x5FC130: &MSInfo 0xD3B614 = 13
		}
		return static_cast<uint32_t>(MobileStaticInfo::Rock); // Fragment 0x76E9D0: &MSInfo 0xD3A930 = 2
	case ScriptType::Creature: // case 6, 0x6F6D6A: info +0x1F4
		// (pending, creature) GCreatureInfo +0x1F4 is the first dword of InfoConstants' opaque field0x1e4 and openblack
		// keeps no creature info row; (inferred) it is the CREATURE_TYPE openblack stores as the species
		return static_cast<uint32_t>(registry.Get<const Creature>(thing).species);
	case ScriptType::DeadTree: // case 7, 0x6F6E46
		ScriptError("Not implemented");
		return k_NoSubtype;
	case ScriptType::Store: // case 8, 0x6F6D75: (info - 0xD4C660, GPotInfo) / 324
		return static_cast<uint32_t>(registry.Get<const Pot>(thing).type);
	case ScriptType::WorshipSite: // case 9, 0x6F6EB1: (+0x8C - 0xDA57A8) / 28
		// (inferred) +0x8C is the site's tribe info (WorshipSite.h): the 28-byte records of 0xDA57A8 by tribe
		return static_cast<uint32_t>(registry.Get<const WorshipSite>(thing).tribe);
	case ScriptType::MobileObject: // case 10, 0x6F6E20: (info - 0xD38448, GMobileObjectInfo) / 276
	case ScriptType::Poo:
	case ScriptType::Whale:
	case ScriptType::Ark:
		if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
		{
			return static_cast<uint32_t>(mobile->type);
		}
		if (registry.AllOf<Shark>(thing))
		{
			return static_cast<uint32_t>(MobileObjectInfo::Whale); // Whale::Create 0x774C50: &GMobileObjectInfo[24]
		}
		return static_cast<uint32_t>(MobileObjectInfo::OneOffSpellSeed); // ctor 0x72A3A0: 0xD39F3C = row 25
	case ScriptType::Tree: // case 11, 0x6F6D93: (info - 0xDA3AD8, GTreeInfo) / 320
		if (const auto* tree = registry.TryGet<const Tree>(thing))
		{
			return static_cast<uint32_t>(tree->type);
		}
		return k_NoSubtype; // (pending) a MagicTree without a Tree component: openblack keeps no info row for it
	case ScriptType::Highlight: // case 17, 0x6F6ED4: (info - 0xD96390, GScriptHighlightInfo) / 272
		return script_highlight::InfoIndexOf(thing);
	case ScriptType::TotemStatue: // case 18, 0x6F6EF2: (info - 0xDA1D18) / 292
		return 0; // (inferred) the first GTotemStatueInfo, as map_cells::TypeOf: openblack's statue keeps no row
	case ScriptType::SpellSeed: // case 13, 0x6F6E6D: (+0x28 - 0xD9D678, GSpellSeedInfo) / 400
		// both ctors store the same info at +0x28 (Object ctor) and +0x58 (0x7280A0: ebx at 0x7280AB and 0x7280F0;
		// 0x727FF0: the icon's +0x80 at 0x728003 and 0x728052): openblack's seedType, the GSpellSeedInfo row
		return static_cast<uint32_t>(registry.Get<const SpellSeed>(thing).seedType);
	case ScriptType::Reward:         // case 4, 0x6F6DE4: (info - 0xD50BF8) / 304
	case ScriptType::Vortex:         // case 12, 0x6F6E3E: +0xE0
	case ScriptType::OneShotSpell:   // case 14, 0x6F6E5A: CastOneOffSpellSeed (vt +0xAC) +0x68, else as case 13
	case ScriptType::PuzzleGame:     // case 15, 0x6F6E8B: +0x48 (a PuzzleGame's TypeOf is 0: never reached)
	case ScriptType::Field:          // case 16, 0x6F6E90: (+0x120 - 0xCCF070) / 340 (a Field's TypeOf is 2)
	case ScriptType::SpellDispenser: // case 0 too: its info against _AbodeInfos
		// (pending) openblack keeps none of these info records on the entity (or has no such thing yet)
		return k_NoSubtype;
	default: // byte 19 of the table, 0x6F6F12
		ScriptError("Unknown type for search");
		return k_NoSubtype;
	}
}

bool script_type::Matches(entt::entity thing, ScriptType type, uint32_t subtype)
{
	if (TypeOf(thing) != type) // 0x6F6FB0
	{
		return false;
	}
	if (subtype == k_AnySubtype) // 0x6F6FBA
	{
		return true;
	}
	const uint32_t own = SubtypeOf(thing);
	return own != k_NoSubtype && own == subtype; // 0x6F6FCB..0x6F6FD6
}
