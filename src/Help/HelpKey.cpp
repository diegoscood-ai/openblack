/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpKey.h"

#include <cstdio>
#include <cstdlib>

#include <string>
#include <variant>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/vec3.hpp>

#include "ECS/HelpQueryFacts.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "HelpMessageSets.h"
#include "Locator.h"

namespace openblack::help::help_key
{
namespace
{
/// What the last press of the help key answered, for the debug readout, in the debug hooks' store
/// (Locator::debugHooks): empty until the key is first pressed
struct HelpKeyDebugState
{
	std::string last;
};

std::string& Last()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("help key: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HelpKeyDebugState>().last;
}

std::string Describe(const help_query::Answer& answer)
{
	if (const auto* set = std::get_if<help_query::SaySet>(&answer))
	{
		return fmt::format("set {}", set->set);
	}
	const auto& say = std::get<help_query::SayTexts>(answer);
	if (say.townNeed.has_value())
	{
		return fmt::format("{} {}..{} (need {} at {:.0f}%)", say.script, say.texts.first, say.texts.last, say.townNeed->first,
		                   say.townNeed->second);
	}
	return fmt::format("{} {}..{}", say.script, say.texts.first, say.texts.last);
}
} // namespace

bool Say(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, const help_query::Answer& answer,
         std::optional<help_query::Texts> thing, const script_control::Vm& vm, uint32_t turn)
{
	if (const auto* set = std::get_if<help_query::SaySet>(&answer))
	{
		return message_sets::RunMessageSet(help, dialogue, set->set, vm, turn, thing);
	}
	const auto& say = std::get<help_query::SayTexts>(answer);
	return script_control::RunMessage(help, dialogue, say.texts.first, say.texts.last, say.script, vm, turn, say.townNeed);
}

bool Ask(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, const script_control::Vm& vm,
         uint32_t turn)
{
	// What is in the hand, else what the hand is over, else the ground under it
	std::optional<entt::entity> thing;
	if (Locator::handGrabSystem::has_value())
	{
		thing = Locator::handGrabSystem::value().GetHeld();
	}
	std::optional<glm::vec3> ground;
	if (Locator::pickingSystem::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		if (!thing.has_value())
		{
			thing = pick.object;
		}
		ground = pick.land;
	}
	if (thing.has_value() && Locator::entitiesRegistry::value().Valid(*thing))
	{
		const auto answer = help_query::About(ecs::help_query_facts::ThingOf(*thing));
		Last() = fmt::format("thing {}: {}", entt::to_integral(*thing), Describe(answer));
		return Say(help, dialogue, answer, ecs::help_query_facts::TextsOf(*thing), vm, turn);
	}
	const auto answer = help_query::AboutGround(ecs::help_query_facts::GroundAt(ground));
	Last() = fmt::format("ground: {}", Describe(answer));
	return Say(help, dialogue, answer, std::nullopt, vm, turn);
}

const std::string& LastAnswer()
{
	return Last();
}

} // namespace openblack::help::help_key
