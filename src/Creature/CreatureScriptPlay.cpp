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

std::optional<std::vector<creature_mind::Step>> creature_script_play::Agenda(const Request& request)
{
	const bool individual = request.animation >= k_FirstIndividualAction;
	if (!individual && request.animation == 0)
	{
		return std::nullopt;
	}
	std::vector<creature_mind::Step> agenda;
	agenda.reserve(static_cast<size_t>(request.plays) + 1);
	const auto animation = static_cast<size_t>(request.animation);
	// None of the steps has a look of its own: the head looks about throughout
	const creature_mind::Gaze about {};
	for (uint32_t i = 0; i < request.plays; ++i)
	{
		// Each starts once its body is free, and is played through
		if (individual)
		{
			agenda.push_back(
			    {.kind = creature_mind::Step::Kind::Action, .seconds = 0.0f, .animation = animation, .gaze = about});
		}
		else
		{
			agenda.push_back({.kind = creature_mind::Step::Kind::Static,
			                  .seconds = k_StaticActionSeconds,
			                  .animation = animation,
			                  .sequence = {animation - 1, animation, animation + 1},
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
