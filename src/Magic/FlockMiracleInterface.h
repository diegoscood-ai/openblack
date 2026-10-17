/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/Core/SpellCastData.h"
#include "Particles/SpellLink.h"

namespace openblack::magic
{

/// What the flock miracles do beyond their particles: the flying flock makes doves or bats and the ground flock wolves
/// along the hand's sweep after the cast, sends them off, and fades them out when it is over.
///
/// The spells' table of class operations reaches the flocks through this, for both flock classes; the spell is the
/// entity of the miracle.
class FlockMiracleInterface
{
public:
	FlockMiracleInterface() = default;
	FlockMiracleInterface(const FlockMiracleInterface&) = delete;
	FlockMiracleInterface& operator=(const FlockMiracleInterface&) = delete;
	FlockMiracleInterface(FlockMiracleInterface&&) = delete;
	FlockMiracleInterface& operator=(FlockMiracleInterface&&) = delete;
	virtual ~FlockMiracleInterface() = default;

	/// The miracle's own particle effect: the doves' sparkles or the bats' smoke by the caster's alignment, the wolves'
	/// dust
	[[nodiscard]] virtual ParticleType ParticleTypeOf(entt::entity spell) const = 0;
	/// It starts at a map position: its flock, and its sweep from the hand. 1 when it started.
	virtual int Start(entt::entity spell, const glm::vec3& position, SpellCastData* castData,
	                  const psys::ProcessInfo& info) = 0;
	/// Its turn: the animals the sweep makes, shields they fly into, and their fading once it is over. 5 when the
	/// miracle is over.
	virtual int ProcessTurn(entt::entity spell) = 0;
};

} // namespace openblack::magic
