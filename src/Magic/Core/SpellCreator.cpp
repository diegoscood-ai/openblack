/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellCreator.h"

#include "Camera/Camera.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"
#include "Players.h"
#include "ECS/Components/SpellIcon.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;
using Kind = openblack::ecs::components::SpellCreator::Kind;

creator::SpellCreator creator::NeutralPlayer()
{
	return {Kind::Player, PlayerNames::NEUTRAL, entt::null};
}

creator::SpellCreator creator::OfPlayer(PlayerNames player)
{
	return {Kind::Player, player, entt::null};
}

float creator::MaintainSpell(const SpellCreator& creator, entt::entity /*spell*/, float amount)
{
	switch (creator.kind)
	{
	case Kind::Player:
		// GPlayer::MaintainSpell 0x64C430: this == the neutral player ? amount : 0
		return creator.player == PlayerNames::NEUTRAL ? amount : 0.0f;
	case Kind::Thing:
		return amount; // GameThing::MaintainSpell 0x56FED0
	case Kind::WorshipSpellIcon:
		// WorshipSpellIcon::MaintainSpell 0x77F6F0 -> WorshipSite::MaintainSpell 0x77BC50 / UseChants 0x77BBB0
		if (creator.entity != entt::null && Locator::entitiesRegistry::value().Valid(creator.entity) &&
		    Locator::entitiesRegistry::value().AllOf<ecs::components::WorshipSpellIcon>(creator.entity))
		{
			return worship::icon::MaintainSpell(creator.entity, amount);
		}
		return 0.0f;
	case Kind::Creature:
		// TODO(M8): Creature::MaintainSpell 0x4F8350 (the creature's physical energy)
		return 0.0f;
	case Kind::None:
		break;
	}
	return 0.0f;
}

void creator::UpdateSpellInfo(const SpellCreator& creator, entt::entity spell, psys::ProcessInfo& info)
{
	// GPlayer::UpdateSpellInfo 0x64C470: not for the neutral player; only a spell with an interface status (a hand
	// cast, DoPostCastThings) -> GInterfaceStatus::UpdateSpellInfo 0x5DC8F0
	// WorshipSpellIcon::UpdateSpellInfo 0x77F750: the icon's player's (vt 0x5C) when it is +0x8E0 == 1
	const bool icon = creator.kind == Kind::WorshipSpellIcon && players::IsHuman(creator.player);
	if ((creator.kind != Kind::Player && !icon) || creator.player == PlayerNames::NEUTRAL)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const ecs::components::Spell>(spell);
	if (component == nullptr || !component->castFromInterface || !Locator::handSystem::has_value())
	{
		return;
	}
	// 0x5DC8F0: +0x00 the status's position, +0x0C the hand, +0x18 the camera's forward, +0x24 the hand's velocity
	Locator::handSystem::value().GetSpellInfo(info.interfacePos, info.handPos, info.cameraForward, info.direction);
}

bool creator::IsFunctional(const SpellCreator& creator)
{
	switch (creator.kind)
	{
	case Kind::None:
		return false;
	case Kind::Player:
		return true; // GameThing::IsFunctional 0x405240 -> IsAvailable
	default:
		return creator.entity == entt::null || Locator::entitiesRegistry::value().Valid(creator.entity);
	}
}

bool creator::IsCreature(const SpellCreator& creator)
{
	return creator.kind == Kind::Creature;
}

bool creator::IsHumanPlayerCasting(const SpellCreator& creator)
{
	if (creator.kind != Kind::Player && creator.kind != Kind::WorshipSpellIcon)
	{
		return false;
	}
	return players::IsHuman(creator.player);
}
