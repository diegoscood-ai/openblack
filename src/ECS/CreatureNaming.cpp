/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureNaming.h"

#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

creature_names::Owner creature_naming::OwnerOf(PlayerNames owner)
{
	if (owner == PlayerNames::NEUTRAL)
	{
		return creature_names::Owner::Nobody;
	}
	const bool computer = Locator::playerSystem::has_value() ? Locator::playerSystem::value().IsComputerPlayer(owner)
	                                                         : owner != PlayerNames::PLAYER_ONE;
	return computer ? creature_names::Owner::Computer : creature_names::Owner::Player;
}

std::u16string creature_naming::HelpWords(uint32_t text)
{
	if (!Locator::helpTextSystem::has_value())
	{
		return {};
	}
	const auto* texts = Locator::helpTextSystem::value().GetTexts();
	return texts != nullptr ? texts->GetHelpText(text).text : std::u16string {};
}

std::optional<std::u16string> creature_naming::ProfileCreatureName()
{
	if (!Locator::playerProfileSystem::has_value())
	{
		return std::nullopt;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	return gui::ToUtf16(profile->creatureName);
}

void creature_naming::NameNewCreature(components::CreatureMindState& mind, PlayerNames owner, size_t creatures)
{
	const auto profileName = ProfileCreatureName();
	mind.givenName = creature_names::NewName(
	    OwnerOf(owner), creatures, profileName.has_value() ? std::optional<std::u16string_view>(*profileName) : std::nullopt,
	    HelpWords);
}

std::u16string creature_naming::NameOf(const Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const components::CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return {};
	}
	if (mind->scriptName.has_value())
	{
		return *mind->scriptName;
	}
	if (mind->learnt.has_value() && !mind->learnt->name.empty())
	{
		return mind->learnt->name;
	}
	return mind->givenName;
}
