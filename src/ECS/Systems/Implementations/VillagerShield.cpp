/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerShield.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <numbers>
#include <unordered_map>

#include <fmt/format.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Spells/SpellShield.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace tq = openblack::ecs::town_queries;

namespace
{
/// Villager +0x94 (the reaction it follows), +0xBC (its object) and +0x10C (the JustWholeMapXZ it looks at; the
/// original shares that field with the fire's saved destination, Living +0x10C)
struct ShieldState
{
	entt::entity spell {entt::null};
	uint32_t reaction {0};
	glm::ivec2 lookAt {0, 0};
};
std::unordered_map<entt::entity, ShieldState> g_States;

auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Reg().TryGet<LivingAction>(villager);
}

VillagerStates Get(const LivingAction& action, LivingAction::Index index)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

const GVillagerStateTableInfo* TableOf(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto i = static_cast<size_t>(state);
	return i < table.size() ? &table[i] : nullptr;
}

/// Villager::GetFinalState 0x751DD0: the top state if it is a final one (table +0x0C), else the destination state
VillagerStates FinalState(const LivingAction& action)
{
	const auto top = Get(action, LivingAction::Index::Top);
	const auto* info = TableOf(top);
	return info != nullptr && info->isFinalState != 0 ? top : Get(action, LivingAction::Index::Final);
}

const ReactionInfo& ShieldReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToMagicShield));
}

/// Villager vt +0x48 GetTown
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* component = Reg().TryGet<const Villager>(villager);
	if (component == nullptr || !Reg().Valid(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// __RTDynamicCast<SpellShield> (0x765BCD, 0x765C7A, 0x765E2B) and GameThing::IsAvailable 0x401810 (vt 0x2C): a live
/// shield spell. (aproximado) openblack has no IsAvailable flags for spells: an entity with a Spell component of the
/// shield class is taken as available, as Magic/Objects/MapShield does
bool IsAvailableShieldSpell(entt::entity spell)
{
	auto& registry = Reg();
	if (spell == entt::null || !registry.Valid(spell) || !registry.AllOf<Spell>(spell))
	{
		return false;
	}
	return registry.Get<const Spell>(spell).spellClass == SpellClass::Shield;
}

/// Object vt +0x60 GetRadius of a shield spell = SpellShield::Get2DRadius 0x72B440: its magnitude
float RadiusOf(entt::entity spell)
{
	return Reg().Get<const Spell>(spell).magnitude;
}

/// Reaction::GetPos 0x6E45C0 = the initiator's GetPos (+0x14): for a shield spell its cast position, as MapCoords
glm::ivec2 ShieldPos(entt::entity spell)
{
	const auto& component = Reg().Get<const Spell>(spell);
	return tq::ToMapCoords(glm::vec2(component.position.x, component.position.z));
}

/// OPENBLACK_TEST_SHIELD_REACTION=1: takes the town as "it wants protection and was just attacked", so the reaction
/// can be seen in game while what feeds those two inputs is not ported (the protection desire's function reads Town
/// +0xEC0, which ProcessPlayerInteract 0x73DEC0 writes and openblack does not yet; only a script boost moves it; and
/// the aggressor's turn only comes from the physical shield's impacts). Off by default: the original's reads
bool TestForceTownGates()
{
	static const bool forced = [] {
		const char* value = std::getenv("OPENBLACK_TEST_SHIELD_REACTION");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return forced;
}

/// TownDesire::GetDesireSignificanceToVillager(town +0x34, TOWN_DESIRE_FOR_PROTECTION 3) 0x746660 (0x765C0A..0x765C0F
/// and 0x765E4A..0x765E4F: `push 3; lea ecx, [town + 0x34]; call`) = max(0, TD[0x118 + 4 d] + TD[0xD4 + 4 d] +
/// TD[0x90 + 4 d] - GTownDesireInfo[d] +0x18): ecs::town_desire
float ProtectionDesireSignificance(entt::entity town)
{
	if (TestForceTownGates())
	{
		return 1.0f;
	}
	return town_desire::GetDesireSignificanceToVillager(town, TownDesireInfo::ForProtection);
}

/// 0x765C21..0x765C46: game turn - town +0xEB0 (the turn of its last aggressor, Town::UpdateAggressor 0x73C9B0)
/// against villagerInfo +0x364 (GVillagerInfo::numGameTurnsAfterAggressionInterestedInShield; the field is matched by
/// name, the offset was not recounted), unsigned
bool AttackedRecently(entt::entity town)
{
	if (TestForceTownGates())
	{
		return true;
	}
	const auto* component = Reg().TryGet<const Town>(town);
	if (component == nullptr)
	{
		return false;
	}
	const uint32_t since = effects::reactions::Turn() - component->aggressorTurn;
	// (aproximado) the original reads the villager's own info row (+0x28); openblack's villager code uses row 0
	return since <= Locator::infoConstants::value().villager.at(0).numGameTurnsAfterAggressionInterestedInShield;
}

/// Villager::IsAvailableForReaction 0x763390: its final state takes reactions (table +0xEC), and it is not held or
/// thrown (as VillagerFire.cpp / VillagerTeleport.cpp: the +0xE0 flags, the life threshold and
/// Living::IsAvailableForReaction are not ported)
bool IsAvailableForReaction(entt::entity villager)
{
	const auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return false;
	}
	const auto top = Get(*action, LivingAction::Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::InHand)
	{
		return false;
	}
	const auto* info = TableOf(FinalState(*action));
	return info != nullptr && info->field0xec != 0;
}

/// fn_006E4340 on the villager's records (Living +0x98): may it react to this type again?
bool MayReactAgain(entt::entity villager, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield), again,
	                                   effects::reactions::Turn());
}

/// Villager::StorePreviousState 0x763470 (AddReaction's vt 0x8EC when it had no reaction): the final state goes to
/// index 2, unless it is a passing one (table +0x10 or +0xB8)
void StorePreviousState(LivingAction& action)
{
	const auto final = FinalState(action);
	const auto* info = TableOf(final);
	auto stored = final;
	if (info != nullptr && (info->field0x10 != 0 || info->field0xb8 != 0))
	{
		stored = Get(action, LivingAction::Index::Previous);
	}
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(stored);
}

/// 0x765D8B..0x765DEF and 0x765EA0..0x765F07, the same point both times: the villager's own position plus 1 m along the
/// bearing from the shield's centre to it, jittered by GameFloatRand(pi / 4) - pi / 8 (0x3F490FDB, 0x8C6CA0). It is
/// what Living::LookAtPos turns towards, so the villager ends up facing away from the dome
glm::ivec2 LookAtPoint(entt::entity villager, entt::entity spell)
{
	const auto me = tq::PosOf(villager);
	const float bearing = tq::Get3DAngleFromXZ(ShieldPos(spell), me);
	const float angle = bearing + villager::GameFloatRand(std::numbers::pi_v<float> / 4.0f) - 0.39269909f;
	return me + tq::GetPosFromAngle(angle, 1.0f);
}

/// 0x765F0C..0x765FB4: the state's clip is kept for GameFloatRand(10) + 20 seconds (as many loops as fit: the loop
/// count IsReadyForNewAnimation 0x5EC960 is asked for is that time x 1000 / the clip's duration in ms) and then rolled
/// again (SetAnim(GetAnimId(), 1), the AmazedByShield set of ECS/VillagerAnimations). The one exception (0x765F55):
/// while the clip is INTO_POINTING (286) it only plays once and is followed by TALKING_AND_POINTING (395)
void UpdateAmazedClip(entt::entity villager, LivingAction& action)
{
	// 0x765F0C..0x765F2E: GameFloatRand(10) + 20 seconds, drawn every turn the state runs (before anything else here, so
	// the random sequence does not depend on openblack's clip guards below)
	const float seconds = villager::GameFloatRand(10.0f) + 20.0f;
	auto* animation = Reg().TryGet<const SkeletalAnimation>(villager);
	if (animation == nullptr || !animation->hasClip || !Locator::resources::has_value())
	{
		return;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip))
	{
		return;
	}
	const int32_t duration = animations.Handle(animation->clip)->GetDurationMs();
	if (duration <= 0)
	{
		return;
	}
	// 0x765F32..0x765F49: those seconds x 1000 / the duration of the clip it has (anim +0x20)
	auto loops = static_cast<int32_t>(seconds * 1000.0f / static_cast<float>(duration));
	constexpr int32_t k_IntoPointing = 286;      ///< 0x765F62: AnimPack entry 0x11E
	constexpr int32_t k_TalkingAndPointing = 395; ///< 0x765F82: 0x18B
	int32_t clip = -1;                            ///< < 0: roll the state's clip again (GetAnimId, vt +0x900)
	if (animation->clipIndex == k_IntoPointing)
	{
		loops = 1;
		clip = k_TalkingAndPointing;
	}
	// Living::IsReadyForNewAnimation(loops) 0x5EC960: turns in the state x the ms per turn (0xD01A38, 100) >= loops x
	// the duration
	if (static_cast<int32_t>(action.turnsSinceStateChange) * 100 < loops * duration)
	{
		return;
	}
	if (clip < 0)
	{
		VillagerSetStateClip(villager, true); // SetAnim(GetAnimId(), 1)
	}
	else
	{
		VillagerSetClip(villager, clip, true); // SetAnim(395, 1)
	}
	action.turnsSinceStateChange = 0; // 0x765FAB: +0x90 = 0
}

/// The REACT_TO_MAGIC_SHIELD part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one villager of the cell, as the
/// fire's and the teleport's: with no reaction of its own, fn_006E4620's score above 0 and not reacted to a shield
/// lately, it starts reacting (0x6E4109 stamps the reaction's turn, then vt +0x994 StartReacting ->
/// SetupReactToMagicShield). Replacing a current reaction by a higher scoring one (0x6E4134, reactions::MaySwitch) is
/// not ported for the villagers
void ApplyShieldReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	if (!registry.AllOf<Villager>(villager) || villager == reaction.initiator || !IsAvailableForReaction(villager))
	{
		return;
	}
	if (villager_reactions::IsReacting(villager))
	{
		return; // Living +0x94: it already follows a reaction (0x6E40D0)
	}
	const auto& info = ShieldReactionInfo();
	const auto* transform = registry.TryGet<const Transform>(villager);
	const auto* spell = registry.TryGet<const Spell>(reaction.initiator);
	if (transform == nullptr || spell == nullptr)
	{
		return;
	}
	const float distance =
	    0.5f * (std::abs(transform->position.x - spell->position.x) + std::abs(transform->position.z - spell->position.z));
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// fn_006E4620 (ECS/Effects/Reactions: Score)
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield), true,
	                                             villager_shield::ReactToMagicShieldPriority(villager, reaction.id), distance);
	if (score == 0 || !MayReactAgain(villager, info.numGameTurnsForNormalThingsBeforeReactingAgain))
	{
		return;
	}
	effects::reactions::MarkStarted(reaction.id, effects::reactions::Turn()); // 0x6E4109: +0x2C = the turn if still 0
	villager_shield::SetupReactToMagicShield(villager, reaction.initiator, reaction.id);
}
} // namespace

uint8_t villager_shield::ReactToMagicShieldPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !IsAvailableShieldSpell(found->initiator))
	{
		return 0; // 0x765BD7 (not a SpellShield) / 0x765BE2 (not available)
	}
	// 0x765BEF..0x765BFA: GetDistanceInMetres(my pos, the reaction's pos) is computed and dropped (`fstp st(0)`)
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return static_cast<uint8_t>(ShieldReactionInfo().priority & 0xFFu); // 0x765C08 -> 0x765C48 (byte 0xD4FBD4)
	}
	// 0x765C0F..0x765C1F: the town's desire for protection (3) must not be 0 (`test ah, 0x40`)
	if (ProtectionDesireSignificance(town) == 0.0f)
	{
		return 0;
	}
	if (!AttackedRecently(town))
	{
		return 0; // 0x765C44: ja -> 0
	}
	return static_cast<uint8_t>(ShieldReactionInfo().priority & 0xFFu);
}

void villager_shield::SetupReactToMagicShield(entt::entity villager, entt::entity spell, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || !IsAvailableShieldSpell(spell))
	{
		return; // 0x765C86: not a SpellShield, nothing happens
	}
	// 0x765C94..0x765CA2: AddReaction(reaction, 0xA8) (vt +0x990 -> Living::AddReaction 0x5F0F30: the reaction is kept
	// and the final state stored), +0xBC = the spell
	auto& state = g_States[villager];
	state.spell = spell;
	state.reaction = reaction;
	StorePreviousState(*action);
	villager_reactions::SetTopState(villager, VillagerStates::AmazedByMagicShieldReaction);
	const float radius = RadiusOf(spell);
	const auto shield = ShieldPos(spell);
	const auto me = tq::PosOf(villager);
	// 0x765CAC..0x765CC9: SpellShield::IsUnder(my pos, 0.2 R) 0x72BD20; already under it -> no walk
	const glm::vec3 at(tq::ToMetres(me).x, 0.0f, tq::ToMetres(me).y);
	if (!magic::spell_shield::IsUnder(spell, at, 0.2f * radius))
	{
		// 0x765CD1..0x765D0D: the bearing from the shield to the villager, + GameFloatRand(pi / 4) - pi / 8
		const float bearing = tq::Get3DAngleFromXZ(shield, me);
		const float angle = bearing + villager::GameFloatRand(std::numbers::pi_v<float> / 4.0f) - 0.39269909f;
		// 0x765D11..0x765D43: (R - 0.2 R) x GameFloatRand(1)^3, taken off that distance (0x765D50 `fsubr`)
		const float band = radius - 0.2f * radius;
		const float r = villager::GameFloatRand(1.0f);
		const float distance = band - band * (r * r * r);
		// 0x765D59..0x765D86: SetupMoveToWithHug(the reaction's pos + GetPosFromAngle(angle, distance), 0xA8)
		const auto goal = tq::ToMetres(shield + tq::GetPosFromAngle(angle, distance));
		villager::SetupMoveToWithHug(villager, goal, VillagerStates::AmazedByMagicShieldReaction);
	}
	// 0x765D8B..0x765DEF: the look-at point (+0x10C), walking or not
	state.lookAt = LookAtPoint(villager, spell);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("SetupReactToMagicShield: spell {} r {:.1f} reaction {}",
		                                      static_cast<uint32_t>(spell), radius, reaction));
	}
}

uint32_t villager_shield::AmazedByMagicShieldReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	const auto it = g_States.find(villager);
	const auto town = TownEntityOf(villager);
	const auto spell = it != g_States.end() ? it->second.spell : entt::entity(entt::null);
	// 0x765E09..0x765E5F: a town (vt +0x48), +0xBC a SpellShield, available, and its desire for protection above 0
	// (`test ah, 0x41`)
	if (town == entt::null || !IsAvailableShieldSpell(spell) || !(ProtectionDesireSignificance(town) > 0.0f))
	{
		// 0x765FC0..0x765FF7: SetupWaitForCounter(ftol(GameRand(60) + 20), DECIDE_WHAT_TO_DO)
		const auto turns = static_cast<uint16_t>(static_cast<float>(villager::GameRand(60)) + 20.0f);
		villager::SetupWaitForCounter(villager, turns, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x765E65..0x765E9E: LookAtPos(+0x10C, 0); once it is facing that way, one chance in four of a new point
	if (villager::LookAtPos(villager, it->second.lookAt, 0) != 0 && villager::GameRand(4) == 0)
	{
		it->second.lookAt = LookAtPoint(villager, spell);
	}
	UpdateAmazedClip(villager, action);
	return 1; // 0x765FB6
}

void villager_shield::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	ApplyShieldReaction(villager, reaction);
}

bool villager_shield::IsReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() && it->second.reaction != 0;
}

entt::entity villager_shield::ReactionObject(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() ? it->second.spell : entt::entity(entt::null);
}

bool villager_shield::IsReactionObjectAvailable(entt::entity villager)
{
	return IsAvailableShieldSpell(ReactionObject(villager));
}

void villager_shield::StopReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	if (it == g_States.end())
	{
		return;
	}
	// Living::StopReacting 0x5F1140: with a reaction (+0x94), fn_005F0FE0 gives its record the turn; +0x94 and +0xBC go
	if (it->second.reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield),
		                                  effects::reactions::Turn());
	}
	g_States.erase(it);
}

void villager_shield::Clear()
{
	g_States.clear();
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}
