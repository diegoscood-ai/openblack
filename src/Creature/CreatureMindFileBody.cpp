/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindFileBody.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <MindFile.h>
#include <PhysiqueFile.h>

#include "Creature/CreatureMorph.h"

using namespace openblack;
using namespace openblack::creature_mind_body;

std::optional<CreatureType> creature_mind_body::SpeciesFromRow(uint32_t row)
{
	if (row == 0)
	{
		return CreatureType::GiantApe;
	}
	if (row < creaturemind::k_SpeciesRows)
	{
		return static_cast<CreatureType>(row);
	}
	return std::nullopt;
}

std::optional<Body> creature_mind_body::FromMindFile(const creaturemind::MindFileData& file)
{
	const auto species = SpeciesFromRow(file.speciesRow);
	if (!species.has_value())
	{
		return std::nullopt;
	}
	return FromMindFile(file, *species);
}

Body creature_mind_body::FromMindFile(const creaturemind::MindFileData& file, CreatureType species)
{
	Body body {.species = species,
	           .name = file.name,
	           .fatness = file.physique.fatness,
	           .previousFatness = file.physique.previousFatness,
	           .needs = {.turns = file.physique.turns,
	                     .age = file.physique.age,
	                     .energy = file.physique.energy,
	                     .itchiness = file.physique.itchiness,
	                     .poo = file.physique.needs[0],
	                     .exhaustion = file.physique.needs[1],
	                     .dehydration = file.physique.needs[2]}};
	const auto finite = [](float value) { return std::isfinite(value); };
	if (file.alignment.has_value() && finite(*file.alignment))
	{
		body.alignment = std::clamp(*file.alignment, -1.0f, 1.0f);
	}
	if (finite(file.physique.strength))
	{
		body.strength = std::clamp(file.physique.strength, 0.0f, 1.0f);
	}
	if (file.physique.size.has_value() && finite(*file.physique.size) && *file.physique.size > 0.0f)
	{
		body.size = std::clamp(*file.physique.size, creature_morph::k_MinScale, creature_morph::k_MaxScale);
	}
	if (file.tattooSlots.has_value())
	{
		creature_tattoo::Slots slots {};
		std::ranges::transform(*file.tattooSlots, slots.begin(), creature_tattoo::FromWord);
		body.tattoos = slots;
	}
	return body;
}

float creature_mind_body::LoadedShownFatness(float previousFatness, float fatness)
{
	// the turn's step (creature_morph::EaseFatness); below a lower limit, or not a number, gives that limit, as the
	// game's comparisons do, so a step that is not a number is the step down
	const auto bringUpToDate = [fatness](float shown) {
		const auto next = std::isnan(fatness - shown) ? shown - creature_morph::k_MaxFatnessStep
		                                              : creature_morph::EaseFatness(shown, fatness);
		if (!(next >= 0.0f))
		{
			return 0.0f;
		}
		return next > 1.0f ? 1.0f : next;
	};
	return bringUpToDate(bringUpToDate(previousFatness));
}

creature_physiology::Needs creature_mind_body::WithSavedNeeds(creature_physiology::Needs needs, const SavedNeeds& saved)
{
	needs.turns = saved.turns;
	needs.age = saved.age;
	needs.energy = saved.energy;
	if (saved.itchiness.has_value())
	{
		needs.itchiness = *saved.itchiness;
	}
	needs.poo = saved.poo;
	needs.exhaustion = saved.exhaustion;
	needs.dehydration = saved.dehydration;
	return needs;
}

creaturemind::MindFileData creature_mind_body::ToMindFile(creaturemind::MindFileData mind, const LiveBody& body)
{
	// which field of the body is the fatness: from bw_creature_editor.py, a creature-editor tool shared on the openblack
	// Discord
	mind.physique.fatness = body.fatness;
	mind.physique.previousFatness = body.shownFatness;
	if (body.needs.has_value())
	{
		const auto& needs = *body.needs;
		mind.physique.turns = needs.turns;
		mind.physique.age = needs.age;
		mind.physique.energy = needs.energy;
		mind.physique.itchiness = needs.itchiness;
		mind.physique.needs[0] = needs.poo;
		mind.physique.needs[1] = needs.exhaustion;
		mind.physique.needs[2] = needs.dehydration;
	}
	mind.inDevScript = body.inDevScript ? 1u : 0u;
	if (body.leashes.has_value())
	{
		auto flags = mind.leashFlags.value_or(std::array<uint32_t, 4> {});
		// a flag for each leash type, by the type's number
		flags[static_cast<size_t>(LeashType::Evil)] = body.leashes->evil ? 1u : 0u;
		flags[static_cast<size_t>(LeashType::Rope)] = body.leashes->rope ? 1u : 0u;
		flags[static_cast<size_t>(LeashType::Good)] = body.leashes->good ? 1u : 0u;
		mind.leashFlags = flags;
	}
	std::array<uint32_t, creature_tattoo::k_SlotCount> slots {};
	std::ranges::transform(body.tattoos, slots.begin(), creature_tattoo::ToWord);
	mind.tattooSlots = slots;
	mind.fightHealth = body.fightHealth;
	return mind;
}

creaturemind::PhysiqueFileData creature_mind_body::ToPhysique(const BodyNow& body,
                                                              const std::optional<creaturemind::PhysiqueFileData>& previous)
{
	creaturemind::PhysiqueFileData physique {
	    .speciesRow = body.speciesRow,
	    .size = creature_morph::ClampScale(body.size),
	    .strength = body.strength,
	    .fatness = body.fatness,
	    .alignment = body.alignment,
	};
	if (previous.has_value())
	{
		physique.listA = previous->listA;
		physique.listB = previous->listB;
	}
	return physique;
}
