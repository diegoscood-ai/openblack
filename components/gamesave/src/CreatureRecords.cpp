/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRecords.h"

using namespace openblack::gamesave;

namespace
{
/// The most queued spells that a read accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostQueued = 0x10000;

/// The creature's values hold no object part, so their visitors need no thing of their own
const ThingPart k_NoThing {};

/// The steps an agenda writes: from the current step to the count
[[nodiscard]] uint32_t StepsWritten(const KeptCreaturePlan& plan)
{
	return plan.count > plan.current ? plan.count - plan.current : 0u;
}

bool ReadPlan(FieldReader& fields, StreamReader& reader, KeptCreaturePlan& plan)
{
	if (!fields(plan.desire) || !fields(plan.action) || !fields(plan.objects[0]) || !fields(plan.objects[1]) ||
	    !fields(plan.objects[2]) || !fields(plan.firstWord) || !fields(plan.current) || !fields(plan.state) ||
	    !fields(plan.count) || !fields(plan.words[0]) || !fields(plan.words[1]) || !fields(plan.words[2]))
	{
		return false;
	}
	if (plan.count > k_MostAgendaSteps)
	{
		reader.Fail();
		return false;
	}
	plan.steps.resize(StepsWritten(plan));
	for (auto& step : plan.steps)
	{
		if (!fields(step))
		{
			return false;
		}
	}
	return fields(plan.creature) && fields(plan.other);
}

void WritePlan(FieldWriter& fields, const KeptCreaturePlan& plan)
{
	fields(plan.desire);
	fields(plan.action);
	for (const auto& object : plan.objects)
	{
		fields(object);
	}
	fields(plan.firstWord);
	fields(plan.current);
	fields(plan.state);
	fields(plan.count);
	for (const auto word : plan.words)
	{
		fields(word);
	}
	const auto steps = StepsWritten(plan);
	for (uint32_t i = 0; i < steps; ++i)
	{
		fields(i < plan.steps.size() ? plan.steps[i] : AgendaStep {});
	}
	fields(plan.creature);
	fields(plan.other);
}
} // namespace

bool openblack::gamesave::ReadPart(StreamReader& reader, CreatureFields& part, const ReadNewObject& readNew)
{
	FieldReader fields(reader, k_NoThing, readNew);
	if (!fields(part.version) || !fields(part.savedInTemple) || !fields(part.index) || !fields(part.owner))
	{
		return false;
	}
	for (auto& slot : part.spells)
	{
		if (!fields(slot))
		{
			return false;
		}
	}
	uint32_t queued = 0;
	if (!reader.Get(queued) || queued > k_MostQueued)
	{
		reader.Fail();
		return false;
	}
	part.queued.resize(queued);
	for (auto& spell : part.queued)
	{
		if (!fields(spell))
		{
			return false;
		}
	}
	if (part.version > k_LastCreatureVersionWithoutProcessing && !fields(part.mindProcessing))
	{
		return false;
	}
	if (!fields(part.largestSize) || !fields(part.smallestSize) || !fields(part.mayGoThroughVortex) ||
	    !fields(part.fizzedForGood) || !fields(part.fizz) || !fields(part.fizzTarget) || !fields(part.fizzPerSecond) ||
	    !fields(part.freeze) || !fields(part.word))
	{
		return false;
	}
	for (auto& visual : part.spellVisuals)
	{
		if (!fields(visual))
		{
			return false;
		}
	}
	for (auto& particles : part.handParticles)
	{
		if (!fields(particles))
		{
			return false;
		}
	}
	if (!fields(part.targetPoint) || !fields(part.targetParticles) || !fields(part.targetObject) || !fields(part.hasTarget) ||
	    !fields(part.arena) || !fields(part.home) || !fields(part.fightHealth) || !fields(part.autoFight) ||
	    !fields(part.dancePartners) || !fields(part.dance) || !fields(part.canDie) || !fields(part.strength) ||
	    !fields(part.size) || !fields(part.alignment) || !fields(part.animationPace) || !fields(part.autoScale) ||
	    !fields(part.autoScaleShare) || !fields(part.keepsPlan))
	{
		return false;
	}
	if (part.keepsPlan != 0 && !ReadPlan(fields, reader, part.plan))
	{
		return false;
	}
	for (auto& power : part.creedPowers)
	{
		if (!fields(power))
		{
			return false;
		}
	}
	return true;
}

void openblack::gamesave::WritePart(StreamWriter& writer, const CreatureFields& part, const WriteNewObject& writeNew)
{
	FieldWriter fields(writer, k_NoThing, writeNew);
	fields(part.version);
	fields(part.savedInTemple);
	fields(part.index);
	fields(part.owner);
	for (const auto& slot : part.spells)
	{
		fields(slot);
	}
	writer.Put(static_cast<uint32_t>(part.queued.size()));
	for (const auto& spell : part.queued)
	{
		fields(spell);
	}
	if (part.version > k_LastCreatureVersionWithoutProcessing)
	{
		fields(part.mindProcessing);
	}
	fields(part.largestSize);
	fields(part.smallestSize);
	fields(part.mayGoThroughVortex);
	fields(part.fizzedForGood);
	fields(part.fizz);
	fields(part.fizzTarget);
	fields(part.fizzPerSecond);
	fields(part.freeze);
	fields(part.word);
	for (const auto& visual : part.spellVisuals)
	{
		fields(visual);
	}
	for (const auto& particles : part.handParticles)
	{
		fields(particles);
	}
	fields(part.targetPoint);
	fields(part.targetParticles);
	fields(part.targetObject);
	fields(part.hasTarget);
	fields(part.arena);
	fields(part.home);
	fields(part.fightHealth);
	fields(part.autoFight);
	fields(part.dancePartners);
	fields(part.dance);
	fields(part.canDie);
	fields(part.strength);
	fields(part.size);
	fields(part.alignment);
	fields(part.animationPace);
	fields(part.autoScale);
	fields(part.autoScaleShare);
	fields(part.keepsPlan);
	if (part.keepsPlan != 0)
	{
		WritePlan(fields, part.plan);
	}
	for (const auto& power : part.creedPowers)
	{
		fields(power);
	}
}
