/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/


#include "VillagerReactions.h"

#include "ECS/Effects/Reactions.h"
#include "VillagerFire.h"
#include "VillagerTeleport.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
void VillagerReaction(entt::entity villager, const effects::reactions::Reaction& reaction, float /*distance*/)
{
	switch (reaction.type)
	{
	case openblack::Reaction::ReactToFire:
		villager_fire::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToTeleport:
		villager_teleport::ApplyReaction(villager, reaction);
		break;
	default:
		break;
	}
}

/// The Villager handler from the start (before any map load too); Register() sets it again at each load
const bool k_VillagerHandlerRegistered = [] {
	effects::reactions::SetLivingReactionHandler(effects::reactions::LivingClass::Villager, &VillagerReaction);
	return true;
}();
} // namespace

void villager_reactions::Register()
{
	effects::reactions::SetLivingReactionHandler(effects::reactions::LivingClass::Villager, &VillagerReaction);
}
