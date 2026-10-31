/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureScriptPlay.h"

using namespace openblack;
using namespace openblack::creature_script_play;

std::vector<creature_mind::Step> creature_script_play::Agenda(const Request& request)
{
	const bool individual = request.animation >= k_FirstIndividualAction;
	// The body has no clip below 0, so the static action 0 (which starts with the clip before it) and an individual
	// action given as a number below 0 (read as 52 or more) are refused every time they are tried
	const bool neverStarts = individual ? request.animation > k_LastClipNumber : request.animation == 0;
	std::vector<creature_mind::Step> agenda;
	agenda.reserve(static_cast<size_t>(request.plays) + 1);
	const auto animation = static_cast<size_t>(request.animation);
	// None of the steps has a look of its own: the head looks about throughout
	const creature_mind::Gaze about {};
	for (uint32_t i = 0; i < request.plays; ++i)
	{
		// Each starts once its body is free, and is played through. A static action plays left to right half the time,
		// drawn on every turn it tries to start, those its body refuses included
		if (individual)
		{
			agenda.push_back({.kind = creature_mind::Step::Kind::Action,
			                  .seconds = 0.0f,
			                  .animation = animation,
			                  .neverStarts = neverStarts,
			                  .gaze = about});
		}
		else
		{
			agenda.push_back({.kind = creature_mind::Step::Kind::Static,
			                  .seconds = k_StaticActionSeconds,
			                  .heldInTurns = true,
			                  .animation = animation,
			                  .sequence = {animation - 1, animation, animation + 1},
			                  .mirrorAtRandom = true,
			                  .neverStarts = neverStarts,
			                  .gaze = about});
		}
	}
	// Then it waits until its body is free, a step of its own: done a turn after the body is seen free
	agenda.push_back({.kind = creature_mind::Step::Kind::WaitUntilFree, .gaze = about});
	return agenda;
}

bool creature_script_play::Played(const creature_mind::IdleMind& mind)
{
	return mind.activity == creature_mind::Activity::BeIdle || mind.step >= mind.agenda.size();
}
