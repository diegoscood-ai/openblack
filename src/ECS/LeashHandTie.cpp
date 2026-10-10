/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashHandTie.h"

#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Components/LeashPost.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using creature_leash::HandTie;
using creature_leash::LeashTap;
using creature_leash::TieTargetKind;
using game_packets::Packet;
using game_packets::Type;

namespace
{
systems::LeashSystemInterface* LocatedLeash()
{
	return Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr;
}

bool Exists(entt::entity entity)
{
	return entity != entt::null && Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(entity);
}
} // namespace

TieTargetKind leash_tie::KindOf(entt::entity target)
{
	if (!Exists(target))
	{
		return TieTargetKind::Object;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (registry.AnyOf<components::LeashPost>(target))
	{
		return TieTargetKind::LeashPost;
	}
	// every kind of spell icon, a worship site's or a town's, has the spell icon
	if (registry.AnyOf<components::SpellIcon>(target))
	{
		return TieTargetKind::SpellIcon;
	}
	if (registry.AnyOf<components::ScriptHighlight>(target))
	{
		return TieTargetKind::ScriptHighlight;
	}
	if (registry.AnyOf<components::OneOffSpellSeed>(target))
	{
		return TieTargetKind::OneOffSpellSeed;
	}
	return TieTargetKind::Object;
}

creature_leash::HandTieCheck leash_tie::CheckDoubleClick(const systems::LeashSystemInterface& leash, PlayerNames player,
                                                         entt::entity target)
{
	creature_leash::HandTieCheck check {
	    .hasTarget = Exists(target),
	    .kind = KindOf(target),
	};
	// The player's creature is their own: it is the one they lead
	const auto creature = leash.PlayersCreature(player);
	if (!creature.has_value())
	{
		return check;
	}
	check.hasCreature = true;
	check.targetIsCreature = target == *creature;
	std::optional<entt::entity> tiedTo;
	if (leash.IsLeashed(*creature))
	{
		tiedTo = leash.TiedTo(*creature);
	}
	check.tied = tiedTo.has_value();
	check.targetIsTiedObject = tiedTo.has_value() && *tiedTo == target;
	// A worn leash is in its holder's hand, who is the creature's owner (the leash comes off otherwise)
	check.leashInThisHand = leash.IsLeashed(*creature);
	return check;
}

creature_leash::LeashTapCheck leash_tie::CheckTap(const systems::LeashSystemInterface& leash, PlayerNames player)
{
	creature_leash::LeashTapCheck check;
	const auto creature = leash.PlayersCreature(player);
	if (!creature.has_value())
	{
		return check;
	}
	check.hasCreature = true;
	check.leashInThisHand = leash.IsLeashed(*creature);
	check.leashOnUntied = check.leashInThisHand && !leash.TiedTo(*creature).has_value();
	return check;
}

HandTie leash_tie::DoubleClick(const systems::LeashSystemInterface* leash, PlayerNames player, entt::entity target)
{
	if (leash == nullptr)
	{
		return HandTie::Nothing;
	}
	const auto decision = creature_leash::DecideHandTie(CheckDoubleClick(*leash, player, target));
	if (decision == HandTie::Tie || decision == HandTie::Untie)
	{
		game_packets::Push({
		    .type = Type::LeashTie,
		    .object = decision == HandTie::Tie ? target : entt::entity {entt::null},
		    .player = player,
		});
	}
	return decision;
}

bool leash_tie::TapObject(const systems::LeashSystemInterface* leash, PlayerNames player, entt::entity target)
{
	if (leash == nullptr || !Exists(target) ||
	    creature_leash::DecideLeashTapOnObject(CheckTap(*leash, player), KindOf(target)) != LeashTap::ActOnObject)
	{
		return false;
	}
	game_packets::Push({.type = Type::LeashActOnObject, .object = target, .player = player});
	return true;
}

LeashTap leash_tie::TapLand(const systems::LeashSystemInterface* leash, PlayerNames player, const glm::vec3& point)
{
	// The origin is tested first, before anything of the leash's
	if (creature_leash::IsTapOrigin(point))
	{
		return LeashTap::Nothing;
	}
	if (leash == nullptr || creature_leash::DecideLeashTapOnLand(CheckTap(*leash, player), false) != LeashTap::ActOnPoint)
	{
		return LeashTap::NotLeash;
	}
	game_packets::Push({.type = Type::LeashActOnPoint, .position = point, .player = player});
	return LeashTap::ActOnPoint;
}

HandTie leash_tie::OnHandDoubleClick(PlayerNames player, entt::entity target)
{
	return DoubleClick(LocatedLeash(), player, target);
}

bool leash_tie::OnHandTap(PlayerNames player, entt::entity target)
{
	return TapObject(LocatedLeash(), player, target);
}

LeashTap leash_tie::OnHandTap(PlayerNames player, const glm::vec3& point)
{
	return TapLand(LocatedLeash(), player, point);
}

bool leash_tie::ApplyTie(systems::LeashSystemInterface* leash, const Packet& packet)
{
	if (leash == nullptr)
	{
		return false;
	}
	const auto creature = leash->PlayersCreature(packet.player);
	if (!creature.has_value() || !leash->Knows(*creature, LeashType::Rope) || !leash->IsLeashed(*creature) ||
	    !leash->Works(*creature))
	{
		return false;
	}
	if (packet.object == entt::null)
	{
		leash->ReturnToHand(*creature);
		return true;
	}
	// A tie that does not take still pulls the creature away and sends it to the object
	(void)leash->TieTo(*creature, packet.object);
	leash->PullAwayFromAction(*creature);
	leash->ActOn(*creature, packet.object);
	return true;
}

bool leash_tie::ApplyActOnObject(systems::LeashSystemInterface* leash, const Packet& packet)
{
	if (leash == nullptr || packet.object == entt::null)
	{
		return false;
	}
	const auto creature = leash->PlayersCreature(packet.player);
	// Only whether the player's leash works is tested, not that it is on
	if (!creature.has_value() || (leash->IsLeashed(*creature) && !leash->Works(*creature)))
	{
		return false;
	}
	leash->ActOn(*creature, packet.object);
	return true;
}

bool leash_tie::ApplyActOnPoint(const systems::LeashSystemInterface* leash, const Packet& packet)
{
	if (leash == nullptr)
	{
		return false;
	}
	const auto creature = leash->PlayersCreature(packet.player);
	if (!creature.has_value() || !leash->IsLeashed(*creature) || !leash->Works(*creature))
	{
		return false;
	}
	// (not ported) acting on a point of the land
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} can't be sent to act on a point yet", entt::to_integral(*creature));
	return true;
}

void leash_tie::RegisterPacketHandlers()
{
	game_packets::SetHandler(Type::LeashTie, [](const Packet& packet) { (void)ApplyTie(LocatedLeash(), packet); });
	game_packets::SetHandler(Type::LeashActOnObject,
	                         [](const Packet& packet) { (void)ApplyActOnObject(LocatedLeash(), packet); });
	game_packets::SetHandler(Type::LeashActOnPoint,
	                         [](const Packet& packet) { (void)ApplyActOnPoint(LocatedLeash(), packet); });
}
