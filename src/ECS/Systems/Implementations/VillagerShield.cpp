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
#include <vector>

#include <fmt/format.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerClips.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Spells/SpellShield.h"
#include "VillagerAnimate.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace tq = openblack::ecs::town_queries;

namespace
{
auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

/// The villager's shield state if it has one (any id, alive or not)
VillagerShieldState* FindState(entt::entity villager)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Reg();
	return registry.Valid(villager) ? registry.TryGet<VillagerShieldState>(villager) : nullptr;
}

/// The villager's shield state, added with its defaults if it has none. The villager must exist.
VillagerShieldState& StateOf(entt::entity villager)
{
	if (auto* state = Reg().TryGet<VillagerShieldState>(villager))
	{
		return *state;
	}
	return Reg().AssignState<VillagerShieldState>(villager);
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

/// The top state if it is a final one (state table), else the destination state
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

const ReactionInfo& StruckReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToMagicShieldStruck));
}

/// A shield spell, available or not (the class test alone)
bool IsShieldSpell(entt::entity spell)
{
	auto& registry = Reg();
	if (spell == entt::null || !registry.Valid(spell) || !registry.AllOf<Spell>(spell))
	{
		return false;
	}
	return registry.Get<const Spell>(spell).spellClass == SpellClass::Shield;
}

/// The villager's town
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* component = Reg().TryGet<const Villager>(villager);
	// (guard) the original reads this link with no availability test
	if (component == nullptr || !ecs::IsAvailable(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// A live shield spell: an entity with a Spell component of the shield class, not marked by ToBeDeleted
/// (ecs::IsAvailable; the spells are destroyed at once today)
bool IsAvailableShieldSpell(entt::entity spell)
{
	auto& registry = Reg();
	if (spell == entt::null || !ecs::IsAvailable(spell) || !registry.AllOf<Spell>(spell))
	{
		return false;
	}
	return registry.Get<const Spell>(spell).spellClass == SpellClass::Shield;
}

/// The radius of a shield spell (Get2DRadius): its magnitude
float RadiusOf(entt::entity spell)
{
	return Reg().Get<const Spell>(spell).magnitude;
}

/// The reaction's position is its initiator's: for a shield spell its cast position, as MapCoords
glm::ivec2 ShieldPos(entt::entity spell)
{
	const auto& component = Reg().Get<const Spell>(spell);
	return tq::ToMapCoords(glm::vec2(component.position.x, component.position.z));
}

/// OPENBLACK_TEST_SHIELD_REACTION=1: takes the town as "it wants protection and was just attacked", so the reaction
/// can be seen in game without attacking a town first. Off by default: the original's reads
bool TestForceTownGates()
{
	static const bool forced = [] {
		const char* value = std::getenv("OPENBLACK_TEST_SHIELD_REACTION");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return forced;
}

/// The town's desire for protection (GetDesireSignificanceToVillager, TOWN_DESIRE_FOR_PROTECTION): the sum of its
/// inputs minus the desire's trigger, at least 0 (ecs::town_desire)
float ProtectionDesireSignificance(entt::entity town)
{
	if (TestForceTownGates())
	{
		return 1.0f;
	}
	return town_desire::GetDesireSignificanceToVillager(town, TownDesireInfo::ForProtection);
}

/// The turns since the town's last aggressor against the villager's own info's
/// numGameTurnsAfterAggressionInterestedInShield, unsigned
bool AttackedRecently(entt::entity villager, entt::entity town)
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
	const uint32_t since = effects::reactions::Turn() - component->aggression.lastTurn;
	return since <= villager::InfoOf(villager).numGameTurnsAfterAggressionInterestedInShield;
}

/// From the villager's reaction records: may it react to this type again?
bool MayReactAgain(entt::entity villager, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield), again,
	                                   effects::reactions::Turn());
}

/// Done by AddReaction when the villager had no reaction: the final state goes to index 2, unless it is a passing one
/// (two flags of the state table)
void StorePreviousState(LivingAction& action)
{
	const auto final = FinalState(action);
	const auto* info = TableOf(final);
	auto stored = final;
	if (info != nullptr && (info->keepsPreviousState != 0 || info->isReactionState != 0))
	{
		stored = Get(action, LivingAction::Index::Previous);
	}
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(stored);
}

/// The same point both times it is set: the villager's own position plus 1 m along the bearing from the shield's
/// centre to it, jittered by GameFloatRand(pi / 4) - pi / 8. It is what LookAtPos turns towards, so the villager ends
/// up facing away from the dome
glm::ivec2 LookAtPoint(entt::entity villager, entt::entity spell)
{
	const auto me = tq::PosOf(villager);
	const float bearing = tq::Get3DAngleFromXZ(ShieldPos(spell), me);
	const float angle = villager_shield::JitteredBearing(bearing, villager::GameFloatRand(std::numbers::pi_v<float> / 4.0f));
	return me + tq::GetPosFromAngle(angle, 1.0f);
}

/// The state's clip is kept for GameFloatRand(10) + 20 seconds (as many loops as fit: the loop count asked for is that
/// time x 1000 / the clip's duration in ms) and then rolled again (SetAnim with the state's clip, the AmazedByShield
/// set of ECS/VillagerAnimations). The one exception: while the clip is INTO_POINTING (286) it only plays once and is
/// followed by TALKING_AND_POINTING (395)
void UpdateAmazedClip(entt::entity villager, LivingAction& action)
{
	// GameFloatRand(10) + 20 seconds, drawn every turn the state runs (before anything else here, so
	// the random sequence does not depend on openblack's clip guards below)
	const float seconds = villager::GameFloatRand(10.0f) + 20.0f;
	auto* animation = Reg().TryGet<const SkeletalAnimation>(villager);
	if (animation == nullptr || !animation->hasClip)
	{
		return;
	}
	const auto playTime = villager_clips::ClipMilliseconds(animation->clip);
	if (!playTime.has_value())
	{
		return;
	}
	const auto duration = static_cast<int32_t>(*playTime);
	if (duration <= 0)
	{
		return;
	}
	// those seconds x 1000 / the duration of the clip it has
	auto loops = static_cast<int32_t>(seconds * 1000.0f / static_cast<float>(duration));
	constexpr int32_t k_IntoPointing = 286;       ///< AnimPack entry
	constexpr int32_t k_TalkingAndPointing = 395; ///< AnimPack entry
	int32_t clip = -1;                            ///< < 0: roll the state's clip again
	if (animation->clipIndex == k_IntoPointing)
	{
		loops = 1;
		clip = k_TalkingAndPointing;
	}
	// ready for a new clip: turns in the state x the ms per turn (100) >= loops x the duration (loops is never
	// negative: the seconds and the duration are positive)
	if (!villager_clips::ClipPlayed(action, animation->clip, static_cast<uint32_t>(loops)))
	{
		return;
	}
	if (clip < 0)
	{
		villager_animate::PlayStateClip(villager, true); // the state's clip again, from the start
	}
	else
	{
		villager_animate::SetAnim(villager, clip, true); // SetAnim(395, 1)
	}
	action.turnsSinceStateChange = 0;
}

/// The REACT_TO_MAGIC_SHIELD part of applying a reaction to the living objects of a cell, for one villager, as the
/// fire's and the teleport's: with no reaction of its own, a reaction score above 0 and not reacted to a shield within
/// TurnsBeforeReactingToShieldAgain, it starts reacting (the reaction's turn is stamped, then StartReacting ->
/// SetupReactToMagicShield). A villager that follows a reaction is not offered it here: the switch to a better one is
/// VillagerReactions.cpp's
void ApplyShieldReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	// the check made for every reaction type: IsAvailableForReaction, the whole one (a dying villager, one at its
	// worship site or one too hurt is not pulled out for it). The one that started the reaction is not turned away
	if (!registry.AllOf<Villager, LivingAction>(villager) ||
	    !villager::IsAvailableForReaction(villager, openblack::Reaction::ReactToMagicShield))
	{
		return;
	}
	if (villager_reactions::IsReacting(villager))
	{
		return; // it already follows a reaction
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
	// the type's own turns before reacting again, at the spread's distance: never again for a town not attacked
	// lately, at once when the dome does not cover the villager's goal, else the standard turns
	const auto again = villager_shield::TurnsBeforeReactingToShieldAgain(villager, reaction.initiator, distance);
	// the reaction's score (ECS/Effects/Reactions)
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield), true,
	                                             villager_shield::ReactToMagicShieldPriority(villager, reaction.id), distance);
	if (score == 0 || !MayReactAgain(villager, again))
	{
		return;
	}
	effects::reactions::MarkStarted(reaction.id, effects::reactions::Turn()); // stamps the turn if not yet set
	villager_shield::SetupReactToMagicShield(villager, reaction.initiator, reaction.id);
}
} // namespace

uint8_t villager_shield::ReactToMagicShieldPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !IsAvailableShieldSpell(found->initiator))
	{
		return 0; // not a SpellShield, or not available
	}
	// the original computes the distance to the reaction here and drops it
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return static_cast<uint8_t>(ShieldReactionInfo().priority & 0xFFu); // no town: the priority's low byte
	}
	// the town's desire for protection must not be 0
	if (ProtectionDesireSignificance(town) == 0.0f)
	{
		return 0;
	}
	if (!AttackedRecently(villager, town))
	{
		return 0;
	}
	return static_cast<uint8_t>(ShieldReactionInfo().priority & 0xFFu);
}

float villager_shield::JitteredBearing(float bearing, float draw)
{
	constexpr float k_HalfRange = std::numbers::pi_v<float> / 8.0f;
	const float turn = draw - k_HalfRange;
	return turn + bearing;
}

uint8_t villager_shield::StruckShieldPriority(bool shieldSpell, bool amazedByShield, glm::ivec2 lookAt, glm::ivec2 shieldPos,
                                              uint8_t priority)
{
	// a shield spell, a villager amazed by a shield, and a look-at point other than the struck shield's position
	if (!shieldSpell || !amazedByShield || lookAt == shieldPos)
	{
		return 0;
	}
	return priority;
}

uint8_t villager_shield::ReactToMagicShieldStruckPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	const auto* action = ActionOf(villager);
	const auto* state = FindState(villager);
	if (found == nullptr || action == nullptr || state == nullptr)
	{
		return 0;
	}
	const bool shield = IsShieldSpell(found->initiator);
	return StruckShieldPriority(shield, FinalState(*action) == VillagerStates::AmazedByMagicShieldReaction, state->lookAt,
	                            shield ? ShieldPos(found->initiator) : glm::ivec2(0),
	                            static_cast<uint8_t>(StruckReactionInfo().priority & 0xFFu));
}

void villager_shield::SetupReactToMagicShieldStruck(entt::entity villager, entt::entity spell, uint32_t reaction)
{
	if (ActionOf(villager) == nullptr || !IsShieldSpell(spell))
	{
		return; // not a SpellShield, nothing happens
	}
	// one more follower of the shield's own reaction, how impressed it is by the struck one, then the shield, its
	// REACT_TO_MAGIC_SHIELD and the shield's position as where it looks
	const auto* data = Reg().TryGet<const magic::SpellShieldData>(spell);
	const uint32_t followed = data != nullptr ? data->shieldReaction : 0;
	villager_reactions::UpdateHowImpressed(villager, reaction, true);
	auto& state = StateOf(villager);
	state.spell = spell;
	state.reaction = followed;
	state.lookAt = ShieldPos(spell);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("SetupReactToMagicShieldStruck: spell {} follows {}",
		                                      static_cast<uint32_t>(spell), followed));
	}
}

void villager_shield::ApplyStruckReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	const auto type = openblack::Reaction::ReactToMagicShieldStruck;
	if (!registry.AllOf<Villager, LivingAction>(villager) || !villager::IsAvailableForReaction(villager, type) ||
	    villager_reactions::IsReacting(villager))
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(villager);
	const auto* spell = registry.TryGet<const Spell>(reaction.initiator);
	if (transform == nullptr || spell == nullptr)
	{
		return;
	}
	// the spread's distance, its score and the standard turns before reacting again
	const float distance =
	    0.5f * (std::abs(transform->position.x - spell->position.x) + std::abs(transform->position.z - spell->position.z));
	const auto t = static_cast<uint8_t>(type);
	const auto score = villager_reactions::ScoreOf(villager, reaction.id, type, distance, 0);
	if (score == 0 ||
	    !effects::reactions::Records(villager, t, effects::reactions::StandardTurnsBeforeReactingAgain(t, distance),
	                                 effects::reactions::Turn()))
	{
		return;
	}
	effects::reactions::MarkStarted(reaction.id, effects::reactions::Turn());
	SetupReactToMagicShieldStruck(villager, reaction.initiator, reaction.id);
}

bool villager_shield::TownAttackedLately(entt::entity villager)
{
	const auto town = TownEntityOf(villager);
	return town != entt::null && AttackedRecently(villager, town);
}

void villager_shield::SetupReactToMagicShield(entt::entity villager, entt::entity spell, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || !IsAvailableShieldSpell(spell))
	{
		return; // not a SpellShield, nothing happens
	}
	// AddReaction(reaction, 168): how impressed it is, then the reaction and the spell are kept and the final state
	// stored, only for a villager that follows no reaction yet (a switch keeps the stored one)
	villager_reactions::UpdateHowImpressed(villager, reaction, true);
	const bool reacting = villager_reactions::IsReacting(villager);
	{
		auto& state = StateOf(villager);
		state.spell = spell;
		state.reaction = reaction;
		state.joinOrder = openblack::ecs::effects::reactions::NextJoinOrder();
	}
	if (!reacting)
	{
		StorePreviousState(*action);
	}
	villager_reactions::SetTopState(villager, VillagerStates::AmazedByMagicShieldReaction);
	const float radius = RadiusOf(spell);
	const auto shield = ShieldPos(spell);
	const auto me = tq::PosOf(villager);
	// already under the shield (minus 0.2 R) -> no walk
	const glm::vec3 at(tq::ToMetres(me).x, 0.0f, tq::ToMetres(me).y);
	if (!magic::spell_shield::IsUnder(spell, at, 0.2f * radius))
	{
		// the bearing from the shield to the villager, + GameFloatRand(pi / 4) - pi / 8
		const float bearing = tq::Get3DAngleFromXZ(shield, me);
		const float angle =
		    villager_shield::JitteredBearing(bearing, villager::GameFloatRand(std::numbers::pi_v<float> / 4.0f));
		// (R - 0.2 R) x GameFloatRand(1)^3, taken off that distance
		const float band = radius - 0.2f * radius;
		const float r = villager::GameFloatRand(1.0f);
		const float distance = band - band * (r * r * r);
		// SetupMoveToWithHug(the reaction's pos + GetPosFromAngle(angle, distance), 168)
		const auto goal = tq::ToMetres(shield + tq::GetPosFromAngle(angle, distance));
		villager::SetupMoveToWithHug(villager, goal, VillagerStates::AmazedByMagicShieldReaction);
	}
	// the look-at point, walking or not
	// the state is looked up again: the calls above may have removed another villager's, which moves the storage
	if (auto* state = FindState(villager))
	{
		state->lookAt = LookAtPoint(villager, spell);
	}
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
	const auto* state = FindState(villager);
	const auto town = TownEntityOf(villager);
	const auto spell = state != nullptr ? state->spell : entt::entity(entt::null);
	// a town, an available SpellShield, and the town's desire for protection above 0
	if (town == entt::null || !IsAvailableShieldSpell(spell) || !(ProtectionDesireSignificance(town) > 0.0f))
	{
		// wait GameRand(60) + 20 turns, then DECIDE_WHAT_TO_DO
		const auto turns = static_cast<uint16_t>(static_cast<float>(villager::GameRand(60)) + 20.0f);
		villager::SetupWaitForCounter(villager, turns, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// turn towards the look-at point; once it is facing that way, one chance in four of a new point
	if (villager::LookAtPos(villager, state->lookAt, 0) != 0 && villager::GameRand(4) == 0)
	{
		// looked up again, as in SetupReactToMagicShield
		if (auto* again = FindState(villager))
		{
			again->lookAt = LookAtPoint(villager, spell);
		}
	}
	UpdateAmazedClip(villager, action);
	return 1;
}

void villager_shield::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	ApplyShieldReaction(villager, reaction);
}

uint32_t villager_shield::TurnsBeforeReactingToShieldAgain(bool hasTown, bool attackedRecently, bool shield, bool goalUnder,
                                                           uint32_t standard)
{
	constexpr uint32_t k_Forever = 0x7FFFFFFFu;
	// a town not attacked lately: never again
	if (hasTown && !attackedRecently)
	{
		return k_Forever;
	}
	// a shield whose dome does not cover where the villager is going: at once
	if (shield && !goalUnder)
	{
		return 0;
	}
	return standard;
}

uint32_t villager_shield::TurnsBeforeReactingToShieldAgain(entt::entity villager, entt::entity thing, float distance)
{
	auto& registry = Reg();
	const auto town = TownEntityOf(villager);
	const bool hasTown = town != entt::null;
	const bool attacked = hasTown && AttackedRecently(villager, town);
	// the thing is a shield spell (its class, whether or not it is still available)
	const auto* spell = registry.Valid(thing) ? registry.TryGet<const Spell>(thing) : nullptr;
	const bool shield = spell != nullptr && spell->spellClass == SpellClass::Shield;
	bool under = false;
	if (shield)
	{
		// where the villager is going (its walk's goal), with no margin
		const auto* wallHug = registry.TryGet<const WallHug>(villager);
		const glm::vec2 goal = wallHug != nullptr ? wallHug->goal : tq::ToMetres(tq::PosOf(villager));
		under = magic::spell_shield::IsUnder(thing, glm::vec3(goal.x, 0.0f, goal.y), 0.0f);
	}
	const auto type = static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield);
	return TurnsBeforeReactingToShieldAgain(hasTown, attacked, shield, under,
	                                        effects::reactions::StandardTurnsBeforeReactingAgain(type, distance));
}

uint32_t villager_shield::TurnsToReactToShield(bool hasTown, uint32_t turnsSinceAttack, uint32_t interestedTurns,
                                               uint32_t standard, const std::function<uint32_t(uint32_t)>& gameRand)
{
	constexpr uint32_t k_Forever = 0x7FFFFFFFu;
	constexpr uint32_t k_InterestChance = 4; ///< one draw in this many keeps only the standard turns
	constexpr uint32_t k_InterestTurns = 50; ///< the most turns the second draw adds to those since the attack
	// with a town, three times in four: for ever while its last attack is recent enough
	if (hasTown && gameRand(k_InterestChance) != 0 && gameRand(k_InterestTurns) + turnsSinceAttack < interestedTurns)
	{
		return k_Forever;
	}
	return standard;
}

uint32_t villager_shield::TurnsToReactToShield(entt::entity villager, float distance)
{
	const auto town = TownEntityOf(villager);
	const auto* component = town != entt::null ? Reg().TryGet<const Town>(town) : nullptr;
	// (guard) a town entity with no town data counts as none
	const bool hasTown = component != nullptr;
	// the turns since the town's last attack; none under OPENBLACK_TEST_SHIELD_REACTION (taken as just attacked)
	const uint32_t since = hasTown && !TestForceTownGates() ? effects::reactions::Turn() - component->aggression.lastTurn : 0u;
	const auto type = static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield);
	return TurnsToReactToShield(hasTown, since, villager::InfoOf(villager).numGameTurnsAfterAggressionInterestedInShield,
	                            effects::reactions::StandardTurnsToReact(type, distance),
	                            [](uint32_t n) { return villager::GameRand(n); });
}

void villager_shield::ProcessReaction(entt::entity villager)
{
	const auto* state = FindState(villager);
	if (state == nullptr || state->reaction == 0)
	{
		return; // no shield reaction
	}
	// the reaction gone -> StopReacting
	if (!effects::reactions::IsAvailable(effects::reactions::Find(state->reaction)))
	{
		villager_reactions::StopReacting(villager);
		return;
	}
	// the spell gone -> StopReactingAndSetState
	const auto spell = state->spell;
	if (!IsAvailableShieldSpell(spell))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return;
	}
	// the turns since it started reacting to a shield against its turns to react, at its distance to the spell,
	// compared signed
	const auto type = static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield);
	const auto elapsed = static_cast<int32_t>(effects::reactions::Turn() - effects::reactions::RecordTurn(villager, type));
	const float distance = tq::GetDistanceInMetres(tq::PosOf(villager), ShieldPos(spell));
	const auto turns = static_cast<int32_t>(TurnsToReactToShield(villager, distance));
	if (elapsed > turns)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("shield reaction: {} turns > {} -> StopReactingAndSetState", elapsed, turns));
		}
		villager_reactions::StopReactingAndSetState(villager);
	}
}

bool villager_shield::IsReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr && state->reaction != 0;
}

entt::entity villager_shield::ReactionObject(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr ? state->spell : entt::entity(entt::null);
}

bool villager_shield::IsReactionObjectAvailable(entt::entity villager)
{
	return IsAvailableShieldSpell(ReactionObject(villager));
}

bool villager_shield::IsShieldSpellAvailable(entt::entity spell)
{
	return IsAvailableShieldSpell(spell);
}

void villager_shield::StopReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	if (state == nullptr)
	{
		return;
	}
	// with a reaction, its record gets the turn; the reaction and its spell go
	if (state->reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield),
		                                  effects::reactions::Turn());
	}
	Reg().RemoveState<VillagerShieldState>(villager);
}

void villager_shield::Clear()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Reg();
		std::vector<entt::entity> owners;
		registry.Each<const VillagerShieldState>(
		    [&owners](entt::entity villager, const VillagerShieldState&) { owners.push_back(villager); });
		for (const auto villager : owners)
		{
			registry.RemoveState<VillagerShieldState>(villager);
		}
	}
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}
