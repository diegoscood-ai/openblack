/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's body for the particles of the creature spells: where it stands, its drawn size, its bones as posed for
// the game turn (the pose the game reads, never the frame being drawn), its eyes, and the invisible spell's share.
// Wiki: docs/wiki/creature/casting.md#the-creature-spells-particles.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>

#include <utility>

#include "3D/CreatureBody.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureTurnPose.h"
#include "CreatureSpellBodies.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// How far the invisible spell is over the creature: its slot's share, as the creature's own spells ease it in and out
float InvisibleShare(const creature_spells::Spells& spells)
{
	constexpr auto k_Invisible = creature_spells::Spell::Invisible;
	creature_spells::Timing timing;
	if (Locator::infoConstants::has_value())
	{
		const auto type =
		    static_cast<MagicType>(static_cast<size_t>(MagicType::CreatureSpellFreeze) + static_cast<size_t>(k_Invisible));
		if (const auto* row = magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(Locator::infoConstants::value(), type))
		{
			timing = {.startSeconds = row->startTransitionDuration, .finishSeconds = row->finishTransitionDuration};
		}
	}
	return creature_spells::Ratio(spells[k_Invisible], timing, 1.0f / game_clock::k_TurnSeconds);
}
} // namespace

bool CreatureSpellBodies::IsAvailable(entt::entity creature) const
{
	return creature != entt::null && Locator::entitiesRegistry::has_value() &&
	       Locator::entitiesRegistry::value().Valid(creature) && ecs::IsAvailable(creature);
}

std::optional<CreatureSpellBody> CreatureSpellBodies::CreatureBody(entt::entity creature) const
{
	if (creature == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* component = registry.TryGet<const ecs::components::Creature>(creature);
	const auto* transform = registry.TryGet<const ecs::components::Transform>(creature);
	if (component == nullptr || transform == nullptr)
	{
		return std::nullopt;
	}
	CreatureSpellBody body {.origin = transform->position, .size = ecs::creature_pose::DrawnSize(registry, creature)};
	if (const auto* pose = registry.TryGet<const ecs::components::CreatureTurnPose>(creature))
	{
		const auto bones = creature_turn_pose::Safe(*pose);
		body.bones.reserve(bones.size());
		for (const auto& bone : bones)
		{
			body.bones.emplace_back(bone[3]);
		}
	}
	if (Locator::resources::has_value())
	{
		const auto& rigs = Locator::resources::value().GetCreatureRigs();
		const auto rigId = creature::GetRigId(component->species);
		if (rigs.Contains(rigId))
		{
			const auto& rig = *rigs.Handle(rigId);
			body.rightEye = rig.rightEye;
			if (rig.rightEye.has_value())
			{
				body.leftEye = creature_turn_pose::MirrorBone(rig.fileTail, body.bones.size(), *rig.rightEye);
			}
			if (rig.actionPoints.has_value())
			{
				body.rightFoot = rig.actionPoints->rightFoot;
				body.leftFoot = creature_turn_pose::MirrorBone(rig.fileTail, body.bones.size(), rig.actionPoints->rightFoot);
			}
		}
	}
	if (const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(creature))
	{
		body.invisible = InvisibleShare(spells->spells);
	}
	return body;
}
