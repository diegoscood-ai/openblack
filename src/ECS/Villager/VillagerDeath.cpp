/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDeath.h"

#include <algorithm>
#include <array>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "Audio/GAudio/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/SmokyStuff.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerSoul.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// Villager.cpp / VillagerStates.cpp / Living.cpp of runblack.exe W120 (VillagerDeath.h; the disassembly is in
// dev\documentacion\aldeanos\v12\states.txt, callers.txt, dead_callees.txt, delete.txt, tables.txt).

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// GInterfaceStatus's player (MyInterfaceStatus, IsMemberOfThisPlayer 0x64D750). (inferred) openblack has one local
/// interface: PLAYER_ONE, as HandSystem.cpp's Dropper and Alignment.cpp's ProcessForPlayer
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// The help table _villager_struct_0x0099a364 + 4 = 0x99A368, 12 bytes per reason (tables.txt): A +0 (0x75077C), B +4
/// (0x750897), C +8 (0x7507A1)
constexpr std::array<uint8_t, 10> k_HelpA = {0, 0, 1, 0, 0, 1, 1, 0, 0, 0};
constexpr std::array<uint8_t, 10> k_HelpB = {0, 0, 1, 1, 0, 1, 0, 0, 0, 0};
constexpr std::array<uint8_t, 10> k_HelpC = {0, 1, 1, 1, 1, 0, 0, 1, 1, 0};

/// DEATH_REASON's names (Enums.h DeathReason)
constexpr std::array<const char*, static_cast<size_t>(DeathReason::_COUNT)> k_DeathReasonNames = {
    "NONE", "STARVING", "SPELL", "ANIMAL", "CHANT", "PLAYER_INTERACTION", "PLAYER_INTERACTION_DROWN", "SACRIFICE",
    "EXHAUSTION", "OLD_AGE"};

/// The villager EndPhysics is ending (EndingPhysicsScope)
entt::entity g_EndingPhysics = entt::null;
std::function<void(entt::entity, const DeathHelp&)> g_HelpSink;
std::function<std::optional<bool>(entt::entity)> g_GraveyardForTests;
std::function<void(entt::entity, const DeadEffects&)> g_DeadEffectsSink;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

const char* DeathName(DeathReason reason)
{
	return k_DeathReasonNames.at(std::min<size_t>(static_cast<size_t>(reason), k_DeathReasonNames.size() - 1));
}

size_t ReasonIndex(DeathReason reason)
{
	return std::min<size_t>(static_cast<size_t>(reason), k_HelpA.size() - 1);
}

/// Villager::GetTown 0x751F00 (vt +0x48): +0x12C, entt::null without one
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->town == entt::null || !Entities().Valid(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

/// Town +0x748, the town's graveyard (graveyard::GetGraveyard; set by Graveyard::MakeFunctional 0x595E00, cleared by
/// Graveyard::DeleteDependancys 0x595CE0) and its IsFunctional (vt +0xD4, abode_queries::IsFunctional): nullopt none,
/// else whether it is functional (V12 spec §8.5: 120 turns with a functional one, else 600)
std::optional<bool> GraveyardOf(entt::entity town)
{
	if (town == entt::null)
	{
		return std::nullopt;
	}
	if (g_GraveyardForTests)
	{
		return g_GraveyardForTests(town);
	}
	const auto yard = graveyard::GetGraveyard(town);
	if (yard == entt::null)
	{
		return std::nullopt;
	}
	return abode_queries::IsFunctional(yard);
}

/// Object::CreateSmokyStuff 0x63A810 (0, 1.0, 0xFFFFFFFF): the puff half the villager's height up (GetHeight vt +0x42C)
void CreateSmokyStuff(entt::entity villager)
{
	const auto* transform = Entities().TryGet<const Transform>(villager);
	if (transform == nullptr)
	{
		return;
	}
	const float half = 0.5f * object::GetHeight(villager);
	SmokyStuff::Create(transform->position + glm::vec3(0.0f, half, 0.0f), 1.0f);
}

/// The 3D object's mesh (obj3d +0x40 GetMesh vt +0xF8): the Mesh, or the one kept while it is not drawn (a -4 clip,
/// VillagerAnimations' hiddenMesh)
entt::id_type CurrentMesh(entt::entity villager)
{
	auto& registry = Entities();
	if (const auto* mesh = registry.TryGet<const Mesh>(villager))
	{
		return mesh->id;
	}
	const auto* animation = registry.TryGet<const SkeletalAnimation>(villager);
	return animation != nullptr ? animation->hiddenMesh : 0;
}

/// obj3d->SetMesh (vt +0xF4), as VillagerHome's SetVillagerMeshes does it
void SetObjectMesh(entt::entity villager, entt::id_type id)
{
	auto& registry = Entities();
	if (auto* mesh = registry.TryGet<Mesh>(villager))
	{
		mesh->id = id;
		return;
	}
	if (auto* animation = registry.TryGet<SkeletalAnimation>(villager); animation != nullptr && animation->hiddenMesh != 0)
	{
		animation->hiddenMesh = id;
	}
}

/// MeshPack[0x1FF] (0x76A611 / 0x7562E2): PersonSkeletonMale, for men, women and children alike
entt::id_type SkeletonMesh()
{
	return resources::HashIdentifier(MeshId::PersonSkeletonMale);
}

void EmitHelp(entt::entity villager, entt::entity town, const DeathHelp& help)
{
	if (g_HelpSink)
	{
		g_HelpSink(villager, help);
		return;
	}
	const auto* transform = Entities().TryGet<const Transform>(villager);
	const glm::vec3 position = transform != nullptr ? transform->position : glm::vec3(0.0f);
	if (help.killingPeople)
	{
		// fn_0071CE70(IS + 0x30, me): the villager's object on screen (fn_0081F1A0, its bounding box). (approximate) its
		// position through GameQueries::pointOnScreen (fn_0081F1D0): the queries have no bounding-box test
		const auto& queries = audio::Queries();
		audio::guidance::HelpSpritesKillingPeople(queries.pointOnScreen && queries.pointOnScreen(position));
	}
	if (help.deathInVillage)
	{
		audio::guidance::DeathInVillageSFX(); // fn_0071C810
	}
	if (help.worshippersDying)
	{
		audio::guidance::HelpSpritesWorshippersDying(); // fn_0071CFE0
	}
	if (help.losingVillagers)
	{
		audio::guidance::HelpSpritesLosingVillagers(position); // 0x71C990
	}
	if (help.lowOnPeople && town != entt::null)
	{
		audio::guidance::HelpSpritesLowOnPeople(town_queries::HelpTownOf(town)); // 0x71CBE0
	}
}

/// Living::Dead 0x5EC400 for a villager (after Villager::Dead): the counter and the vanish
uint32_t LivingDead(entt::entity villager, LivingAction& action)
{
	// 0x5EC403..0x5EC417: the flock (+0xB8): a villager has none
	const auto tick = living::DeadTick(action.turnsUntilStateChange, IsScriptControlled(villager), GetDeathReason(villager));
	action.turnsUntilStateChange = tick.counter;
	if (!tick.vanish)
	{
		if (TraceOn(villager) && tick.counter % 100 == 0)
		{
			Trace(villager, fmt::format("dead: {}", tick.counter));
		}
		return 1;
	}
	// 0x5EC447..0x5EC45F: CreateSmokyStuff(0, 1.0, 0xFFFFFFFF), ToBeDeleted(0) (vt +0xC = Villager::ToBeDeleted 0x7521B0); 5
	if (TraceOn(villager))
	{
		Trace(villager, "dead: vanish");
	}
	if (g_DeadEffectsSink)
	{
		g_DeadEffectsSink(villager, DeadEffects {.smoke = true});
	}
	else
	{
		CreateSmokyStuff(villager);
	}
	Delete(villager);
	return 5;
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

DeathHelp HelpFor(DeathReason reason, bool killerLocal, bool ownerLocal, bool killerIsOwner, bool hasTown, uint32_t adults,
                  uint32_t threshold)
{
	const auto r = ReasonIndex(reason);
	DeathHelp help;
	// 0x750770..0x7507B8: the killer is the local player -> A; else the owner is -> C
	if (killerLocal)
	{
		help.killingPeople = k_HelpA.at(r) != 0;
	}
	else if (ownerLocal)
	{
		help.deathInVillage = k_HelpC.at(r) != 0;
	}
	if (!hasTown)
	{
		return help;
	}
	// 0x75085B..0x7508EF: CHANT -> the owner local: worshippers dying; else adults (+0x618) > GTownInfo +0x150 (unsigned,
	// jbe) -> B, the killer not the owner and the owner local: losing villagers; else the owner local: low on people
	if (reason == DeathReason::Chant)
	{
		help.worshippersDying = ownerLocal;
	}
	else if (adults > threshold)
	{
		help.losingVillagers = k_HelpB.at(r) != 0 && !killerIsOwner && ownerLocal;
	}
	else
	{
		help.lowOnPeople = ownerLocal;
	}
	return help;
}

uint16_t DyingTime(bool functionalGraveyard, const GVillagerInfo& info)
{
	// 0x76A523 `mov cx, word [eax + 0x294]` / 0x76A533 `mov ax, word [edx + 0x290]`: the low words
	return static_cast<uint16_t>(functionalGraveyard ? info.dyingTimeWithGraveyard : info.dyingTimeWithoutGraveyard);
}

int32_t DyingClip(bool water, uint8_t landType)
{
	// 0x42377F: 0x11B; 0x423786..0x423798: (landType bits 0x30 - 0x20) != 0 -> 0xF6 + 7, else 0xF6
	if (water)
	{
		return 283;
	}
	return (landType & 3) == 2 ? 246 : 253;
}

int32_t DeadClip(bool water, uint8_t landType)
{
	// 0x4237AF: 0xF9; 0x4237B6..0x4237C8: != 2 -> 0xF6 - 3, else 0xF6
	if (water)
	{
		return 249;
	}
	return (landType & 3) == 2 ? 246 : 243;
}

// ---- VillagerDead and the states ---------------------------------------------------------------------------------

void VillagerDead(entt::entity villager, DeathReason reason, std::optional<PlayerNames> killer, float amount, int drop)
{
	auto& registry = Entities();
	if (!registry.Valid(villager) || VillagerOf(villager) == nullptr || ActionOf(villager) == nullptr)
	{
		return;
	}
	// 0x7506C3: +0x24 & 0x40 (in the physics): nothing; EndPhysics 0x5F0A60 kills it at rest with reason 5 / 6
	if (IsInPhysics(villager))
	{
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("death: {} ignored (in the physics)", DeathName(reason)));
		}
		return;
	}
	// 0x7506CF: IsDead (vt +0xAF4, Living::IsDead 0x417270)
	if (IsDead(villager))
	{
		return;
	}
	// 0x7506E2 GetTown (its result unused here). 0x7506E9..0x75070E: a reason not in {0, 1, 3, 4, 8, 9} ->
	// killer->FUN_0064DA80(0, 1) only when IsMultiplayerGame 0x552F80 (not ported: no multiplayer)
	const auto town = TownEntityOf(villager);
	// 0x750722..0x75073C: owner = GetPlayer (0x7502F0), else the neutral player (g_game +0x18 + 0xA60 x [+0x205A5B]);
	// 0x750740..0x750760: killer none -> the neutral player
	const auto ownerPlayer = GetPlayerOf(villager);
	const PlayerNames owner = ownerPlayer.value_or(PlayerNames::NEUTRAL);
	const PlayerNames by = killer.value_or(PlayerNames::NEUTRAL);
	const auto* t = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	// the town's adults (+0x618) are read at 0x750886, before SetDying: the dying villager still counts
	const uint32_t adults = t != nullptr ? t->stats.adults : 0;
	const uint32_t threshold =
	    Locator::infoConstants::has_value() ? Locator::infoConstants::value().town.populationUnderWhichHelpSpritesWarn : 0;
	const auto help =
	    HelpFor(reason, by == k_LocalPlayer, owner == k_LocalPlayer, by == owner, t != nullptr, adults, threshold);
	// 0x750770..0x7507B8: the killer's / owner's help sprite (before the drops)
	EmitHelp(villager, town, DeathHelp {.killingPeople = help.killingPeople, .deathInVillage = help.deathInVillage});
	// 0x7507C4..0x7507E2: drop != 0 -> CreateDroppedResource(0, 0, 0) 0x750940; then DropWood(0) 0x751240 and DropFood(0)
	// 0x7511E0 always (the town's carried totals go back)
	if (drop != 0)
	{
		CreateDroppedResource(villager, std::nullopt, std::nullopt, std::nullopt);
	}
	DropWood(villager, 0);
	DropFood(villager, 0);
	// 0x7507F3..0x750800: a disciple (+0xE0 & 0x200) -> SetVillagerDisciple(0, 0, 0) 0x756000. TODO(V14): disciples
	// 0x750805..0x750818: (owner)->+0x60 GAlignment::Update(GetPlayer(), me, reason) 0x4143B0: nothing without a player
	const bool child = IsChild(villager);
	if (ownerPlayer)
	{
		effects::alignment::UpdateForDeath(*ownerPlayer, reason, child, false);
	}
	if (t != nullptr)
	{
		// fn_0073E0A0(town, killer, reason, amount): Town +0xA08[killerNo x 32 + reason] += amount. (not ported) no
		// reader of +0xA08 was found (dispscan_out2.txt)
		(void)amount;
		// fn_0073E440 -> TownStats fn_00749780(me, reason, killer, amount)
		auto& deaths = registry.AllOf<TownDeaths>(town) ? registry.Get<TownDeaths>(town) : registry.Assign<TownDeaths>(town);
		deaths.lastDeathTurn = CurrentTurn(); // 0x74978E +0xD8
		++deaths.count;                       // 0x749794 +0xDC
		++deaths.byPlayer.at(std::min<size_t>(static_cast<size_t>(by), deaths.byPlayer.size() - 1)); // 0x7497C5 +0xA4[n]
		++deaths.byReason.at(ReasonIndex(reason)); // 0x7497DA +0x7C[r]
		++deaths.total5C.at(0);                    // 0x7497E4 +0x5C
		++deaths.total38;                          // 0x7497E5 +0x38
		// fn_0073E440: the town's player's GameStats (+0xA44) +0x4C ++ and the killer's +0x106C ++. TODO(statistics):
		// openblack has no GameStats
		// fn_0073E440: the town's graveyard (+0x748) -> fn_00595E50 (graveyard::AddDead)
		// (0x73E48B: only `if (town +0x748)`; AddDead tests IsFunctional and the 50 itself)
		if (const auto yard = graveyard::GetGraveyard(town); yard != entt::null)
		{
			graveyard::AddDead(yard);
		}
		// fn_0073E440: +0x5EC = 0, +0x5E8 = 1 (the town's pulse, TownProcess reads it)
		auto& changed = registry.Get<Town>(town);
		changed.buildPulsePrevious = 0;
		changed.buildPulse = 1;
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("death: town {} deaths[{}] = {} total {}", changed.id, static_cast<uint32_t>(reason),
			                            deaths.byReason.at(ReasonIndex(reason)), deaths.count));
		}
		// 0x75085B..0x7508EF: the town's help sprite
		EmitHelp(villager, town,
		         DeathHelp {.worshippersDying = help.worshippersDying,
		                    .losingVillagers = help.losingVillagers,
		                    .lowOnPeople = help.lowOnPeople});
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("death: {} killer {} owner {} amount {:.4f} drop {} help {}{}{}{}{}", DeathName(reason),
		                            static_cast<int>(by), static_cast<int>(owner), amount, drop, help.killingPeople ? "k" : "",
		                            help.deathInVillage ? "v" : "", help.worshippersDying ? "w" : "",
		                            help.losingVillagers ? "l" : "", help.lowOnPeople ? "p" : ""));
	}
	else if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Villager {} died ({})", object_index::Of(villager), DeathName(reason));
	}
	// 0x7508FD..0x750923: not dead yet (+0xB4 & 1) -> SetDying (vt +0x6A4); SACRIFICE: the killer's GameStats +0x1124 ++
	// (TODO(statistics)) and the counter (+0x58) = 0
	if (const auto* v = VillagerOf(villager); v != nullptr && (v->status & Villager::k_StatusDead) == 0)
	{
		SetDying(villager);
		if (reason == DeathReason::Sacrifice)
		{
			if (auto* action = ActionOf(villager))
			{
				action->turnsUntilStateChange = 0;
			}
		}
	}
	// 0x750929: +0x118 = (u8) reason, after SetDying
	if (auto* v = VillagerOf(villager))
	{
		v->deathReason = reason;
	}
}

uint32_t DestroyedByEffect(entt::entity villager, std::optional<PlayerNames> player, float amount)
{
	// 0x7502D0..0x7502E3: VillagerDead(2 SPELL, player, amount, 1); 1
	VillagerDead(villager, DeathReason::Spell, player, amount, 1);
	return 1;
}

uint32_t SetDying(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	auto* action = ActionOf(villager);
	if (v == nullptr || action == nullptr)
	{
		return 1;
	}
	// 0x76A4C6: GetTown first (DeleteDependancys takes it out of the town)
	const auto town = TownEntityOf(villager);
	if ((v->status & Villager::k_StatusDead) == 0)
	{
		// 0x76A4DF SetLife(0) (vt +0x5B0, Villager::SetLife 0x756B40)
		life::SetLife(villager, 0.0f);
		// 0x76A4EB SetTopState(14 DYING): the exits of the state left run and the dying clip is chosen now, with the
		// landType of the last landing (the 0x30 below comes after). (approximate) villager_reactions::SetTopState: it also
		// ends openblack's walk, as MOVE_TO_POS' exit ExitMoveToPos 0x5EDDA0 would (not ported)
		villager_reactions::SetTopState(villager, VillagerStates::Dying);
		// 0x76A4F1 status |= 1; 0x76A4FA DeleteDependancys 0x74FD60; 0x76A4FF status |= 0x30 (landType 3)
		if (auto* now = VillagerOf(villager))
		{
			now->status = static_cast<uint16_t>(now->status | Villager::k_StatusDead);
		}
		DeleteDependancys(villager);
		if (auto* now = VillagerOf(villager))
		{
			now->status = static_cast<uint16_t>(now->status | Villager::k_StatusLandTypeMask);
		}
	}
	// 0x76A506..0x76A53A: the counter (+0x58) = the town's graveyard (+0x748) functional (vt +0xD4) ?
	// DyingTimeWithGraveyard (+0x294) : DyingTimeWithoutGraveyard (+0x290); also when it was dead already
	const bool graveyard = HasFunctionalGraveyard(town);
	if (auto* again = ActionOf(villager))
	{
		again->turnsUntilStateChange = DyingTime(graveyard, InfoOf(villager));
	}
	// 0x76A53E..0x76A558: not counted out (+0xE0 & 0x40) -> --g_game +0x205A54 (the world population) and flags |= 0x40.
	// openblack counts the entities (magic::players::WorldPopulation skips the counted-out ones)
	if (auto* now = VillagerOf(villager); now != nullptr && (now->flags & Villager::k_FlagCountedOut) == 0)
	{
		now->flags = static_cast<uint16_t>(now->flags | Villager::k_FlagCountedOut);
	}
	if (TraceOn(villager))
	{
		const auto* again = ActionOf(villager);
		Trace(villager, fmt::format("setdying: counter {} (graveyard {})", again != nullptr ? again->turnsUntilStateChange : 0,
		                            graveyard ? 1 : 0));
	}
	return 1;
}

uint32_t SetDyingState(LivingAction& action)
{
	return SetDying(Entities().ToEntity(action));
}

uint32_t Dying(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x76A575..0x76A593: GetDeathReason (vt +0x444) 7 SACRIFICE -> SetTopState(15) (no dying clip); else
	// PlayAnimThenSetState(15, 1) 0x5ECAC0: 23 WAIT_FOR_ANIMATION until the dying clip has played, then 15
	if (GetDeathReason(villager) == DeathReason::Sacrifice)
	{
		SetTopState(villager, VillagerStates::Dead);
	}
	else
	{
		PlayAnimThenSetState(villager, VillagerStates::Dead);
	}
	// 0x76A599..0x76A5C5: not at home (+0xE0 & 4) and (no town or no graveyard +0x748) -> CreateReaction(me, 23
	// REACT_TO_DEATH, no player, 0). The town is already none here (SetDying left it), so the graveyard test never
	// stops it (literal)
	bool reaction = false;
	if (const auto* v = VillagerOf(villager); v != nullptr && (v->flags & Villager::k_FlagAtHome) == 0)
	{
		const auto town = TownEntityOf(villager);
		if (town == entt::null || !GraveyardOf(town))
		{
			effects::reactions::CreateReaction(villager, openblack::Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
			reaction = true;
		}
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("dying: -> 15 (reaction {})", reaction ? "REACT_TO_DEATH" : "none"));
	}
	return 1;
}

uint32_t Dead(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x76A5E3..0x76A5F2: a fire (+0x44) -> its ToBeDeleted(0), +0x44 = 0: a burning corpse stops burning
	if (auto* fire = fire::Find(villager); fire != nullptr)
	{
		fire::ToBeDeleted(*fire);
	}
	// 0x76A5F9..0x76A620: the static 0xDCB164 = MeshPack[0x1FF], once
	const auto skeleton = SkeletonMesh();
	// 0x76A626: not controlled by a script (+0x25 & 4)
	if (!IsScriptControlled(villager))
	{
		// 0x76A630..0x76A641: obj3d GetMesh (vt +0xF8) != the skeleton: only the first DEAD turn
		if (CurrentMesh(villager) != skeleton)
		{
			DeadEffects effects {.smoke = true, .skeleton = true};
			// 0x76A659 CreateSmokyStuff(0, 1.0, 0xFFFFFFFF)
			if (!g_DeadEffectsSink)
			{
				CreateSmokyStuff(villager);
			}
			// 0x76A65E..0x76A668: !MapCoords::IsWater(Pos) -> the soul (fn_00828790); a drowned corpse has none
			const auto* transform = Entities().TryGet<const Transform>(villager);
			if (transform != nullptr && !sea_cells::IsWater(transform->position))
			{
				// 0x76A6E6..0x76A6F2: GetAge (vt +0x8D0) < grownUpAge (+0x138, unsigned jae): the child mesh (+0x204
				// ChildMeshHigh), heaven forced; else GVillagerInfo::GetMesh 0x74F880 (+0x214 StdDetail), not forced
				const auto& info = InfoOf(villager);
				const bool young = GetAge(villager) < info.grownUpAge;
				effects.soul = true;
				effects.soulMesh = young ? info.childMeshHigh : info.stdDetail;
				effects.heavenForced = young;
				if (!g_DeadEffectsSink)
				{
					const auto clip = villager_soul::Create(villager, effects.soulMesh, effects.heavenForced);
					if (TraceOn(villager))
					{
						Trace(villager, fmt::format("dead: smoke, soul {} mesh {}, skeleton", clip,
						                            static_cast<uint32_t>(effects.soulMesh)));
					}
				}
			}
			else if (TraceOn(villager))
			{
				Trace(villager, "dead: smoke, water, skeleton");
			}
			if (g_DeadEffectsSink)
			{
				g_DeadEffectsSink(villager, effects);
			}
		}
		// 0x76A750..0x76A76A: obj3d SetMesh(skeleton) (vt +0xF4), every turn; vt +0xC4 (edx 0) (not ported: (inferred) a
		// reset of the 3D object)
		SetObjectMesh(villager, skeleton);
	}
	// 0x76A772 Living::Dead 0x5EC400
	auto* again = ActionOf(villager);
	return again != nullptr ? LivingDead(villager, *again) : 1;
}

uint32_t CannotExitState(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// 0x768650 IsStateExitFunctionSameAs (vt +0x96C) -> 1; 0x76865A / 0x76865F: 0x18 IN_HAND, 0xA FLYING -> 1; else 0
	if (IsStateExitFunctionSameAs(villager, next) || next == VillagerStates::InHand || next == VillagerStates::Flying)
	{
		return 1;
	}
	return 0;
}

// ---- queries -----------------------------------------------------------------------------------------------------

bool IsDead(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return false;
	}
	// 0x417270..0x41727A: +0xB4 & 1; 0x41727C: TOP (+0x8C) == 0xF; 0x417287 IsFunctional (vt +0xD4) != 1. (inferred) a
	// villager is always functional (GameThing::IsFunctional)
	return (v->status & Villager::k_StatusDead) != 0 || GetState(villager, Index::Top) == VillagerStates::Dead;
}

DeathReason GetDeathReason(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? v->deathReason : DeathReason::None;
}

bool IsCountedOut(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->flags & Villager::k_FlagCountedOut) != 0;
}

std::optional<PlayerNames> GetPlayerOf(entt::entity villager)
{
	// 0x7502F0: GetTown ? Town +0x2C : 0
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	return Entities().Get<const Town>(town).owner;
}

bool IsSkeleton(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->status & Villager::k_StatusSkeleton) != 0;
}

void SetSkeleton(entt::entity villager, bool on)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// 0x7562F6..0x756315: status bit 6 = on & 1
	v->status = static_cast<uint16_t>((v->status & ~Villager::k_StatusSkeleton) | (on ? Villager::k_StatusSkeleton : 0));
	const auto& info = InfoOf(villager);
	if (IsSkeleton(villager))
	{
		// 0x756328..0x75633D: obj3d SetMesh(MeshPack[0x1FF])
		SetObjectMesh(villager, SkeletonMesh());
	}
	else
	{
		// 0x756348..0x756423: a child (GetAge < grownUpAge) its child meshes (+0x20C / +0x208 / +0x204), else
		// GetDetailMesh(2 / 1 / 0): openblack's one mesh of the LODs (VillagerHome's SetVillagerMeshes)
		SetVillagerMeshes(villager, info, GetAge(villager) < info.grownUpAge, false);
	}
	// 0x756429..0x756436: SetScaleForAge(GetAge()) (draws GameFloatRand)
	SetScaleForAge(villager, GetAge(villager));
}

// ---- deletion ----------------------------------------------------------------------------------------------------

void DeleteDependancys(entt::entity villager)
{
	auto& registry = Entities();
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// 0x74FD69 GetAbode 0x752160, 0x74FD78 GetTown (vt +0x48): read first
	auto abode = entt::entity(entt::null);
	if (v->abode != entt::null && registry.Valid(v->abode) && registry.AllOf<Abode>(v->abode))
	{
		abode = v->abode;
	}
	const auto town = TownEntityOf(villager);
	// 0x74FD7F..0x74FD97: TOP not 13 / 14 / 15 -> SetTopState(13 SET_DYING) (vt +0x8E8): the exits of the state left
	// run (ExitAtHome 0x761B40 with row 13 +0xC0 = 0: LeaveHome). (approximate) villager_reactions::SetTopState: the walk
	// ends too (ExitMoveToPos 0x5EDDA0 is not ported)
	const auto top = GetState(villager, Index::Top);
	if (top != VillagerStates::SetDying && top != VillagerStates::Dying && top != VillagerStates::Dead &&
	    ActionOf(villager) != nullptr)
	{
		villager_reactions::SetTopState(villager, VillagerStates::SetDying);
	}
	// 0x74FD9D..0x74FE05: TOP != 29 MOVE_ON_PATH and a footpath (+0xCC): out of its walker list (+0x28, count +0x2C);
	// +0xCC = 0. (not ported) openblack's villagers walk no footpaths
	// 0x74FE0B..0x74FE17: IsAMother 0x751110 (GVillagerInfo +0x1F8 sex == 1) -> FindChildrenAndOrphanThem 0x756BE0
	if (InfoOf(villager).sex == SexType::Female)
	{
		villager_mourning::FindChildrenAndOrphanThem(villager);
	}
	// 0x74FE1C..0x74FE46: an abode -> Abode::RemoveDeletedVillagerFromAbode 0x404220 (which calls Town::RemoveVillager
	// when the abode has a town); else a town -> Town::RemoveVillager 0x73E210; else out of the vagrants (g_game
	// +0x205BFC, 0x74FE4B..0x74FEAC)
	if (abode != entt::null)
	{
		abode_villagers::RemoveDeletedVillagerFromAbode(abode, villager);
	}
	else if (town != entt::null)
	{
		town_villagers::RemoveVillager(town, villager);
	}
	else
	{
		town_villagers::RemoveFromVagrants(villager);
	}
}

void ToBeDeletedOverride(entt::entity villager)
{
	if (!Entities().Valid(villager) || VillagerOf(villager) == nullptr)
	{
		return;
	}
	// Villager::ToBeDeleted 0x7521B0: 0x7521B3 DeleteDependancys 0x74FD60, then 0x7521BF Living::ToBeDeleted 0x5EC0A0:
	// 0x5EC0AA..0x5EC0BA a reaction (+0x94) -> StopReacting (vt +0x998: villager_reactions::StopReacting, the fire's,
	// teleport's, shield's and the mourning's); out of the Living list; the data path (+0xAC) and script remind (+0xB0)
	// (not ported); a dance (vt +0x978 / +0xB08, pending); the flock (none). Object::ToBeDeleted 0x636670 is
	// ecs::ToBeDeleted's tail
	DeleteDependancys(villager);
	if (villager_reactions::IsReacting(villager))
	{
		villager_reactions::StopReacting(villager);
	}
}

void Delete(entt::entity villager)
{
	// Villager::ToBeDeleted 0x7521B0 (vt +0xC) through ecs::ToBeDeleted, whose villager branch is ToBeDeletedOverride.
	// ~Villager 0x74FBC0: not counted out (+0xE0 & 0x40) -> --g_game +0x205A54. openblack counts the entities: the
	// deleted one is no longer one
	ecs::ToBeDeleted(villager);
}

// ---- physics -----------------------------------------------------------------------------------------------------

EndingPhysicsScope::EndingPhysicsScope(entt::entity villager)
    : _previous(g_EndingPhysics)
{
	g_EndingPhysics = villager;
}

EndingPhysicsScope::~EndingPhysicsScope()
{
	g_EndingPhysics = _previous;
}

bool IsInPhysics(entt::entity villager)
{
	return villager != g_EndingPhysics && physics::PhysicsObjects::IsFlying(villager);
}

// ---- tests -------------------------------------------------------------------------------------------------------

void SetDeathHelpForTests(std::function<void(entt::entity, const DeathHelp&)> sink)
{
	g_HelpSink = std::move(sink);
}

void SetGraveyardForTests(std::function<std::optional<bool>(entt::entity)> graveyard)
{
	g_GraveyardForTests = std::move(graveyard);
}

bool HasFunctionalGraveyard(entt::entity town)
{
	const auto graveyard = GraveyardOf(town);
	return graveyard.has_value() && *graveyard;
}

void SetDeadEffectsForTests(std::function<void(entt::entity, const DeadEffects&)> sink)
{
	g_DeadEffectsSink = std::move(sink);
}
} // namespace openblack::ecs::villager

namespace openblack::ecs::living
{
DeadTickResult DeadTick(uint16_t counter, bool scriptControlled, DeathReason reason)
{
	DeadTickResult result {counter, false};
	// 0x5EC41E: not controlled by a script (+0x25 & 4): 0x5EC425..0x5EC437 ax = +0x58; +0x58 = ax - 1; ax == 0 -> vanish
	if (!scriptControlled)
	{
		result.counter = static_cast<uint16_t>(counter - 1);
		if (counter == 0)
		{
			result.vanish = true;
			return result;
		}
	}
	// 0x5EC439..0x5EC443: GetDeathReason (vt +0x444) == 7 SACRIFICE -> vanish
	if (reason == DeathReason::Sacrifice)
	{
		result.vanish = true;
	}
	return result;
}
} // namespace openblack::ecs::living
