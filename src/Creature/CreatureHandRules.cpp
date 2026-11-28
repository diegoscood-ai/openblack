/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandRules.h"

#include <algorithm>

#include "Common/X87Precision.h"
#include "ECS/PlayerAlliances.h"

using namespace openblack;

bool creature_hand::MayHold(const Holdable& creature)
{
	return creature.owner != PlayerNames::NEUTRAL && creature.species != CreatureType::Ogre && !creature.frozen &&
	       !creature.scriptControlled && !creature.outCold;
}

bool creature_hand::IsOutCold(ecs::components::CreatureKnockedOut::Stage stage, bool permanent)
{
	using Stage = ecs::components::CreatureKnockedOut::Stage;
	return !permanent && (stage == Stage::Lying || stage == Stage::FadingOut || stage == Stage::FadingIn);
}

bool creature_hand::IsFriendlyTo(PlayerNames owner, PlayerNames player, const ecs::components::PlayerAlliances& owners)
{
	return owner != PlayerNames::NEUTRAL && (owner == player || ecs::alliances::IsAllied(owners, player));
}

bool creature_hand::TakesPress(const PressFacts& facts)
{
	return facts.isCreature && facts.mayHold && !facts.cannotBePickedUp && !facts.inLockedSelect && !facts.locked &&
	       facts.friendly;
}

bool creature_hand::IsClick(uint32_t heldCameraMs)
{
	return heldCameraMs < k_ClickMaxCameraMs;
}

bool creature_hand::ShowsInteractTip(bool isMine, bool hasPlayer, bool inInfluence, bool scriptControlled)
{
	return !scriptControlled && hasPlayer && (isMine || inInfluence);
}

float creature_hand::HandScaleOnCreature(float drawnSize)
{
	// the runtime's power, the size as a double to the double 0.5, stored as a float
	return game_math::x87::Store(game_math::x87::Pow(std::max(drawnSize, 0.0f), 0.5));
}
