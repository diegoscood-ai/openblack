/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureWatching.h"

#include <algorithm>

#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_watching;

namespace
{
/// Short of learning a skill, the meter shows it half learnt
constexpr float k_SkillMeterShare = 0.5f;
/// A miracle seen this many times or more beyond those it needed no longer fills the learning meter
constexpr float k_LearntMeterSightings = 3.0f;
/// Seen this share of the times it needs, a miracle is nearly learnt
constexpr float k_NearlyLearntShare = 0.75f;

/// How many finished actions end a stage: the table's count and a draw of random(1), drawn as the stage begins
uint32_t StageLimit(const MimicRule& rule, const std::function<uint32_t(uint32_t)>& random)
{
	return rule.stageSteps + random(1);
}
} // namespace

Knowledge creature_watching::StartKnowledge(std::span<const SkillRule> skills, std::span<const MiracleRule> miracles)
{
	Knowledge knowledge {
	    .skillsSeen = std::vector<Sighting>(skills.size()),
	    .miraclesSeen = std::vector<Sighting>(miracles.size()),
	    .skillsKnown = std::vector<bool>(skills.size(), false),
	    .miraclesKnown = std::vector<bool>(miracles.size(), false),
	};
	return knowledge;
}

Progress creature_watching::SeeSkill(Knowledge& knowledge, size_t skill, std::span<const SkillRule> rules, uint32_t phase,
                                     uint32_t turn, float turnsPerSecond)
{
	if (skill >= rules.size() || skill >= knowledge.skillsSeen.size() || skill >= knowledge.skillsKnown.size())
	{
		return {.ignored = true};
	}
	// Too young to learn it, also when it is known already: the creature is told it can't learn it yet
	if (phase < rules[skill].minPhase)
	{
		return {.ignored = true};
	}
	if (knowledge.skillsKnown[skill])
	{
		return {.learnt = true, .share = 1.0f, .ignored = true};
	}
	auto& seen = knowledge.skillsSeen[skill];
	++seen.count;
	if (!seen.turn.has_value())
	{
		seen.turn = turn;
	}
	const auto seconds = static_cast<float>(turn - *seen.turn) / std::max(turnsPerSecond, 1.0f);
	if (seconds >= rules[skill].watchSeconds)
	{
		MarkKnown(knowledge.skillsKnown, knowledge.skillsLearnt, static_cast<uint32_t>(skill), true);
		return {.learnt = true, .share = 1.0f};
	}
	return {.share = k_SkillMeterShare};
}

bool creature_watching::WatchesVillagers(uint32_t phase, bool controlledByScript,
                                         std::optional<creature_desires::Desire> planDesire)
{
	if (phase < k_MinVillagerWatchPhase || controlledByScript)
	{
		return false;
	}
	if (!planDesire.has_value())
	{
		return true;
	}
	using creature_desires::Desire;
	switch (*planDesire)
	{
	case Desire::Impress:
	case Desire::Compassion:
	case Desire::Anger:
	case Desire::Play:
	case Desire::Hunger:
	case Desire::Fear:
	case Desire::Poo:
	case Desire::Tiredness:
	case Desire::Puke:
	case Desire::Water:
	case Desire::RestoreHealth:
	case Desire::BeFriends:
	case Desire::GetWarmer:
	case Desire::GetColder:
	case Desire::Scratch:
	case Desire::RunAwayFromPlayer:
	case Desire::Rest:
	case Desire::Illness:
	case Desire::ObeyCreature:
	case Desire::Steal:
		return false;
	default:
		return true;
	}
}

bool creature_watching::RaisesWishToImpress(PlayerNames townOwner, PlayerNames creatureOwner)
{
	return townOwner == PlayerNames::NEUTRAL || townOwner != creatureOwner;
}

bool creature_watching::CopiesAngerAtOwnPlayer(uint32_t deedDesire, std::optional<PlayerNames> thingPlayer,
                                               PlayerNames creaturePlayer)
{
	return deedDesire == static_cast<uint32_t>(creature_desires::Desire::Anger) && thingPlayer.has_value() &&
	       *thingPlayer == creaturePlayer;
}

VillagerStates creature_watching::StateShown(VillagerStates top, VillagerStates final, float metresToGoal)
{
	const auto state = top == VillagerStates::WaitForAnimation ? final : top;
	if (state == VillagerStates::GotoStoragePitForFood && metresToGoal > k_FarFromStoragePitMetres)
	{
		return VillagerStates::DecideWhatToDo;
	}
	return state;
}

float creature_watching::TimesNeeded(uint32_t timesToSee, float speciesMultiplier)
{
	// The product of a 32-bit count and a float is exact in a double for any count below 2^29, so this rounds once
	return static_cast<float>(static_cast<double>(timesToSee) * static_cast<double>(speciesMultiplier));
}

std::optional<Prerequisite> creature_watching::MiraclePrerequisite(size_t miracle)
{
	using Kind = Prerequisite::Kind;
	switch (miracle)
	{
	case 2: // the fireball's power-ups
	case 3:
		return Prerequisite {.kind = Kind::Miracle, .index = 1};
	case 5: // the lightning bolt's
	case 6:
		return Prerequisite {.kind = Kind::Miracle, .index = 4};
	case 8: // the explosion's first power-up, then its second
		return Prerequisite {.kind = Kind::Miracle, .index = 7};
	case 9:
		return Prerequisite {.kind = Kind::Miracle, .index = 8};
	case 11: // the heal's
		return Prerequisite {.kind = Kind::Miracle, .index = 10};
	case 17: // the storm with lightning, then the tornado
		return Prerequisite {.kind = Kind::Miracle, .index = 16};
	case 18:
		return Prerequisite {.kind = Kind::Miracle, .index = 17};
	case 40: // the thirst and itch spells need the skill of building
	case 41:
		return Prerequisite {.kind = Kind::Skill, .index = 0};
	default:
		return std::nullopt;
	}
}

Progress creature_watching::SeeMiracle(Knowledge& knowledge, size_t miracle, std::span<const MiracleRule> rules, uint32_t phase,
                                       uint32_t turn, uint32_t weight, float speciesMultiplier)
{
	if (miracle >= rules.size() || miracle >= knowledge.miraclesSeen.size() || miracle >= knowledge.miraclesKnown.size())
	{
		return {.ignored = true};
	}
	if (const auto needs = MiraclePrerequisite(miracle))
	{
		const auto& known = needs->kind == Prerequisite::Kind::Skill ? knowledge.skillsKnown : knowledge.miraclesKnown;
		if (needs->index >= known.size() || !known[needs->index])
		{
			return {.ignored = true};
		}
	}
	const auto& rule = rules[miracle];
	if (phase < rule.minPhase)
	{
		return {.ignored = true, .event = LearningEvent::TooYoung};
	}
	auto& seen = knowledge.miraclesSeen[miracle];
	// A sighting soon after the last doesn't count, but is remembered as the last; a last turn of 0 is never seen
	if (!seen.turn.has_value() || *seen.turn == 0 || turn - *seen.turn > k_MiracleSightingTurns)
	{
		seen.count += weight;
	}
	seen.turn = turn;
	MarkKnown(knowledge.miraclesKnown, knowledge.miraclesLearnt, static_cast<uint32_t>(miracle), true);
	const auto needed = TimesNeeded(rule.timesToSee, speciesMultiplier);
	const auto count = static_cast<float>(seen.count);
	if (needed <= count)
	{
		// The meter shows it full for the first few sightings beyond those needed
		Progress progress {.learnt = true, .share = 1.0f, .event = LearningEvent::Learnt};
		if (count - needed < k_LearntMeterSightings)
		{
			progress.meter = 1.0f;
		}
		return progress;
	}
	const float share = count / needed;
	return {.share = share,
	        .event = share >= k_NearlyLearntShare ? std::optional(LearningEvent::NearlyLearnt) : std::nullopt,
	        .meter = share};
}

uint32_t creature_watching::TaughtSightings(uint32_t timesToSee, float speciesMultiplier)
{
	// The float product the sightings are measured against, cut to a whole number towards zero
	return static_cast<uint32_t>(static_cast<int64_t>(TimesNeeded(timesToSee, speciesMultiplier)));
}

bool creature_watching::Knows(const Knowledge& knowledge, uint32_t kind, uint32_t action)
{
	if (kind == static_cast<uint32_t>(CreatureActionLearningType::Normal))
	{
		return action < knowledge.skillsKnown.size() && knowledge.skillsKnown[action];
	}
	return kind == static_cast<uint32_t>(CreatureActionLearningType::Magic) && action < knowledge.miraclesKnown.size() &&
	       knowledge.miraclesKnown[action];
}

void creature_watching::MarkKnown(std::vector<bool>& known, std::vector<uint32_t>& learnt, uint32_t action, bool knows)
{
	// The game's list of what it knows takes any action number, past the tables too; one it does not know is absent
	if (action >= known.size())
	{
		if (!knows)
		{
			return;
		}
		known.resize(static_cast<size_t>(action) + 1, false);
	}
	const bool listed = std::ranges::find(learnt, action) != learnt.end();
	if (knows && !listed)
	{
		learnt.push_back(action);
	}
	else if (!knows && listed)
	{
		std::erase(learnt, action);
	}
	known[action] = knows;
}

bool creature_watching::SetKnows(Knowledge& knowledge, uint32_t kind, uint32_t action, bool knows,
                                 std::span<const MiracleRule> miracles, float speciesMultiplier, bool countsSightings)
{
	if (kind == static_cast<uint32_t>(CreatureActionLearningType::Normal))
	{
		MarkKnown(knowledge.skillsKnown, knowledge.skillsLearnt, action, knows);
		return true;
	}
	if (kind != static_cast<uint32_t>(CreatureActionLearningType::Magic))
	{
		return false;
	}
	MarkKnown(knowledge.miraclesKnown, knowledge.miraclesLearnt, action, knows);
	// Its sightings are set only for a miracle of the tables, the ones the game counts
	if (knows && countsSightings && action < miracles.size() && action < knowledge.miraclesSeen.size())
	{
		knowledge.miraclesSeen[action].count = TaughtSightings(miracles[action].timesToSee, speciesMultiplier);
	}
	return true;
}

const char* creature_watching::Name(MimicStage stage)
{
	switch (stage)
	{
	case MimicStage::Notice:
		return "noticing";
	case MimicStage::CopyAction:
		return "copying the action";
	case MimicStage::CopyDesire:
		return "copying the desire";
	}
	return "?";
}

bool creature_watching::WithinCopyReach(float distance, uint32_t sweepCells)
{
	return distance < static_cast<float>(sweepCells);
}

creature_watching::CastDeedStep creature_watching::CastDeedGate(uint32_t sightings, uint32_t timesToSee,
                                                                float speciesMultiplier)
{
	// The times needed are rounded to a float, as the game's own count of them is; less the sightings, then cut
	// towards zero
	const float needed = static_cast<float>(timesToSee) * speciesMultiplier;
	const auto stillNeeded =
	    static_cast<int64_t>(static_cast<double>(needed) - static_cast<double>(static_cast<float>(sightings)));
	if (stillNeeded > 1)
	{
		return CastDeedStep::CountAndStop;
	}
	if (stillNeeded == 1)
	{
		return CastDeedStep::CountThenRoll;
	}
	return CastDeedStep::Copy;
}

bool creature_watching::MayStartMimicry(const std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                        const MimicConditions& conditions)
{
	if (rule >= rules.size())
	{
		return false;
	}
	const auto& wanted = rules[rule];
	if (conditions.lockedSelect || !conditions.leashWorks || conditions.computerPlayer || conditions.phase < k_MinMimicPhase ||
	    (wanted.needsLearningLeash && !conditions.learningLeashInHand) ||
	    conditions.reactionPriority >= k_MaxReactionPriorityToMimic || !conditions.canSee)
	{
		return false;
	}
	if (!mimicry.has_value())
	{
		return wanted.chance > 0.0f;
	}
	return mimicry->stage == MimicStage::CopyDesire || mimicry->rule >= rules.size() ||
	       (mimicry->rule != rule && wanted.chance >= rules[mimicry->rule].chance);
}

void creature_watching::BeginMimicry(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                     std::optional<uint32_t> object, uint32_t turn,
                                     const std::function<uint32_t(uint32_t)>& random)
{
	if (rule >= rules.size())
	{
		return;
	}
	mimicry = Mimicry {.rule = rule,
	                   .stage = MimicStage::Notice,
	                   .progress = 0,
	                   .limit = StageLimit(rules[rule], random),
	                   .stageStarted = turn,
	                   .object = object};
}

void creature_watching::CopyDeedAgain(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                      std::optional<uint32_t> object, uint32_t turn,
                                      const std::function<uint32_t(uint32_t)>& random)
{
	BeginMimicry(mimicry, rule, rules, object, turn, random);
	if (!mimicry.has_value())
	{
		return;
	}
	mimicry->progress = mimicry->limit / 2;
	mimicry->stage = MimicStage::CopyAction;
}

bool creature_watching::StartMimicry(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                     const MimicConditions& conditions, std::optional<uint32_t> object, uint32_t turn,
                                     const std::function<uint32_t(uint32_t)>& random)
{
	if (!MayStartMimicry(mimicry, rule, rules, conditions))
	{
		return false;
	}
	BeginMimicry(mimicry, rule, rules, object, turn, random);
	return true;
}

void creature_watching::CountFinishedAction(std::optional<Mimicry>& mimicry)
{
	if (mimicry.has_value())
	{
		++mimicry->progress;
	}
}

std::optional<MimicStage> creature_watching::ProcessMimicry(std::optional<Mimicry>& mimicry, std::span<const MimicRule> rules,
                                                            uint32_t turn, uint32_t timeoutTurns,
                                                            const std::function<uint32_t(uint32_t)>& random)
{
	if (!mimicry.has_value() || mimicry->rule >= rules.size())
	{
		mimicry.reset();
		return std::nullopt;
	}
	// the turns gone are counted as unsigned whole numbers
	if (mimicry->progress < mimicry->limit && (mimicry->stage != MimicStage::Notice || mimicry->progress < 1) &&
	    turn - mimicry->stageStarted <= timeoutTurns)
	{
		return mimicry->stage;
	}
	const auto& rule = rules[mimicry->rule];
	if (mimicry->stage == MimicStage::Notice || (mimicry->stage == MimicStage::CopyAction && rule.copiesDesire))
	{
		mimicry->stage = mimicry->stage == MimicStage::Notice ? MimicStage::CopyAction : MimicStage::CopyDesire;
		mimicry->progress = 0;
		mimicry->limit = StageLimit(rule, random);
		mimicry->stageStarted = turn;
		return std::nullopt;
	}
	mimicry.reset();
	return std::nullopt;
}

void creature_watching::StrokedWhileMimicking(std::optional<Mimicry>& mimicry, std::span<const MimicRule> rules, uint32_t drawn)
{
	if (!mimicry.has_value() || mimicry->rule >= rules.size())
	{
		return;
	}
	mimicry->stage = MimicStage::CopyAction;
	mimicry->progress = 0;
	mimicry->limit = rules[mimicry->rule].stageSteps + drawn;
}
