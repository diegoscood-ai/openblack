/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EffectSourceRules.h"

#include "ECS/Effects/EffectValues.h"

using namespace openblack;

bool magic::NamesNoApplier(const EffectValues& values)
{
	return values.appliedBy == entt::null && !values.appliedByCreature && !values.appliedByPlayer &&
	       !values.player.has_value() && !values.causedPlayer.has_value();
}

magic::EffectValues magic::ApplierFrom(EffectValues values, const EffectSource& source, bool struck, bool struckIsCreature,
                                       std::optional<PlayerNames> struckPlayer)
{
	const auto sourcePlayer = source.playerless ? std::nullopt : std::optional(source.player);
	if (source.appliedBy.has_value())
	{
		// A blow, or a fall onto the land with nothing that struck: the striker answers for itself, the source's player
		// is blamed
		values.appliedBy = struck ? *source.appliedBy : entt::entity {entt::null};
		values.appliedByCreature = struck && struckIsCreature;
		values.causedPlayer = sourcePlayer;
		values.player = ecs::effects::EffectPlayer(struck, struckPlayer, values.causedPlayer);
		return values;
	}
	if (source.casterCreature != entt::null)
	{
		values.appliedBy = source.casterCreature;
		values.appliedByCreature = true;
		values.player = sourcePlayer;
		return values;
	}
	values.appliedByPlayer = sourcePlayer.has_value();
	values.player = sourcePlayer;
	return values;
}
