/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashTie.h"

#include <cmath>
#include <cstdint>

#include <bit>

using namespace openblack;
using namespace openblack::creature_leash;

bool creature_leash::ValidAsTarget(TieTargetKind kind)
{
	return kind != TieTargetKind::ScriptHighlight;
}

bool creature_leash::ValidAsLeashTarget(TieTargetKind kind)
{
	return kind != TieTargetKind::LeashPost && kind != TieTargetKind::SpellIcon;
}

HandTie creature_leash::DecideHandTie(const HandTieCheck& check)
{
	if (!check.hasTarget || !check.hasCreature)
	{
		return HandTie::Nothing;
	}
	if (check.tied)
	{
		return check.targetIsTiedObject || check.targetIsCreature ? HandTie::Untie : HandTie::Nothing;
	}
	if (check.targetIsCreature || !ValidAsTarget(check.kind) || !check.leashInThisHand)
	{
		return HandTie::Nothing;
	}
	if (check.kind == TieTargetKind::OneOffSpellSeed)
	{
		return HandTie::Tap;
	}
	return ValidAsLeashTarget(check.kind) ? HandTie::Tie : HandTie::Nothing;
}

LeashTap creature_leash::DecideLeashTapOnObject(const LeashTapCheck& check, TieTargetKind kind)
{
	if (!check.leashOnUntied || !ValidAsTarget(kind) || !ValidAsLeashTarget(kind) || !check.hasCreature ||
	    !check.leashInThisHand || check.alreadyActingForLeash)
	{
		return LeashTap::NotLeash;
	}
	return LeashTap::ActOnObject;
}

bool creature_leash::IsTapOrigin(const glm::vec3& point)
{
	return std::bit_cast<uint32_t>(point.x) == 0 && std::bit_cast<uint32_t>(point.y) == 0 &&
	       (point.z == 0.0f || std::isnan(point.z));
}

LeashTap creature_leash::DecideLeashTapOnLand(const LeashTapCheck& check, bool pointIsOrigin)
{
	if (pointIsOrigin)
	{
		return LeashTap::Nothing;
	}
	if (!check.leashOnUntied || !check.hasCreature || check.alreadyActingForLeash)
	{
		return LeashTap::NotLeash;
	}
	return LeashTap::ActOnPoint;
}
