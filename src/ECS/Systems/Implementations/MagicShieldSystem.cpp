/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MagicShieldSystem.h"

#include "Magic/Objects/MapShield.h"
#include "Magic/Spells/SpellShield.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void MagicShieldSystem::ProcessTurn()
{
	magic::map_shield::ProcessShields();
}

void MagicShieldSystem::Update([[maybe_unused]] float seconds)
{
	// the domes are drawn by the turn's fraction, not by the frame's time
	magic::map_shield::DrawShields();
}

void MagicShieldSystem::Reset()
{
	magic::map_shield::Clear();
	magic::spell_shield::Clear();
}

bool MagicShieldSystem::KeepsReactionOff(glm::vec3 watcher, glm::vec3 initiator) const
{
	return magic::map_shield::IsReactionBlockedByShield(watcher, initiator);
}
