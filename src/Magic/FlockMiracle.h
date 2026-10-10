/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "FlockMiracleInterface.h"

namespace openblack::magic
{

/// The flock miracles in the game: their animals come from the animals' AI, their effects from the particle system.
/// It holds no state: the flock and its animals keep theirs on their entities. Its methods are in
/// Spells/SpellFlock.cpp, beside the flock spells' own steps.
class FlockMiracle final: public FlockMiracleInterface
{
public:
	[[nodiscard]] ParticleType ParticleTypeOf(entt::entity spell) const override;
	int Start(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info) override;
	int ProcessTurn(entt::entity spell) override;
};

} // namespace openblack::magic
