/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDisciple.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "3D/MapCoords.h"
#include "Audio/Services/Guidance.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureMimic.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/VillagerDiscipleJobsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMissionary.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerAnimations.h"
#include "Game/GameStats.h"
#include "InfoConstants.h"
#include "Locator.h"

// The disciples (VillagerDisciple.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// The disciple table, as the original has it (13 records): {start state, reaction 0x18, marker, held at its job,
/// fetches wood, town desire, moves into town}
constexpr std::array<DiscipleInfo, k_DiscipleInfoCount> k_DiscipleInfos = {{
    {0, 0, 0, 0, 0, -1, 0},    // 0 NONE
    {0, 1, 1, 1, 0, 0, 1},     // 1 FARMER
    {0x31, 1, 1, 1, 1, 1, 0},  // 2 FORESTER: 49 FORESTER_ARRIVES_AT_FOREST
    {0, 1, 1, 1, 0, 0, 0},     // 3 FISHERMAN
    {0, 1, 1, 1, 1, 9, 1},     // 4 BUILDER
    {0xFE, 0, 1, 1, 0, 8, 1},  // 5 BREEDER: 254 BREEDER_JUST_LANDED
    {0, 1, 1, 1, 0, -1, 0},    // 6 PROTECTION
    {0xE2, 0, 1, 0, 0, -1, 0}, // 7 MISSIONARY: 226 MISSIONARY_DISCIPLE
    {0, 1, 1, 1, 1, -1, 1},    // 8 CRAFTSMAN
    {0xA3, 0, 1, 1, 0, -1, 0}, // 9 TRADER: 163 DECIDE_WHAT_TO_DO
    {0xA3, 0, 1, 0, 0, -1, 0}, // 10 CHANGE_HOUSE: 163
    {0x3A, 0, 0, 0, 0, -1, 0}, // 11 WORSHIP: 58 GOTO_WORSHIP_SITE_FOR_WORSHIP
    {0xF4, 0, 0, 0, 0, -1, 0}, // 12 FROM_VORTEX: 244 SCRIPT_IN_CROWD
}};

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// The villager's town entity (entt::null without one)
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

/// The town centre (Town::centre); entt::null without one
entt::entity CentreOf(entt::entity town)
{
	const auto& t = Entities().Get<const Town>(town);
	// (guard) the original tests the pointer only: an id openblack kept after the centre went is none
	return t.centre != entt::null && Entities().Valid(t.centre) ? t.centre : entt::null;
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// The job call of a case: the disciple jobs service runs the type's check
uint32_t Job(entt::entity villager, uint8_t disciple)
{
	return Locator::villagerDiscipleJobs::value().DiscipleJob(villager, disciple);
}

/// DiscipleDecideWhatToDo's fallback
uint32_t Fallback(entt::entity villager)
{
	// Already in 221 DISCIPLE_NOTHING_TO_DO -> 0
	if (GetFinalState(villager) == VillagerStates::DiscipleNothingToDo)
	{
		return 0;
	}
	// Only a disciple held at its job
	const auto* v = VillagerOf(villager);
	if (v == nullptr || (v->flags & Villager::k_FlagDisciple) == 0 || !DiscipleHeldAtJob(v->discipleType))
	{
		return 0;
	}
	// SetDiscipleNothingToDo found a point -> 1
	return SetDiscipleNothingToDo(villager) != 0 ? 1 : 0;
}

// ---- the hand's disciples' helpers

/// A walk up to the thing it inspects stops this far from its edge, in metres
constexpr float k_InspectReach = disciple::k_InspectReach;

/// The player of the hand: the local player's (the hand that picks villagers up is always the local player's)
std::optional<PlayerNames> HandPlayer()
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::playerSystem::value().GetLocalPlayer();
}

/// The town's owner; none without a town
std::optional<PlayerNames> OwnerOf(entt::entity town)
{
	if (town == entt::null || !Entities().Valid(town) || !Entities().AllOf<Town>(town))
	{
		return std::nullopt;
	}
	return Entities().Get<const Town>(town).owner;
}

/// How much the kind of a thing draws villagers (its info's villagerInteractDesire); none for a kind that is not
/// looked at
std::optional<float> KindDesire(entt::entity thing)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Entities();
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<FishFarm>(thing))
	{
		return info.fishFarm.villagerInteractDesire;
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		const auto row = static_cast<size_t>(site->infoIndex);
		return row < info.worshipSite.size() ? std::optional(info.worshipSite.at(row).villagerInteractDesire) : std::nullopt;
	}
	if (!registry.AnyOf<Abode, Field, Tree, BigForest, Villager>(thing))
	{
		return std::nullopt;
	}
	const auto* row = fire::traits::InfoOf(thing);
	return row != nullptr ? std::optional(row->villagerInteractDesire) : std::nullopt;
}

/// Room in a town for another villager: room for adults left in its abodes, or, with no places at all, fewer people
/// than an empty town is allowed
bool TownHasRoom(entt::entity town)
{
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	int64_t places = 0;
	int64_t room = 0;
	for (const auto abode : town_stats::AbodesOf(town))
	{
		places += abode_villagers::MaxVillagers(abode);
		room += abode_villagers::GetRoomLeftForAdults(abode);
	}
	if (places == 0)
	{
		int64_t people = 0;
		Entities().Each<const Villager>([&people, town](const Villager& villager) {
			if (villager.town == town)
			{
				++people;
			}
		});
		return static_cast<int64_t>(Locator::infoConstants::value().town.emptyTownAllowedThisManyVillagers) - people > 0;
	}
	return room > 0;
}

/// What the choice reads of a thing round the held villager
disciple::NearFacts NearFactsOf(entt::entity thing, entt::entity held)
{
	const auto& registry = Entities();
	disciple::NearFacts facts;
	const auto heldTown = TownEntityOf(held);
	const auto town = tq::GetTown(thing);
	facts.hasTown = town != entt::null;
	facts.townHabitable = town == entt::null || !registry.Get<const Town>(town).uninhabitable;
	facts.heldVillagersTown = town == heldTown;
	facts.owner = OwnerOf(town);
	// A field is an abode of its town too, but farmed rather than lived in
	if (registry.AllOf<Field>(thing))
	{
		facts.kind = disciple::NearKind::Field;
		facts.built = abodes::IsBuilt(thing);
		return facts;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		facts.built = abodes::IsBuilt(thing);
		facts.repaired = abodes::IsRepaired(thing);
		facts.functional = abode_queries::IsFunctional(thing);
		switch (abode->type)
		{
		case AbodeNumber::StoragePit:
			facts.kind = disciple::NearKind::StoragePit;
			break;
		case AbodeNumber::TownCentre:
			facts.kind = disciple::NearKind::TownCentre;
			facts.townHasRoom = town != entt::null && TownHasRoom(town);
			break;
		case AbodeNumber::Workshop:
			facts.kind = disciple::NearKind::Workshop;
			break;
		default:
		{
			const bool house = abode_villagers::MaxVillagers(thing) > 0;
			facts.kind = house ? disciple::NearKind::House : disciple::NearKind::Building;
			facts.roomForAdults = house && abode_villagers::GetPercentAbodeFullWithAdults(thing) < 1.0f;
			const auto* person = registry.TryGet<const Villager>(held);
			facts.heldVillagersHome = person != nullptr && person->abode == thing;
			break;
		}
		}
		return facts;
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		// The citadel's player owns it; the towns that worship there are its towns, the newest first
		facts.kind = disciple::NearKind::WorshipSite;
		facts.owner = site->player;
		facts.hasTown = !site->towns.empty();
		const auto newest = facts.hasTown ? site->towns.front() : entt::null;
		facts.townHabitable = !OwnerOf(newest).has_value() || !registry.Get<const Town>(newest).uninhabitable;
		facts.built = abodes::IsBuilt(thing);
		facts.repaired = abodes::IsRepaired(thing);
		facts.heldVillagersWorshipSite =
		    heldTown != entt::null && std::ranges::find(site->towns, heldTown) != site->towns.end();
		return facts;
	}
	if (registry.AllOf<FishFarm>(thing))
	{
		facts.kind = disciple::NearKind::FishFarm;
		return facts;
	}
	if (registry.AllOf<BigForest>(thing))
	{
		facts.kind = disciple::NearKind::Forest;
		return facts;
	}
	if (registry.AllOf<Tree>(thing))
	{
		facts.kind = disciple::NearKind::Tree;
		return facts;
	}
	if (registry.AllOf<Villager>(thing))
	{
		facts.kind = disciple::NearKind::Villager;
		facts.villagerFunctional = !IsDead(thing);
		facts.villagerSexuallyActive = IsSexuallyActive(thing);
		facts.villagerMale = InfoOf(thing).sex != SexType::Female;
	}
	return facts;
}

/// What the choice reads of the held villager and the hand
disciple::HeldFacts HeldFactsOf(entt::entity villager)
{
	return {
	    .hand = HandPlayer(),
	    .villager = GetPlayerOf(villager),
	    .sexuallyActive = IsSexuallyActive(villager),
	    .male = InfoOf(villager).sex != SexType::Female,
	    .child = IsChild(villager),
	    .otherwiseRefused = Entities().AllOf<ScriptControlled>(villager) || IsDead(villager),
	};
}

/// Walks up to the thing, unless already within k_InspectReach of its edge, then INSPECT_OBJECT
void SetupInspectObject(entt::entity villager, entt::entity thing)
{
	const auto there = tq::PosOf(thing);
	const auto here = tq::PosOf(villager);
	const float radius = tq::Get2DRadius(thing);
	if (tq::GetDistanceInMetres(here, there) - radius > k_InspectReach)
	{
		const auto goal = there + tq::GetPosFromAngle(tq::Get3DAngleFromXZ(there, here), radius + k_InspectReach);
		SetupMoveToWithHug(villager, tq::ToMetres(goal), VillagerStates::InspectObject);
		return;
	}
	SetTopState(villager, VillagerStates::InspectObject);
}

/// The start states of the disciple table that openblack's state table has: the others are not ported yet, and their
/// disciples decide afresh
bool StartStatePorted(VillagerStates state)
{
	switch (state)
	{
	case VillagerStates::DecideWhatToDo:
	case VillagerStates::ForesterArrivesAtForest:
	case VillagerStates::GotoWorshipSiteForWorship:
	case VillagerStates::MissionaryDisciple:
	case VillagerStates::BreederJustLanded:
		return true;
	default:
		return false;
	}
}
} // namespace

// ---- the disciple table ------------------------------------------------------------------------------------------

const DiscipleInfo& GetDiscipleInfo(uint8_t disciple)
{
	return k_DiscipleInfos.at(disciple < k_DiscipleInfos.size() ? disciple : 0);
}

VillagerStates DiscipleStartState(uint8_t disciple)
{
	return static_cast<VillagerStates>(static_cast<uint8_t>(GetDiscipleInfo(disciple).startState));
}

bool DiscipleCreatesJobReaction(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).createsJobReaction != 0;
}

uint32_t DiscipleMarker(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).marker;
}

bool DiscipleHeldAtJob(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).heldAtJob == 1;
}

bool DiscipleFetchesWood(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).fetchesWood != 0;
}

int32_t DiscipleTownDesire(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).townDesire;
}

bool DiscipleMovesIntoTown(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).movesIntoTown != 0;
}

// ---- the disciple ------------------------------------------------------------------------------------------------

uint32_t SetVillagerDisciple(entt::entity villager, [[maybe_unused]] entt::entity thing, VillagerDisciple disciple,
                             [[maybe_unused]] int32_t h)
{
	// Outside 0..12 -> 0, nothing done
	const auto d = static_cast<int32_t>(disciple);
	if (d < 0 || d >= static_cast<int32_t>(k_DiscipleInfoCount))
	{
		return 0;
	}
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0; // (guard) the original's villager is always one
	}
	const auto type = static_cast<uint8_t>(d);
	// With a town and a new disciple type the town's disciple counts move from the old type to the new (0 skipped),
	// bytes. The town's turn counts them afresh too (town_stats::Compute), so the moves only show between two of its
	// turns
	if (const auto town = TownEntityOf(villager); town != entt::null && type != v->discipleType)
	{
		auto& counts = Entities().Get<Town>(town).stats.disciples;
		if (v->discipleType != 0 && v->discipleType < counts.size())
		{
			counts.at(v->discipleType) = static_cast<uint8_t>(counts.at(v->discipleType) - 1);
		}
		if (type != 0)
		{
			counts.at(type) = static_cast<uint8_t>(counts.at(type) + 1);
		}
	}
	villager::TraceFormatted(villager, "disciple: {} -> {}", static_cast<uint32_t>(v->discipleType),
	                         static_cast<uint32_t>(type));
	if (type != 0)
	{
		// A disciple, no longer a follower
		v->flags = static_cast<uint16_t>((v->flags & 0xFBFF) | Villager::k_FlagDisciple);
		// The 3D object is given the type's marker. (pending) what that shows (a disciple marker, inferred): nothing is
		// drawn
		v->discipleType = type;
		return 1;
	}
	// Neither a disciple nor a follower; the marker cleared (pending, as above)
	v->flags = static_cast<uint16_t>(v->flags & 0xF9FF);
	v->discipleType = 0;
	return 1;
}

uint32_t DiscipleDecideWhatToDo(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// One case per disciple type 1..12; any other -> the fallback
	const uint8_t disciple = v->discipleType;
	switch (disciple)
	{
	case 1:
	{
		// FARMER: the town's best field (the score is not read); no town or no field -> the fallback
		const auto town = TownEntityOf(villager);
		if (town == entt::null)
		{
			break;
		}
		float score = 0.0f;
		const auto field = FindBestField(town, villager, score);
		if (field == entt::null)
		{
			break;
		}
		// A field to harvest and more food carried than minFoodToShowGraphic (signed 16 bits) -> 31
		// GOTO_STORAGE_PIT_FOR_DROP_OFF; 1
		const auto food = VillagerOf(villager)->resourceHeld.at(0);
		if (FieldActivityOf(field) == 2 && food > static_cast<int16_t>(InfoOf(villager).minFoodToShowGraphic))
		{
			villager::TraceFormatted(villager, "disciple 1: harvest field, food {} -> 31", food);
			SetTopState(villager, VillagerStates::GotoStoragePitForDropOff);
			return 1;
		}
		// Becomes the field's farmer -> 1
		if (VillagerBecomesFarmer(villager, field) == 1)
		{
			TraceIf(villager, "disciple 1: farmer");
			return 1;
		}
		break;
	}
	case 2:
		// FORESTER: the wood desire check == 1 -> 1
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 3:
		// FISHERMAN: looking for water == 1 -> 1
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 4:
		// BUILDER: needed for a building == 1 -> 1
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 5:
		// BREEDER: SetupBreederDisciple (VillagerBreeding.cpp) == 1 -> 1; else the fallback, as the original takes it
		// for any other result
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 6:
		// PROTECTION: the fallback itself
		break;
	case 7:
		// MISSIONARY: 0 at once, not even the fallback
		return 0;
	case 8:
		// CRAFTSMAN: the workshop supply check != 0 -> 1; else the fallback
		if (Job(villager, disciple) != 0)
		{
			return 1;
		}
		break;
	case 9:
		// TRADER: CheckTrader == 1 -> 1; else the fallback, as the original takes it for any other result
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 10:
	{
		// CHANGE_HOUSE: no longer a disciple, whatever follows
		SetVillagerDisciple(villager, villager, VillagerDisciple::None, 0);
		// Moving into the closest abode of any town and tribe (or elsewhere in its town when it is full) -> 234
		// GO_HOME_AND_CHANGE; 1. The original then counts the move for the new player's win conditions, which only a
		// multiplayer game keeps. Else the fallback, which then answers 0 (the disciple flag was just cleared)
		const auto abode = abode_queries::FindClosestAbode(object::MapCoordsOf(villager), std::nullopt, false, false);
		if (CheckMoveHouse(villager, abode) == 1)
		{
			TraceIf(villager, "disciple 10: moved house, 234 GO_HOME_AND_CHANGE");
			SetTopState(villager, VillagerStates::GoHomeAndChange);
			return 1;
		}
		break;
	}
	case 11:
		// WORSHIP: 58 GOTO_WORSHIP_SITE_FOR_WORSHIP; 1
		SetTopState(villager, VillagerStates::GotoWorshipSiteForWorship);
		return 1;
	case 12:
		// FROM_VORTEX: the table's start state 244 SCRIPT_IN_CROWD, then no longer a disciple; 1
		SetTopState(villager, DiscipleStartState(disciple));
		SetVillagerDisciple(villager, entt::null, VillagerDisciple::None, 0);
		return 1;
	default:
		// 0 and 13..255: the fallback
		break;
	}
	return Fallback(villager);
}

// ---- 221 DISCIPLE_NOTHING_TO_DO ----------------------------------------------------------------------------------

uint32_t SetDiscipleNothingToDo(entt::entity villager)
{
	// No prayer point -> 0
	glm::ivec2 p(0);
	if (FindDisciplePrayerPos(villager, p) == 0)
	{
		return 0;
	}
	// Reset the countdown, then walk to the point into 221 (the walk's result unused); 1
	if (auto* action = Entities().TryGet<LivingAction>(villager))
	{
		action->turnsUntilStateChange = 0;
	}
	const auto m = tq::ToMetres(p);
	villager::TraceFormatted(villager, "disciple: nothing to do -> 221 ({:.1f}, {:.1f})", m.x, m.y);
	SetupMoveToWithHug(villager, m, VillagerStates::DiscipleNothingToDo);
	return 1;
}

uint32_t FindDisciplePrayerPos(entt::entity villager, glm::ivec2& out)
{
	// No town -> 0, out untouched
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// No town centre -> out = the town's position; 1
	const auto centre = CentreOf(town);
	if (centre == entt::null)
	{
		out = tq::PosOf(town);
		return 1;
	}
	// The angle from the centre towards me
	const auto at = tq::PosOf(centre);
	const float toMe = tq::Get3DAngleFromXZ(at, tq::PosOf(villager));
	// Within +-45 degrees of my side. The original runs the FPU at float precision, so float arithmetic matches it
	// exactly
	const float angle = GameFloatRand(glm::half_pi<float>()) - glm::quarter_pi<float>() + toMe;
	// Up to 4 beyond the centre's 2D radius
	const float spread = GameFloatRand(4.0f);
	const float distance = tq::Get2DRadius(centre) + spread;
	// out = the centre + that offset; 1
	out = at + tq::GetPosFromAngle(angle, distance);
	return 1;
}

uint32_t DiscipleNothingToDo(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// The town's build pulse (a building or resource pulse) -> a new random countdown. (pending) the original reads
	// the town without a null test; (guard) no town: no pulse
	const auto town = TownEntityOf(villager);
	if (town != entt::null && Entities().Get<const Town>(town).buildPulse != 0)
	{
		action.turnsUntilStateChange = static_cast<uint16_t>(GameRand(10));
	}
	// Count down; still > 0 (signed 16 bits) -> 1
	action.turnsUntilStateChange = static_cast<uint16_t>(static_cast<int16_t>(action.turnsUntilStateChange) - 1);
	if (static_cast<int16_t>(action.turnsUntilStateChange) > 0)
	{
		return 1;
	}
	// The clip not done -> wait one more turn; 1
	if (!ecs::VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		action.turnsUntilStateChange = 1;
		return 1;
	}
	// Nothing found to do -> wait 300 turns; 1
	if (DiscipleDecideWhatToDo(villager) == 0)
	{
		action.turnsUntilStateChange = 300;
	}
	return 1;
}

uint32_t EnterDiscipleNothingToDo(LivingAction& action, [[maybe_unused]] VillagerStates final,
                                  [[maybe_unused]] VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// A town with a centre -> look at the centre; 1
	if (const auto town = TownEntityOf(villager); town != entt::null)
	{
		if (const auto centre = CentreOf(town); centre != entt::null)
		{
			LookAtPos(villager, tq::PosOf(centre), 2);
		}
	}
	return 1;
}

// ---- the hand's disciples ----------------------------------------------------------------------------------------

uint32_t EnterInHand(LivingAction& action, [[maybe_unused]] VillagerStates final, [[maybe_unused]] VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	if (auto* v = VillagerOf(villager); v != nullptr)
	{
		v->discipleOnPickUp = static_cast<uint8_t>(disciple::AtPickUp(static_cast<VillagerDisciple>(v->discipleType)));
		// kept in the same storage as the x of the place a script gave it for its script states
		villager_script::SetScriptPlaceX(villager, v->discipleOnPickUp);
	}
	return 1;
}

std::vector<entt::entity> DiscipleThingsNear(entt::entity villager)
{
	std::vector<entt::entity> found;
	std::vector<float> interests;
	if (!Locator::infoConstants::has_value())
	{
		return found;
	}
	const auto& registry = Entities();
	const auto coords = object::MapCoordsOf(villager);
	const float ownRadius = tq::Get2DRadius(villager);
	// Its own cell, then the spiral's next eight: the 3 x 3 cells round it
	auto walk = coords;
	map_coords::Spiral spiral;
	for (size_t cells = 0; cells < disciple::k_CellsSearched; ++cells)
	{
		if (map_coords::InBounds(walk))
		{
			for (const auto thing : map_cells::ObjectsInCell(map_coords::Cell(walk)))
			{
				if (thing == villager || !ecs::IsAvailable(thing) || std::ranges::find(found, thing) != found.end())
				{
					continue;
				}
				// Only the villagers about their lives count
				if (registry.AllOf<Villager>(thing) && IsDead(thing))
				{
					continue;
				}
				const auto desire = KindDesire(thing);
				if (!desire.has_value())
				{
					continue;
				}
				const float gap =
				    gutils::GetDistanceInMetres(coords, object::MapCoordsOf(thing)) - (ownRadius + tq::Get2DRadius(thing));
				found.push_back(thing);
				interests.push_back(disciple::Interest(gap, *desire));
			}
		}
		map_coords::AddCells(walk, spiral.Next());
	}
	std::vector<entt::entity> ordered;
	for (const auto index : disciple::OrderByInterest(interests))
	{
		ordered.push_back(found.at(index));
	}
	return ordered;
}

void DiscipleInHand(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	// Only in the player's hand
	if (v == nullptr || (v->flags & Villager::k_FlagInHand) == 0)
	{
		return;
	}
	const auto things = DiscipleThingsNear(villager);
	std::vector<disciple::NearFacts> facts;
	facts.reserve(things.size());
	for (const auto thing : things)
	{
		facts.push_back(NearFactsOf(thing, villager));
	}
	const auto choice =
	    disciple::ChooseWhileHeld(facts, HeldFactsOf(villager), static_cast<VillagerDisciple>(v->discipleOnPickUp));
	const auto thing = choice.from.has_value() ? things.at(*choice.from) : entt::null;
	if (choice.type == VillagerDisciple::Trader && thing != entt::null)
	{
		v->tradeTown = tq::GetTown(thing);
	}
	SetVillagerDisciple(villager, thing, choice.type, 0);
}

void DiscipleThrownFromHand(entt::entity villager, bool dontReplant)
{
	if (!dontReplant || VillagerOf(villager) == nullptr)
	{
		return;
	}
	const auto player = GetPlayerOf(villager);
	if (!player.has_value() || player == HandPlayer())
	{
		SetVillagerDisciple(villager, entt::null, VillagerDisciple::None, 0);
	}
}

void DiscipleFlownFromHand(entt::entity villager)
{
	if (const auto* v = VillagerOf(villager); v != nullptr)
	{
		SetVillagerDisciple(villager, entt::null, static_cast<VillagerDisciple>(v->discipleOnPickUp), 0);
	}
}

void DisciplePutDownByHand(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	const auto local = HandPlayer();
	if (v == nullptr || !local.has_value() || v->discipleOnPickUp == v->discipleType)
	{
		return;
	}
	audio::guidance::PlayDiscipleRemark(v->discipleType, effects::alignment::Get(*local));
}

void ShowDiscipleToCreature(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	const auto player = HandPlayer();
	if (v == nullptr || !player.has_value())
	{
		return;
	}
	if (const auto deed = creature_watching::DiscipleDeed(static_cast<VillagerDisciple>(v->discipleType)); deed.has_value())
	{
		creature_mimic::Consider(*player, *deed, villager);
	}
}

void DiscipleTouchDown(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	const auto player = HandPlayer();
	if (v == nullptr || !player.has_value() || v->discipleType == 0 || v->discipleType == v->discipleOnPickUp)
	{
		return;
	}
	const auto town = TownEntityOf(villager);
	if (town == entt::null || !Locator::infoConstants::has_value())
	{
		return;
	}
	// The town's want of the work the disciple does, none for a disciple serving no desire
	std::optional<float> desire;
	if (const auto served = DiscipleTownDesire(v->discipleType); served >= 0 && Locator::townDesireSystem::has_value())
	{
		desire = Locator::townDesireSystem::value().GetDesire(town, static_cast<TownDesireInfo>(served));
	}
	const auto& info = Locator::infoConstants::value().town;
	const float change = disciple::AlignmentForMaking(desire, info.consideredEvilIfMakingDisciplesAndDesireIsLessThan,
	                                                  info.consideredGoodIfMakingDisciplesAndDesireIsGreaterThan,
	                                                  info.alignmentChangePerDisciple);
	effects::alignment::UpdateForDisciple(*player, change);
}

uint32_t DiscipleLanded(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto things = DiscipleThingsNear(villager);
	if (things.empty())
	{
		return 0;
	}
	const uint8_t type = v->discipleType;
	if (type != v->discipleOnPickUp)
	{
		if (const auto owner = GetPlayerOf(villager); owner.has_value())
		{
			game_stats::DiscipleMade(static_cast<size_t>(*owner), type);
		}
	}
	// A kind that moves into the town of the thing it was put by, when that is another town: it moves in, and takes on
	// that town's tribe
	if (DiscipleMovesIntoTown(type))
	{
		const auto there = tq::GetTown(things.front());
		if (there != entt::null && there != TownEntityOf(villager) && CheckMoveIntoTown(villager, there) != 0)
		{
			ChangeTribeIfRequired(villager, tq::TribeOf(there), true);
		}
	}
	v = VillagerOf(villager);
	TraceIf(villager, fmt::format("disciple {}: landed by {} things", static_cast<uint32_t>(type), things.size()));
	const auto start = DiscipleStartState(type);
	if (start == VillagerStates::InvalidState)
	{
		v->targetThing = things.front();
		SetupInspectObject(villager, things.front());
	}
	else
	{
		SetTopState(villager, StartStatePorted(start) ? start : VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t InspectObject(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// Facing the target first
	const auto target = v->targetThing;
	if (target != entt::null && LookAtPos(villager, tq::PosOf(target), 1) == 0)
	{
		return 1;
	}
	if (!ecs::VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		return 1;
	}
	// A builder by a building going up builds it. (inferred) the original's test of the thing's flag 0x2 is the
	// building's under-construction flag
	const auto* abode = target != entt::null ? Entities().TryGet<const Abode>(target) : nullptr;
	if (v->discipleType == static_cast<uint8_t>(VillagerDisciple::Builder) && abode != nullptr &&
	    (abode->buildFlags & Abode::k_UnderConstruction) != 0 && SetupBuildingObjectForBuilding(villager, target) == 1)
	{
		return 1;
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

bool ClosestObjectValidate(LivingAction& action)
{
	auto* v = VillagerOf(Entities().ToEntity(action));
	if (v != nullptr && v->targetThing != entt::null && !ecs::IsAvailable(v->targetThing))
	{
		v->targetThing = entt::null;
	}
	return true;
}

void ResetAllDiscipleStates(entt::entity town)
{
	if (town == entt::null || !Entities().Valid(town) || !Entities().AllOf<Town>(town))
	{
		return;
	}
	for (const auto abode : town_stats::AbodesOf(town))
	{
		// a copy: the reset changes no abode's list, but the walk stays safe whatever it does
		const auto inhabitants = Entities().Get<const Abode>(abode).inhabitants;
		for (const auto person : inhabitants)
		{
			const auto* v = VillagerOf(person);
			if (v != nullptr && (v->flags & Villager::k_FlagDisciple) != 0 && DiscipleHeldAtJob(v->discipleType))
			{
				SetVillagerDisciple(person, entt::null, VillagerDisciple::None, 0);
			}
		}
	}
	// its missionaries are deleted
	DeleteAllMissionaries(town);
}
} // namespace openblack::ecs::villager
